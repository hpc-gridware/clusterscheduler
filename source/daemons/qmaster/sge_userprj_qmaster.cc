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
#include "sgeobj/ocs_parse_util.h"
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
   /** @brief The fields and events a user object (UU) shares with a project object */
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

   /** @brief The fields and events a project object (PR) shares with a user object */
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

   /** @brief UserFields for a user object (UU), ProjectFields for a project object (PR) */
   template <ocs::tom::ObjectType T>
   using FieldsOf = std::conditional_t<std::is_same_v<T, ocs::tom::UU>, UserFields, ProjectFields>;
}

static int
do_add_auto_user(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::Element<ocs::tom::UU> anUser,
                 ocs::tom::ListPtr<ocs::tom::AN> *anAnswer, monitoring_t *monitor);

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
ocs_userprj_mod(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::Element<typename F::Type> modp,
                const ocs::tom::Element<typename F::Type> ep, const int add, const char *obj_name) {
   DENTER(TOP_LAYER);

   constexpr bool user_flag = std::is_same_v<typename F::Type, ocs::tom::UU>;
   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   const auto obj_master_list = ocs::DataStore::master_list<user_flag ? SGE_TYPE_USER : SGE_TYPE_PROJECT>();

   /* ---- UP_name */
   const char *userprj = ep.template get<F::name>();
   if (add) {
      if (!strcmp(userprj, "default")) {
         ERROR(MSG_UP_NOADDDEFAULT_S, obj_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      /* may not add user/project with same name as an existing project/user */
      if (obj_master_list.template find_first<F::name>(userprj)) {
         ERROR(MSG_UP_ALREADYEXISTS_SS, obj_name, userprj);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
      if (verify_obj_name(alpp, userprj, MAX_VERIFY_STRING, obj_name,
         user_flag ? "\\" : nullptr) != STATUS_OK) {
         DRETURN(STATUS_EUNKNOWN);
      }
      modp.template set<F::name>(userprj);
   }

   /* ---- UP_oticket */
   if (ep.template has<F::oticket>()) {
      modp.template set<F::oticket>(ep.template get<F::oticket>());
   }

   /* ---- UP_fshare */
   if (ep.template has<F::fshare>()) {
      modp.template set<F::fshare>(ep.template get<F::fshare>());
   }

   if constexpr (user_flag) {
      /* ---- UU_delete_time */
      if (ep.template has<UU_delete_time>()) {
         modp.template set<UU_delete_time>(ep.template get<UU_delete_time>());
      }
   }

   /* ---- UP_usage */
   if (ep.template has<F::usage>()) {
      modp.template set<F::usage>(ep.template get<F::usage>().clone("usage"));
      modp.template add<F::version>(1);
   }

   /* ---- UP_project */
   if (ep.template has<F::project>()) {
      modp.template set<F::project>(ep.template get<F::project>().clone("project"));
      modp.template add<F::version>(1);
   }

   if constexpr (user_flag) {
      /* ---- UU_default_project */
      if (ep.template has<UU_default_project>()) {
         ocs_null_out_none<UU_default_project>(ep);
         /* make sure default project exists */
         const char *dproj = ep.template get<UU_default_project>();
         if (dproj) {
            const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();
            if (!master_project_list || !master_project_list.find_first<PR_name>(dproj)) {
               ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_PRJ, dproj);
               answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
               DRETURN(STATUS_EUNKNOWN);
            }
         }

         modp.template set<UU_default_project>(dproj);
      }
   } else {
      /* ---- PR_acl */
      if (ep.template has<PR_acl>()) {
         modp.template set<PR_acl>(ep.template get<PR_acl>().clone("acl"));

         if (userset_list_validate_acl_list(ep.template get<PR_acl>(), alpp, master_userset_list) != STATUS_OK) {
            /* answerlist gets filled by userset_list_validate_acl_list() in case of errors */
            DRETURN(STATUS_EUNKNOWN);
         }
      }

      /* ---- PR_xacl */
      if (ep.template has<PR_xacl>()) {
         modp.template set<PR_xacl>(ep.template get<PR_xacl>().clone("xacl"));
         if (userset_list_validate_acl_list(ep.template get<PR_xacl>(), alpp, master_userset_list) != STATUS_OK) {
            /* answerlist gets filled by userset_list_validate_acl_list() in case of errors */
            DRETURN(STATUS_EUNKNOWN);
         }
      }

      if (modp.template has<PR_xacl>() ||
          modp.template has<PR_acl>()) {
         if (multiple_occurances<US_name>(alpp,
                                          modp.template get<PR_acl>(),
                                          modp.template get<PR_xacl>(),
                                          userprj,
                                          "project")) {
            DRETURN(STATUS_EUNKNOWN);
         }
      }
   }

   DRETURN(0);
}

/** @brief Apply one attribute change to an user or project
 *
 * The modifier of ocs_gdi_hooks for users (UU_LIST) and projects (PR_LIST); see ocs_gdi_add_mod() for the sequence
 * it is called from.
 *
 * @tparam T the object type of the table entry, ocs::tom::UU or ocs::tom::PR
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
template <ocs::tom::ObjectType T>
int
userprj_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
            const ocs::tom::Element<T> modp, const ocs::tom::Element<T> ep, int add, const char *ruser,
            const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
            monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   constexpr bool user_flag = std::is_same_v<T, ocs::tom::UU>;
   const int ret = ocs_userprj_mod<FieldsOf<T>>(alpp, modp, ep, add, user_flag ? MSG_OBJ_USER : MSG_OBJ_PRJ);

   DRETURN(ret);
}

/// @cond explicit instantiations for the gdi_object_t table in sge_c_gdi.cc
template int userprj_mod<ocs::tom::UU>(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                                       ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::Element<ocs::tom::UU> modp,
                                       ocs::tom::Element<ocs::tom::UU> ep, int add, const char *ruser,
                                       const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd,
                                       ocs::gdi::SubCommand sub_command, monitoring_t *monitor);
template int userprj_mod<ocs::tom::PR>(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                                       ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::Element<ocs::tom::PR> modp,
                                       ocs::tom::Element<ocs::tom::PR> ep, int add, const char *ruser,
                                       const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd,
                                       ocs::gdi::SubCommand sub_command, monitoring_t *monitor);
/// @endcond

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

   const auto master_rqs_list = ocs::DataStore::master_list<SGE_TYPE_RQS>();
   bool reattach_categories = false;
   for (const auto rqs : master_rqs_list) {
      if (scope_is_referenced_rqs(rqs, F::filter, ep.template get<F::name>())) {
         ep.template set<F::consider>(true);
         reattach_categories = true;
         break;
      }
   }

   if (reattach_categories) {
      const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
      const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
      const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();
      ocs::CategoryQmaster::reattach_all_jobs(master_job_list, master_userset_list, master_project_list,
                                              master_rqs_list, true, packet->gdi_session);
   }

   sge_add_event(0, is_mod ? F::mod_event : F::add_event, 0, 0, ep.template get<F::name>(), nullptr, nullptr, ep,
                 packet->gdi_session);

   DRETURN_VOID;
}

/** @brief Announce an user or project change once it is safely spooled
 *
 * The on_success of ocs_gdi_hooks for users (UU_LIST) and projects (PR_LIST).
 *
 * @tparam T the object type of the table entry, ocs::tom::UU or ocs::tom::PR
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the object
 * @param old_ep the object as it was, or an empty handle on add
 * @param object the table entry for this object type
 * @param post what is left for after the last object of the request, unused
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
template <ocs::tom::ObjectType T>
int
userprj_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<T> ep,
                const ocs::tom::ConstElement<T> old_ep, gdi_object_t *object,
                [[maybe_unused]] ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   ocs_userprj_success<FieldsOf<T>>(packet, ep, old_ep != nullptr);

   DRETURN(0);
}

/// @cond explicit instantiations for the gdi_object_t table in sge_c_gdi.cc
template int userprj_success<ocs::tom::UU>(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                                           ocs::tom::Element<ocs::tom::UU> ep,
                                           ocs::tom::ConstElement<ocs::tom::UU> old_ep, gdi_object_t *object,
                                           ocs_gdi_post_processing *post, monitoring_t *monitor);
template int userprj_success<ocs::tom::PR>(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                                           ocs::tom::Element<ocs::tom::PR> ep,
                                           ocs::tom::ConstElement<ocs::tom::PR> old_ep, gdi_object_t *object,
                                           ocs_gdi_post_processing *post, monitoring_t *monitor);
/// @endcond

/** @brief Write an user or project to the spool
 *
 * The writer of ocs_gdi_hooks for users (UU_LIST) and projects (PR_LIST).
 *
 * @tparam T the object type of the table entry, ocs::tom::UU or ocs::tom::PR
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param upe the user or project to spool
 * @param object the table entry for this object type
 * @return 0 on success
 */
template <ocs::tom::ObjectType T>
int
userprj_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
              const ocs::tom::ConstElement<T> upe, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   constexpr bool user_flag = std::is_same_v<T, ocs::tom::UU>;
   const char *name = upe.template get<FieldsOf<T>::name>();

   /* write user or project to file */
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   const bool dbret = spool_write_object(&answer_list, spool_get_default_context(), upe, name,
                                         user_flag ? SGE_TYPE_USER : SGE_TYPE_PROJECT);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, name);
   }

   DRETURN(dbret ? 0 : 1);
}

