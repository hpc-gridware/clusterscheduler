/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Users and projects
 */
/*
   This is the module for handling users and projects.
   We save users to <spool>/qmaster/$USER_DIR and
                                    $PROJECT_DIR
 */

#include <cstdio>
#include <cctype>
#include <cstring>
#include <unistd.h>
#include <type_traits>

#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_Session.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_sharetree.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_suser.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_APRJLIST.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_SU.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#include "ocs_CategoryQmaster.h"
#include "sge_utility_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_userset_qmaster.h"
#include "sge_sharetree_qmaster.h"
#include "evm/sge_event_master.h"
#include "sge_resource_quota_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "spool/sge_spooling.h"

#include "msg_common.h"
#include "msg_qmaster.h"
#include "ocs_Bootstrap.h"


namespace {
   /** @brief The fields and events a user object (UU_Type) shares with a project object */
   struct UserFields {
      using Type = ocs::tom::UU;                                         ///< the object type
      static constexpr auto name = UU_name;                              ///< the key field
      static constexpr auto oticket = UU_oticket;                        ///< override tickets
      static constexpr auto fshare = UU_fshare;                          ///< functional shares
      static constexpr auto usage = UU_usage;                            ///< the usage list
      static constexpr auto version = UU_version;                        ///< the usage version
      static constexpr auto project = UU_project;                        ///< the per project usage
      static constexpr auto consider = UU_consider_with_categories;      ///< used by categories
      static constexpr int filter = RQR_filter_users;                    ///< the resource quota filter
      static constexpr ev_event add_event = sgeE_USER_ADD;               ///< the add event
      static constexpr ev_event mod_event = sgeE_USER_MOD;               ///< the modify event
      static constexpr ev_event del_event = sgeE_USER_DEL;               ///< the delete event
      static constexpr int node_type = STT_USER;                         ///< the share tree node type
   };

   /** @brief The fields and events a project object (PR_Type) shares with a user object */
   struct ProjectFields {
      using Type = ocs::tom::PR;                                         ///< the object type
      static constexpr auto name = PR_name;                              ///< the key field
      static constexpr auto oticket = PR_oticket;                        ///< override tickets
      static constexpr auto fshare = PR_fshare;                          ///< functional shares
      static constexpr auto usage = PR_usage;                            ///< the usage list
      static constexpr auto version = PR_version;                        ///< the usage version
      static constexpr auto project = PR_project;                        ///< the per project usage
      static constexpr auto consider = PR_consider_with_categories;      ///< used by categories
      static constexpr int filter = RQR_filter_projects;                 ///< the resource quota filter
      static constexpr ev_event add_event = sgeE_PROJECT_ADD;            ///< the add event
      static constexpr ev_event mod_event = sgeE_PROJECT_MOD;            ///< the modify event
      static constexpr ev_event del_event = sgeE_PROJECT_DEL;            ///< the delete event
      static constexpr int node_type = STT_PROJECT;                      ///< the share tree node type
   };
}

static int
do_add_auto_user(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::UU> anUser,
                 lList **anAnswer, monitoring_t *monitor);

/** @brief Apply the attribute changes to a user or a project
 *
 * @tparam F UserFields or ProjectFields
 * @param alpp receives messages for the caller
 * @param modp the object to change, an empty one on add
 * @param ep the object holding the new values, reduced to the attributes to change
 * @param add 1 for add, 0 for modify
 * @param obj_name the name of the object type, for messages
 * @return 0 on success, STATUS_EUNKNOWN on error
 */
template <typename F>
static int
ocs_userprj_mod(lList **alpp, const ocs::tom::Element<typename F::Type> modp,
                const ocs::tom::Element<typename F::Type> ep, const int add, const char *obj_name) {
   DENTER(TOP_LAYER);

   constexpr bool user_flag = std::is_same_v<typename F::Type, ocs::tom::UU>;
   const ocs::tom::ConstList<ocs::tom::US> master_userset_list(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET));
   const ocs::tom::ConstList<typename F::Type> obj_master_list(
      *ocs::DataStore::get_master_list(user_flag ? SGE_TYPE_USER : SGE_TYPE_PROJECT));

   /* ---- UP_name */
   const char *userprj = ocs::tom::get<F::name>(ep);
   if (add) {
      if (!strcmp(userprj, "default")) {
         ERROR(MSG_UP_NOADDDEFAULT_S, obj_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      /* may not add user/project with same name as an existing project/user */
      if (ocs::tom::find_first<F::name>(obj_master_list, userprj)) {
         ERROR(MSG_UP_ALREADYEXISTS_SS, obj_name, userprj);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      if (verify_obj_name(alpp, userprj, MAX_VERIFY_STRING, obj_name,
         user_flag ? "\\" : nullptr) != STATUS_OK) {
         DRETURN(STATUS_EUNKNOWN);
      }
      ocs::tom::set<F::name>(modp, userprj);
   }

   /* ---- UP_oticket */
   if (ocs::tom::has<F::oticket>(ep)) {
      ocs::tom::set<F::oticket>(modp, ocs::tom::get<F::oticket>(ep));
   }

   /* ---- UP_fshare */
   if (ocs::tom::has<F::fshare>(ep)) {
      ocs::tom::set<F::fshare>(modp, ocs::tom::get<F::fshare>(ep));
   }

   if constexpr (user_flag) {
      /* ---- UU_delete_time */
      if (ocs::tom::has<UU_delete_time>(ep)) {
         ocs::tom::set<UU_delete_time>(modp, ocs::tom::get<UU_delete_time>(ep));
      }
   }

   /* ---- UP_usage */
   if (ocs::tom::has<F::usage>(ep)) {
      ocs::tom::set<F::usage>(modp, ocs::tom::get<F::usage>(ep).clone("usage"));
      ocs::tom::add<F::version>(modp, 1);
   }

   /* ---- UP_project */
   if (ocs::tom::has<F::project>(ep)) {
      ocs::tom::set<F::project>(modp, ocs::tom::get<F::project>(ep).clone("project"));
      ocs::tom::add<F::version>(modp, 1);
   }

   if constexpr (user_flag) {
      /* ---- UU_default_project */
      if (ocs::tom::has<UU_default_project>(ep)) {
         ocs_null_out_none<UU_default_project>(ep);
         /* make sure default project exists */
         const char *dproj = ocs::tom::get<UU_default_project>(ep);
         if (dproj) {
            const ocs::tom::ConstList<ocs::tom::PR> master_project_list(
               *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT));
            if (!master_project_list || !prj_list_locate(master_project_list, dproj)) {
               ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_PRJ, dproj);
               answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_EUNKNOWN);
            }
         }

         ocs::tom::set<UU_default_project>(modp, dproj);
      }
   } else {
      /* ---- PR_acl */
      if (ocs::tom::has<PR_acl>(ep)) {
         ocs::tom::set<PR_acl>(modp, ocs::tom::get<PR_acl>(ep).clone("acl"));

         if (userset_list_validate_acl_list(ocs::tom::get<PR_acl>(ep), alpp, master_userset_list) != STATUS_OK) {
            /* answerlist gets filled by userset_list_validate_acl_list() in case of errors */
            DRETURN(STATUS_EUNKNOWN);
         }
      }

      /* ---- PR_xacl */
      if (ocs::tom::has<PR_xacl>(ep)) {
         ocs::tom::set<PR_xacl>(modp, ocs::tom::get<PR_xacl>(ep).clone("xacl"));
         if (userset_list_validate_acl_list(ocs::tom::get<PR_xacl>(ep), alpp, master_userset_list) != STATUS_OK) {
            /* answerlist gets filled by userset_list_validate_acl_list() in case of errors */
            DRETURN(STATUS_EUNKNOWN);
         }
      }

      if (ocs::tom::has<PR_xacl>(modp) ||
          ocs::tom::has<PR_acl>(modp)) {
         if (multiple_occurances(alpp,
                                 ocs::tom::get<PR_acl>(modp),
                                 ocs::tom::get<PR_xacl>(modp),
                                 US_name, userprj,
                                 "project")) {
            DRETURN(STATUS_EUNKNOWN);
         }
      }
   }

   DRETURN(0);
}