/// @cond explicit instantiations for the gdi_object_t table in sge_c_gdi.cc
template int userprj_spool<ocs::tom::UU>(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                                         ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                         ocs::tom::ConstElement<ocs::tom::UU> upe, gdi_object_t *object);
template int userprj_spool<ocs::tom::PR>(ocs::gdi::Packet *packet, ocs::gdi::Task *task,
                                         ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                         ocs::tom::ConstElement<ocs::tom::PR> upe, gdi_object_t *object);
/// @endcond

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
ocs_verify_project_deletion(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *name) {
   DENTER(TOP_LAYER);

   for (const auto cqueue : ocs::DataStore::master_list<SGE_TYPE_CQUEUE>()) {
      for (const auto prj : cqueue.get<CQ_projects>()) {
         if (prj.get<APRJLIST_value>().find_first<PR_name>(name)) {
            ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_PRJS, MSG_OBJ_QUEUE,
                  cqueue.get<CQ_name>());
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      }
      for (const auto prj : cqueue.get<CQ_xprojects>()) {
         if (prj.get<APRJLIST_value>().find_first<PR_name>(name)) {
            ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_XPRJS, MSG_OBJ_QUEUE,
                  cqueue.get<CQ_name>());
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EEXIST);
         }
      }
   }

   /* check hosts */
   for (const auto host : ocs::DataStore::master_list<SGE_TYPE_EXECHOST>()) {
      if (host.get<EH_prj>().find_first<PR_name>(name)) {
         ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_PRJS, MSG_OBJ_EH,
               host.get<EH_name>().c_str());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
      if (host.get<EH_xprj>().find_first<PR_name>(name)) {
         ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_XPRJS, MSG_OBJ_EH,
               host.get<EH_name>().c_str());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   }

   /* check global configuration */
   if (mconf_get_projects()->find_first<PR_name>(name)) {
      ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_PRJS, MSG_OBJ_CONF, MSG_OBJ_GLOBAL);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }
   if (mconf_get_xprojects()->find_first<PR_name>(name)) {
      ERROR(MSG_SGETEXT_PROJECTSTILLREFERENCED_SSSS, name, MSG_OBJ_XPRJS, MSG_OBJ_CONF, MSG_OBJ_GLOBAL);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* check user list for reference */
   const auto master_user_list = ocs::DataStore::master_list<SGE_TYPE_USER>();
   if (const auto user = master_user_list.find_first<UU_default_project>(name); user) {
      ERROR(MSG_USERPRJ_PRJXSTILLREFERENCEDINENTRYX_SS, name, user.get<UU_name>());
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
                ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::List<typename F::Type> upl, const char *ruser,
                const char *rhost) {
   DENTER(TOP_LAYER);

   constexpr bool user = std::is_same_v<typename F::Type, ocs::tom::UU>;

   if (!up_ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *name = up_ep.template get<F::name>();
   ocs::tom::Element<typename F::Type> ep;
   if constexpr (user) {
      ep = upl.template find_first<UU_name>(name);
   } else {
      ep = upl.template find_first<PR_name>(name);
   }
   if (!ep) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, user ? MSG_OBJ_USER : MSG_OBJ_PRJ, name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* ensure this u/p object is not referenced in actual share tree */
   const auto share_tree = ocs::DataStore::master_list<SGE_TYPE_SHARETREE>();
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
                ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::List<ocs::tom::UU> upl, const char *ruser,
                const char *rhost) {
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
                ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::List<ocs::tom::PR> upl, const char *ruser,
                const char *rhost) {
   DENTER(TOP_LAYER);
   const int ret = ocs_del_userprj<ProjectFields>(packet, task, up_ep, alpp, upl, ruser, rhost);
   DRETURN(ret);
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
verify_project_list(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::ConstList<ocs::tom::PR> name_list,
                    const ocs::tom::ConstList<ocs::tom::PR> prj_list, const char *attr_name, const char *obj_descr,
                    const char *obj_name) {
   DENTER(TOP_LAYER);

   for (const auto up : name_list) {
      if (!prj_list.find_first<PR_name>(up.get<PR_name>())) {
         ERROR(MSG_SGETEXT_UNKNOWNPROJECT_SSSS, up.get<PR_name>(), attr_name, obj_descr, obj_name);
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
      const auto master_user_list = ocs::DataStore::master_list_rw<SGE_TYPE_USER>();
      const auto master_suser_list = ocs::DataStore::master_list<SGE_TYPE_SUSER>();

      /*
       * Check each user for deletion time. We don't use a range-for
       * because we are deleting entries.
       */
      ocs::tom::Element<ocs::tom::UU> next;
      for (auto user = master_user_list.front(); user; user = next) {
         uint64_t delete_time = user.get<UU_delete_time>();
         next = user.next();

         /* 
          * For non auto users, delete_time = 0.
          * Never delete them automatically 
          */
         if (delete_time > 0) {
            const char *name = user.get<UU_name>();

            /* if the user has jobs, we increment the delete time */
            if (suser_get_job_counter(suser_list_find(master_suser_list, name)) > 0) {
               user.set<UU_delete_time>(next_delete);
            } else {
               /* if the delete time has expired, delete user */
               if (delete_time <= now) {
                  ocs::tom::ListPtr<ocs::tom::AN> answer_list;
                  ocs::gdi::Packet packet;
                  ocs::gdi::Task task;

                  packet.gdi_session = ocs::SessionManager::GDI_SESSION_NONE;
                  if (sge_del_userprj(&packet, &task, user, &answer_list, master_user_list, admin, qmaster_host) !=
                      STATUS_OK) {
                     /* 
                      * if deleting the user failes (due to user being referenced
                      * in other objects, e.g. queue), regard him as non auto user
                      */
                     user.set<UU_delete_time>(0);
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
sge_add_auto_user(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *user,
                  ocs::tom::ListPtr<ocs::tom::AN> *alpp, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int status = STATUS_OK;
   uint64_t auto_user_delete_time = sge_gmt32_to_gmt64(mconf_get_auto_user_delete_time());

   const auto uep = ocs::DataStore::master_list_rw<SGE_TYPE_USER>().find_first<UU_name>(user);

   /* if the user already exists */
   if (uep) {
      /* and is an auto user */
      if (uep.get<UU_delete_time>() != 0) {
         /* and we shall not keep auto users forever */
         if (auto_user_delete_time > 0) {
            uep.set<UU_delete_time>(sge_get_gmt64() + auto_user_delete_time);
         }
      }
      /* and we are done */
      DRETURN(STATUS_OK);
   }

   /* create a new auto user */
   auto uep_uu = ocs::tom::ElementPtr<ocs::tom::UU>::create();
   char *auto_user_default_project = nullptr;

   /* set user object attributes */
   uep_uu->set<UU_name>(user);

   if (auto_user_delete_time > 0) {
      uep_uu->set<UU_delete_time>(sge_get_gmt64() + auto_user_delete_time);
   } else {
      uep_uu->set<UU_delete_time>(0);
   }

   uep_uu->set<UU_oticket>(mconf_get_auto_user_oticket());
   uep_uu->set<UU_fshare>(mconf_get_auto_user_fshare());

   auto_user_default_project = mconf_get_auto_user_default_project();
   if (auto_user_default_project == nullptr || strcasecmp(auto_user_default_project, "none") == 0) {
      uep_uu->set<UU_default_project>(nullptr);
   } else {
      uep_uu->set<UU_default_project>(auto_user_default_project);
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
                 ocs::tom::ListPtr<ocs::tom::AN> *anAnswer, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   int res = STATUS_EUNKNOWN;
   gdi_object_t *userList = nullptr;
   ocs::tom::ListPtr<ocs::tom::AN> tmpAnswer;
   const char *admin_user = ocs::Bootstrap::get_admin_user();
   const char *qualified_hostname = component_get_qualified_hostname();

   userList = get_gdi_object(ocs::gdi::Target::UU_LIST);

   /* 
    * Add anUser to the user list.
    * Owner of the operation is the admin user on the qmaster host.
    */
   res = ocs_gdi_add_mod<ocs::tom::UU>(packet, task, &tmpAnswer, anUser, 1, userList, admin_user, qualified_hostname,
                                       ocs::gdi::Command::NONE, ocs::gdi::SubCommand::NONE, nullptr, monitor);

   if ((STATUS_OK != res) && (nullptr != tmpAnswer)) {
      const auto err = tmpAnswer->front();
      const char *text = err.get<AN_text>();
      uint32_t status = err.get<AN_status>();
      answer_quality_t quality = (answer_quality_t) err.get<AN_quality>();

      answer_list_add(anAnswer, text, status, quality);
   }

   if (tmpAnswer != nullptr) {
      tmpAnswer.reset();
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
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   uint64_t now = sge_get_gmt64();

   /* this function is used on qmaster shutdown, no need to monitor this lock */
   SGE_LOCK(LOCK_GLOBAL, LOCK_READ);

   for (const auto elem : ocs::DataStore::master_list_rw<SGE_TYPE_USER>(true)) {
      const char *name = elem.get<UU_name>();
      sge_event_spool(&answer_list, now, sgeE_USER_MOD, 0, 0, name, nullptr, nullptr, elem, nullptr, nullptr, false,
                      true, gdi_session);
   }

   for (const auto elem : ocs::DataStore::master_list_rw<SGE_TYPE_PROJECT>(true)) {
      const char *name = elem.get<PR_name>();
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

   for (const auto rqs : ocs::DataStore::master_list<SGE_TYPE_RQS>()) {
      if (scope_is_referenced_rqs(rqs, RQR_filter_projects, p)) {
         DRETURN(true);
      }
   }

   for (const auto hep : ocs::DataStore::master_list<SGE_TYPE_EXECHOST>()) {
      if (hep.get<EH_prj>().find_first<PR_name>(p) ||
          hep.get<EH_xprj>().find_first<PR_name>(p)) {
         DRETURN(true);
      }
   }

   for (const auto cq : ocs::DataStore::master_list<SGE_TYPE_CQUEUE>()) {
      for (const auto qc : cq.get<CQ_projects>()) {
         if (qc.get<APRJLIST_value>().find_first<PR_name>(p)) {
            DRETURN(true);
         }
      }
      for (const auto qc : cq.get<CQ_xprojects>()) {
         if (qc.get<APRJLIST_value>().find_first<PR_name>(p)) {
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
 * @param added List of added project references (PR)
 * @param removed List of removed project references (PR)
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: project_update_categories() is not MT safe
 */
void
project_update_categories(const ocs::tom::ConstList<ocs::tom::PR> added,
                          const ocs::tom::ConstList<ocs::tom::PR> removed, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   const auto master_project_list = ocs::DataStore::master_list_rw<SGE_TYPE_PROJECT>(true);
   bool reattach_categories = false;

   for (const auto ep : added) {
      const char *p = ep.get<PR_name>();
      DPRINTF("added project: \"%s\"\n", p);
      const auto prj = master_project_list.find_first<PR_name>(p);
      if (prj && !prj.get<PR_consider_with_categories>()) {
         prj.set<PR_consider_with_categories>(true);
         reattach_categories = true;
         sge_add_event(0, sgeE_PROJECT_MOD, 0, 0, p, nullptr, nullptr, prj, gdi_session);
      }
   }

   for (const auto ep : removed) {
      const char *p = ep.get<PR_name>();
      DPRINTF("removed project: \"%s\"\n", p);
      const auto prj = master_project_list.find_first<PR_name>(p);

      if (prj && !project_still_used(p)) {
         prj.set<PR_consider_with_categories>(false);
         reattach_categories = true;
         sge_add_event(0, sgeE_PROJECT_MOD, 0, 0, p, nullptr, nullptr, prj, gdi_session);
      }
   }

   // reattach all jobs to categories and consider the new project attributes
   if (reattach_categories) {
      const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
      const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
      const auto master_rqs_list = ocs::DataStore::master_list<SGE_TYPE_RQS>();
      ocs::CategoryQmaster::reattach_all_jobs(master_job_list, master_userset_list, master_project_list,
                                              master_rqs_list, true, gdi_session);
   }

   DRETURN_VOID;
}