/** @brief Apply one attribute change to an user or project
 *
 * The gdi_object_t::modifier for user or projects; see sge_c_gdi.h for the sequence it is called from.
 * The raw parameters are fixed by modifier_func_t; the elements are of the type of the table entry.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param modp the object to change, an empty one on add
 * @param ep the object holding the new values
 * @param add 1 for add, 0 for modify
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param object the table entry for this object type
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
userprj_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *modp, lListElem *ep, int add,
            const char *ruser, const char *rhost, gdi_object_t *object,
            ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
            monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int ret;
   if (object->target == ocs::gdi::Target::UU_LIST) {
      ret = ocs_userprj_mod<UserFields>(alpp, ocs::tom::Element<ocs::tom::UU>(modp),
                                        ocs::tom::Element<ocs::tom::UU>(ep), add, MSG_OBJ_USER);
   } else {
      ret = ocs_userprj_mod<ProjectFields>(alpp, ocs::tom::Element<ocs::tom::PR>(modp),
                                           ocs::tom::Element<ocs::tom::PR>(ep), add, MSG_OBJ_PRJ);
   }

   DRETURN(ret);
}

/** @brief Announce a user or project change once it is safely spooled
 *
 * @tparam F UserFields or ProjectFields
 * @param packet the client request
 * @param ep the object
 * @param is_mod true for a modification, false for an add
 */
template <typename F>
static void
ocs_userprj_success(ocs::gdi::Packet *packet, const ocs::tom::Element<typename F::Type> ep, const bool is_mod) {
   DENTER(TOP_LAYER);

   const ocs::tom::ConstList<ocs::tom::RQS> master_rqs_list(*ocs::DataStore::get_master_list(SGE_TYPE_RQS));
   bool reattach_categories = false;
   for (const auto rqs : master_rqs_list) {
      if (scope_is_referenced_rqs(rqs, F::filter, ocs::tom::get<F::name>(ep))) {
         ocs::tom::set<F::consider>(ep, true);
         reattach_categories = true;
         break;
      }
   }

   if (reattach_categories) {
      const ocs::tom::List<ocs::tom::JB> master_job_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB));
      const ocs::tom::ConstList<ocs::tom::US> master_userset_list(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET));
      const ocs::tom::ConstList<ocs::tom::PR> master_project_list(*ocs::DataStore::get_master_list(SGE_TYPE_PROJECT));
      ocs::CategoryQmaster::reattach_all_jobs(master_job_list, master_userset_list, master_project_list,
                                              master_rqs_list, true, packet->gdi_session);
   }

   sge_add_event(0, is_mod ? F::mod_event : F::add_event, 0, 0, ocs::tom::get<F::name>(ep), nullptr, nullptr, ep,
                 packet->gdi_session);

   DRETURN_VOID;
}

/** @brief Announce an user or project change once it is safely spooled
 *
 * The gdi_object_t::on_success for user or projects. The raw parameters are
 * fixed by on_success_func_t; the element is of the type of the table entry.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the object
 * @param old_ep the object as it was, or nullptr on add
 * @param object the table entry for this object type
 * @param ppList receives information for post processing
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
userprj_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object,
                lList **ppList, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   if (object->target == ocs::gdi::Target::UU_LIST) {
      ocs_userprj_success<UserFields>(packet, ocs::tom::Element<ocs::tom::UU>(ep), old_ep != nullptr);
   } else {
      ocs_userprj_success<ProjectFields>(packet, ocs::tom::Element<ocs::tom::PR>(ep), old_ep != nullptr);
   }

   DRETURN(0);
}

/** @brief Write an user or project to the spool
 *
 * The gdi_object_t::writer for user or projects. The raw parameters are fixed
 * by writer_func_t; the element is of the type of the table entry.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param upe the user or project to spool
 * @param object the table entry for this object type
 * @return 0 on success
 */
int
userprj_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *upe, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   const bool user_flag = object->target == ocs::gdi::Target::UU_LIST;
   const char *name = user_flag ? ocs::tom::get<UU_name>(ocs::tom::ConstElement<ocs::tom::UU>(upe))
                                : ocs::tom::get<PR_name>(ocs::tom::ConstElement<ocs::tom::PR>(upe));

   /* write user or project to file */
   lList *answer_list = nullptr;
   const bool dbret = spool_write_object(&answer_list, spool_get_default_context(), upe, name,
                                         user_flag ? SGE_TYPE_USER : SGE_TYPE_PROJECT);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, name);
   }

   DRETURN(dbret ? 0 : 1);
}

/** @brief Check that no queue, host, the global configuration or user references a project
 *
 * Fix for bug 6422335: the cq configuration is checked for project
 * references instead of the qinstances.
 *
 * @param alpp receives messages for the caller
 * @param name the project
 * @return STATUS_OK if the project is not referenced, STATUS_EEXIST otherwise
 */
static int
ocs_verify_project_deletion(lList **alpp, const char *name) {
   DENTER(TOP_LAYER);

   for (const auto cqueue : ocs::tom::ConstList<ocs::tom::CQ>(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE))) {
      for (const auto prj : ocs::tom::get<CQ_projects>(cqueue)) {
         if (ocs::tom::find_first<PR_name>(ocs::tom::get<APRJLIST_value>(prj), name)) {
            ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_PRJS, MSG_OBJ_QUEUE,
                  ocs::tom::get<CQ_name>(cqueue));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      }
      for (const auto prj : ocs::tom::get<CQ_xprojects>(cqueue)) {
         if (ocs::tom::find_first<PR_name>(ocs::tom::get<APRJLIST_value>(prj), name)) {
            ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_XPRJS, MSG_OBJ_QUEUE,
                  ocs::tom::get<CQ_name>(cqueue));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      }
   }

   /* check hosts */
   for (const auto host : ocs::tom::ConstList<ocs::tom::EH>(*ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST))) {
      if (prj_list_locate(ocs::tom::get<EH_prj>(host), name)) {
         ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_PRJS, MSG_OBJ_EH,
               static_cast<const char *>(ocs::tom::get<EH_name>(host)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
      if (prj_list_locate(ocs::tom::get<EH_xprj>(host), name)) {
         ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_XPRJS, MSG_OBJ_EH,
               static_cast<const char *>(ocs::tom::get<EH_name>(host)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   }

   /* check global configuration */
   if (prj_list_locate(*mconf_get_projects(), name)) {
      ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_PRJS, MSG_OBJ_CONF, MSG_OBJ_GLOBAL);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }
   if (prj_list_locate(*mconf_get_xprojects(), name)) {
      ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_XPRJS, MSG_OBJ_CONF, MSG_OBJ_GLOBAL);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* check user list for reference */
   const ocs::tom::ConstList<ocs::tom::UU> master_user_list(*ocs::DataStore::get_master_list(SGE_TYPE_USER));
   if (const auto user = ocs::tom::find_first<UU_default_project>(master_user_list, name); user) {
      ERROR(MSG_USERPRJ_PRJXSTILLREFERENCEDINENTRYX_SS, name, ocs::tom::get<UU_name>(user));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   DRETURN(STATUS_OK);
}

/** @brief Delete a user or a project
 *
 * Master code: delete a user or project. Refused while the object is
 * referenced in the share tree, and for a project while a queue, a host, the
 * global configuration or a user references it.
 *
 * @tparam F UserFields or ProjectFields
 * @param packet the client request
 * @param task the GDI task being answered
 * @param up_ep the user or project
 * @param alpp receives messages for the caller
 * @param upl the master list
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success
 */
template <typename F>
static int
ocs_del_userprj(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<typename F::Type> up_ep,
                lList **alpp, const ocs::tom::List<typename F::Type> upl, const char *ruser, const char *rhost) {
   DENTER(TOP_LAYER);

   constexpr bool user = std::is_same_v<typename F::Type, ocs::tom::UU>;

   if (!up_ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *name = ocs::tom::get<F::name>(up_ep);
   ocs::tom::Element<typename F::Type> ep;
   if constexpr (user) {
      ep = user_list_locate(upl, name);
   } else {
      ep = prj_list_locate(upl, name);
   }
   if (!ep) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, user ? MSG_OBJ_USER : MSG_OBJ_PRJ, name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* ensure this u/p object is not referenced in actual share tree */
   const ocs::tom::ConstList<ocs::tom::STN> share_tree(*ocs::DataStore::get_master_list(SGE_TYPE_SHARETREE));
   if (getNode(share_tree, name, F::node_type, 0)) {
      ERROR(MSG_SGETEXT_CANT_DELETE_UP_IN_SHARE_TREE_SS, user ? MSG_OBJ_USER : MSG_OBJ_PRJ, name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   if constexpr (!user) { /* ensure this project is not referenced in any queue */
      if (const int ret = ocs_verify_project_deletion(alpp, name); ret != STATUS_OK) {
         DRETURN(ret);
      }
   }

   /* delete user or project file */
   if (!sge_event_spool(alpp, 0, F::del_event, 0, 0, name, nullptr, nullptr, nullptr, nullptr, nullptr,
                        true, true, packet->gdi_session)) {

      DRETURN(STATUS_EDISK);
   }

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, name, user ? MSG_OBJ_USER : MSG_OBJ_PRJ);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);

   upl.erase(ep);

   DRETURN(STATUS_OK);
}

/** @brief Delete a user
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param up_ep the user
 * @param alpp receives messages for the caller
 * @param upl the master user list
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success
 */
int
sge_del_userprj(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::UU> up_ep,
                lList **alpp, const ocs::tom::List<ocs::tom::UU> upl, const char *ruser, const char *rhost) {
   DENTER(TOP_LAYER);
   const int ret = ocs_del_userprj<UserFields>(packet, task, up_ep, alpp, upl, ruser, rhost);
   DRETURN(ret);
}

/** @brief Delete a project
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param up_ep the project
 * @param alpp receives messages for the caller
 * @param upl the master project list
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success
 */
int
sge_del_userprj(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::PR> up_ep,
                lList **alpp, const ocs::tom::List<ocs::tom::PR> upl, const char *ruser, const char *rhost) {
   DENTER(TOP_LAYER);
   const int ret = ocs_del_userprj<ProjectFields>(packet, task, up_ep, alpp, upl, ruser, rhost);
   DRETURN(ret);
}

/** @brief sge_del_userprj() for an untyped caller
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the user (UU_Type) or project (PR_Type)
 * @param alpp receives messages for the caller
 * @param upl the master list of the type
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param user 1 for a user, 0 for a project
 * @return STATUS_OK on success
 */
int
sge_del_userprj(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **alpp, lList **upl,
                const char *ruser, const char *rhost, int user) {
   if (user) {
      return sge_del_userprj(packet, task, ocs::tom::ConstElement<ocs::tom::UU>(ep), alpp,
                             ocs::tom::List<ocs::tom::UU>(*upl), ruser, rhost);
   }
   return sge_del_userprj(packet, task, ocs::tom::ConstElement<ocs::tom::PR>(ep), alpp,
                          ocs::tom::List<ocs::tom::PR>(*upl), ruser, rhost);
}

/** @brief Check that every named project exists
 * @param alpp receives messages for the caller
 * @param name_list the project names to verify
 * @param prj_list the known projects
 * @param attr_name the attribute the names came from, for the message
 * @param obj_descr what kind of object it belongs to, for the message
 * @param obj_name the object's name, for the message
 * @return STATUS_OK when every project exists
 */
int
verify_project_list(lList **alpp, const ocs::tom::ConstList<ocs::tom::PR> name_list,
                    const ocs::tom::ConstList<ocs::tom::PR> prj_list, const char *attr_name, const char *obj_descr,
                    const char *obj_name) {
   DENTER(TOP_LAYER);

   for (const auto up : name_list) {
      if (!ocs::tom::find_first<PR_name>(prj_list, ocs::tom::get<PR_name>(up))) {
         ERROR(MSG_SGETEXT_UNKNOWNPROJECT_SSSS, ocs::tom::get<PR_name>(up), attr_name, obj_descr, obj_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   DRETURN(STATUS_OK);
}

/** @brief Remove automatically created users that have gone idle
 *
 * Handles automatically deleting automatic user objects which have expired.
 * A user that still has jobs gets its delete time pushed out, a user that
 * cannot be deleted becomes a non auto user.
 *
 * @param anEvent the timed event that fired
 * @param monitor for monitoring qmaster threads
 */
void
sge_automatic_user_cleanup_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   uint64_t auto_user_delete_time = sge_gmt32_to_gmt64(mconf_get_auto_user_delete_time());
   const char *admin = ocs::Bootstrap::get_admin_user();
   const char *qmaster_host = component_get_qualified_hostname();

   /* shall auto users be deleted again? */
   if (auto_user_delete_time > 0) {
      uint64_t now = sge_get_gmt64();
      uint64_t next_delete = now + auto_user_delete_time;

      MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), monitor);
      const ocs::tom::List<ocs::tom::UU> master_user_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_USER));
      const ocs::tom::ConstList<ocs::tom::SU> master_suser_list(*ocs::DataStore::get_master_list(SGE_TYPE_SUSER));

      /*
       * Check each user for deletion time. We don't use a range-for
       * because we are deleting entries.
       */
      ocs::tom::Element<ocs::tom::UU> next;
      for (auto user = master_user_list.front(); user; user = next) {
         uint64_t delete_time = ocs::tom::get<UU_delete_time>(user);
         next = user.next();

         /* 
          * For non auto users, delete_time = 0.
          * Never delete them automatically 
          */
         if (delete_time > 0) {
            const char *name = ocs::tom::get<UU_name>(user);

            /* if the user has jobs, we increment the delete time */
            if (suser_get_job_counter(suser_list_find(master_suser_list, name)) > 0) {
               ocs::tom::set<UU_delete_time>(user, next_delete);
            } else {
               /* if the delete time has expired, delete user */
               if (delete_time <= now) {
                  lList *answer_list = nullptr;
                  ocs::gdi::Packet packet;
                  ocs::gdi::Task task;

                  packet.gdi_session = ocs::SessionManager::GDI_SESSION_NONE;
                  if (sge_del_userprj(&packet, &task, user, &answer_list, master_user_list, admin, qmaster_host) !=
                      STATUS_OK) {
                     /* 
                      * if deleting the user failes (due to user being referenced
                      * in other objects, e.g. queue), regard him as non auto user
                      */
                     ocs::tom::set<UU_delete_time>(user, 0);
                  }
                  /* output low level error messages */
                  answer_list_output(&answer_list);
               }
            }
         }
      }
      SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
   }

   DRETURN_VOID;
}

/** @brief Create a user object on demand, from the auto_user template
 *
 * Handles automatically adding GEEE user objects, called in
 * ocs::gdi::Client::sge_gdi_add_job.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param user the user
 * @param alpp receives messages for the caller
 * @param monitor for monitoring qmaster threads
 * @return STATUS_OK on success
 */
int
sge_add_auto_user(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *user, lList **alpp,
                  monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int status = STATUS_OK;
   uint64_t auto_user_delete_time = sge_gmt32_to_gmt64(mconf_get_auto_user_delete_time());

   const auto uep = user_list_locate(ocs::tom::ConstList<ocs::tom::UU>(*ocs::DataStore::get_master_list(SGE_TYPE_USER)),
                                     user);

   /* if the user already exists */
   if (uep) {
      /* and is an auto user */
      if (ocs::tom::get<UU_delete_time>(uep) != 0) {
         /* and we shall not keep auto users forever */
         if (auto_user_delete_time > 0) {
            ocs::tom::set<UU_delete_time>(uep, sge_get_gmt64() + auto_user_delete_time);
         }
      }
      /* and we are done */
      DRETURN(STATUS_OK);
   }

   /* create a new auto user */
   auto uep_uu = ocs::tom::ElementPtr<ocs::tom::UU>::create();
   char *auto_user_default_project = nullptr;

   /* set user object attributes */
   ocs::tom::set<UU_name>(uep_uu, user);

   if (auto_user_delete_time > 0) {
      ocs::tom::set<UU_delete_time>(uep_uu, sge_get_gmt64() + auto_user_delete_time);
   } else {
      ocs::tom::set<UU_delete_time>(uep_uu, 0);
   }

   ocs::tom::set<UU_oticket>(uep_uu, mconf_get_auto_user_oticket());
   ocs::tom::set<UU_fshare>(uep_uu, mconf_get_auto_user_fshare());

   auto_user_default_project = mconf_get_auto_user_default_project();
   if (auto_user_default_project == nullptr || strcasecmp(auto_user_default_project, "none") == 0) {
      ocs::tom::set<UU_default_project>(uep_uu, nullptr);
   } else {
      ocs::tom::set<UU_default_project>(uep_uu, auto_user_default_project);
   }

   /* add the auto user via GDI request */
   status = do_add_auto_user(packet, task, *uep_uu, alpp, monitor);
   uep_uu.reset();
   sge_free(&auto_user_default_project);

   DRETURN(status);
}

/**
 * @brief Add auto user to SGE_UU_LIST
 *
 * The user is added by a GDI add request of the admin user on the qmaster host.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param anUser the user to add
 * @param anAnswer receives the first message of a failed add
 * @param monitor for monitoring qmaster threads
 * @return STATUS_OK on success
 *
 * @note MT-NOTE: do_add_auto_user() is not MT safe
 */
static int
do_add_auto_user(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::UU> anUser,
                 lList **anAnswer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int res = STATUS_EUNKNOWN;
   gdi_object_t *userList = nullptr;
   lList *tmpAnswer = nullptr;
   lList *ppList = nullptr;
   const char *admin_user = ocs::Bootstrap::get_admin_user();
   const char *qualified_hostname = component_get_qualified_hostname();

   userList = get_gdi_object(ocs::gdi::Target::UU_LIST);

   /* 
    * Add anUser to the user list.
    * Owner of the operation is the admin user on the qmaster host.
    */
   res = sge_gdi_add_mod_generic(packet, task, &tmpAnswer, anUser, 1, userList, admin_user, qualified_hostname,
                                 ocs::gdi::Command::NONE, ocs::gdi::SubCommand::NONE, &ppList, monitor);

   lFreeList(&ppList);
   if ((STATUS_OK != res) && (nullptr != tmpAnswer)) {
      const lListElem *err = lFirst(tmpAnswer);
      const char *text = ocs::tom::get<AN_text>(err);
      uint32_t status = ocs::tom::get<AN_status>(err);
      answer_quality_t quality = (answer_quality_t) ocs::tom::get<AN_quality>(err);

      answer_list_add(anAnswer, text, status, quality);
   }

   if (tmpAnswer != nullptr) {
      lFreeList(&tmpAnswer);
   }

   DRETURN(res);
}

/**
 * @brief Updates the spooled user and projects
 *
 * The usage is only stored every 2 min. To have the acual usage stored when
 * the qmaster is going down, we have to through all user/projects and store
 * them again.
 *
 * @param gdi_session the session the spooling belongs to
 *
 * @note MT-NOTE: sge_userprj_spool() is not MT safe, because it is working on global
 *       master lists (only reading)
 */
void sge_userprj_spool(uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   lList *answer_list = nullptr;
   uint64_t now = sge_get_gmt64();

   /* this function is used on qmaster shutdown, no need to monitor this lock */
   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);

   for (const auto elem : ocs::tom::List<ocs::tom::UU>(*ocs::DataStore::get_master_list_rw(SGE_TYPE_USER, true))) {
      const char *name = ocs::tom::get<UU_name>(elem);
      sge_event_spool(&answer_list, now, sgeE_USER_MOD, 0, 0, name, nullptr, nullptr, elem, nullptr, nullptr, false,
                      true, gdi_session);
   }

   for (const auto elem : ocs::tom::List<ocs::tom::PR>(*ocs::DataStore::get_master_list_rw(SGE_TYPE_PROJECT, true))) {
      const char *name = ocs::tom::get<PR_name>(elem);
      sge_event_spool(&answer_list, now, sgeE_PROJECT_MOD, 0, 0, name, nullptr, nullptr, elem, nullptr, nullptr, false,
                      true, gdi_session);
   }

   SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);

   answer_list_output(&answer_list);

   DRETURN_VOID;
}

/**
 * @brief True, if project still used
 *
 * Returns true, if project is still used as ACL with host_conf(5),
 * queue_conf(5) or in a resource quota set.
 * Use of projects as ACLs in sge_conf(5) play no role here,
 * since such ACLs are checked in qmaster and thus are not
 * relevant for the scheduling algorithm.
 *
 * @param p the project
 *
 * @return True, if project still used
 *
 * @note MT-NOTE: project_still_used() is not MT safe
 */
static bool project_still_used(const char *p) {
   DENTER(TOP_LAYER);

   for (const auto rqs : ocs::tom::ConstList<ocs::tom::RQS>(*ocs::DataStore::get_master_list(SGE_TYPE_RQS))) {
      if (scope_is_referenced_rqs(rqs, RQR_filter_projects, p)) {
         DRETURN(true);
      }
   }

   for (const auto hep : ocs::tom::ConstList<ocs::tom::EH>(*ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST))) {
      if (ocs::tom::find_first<PR_name>(ocs::tom::get<EH_prj>(hep), p) ||
          ocs::tom::find_first<PR_name>(ocs::tom::get<EH_xprj>(hep), p)) {
         DRETURN(true);
      }
   }

   for (const auto cq : ocs::tom::ConstList<ocs::tom::CQ>(*ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE))) {
      for (const auto qc : ocs::tom::get<CQ_projects>(cq)) {
         if (ocs::tom::find_first<PR_name>(ocs::tom::get<APRJLIST_value>(qc), p)) {
            DRETURN(true);
         }
      }
      for (const auto qc : ocs::tom::get<CQ_xprojects>(cq)) {
         if (ocs::tom::find_first<PR_name>(ocs::tom::get<APRJLIST_value>(qc), p)) {
            DRETURN(true);
         }
      }
   }

   DRETURN(false);
}


/**
 * @brief Update all projects wrts categories
 *
 * Each added/removed project is verified whether it is used first
 * time/still as ACL for host_conf(5)/queue_conf(5). If so an event
 * is sent.
 *
 * @param added List of added project references (PR_Type)
 * @param removed List of removed project references (PR_Type)
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: project_update_categories() is not MT safe
 */
void
project_update_categories(const ocs::tom::ConstList<ocs::tom::PR> added,
                          const ocs::tom::ConstList<ocs::tom::PR> removed, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   const ocs::tom::List<ocs::tom::PR> master_project_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_PROJECT,
                                                                                               true));
   bool reattach_categories = false;

   for (const auto ep : added) {
      const char *p = ocs::tom::get<PR_name>(ep);
      DPRINTF("added project: \"%s\"\n", p);
      const auto prj = ocs::tom::find_first_rw<PR_name>(master_project_list, p);
      if (prj && !ocs::tom::get<PR_consider_with_categories>(prj)) {
         ocs::tom::set<PR_consider_with_categories>(prj, true);
         reattach_categories = true;
         sge_add_event(0, sgeE_PROJECT_MOD, 0, 0, p, nullptr, nullptr, prj, gdi_session);
      }
   }

   for (const auto ep : removed) {
      const char *p = ocs::tom::get<PR_name>(ep);
      DPRINTF("removed project: \"%s\"\n", p);
      const auto prj = ocs::tom::find_first_rw<PR_name>(master_project_list, p);

      if (prj && !project_still_used(p)) {
         ocs::tom::set<PR_consider_with_categories>(prj, false);
         reattach_categories = true;
         sge_add_event(0, sgeE_PROJECT_MOD, 0, 0, p, nullptr, nullptr, prj, gdi_session);
      }
   }

   // reattach all jobs to categories and consider the new project attributes
   if (reattach_categories) {
      const ocs::tom::List<ocs::tom::JB> master_job_list(*ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB));
      const ocs::tom::ConstList<ocs::tom::US> master_userset_list(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET));
      const ocs::tom::ConstList<ocs::tom::RQS> master_rqs_list(*ocs::DataStore::get_master_list(SGE_TYPE_RQS));
      ocs::CategoryQmaster::reattach_all_jobs(master_job_list, master_userset_list, master_project_list,
                                              master_rqs_list, true, gdi_session);
   }

   DRETURN_VOID;
}
