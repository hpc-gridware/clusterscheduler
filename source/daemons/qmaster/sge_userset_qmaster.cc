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
 * @brief User sets, access lists and departments
 */
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/ocs_Role.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_AUSRLIST.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RL.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "spool/sge_spooling.h"

#include "sched/valid_queue_user.h"

#include "evm/sge_event_master.h"

#include "ocs_CategoryQmaster.h"
#include "sge.h"
#include "sge_userset_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "sge_utility_qmaster.h"
#include "sge_resource_quota_qmaster.h"
#include "ocs_Bootstrap.h"
#include "msg_common.h"
#include "msg_qmaster.h"

static void sge_change_queue_version_acl(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *acl_name);

static int verify_userset_deletion(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *userset_name);

static int dept_is_valid_defaultdepartment(ocs::tom::ConstElement<ocs::tom::US> dept,
                                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

static int acl_is_valid_acl(ocs::tom::ConstElement<ocs::tom::US> acl, ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

/** @brief Delete a user set from the master list
 *
 * Qmaster code: deletes a user set from the global user set list.
 * Refuses while the set is still referenced - by a queue's access lists, a
 * project, or a resource quota - because a dangling reference would silently
 * change who may run where.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the user set to delete
 * @param alpp receives messages for the caller
 * @param userset_list the master user set list
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @return STATUS_OK on success
 */
int
sge_del_userset(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::US> ep,
                ocs::tom::ListPtr<ocs::tom::AN> *alpp, const ocs::tom::List<ocs::tom::US> userset_list,
                const char *ruser, const char *rhost) {
   DENTER(TOP_LAYER);

   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no userset element, if ep has no US_name */
   if (!ep.has<US_name>()) {
      CRITICAL(MSG_SGETEXT_MISSINGFIELD_SS, ocs::tom::FieldTraits<US_name>::info.name, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *userset_name = ep.get<US_name>();
   if (!userset_name) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* the manager/operator usersets back the manager/operator lists (CS-2394);
    * they are reserved and must be changed via qconf -am/-dm/-ao/-do, not deleted */
   if (strcmp(userset_name, MANAGER_USERSET) == 0 || strcmp(userset_name, OPERATOR_USERSET) == 0) {
      ERROR(MSG_USERSET_RESERVED_NODELETE_S, userset_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* search for userset with this name and remove it from the list */
   const auto found = userset_list.find_first<US_name>(userset_name);
   if (!found) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_USERSET, userset_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* ensure there are no (x)acl lists in 
      a queue/pe/project/.. refering to this userset */
   if (const int ret = verify_userset_deletion(alpp, userset_name); ret != STATUS_OK) {
      /* answerlist gets filled by verify_userset_deletion() in case of errors */
      DRETURN(ret);
   }

   userset_list.erase(found);

   sge_event_spool(alpp, 0, sgeE_USERSET_DEL, 0, 0, userset_name, nullptr, nullptr,
                   nullptr, nullptr, nullptr, true, true, packet->gdi_session);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, userset_name, MSG_OBJ_USERSET);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}


/** @brief Increase the version of all queue instances using a user set as access list
 *
 * Qmaster code: having changed a user set we have to increase the versions of all
 * queue instances referencing it in their acl or xacl.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param acl_name the name of the changed user set
 */
static void
sge_change_queue_version_acl(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *acl_name) {
   DENTER(TOP_LAYER);
   const auto master_cqueue_list = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>(true);

   for (const auto cqueue : master_cqueue_list) {
      for (const auto qinstance : cqueue.get<CQ_qinstances>()) {
         const auto acl = qinstance.get<QU_acl>().find_first<US_name>(acl_name);
         const auto xacl = qinstance.get<QU_xacl>().find_first<US_name>(acl_name);
         const bool is_used = acl || xacl;

         if (is_used) {
            ocs::tom::ListPtr<ocs::tom::AN> answer_list;

            DPRINTF("increasing version of queue " SFQ " because acl " SFQ
                    " changed\n", qinstance.get<QU_full_name>(), acl_name);
            qinstance_increase_qversion(qinstance);
            sge_event_spool(&answer_list, 0, sgeE_QINSTANCE_MOD, 0, 0, qinstance.get<QU_qname>(),
                            qinstance.get<QU_qhostname>(), nullptr, qinstance, nullptr, nullptr, true, false,
                            packet->gdi_session);
            answer_list_output(&answer_list);
         }
      }
   }

   DRETURN_VOID;
}

/** @brief Check that a user set may become, or remain, a department
 *
 * Resolves user set/department. A user may belong to only one department, so a
 * set marked as one must not overlap any other department.
 *
 * @param userset_list the existing user sets, the current master user set list
 * @param new_userset the set being added or changed
 * @param alpp receives messages for the caller, may be nullptr
 * @return STATUS_OK when the departments stay consistent, STATUS_ESEMANTIC on error
 */
int
sge_verify_department_entries(const ocs::tom::ConstList<ocs::tom::US> userset_list,
                              const ocs::tom::ConstElement<ocs::tom::US> new_userset,
                              ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   if (!strcmp(new_userset.get<US_name>(), DEFAULT_DEPARTMENT)) {
      if (!dept_is_valid_defaultdepartment(new_userset, alpp)) {
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   if (!(new_userset.get<US_type>() & US_DEPT)) {
      DRETURN(STATUS_OK);
   }

   DRETURN(STATUS_OK);
}

/**
 * @brief Is defaultdepartment correct
 *
 * Check if the given defaultdepartment "dept" is valid.
 *
 * @param dept US defaultdepartment
 * @param answer_list AN answer list
 *
 * @return 0 or 1
 */
static int
dept_is_valid_defaultdepartment(const ocs::tom::ConstElement<ocs::tom::US> dept,
                                ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   int ret = 1;
   if (dept) {
      /* test 'type' */
      if (!(dept.get<US_type>() & US_DEPT)) {
         ERROR(SFNMAX, MSG_QMASTER_DEPTFORDEFDEPARTMENT);
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         ret = 0;
      }
      /* test user list */
      if (!dept.get<US_entries>().empty()) {
         ERROR(SFNMAX, MSG_QMASTER_AUTODEFDEPARTMENT);
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         ret = 0;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Is the acl correct
 *
 * Check if the given "acl" is correct
 *
 * @param acl US acl
 * @param answer_list AN list
 *
 * @return 0 or 1
 */
static int
acl_is_valid_acl(const ocs::tom::ConstElement<ocs::tom::US> acl, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   int ret = 1;
   if (acl) {
      if (!(acl.get<US_type>() & US_DEPT)) {
         if (acl.get<US_fshare>() > 0) {
            ERROR(SFNMAX, MSG_QMASTER_ACLNOSHARE);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            ret = 0;
         }
         if (acl.get<US_oticket>() > 0) {
            ERROR(SFNMAX, MSG_QMASTER_ACLNOTICKET);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            ret = 0;
         }
      }
   }
   DRETURN(ret);
}

/** @brief Is the job's owner a member of the given department?
 *
 * @param job the job (`JB`) whose owner and groups are checked
 * @param alpp used to return error messages; may be `nullptr` when the caller
 *             is probing several departments and a non-match is not an error
 * @param dept_name the department to check against
 * @param userset_list the departments (`US`)
 * @return true if the owner is a member of `dept_name`
 */
bool
job_is_valid_department(const ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                        const char *dept_name, const ocs::tom::ConstList<ocs::tom::US> userset_list) {
   DENTER(TOP_LAYER);

   // Do we have a department name?
   if (dept_name == nullptr) {
      ERROR(SFNMAX, MSG_JOB_NODEPTFOUND);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // Does the department exist?
   const auto dept = userset_list.find_first<US_name>(dept_name);
   if (!dept || !(dept.get<US_type>() & US_DEPT)) {
      if (alpp) {
         ERROR(MSG_JOB_DEPTNOEXIST_S, dept_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      }
      DRETURN(false);
   }

   // Is the department name the default department then we can skip remaining tests.
   // The default department is always valid and the user or group(s) do not need to be part of it.
   if (strcmp(dept_name, DEFAULT_DEPARTMENT) == 0) {
      DRETURN(true);
   }

   // Does the user belong to the department?
   const char *owner = job.get<JB_owner>();
   if (sge_contained_in_access_list(owner, nullptr, nullptr, dept)) {
      DRETURN(true);
   }

   // Does the group or a supplementary group belong to the department?
   const char *group = job.get<JB_group>();
   const ocs::tom::ConstList<ocs::tom::ST> grp_list = job.get<JB_grp_list>();
   if (sge_contained_in_access_list(nullptr, group, grp_list, dept)) {
      DRETURN(true);
   }

   // User is not part of the department
   if (alpp) {
      ERROR(MSG_JOB_USERNOTPARTDEPT_SS, owner, dept_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   }
   DRETURN(false);
}

/** @brief Assign the job to the first department its owner belongs to
 *
 * The departments are tried in list order and the first match wins, so a user
 * in several departments gets whichever comes first - the order is the
 * assignment rule. `defaultdepartment` is skipped during the search and used
 * as the fallback when no other department matches.
 *
 * @param job the job (`JB`); `JB_department` is set on success
 * @param alpp used to return error messages
 * @param userset_list the departments (`US`)
 * @return true if a department could be assigned
 */
bool
job_set_department(const ocs::tom::Element<ocs::tom::JB> job, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                   const ocs::tom::ConstList<ocs::tom::US> userset_list) {
   DENTER(TOP_LAYER);

   // Find the first department where the user is a member of
   for (const auto dept : userset_list) {
      const char *dept_name = dept.get<US_name>();
      if (strcmp(dept_name, DEFAULT_DEPARTMENT) == 0) {
         continue;
      }
      if (job_is_valid_department(job, nullptr, dept_name, userset_list)) {
         job.set<JB_department>(dept.get<US_name>());
         DRETURN(true);
      }
   }

   // No department found => set default department
   if (userset_list.find_first<US_name>(DEFAULT_DEPARTMENT)) {
      job.set<JB_department>(DEFAULT_DEPARTMENT);
      DRETURN(true);
   }

   // Also no default department found
   ERROR(SFNMAX, MSG_JOB_NODEPTFOUND);
   answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   DRETURN(false);
}

/** @brief Check that no object still references a user set that is to be deleted
 *
 * Checks the cluster queue configurations (fix for bug 6422335: the cq
 * configuration instead of the qinstances), PEs, projects, exec hosts, roles
 * and the global configuration.
 *
 * @param alpp receives an error message per reference found
 * @param userset_name the user set to be deleted
 * @return STATUS_OK if unreferenced, STATUS_EUNKNOWN or STATUS_EEXIST (exec host reference) otherwise
 */
static int
verify_userset_deletion(ocs::tom::ListPtr<ocs::tom::AN> *alpp, const char *userset_name) {
   DENTER(TOP_LAYER);
   int ret = STATUS_OK;
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();
   const auto master_ehost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
   const auto master_role_list = ocs::DataStore::master_list<SGE_TYPE_RL>();

   /*
    * fix for bug 6422335
    * check the cq configuration for userset references instead of qinstances
    */
   for (const auto cqueue : master_cqueue_list) {
      for (const auto cl : cqueue.get<CQ_acl>()) {
         if (cl.get<AUSRLIST_value>().find_first<US_name>(userset_name)) {
            ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_QUEUE,
                  cqueue.get<CQ_name>());
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
      }
      for (const auto cl : cqueue.get<CQ_xacl>()) {
         if (cl.get<AUSRLIST_value>().find_first<US_name>(userset_name)) {
            ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_QUEUE,
                  cqueue.get<CQ_name>());
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
      }
   }

   for (const auto ep : master_pe_list) {
      if (ep.get<PE_user_list>().find_first<US_name>(userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_PE,
               ep.get<PE_name>());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
      if (ep.get<PE_xuser_list>().find_first<US_name>(userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_PE,
               ep.get<PE_name>());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   for (const auto ep : master_project_list) {
      if (ep.get<PR_acl>().find_first<US_name>(userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_PRJ,
               ep.get<PR_name>());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
      if (ep.get<PR_xacl>().find_first<US_name>(userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_PRJ,
               ep.get<PR_name>());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /* hosts */
   for (const auto ep : master_ehost_list) {
      if (ep.get<EH_acl>().find_first<US_name>(userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_EH,
               ep.get<EH_name>().c_str());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
      if (ep.get<EH_xacl>().find_first<US_name>(userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_EH,
               ep.get<EH_name>().c_str());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   }

   // refuse deletion if any role still references this userset in its user_list
   for (const auto role_ep : master_role_list) {
      if (role_ep.get<RL_user_list>().find_first<US_name>(userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_ROLE,
               role_ep.get<RL_name>());
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /* global configuration */
   if (mconf_get_user_lists()->find_first<US_name>(userset_name)) {
      ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_CONF, MSG_OBJ_GLOBAL);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = STATUS_EUNKNOWN;
   }

   if (mconf_get_xuser_lists()->find_first<US_name>(userset_name)) {
      ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_CONF, MSG_OBJ_GLOBAL);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = STATUS_EUNKNOWN;
   }

   DRETURN(ret);
}

/**
 * @brief True, if userset still used
 *
 * Returns true, if userset is still used as ACL with host_conf(5),
 * queue_conf(5), or sge_pe(5) or in a resource quota set.
 * Use of usersets as ACLs in sge_conf(5) play no role here,
 * since such ACLs are checked in qmaster and thus are not
 * relevant for the scheduling algorithm.
 *
 * @param u the name of the userset
 *
 * @return True, if userset still used
 *
 * @note MT-NOTE: userset_still_used() is not MT safe
 */
static bool userset_still_used(const char *u) {
   DENTER(TOP_LAYER);
   dstring ds = DSTRING_INIT;
   const auto master_rqs_list = ocs::DataStore::master_list<SGE_TYPE_RQS>();
   const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
   const auto master_ehost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();

   sge_dstring_sprintf(&ds, "@%s", u);

   for (const auto rqs : master_rqs_list) {
      if (scope_is_referenced_rqs(rqs, RQR_filter_users, sge_dstring_get_string(&ds))) {
         sge_dstring_free(&ds);
         DRETURN(true);
      }
   }
   sge_dstring_free(&ds);

   for (const auto pe : master_pe_list) {
      if (pe.get<PE_user_list>().find_first<US_name>(u) ||
          pe.get<PE_xuser_list>().find_first<US_name>(u)) {
         DRETURN(true);
      }
   }

   for (const auto hep : master_ehost_list) {
      if (hep.get<EH_acl>().find_first<US_name>(u) ||
          hep.get<EH_xacl>().find_first<US_name>(u)) {
         DRETURN(true);
      }
   }

   for (const auto cq : master_cqueue_list) {
      for (const auto qc : cq.get<CQ_acl>()) {
         if (qc.get<AUSRLIST_value>().find_first<US_name>(u)) {
            DRETURN(true);
         }
      }
      for (const auto qc : cq.get<CQ_xacl>()) {
         if (qc.get<AUSRLIST_value>().find_first<US_name>(u)) {
            DRETURN(true);
         }
      }
   }

   DRETURN(false);
}

/**
 * @brief Update all usersets wrts categories
 *
 * Each added/removed userset is verified whether it is used first
 * time/still as ACL for host_conf(5)/queue_conf(5)/sge_pe(5). If
 * so an event is sent.
 *
 * @param added List of added userset references (US)
 * @param removed List of removed userset references (US)
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: userset_update_categories() is not MT safe
 */
void
userset_update_categories(const ocs::tom::ConstList<ocs::tom::US> added,
                          const ocs::tom::ConstList<ocs::tom::US> removed, const uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   const auto master_userset_list = ocs::DataStore::master_list_rw<SGE_TYPE_USERSET>(true);
   bool reattach_categories = false;

   for (const auto ep : added) {
      const char *u = ep.get<US_name>();
      DPRINTF("added userset: \"%s\"\n", u);
      const auto acl = master_userset_list.find_first<US_name>(u);
      if (acl && !acl.get<US_consider_with_categories>()) {
         acl.set<US_consider_with_categories>(true);
         reattach_categories = true;
         sge_add_event(0, sgeE_USERSET_MOD, 0, 0, u, nullptr, nullptr, acl, gdi_session);
      }
   }

   for (const auto ep : removed) {
      const char *u = ep.get<US_name>();
      DPRINTF("removed userset: \"%s\"\n", u);
      const auto acl = master_userset_list.find_first<US_name>(u);

      if (acl && !userset_still_used(u)) {
         acl.set<US_consider_with_categories>(false);
         reattach_categories = true;
         sge_add_event(0, sgeE_USERSET_MOD, 0, 0, u, nullptr, nullptr, acl, gdi_session);
      }
   }

   // reattach all jobs to categories and consider the new project attributes
   if (reattach_categories) {
      const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
      const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();
      const auto master_rqs_list = ocs::DataStore::master_list<SGE_TYPE_RQS>();
      ocs::CategoryQmaster::reattach_all_jobs(master_job_list, master_userset_list, master_project_list,
                                              master_rqs_list, true, gdi_session);
   }

   DRETURN_VOID;
}

/**
 * @brief Gdi callback function for adding/modifing a userset
 *
 * This function is called from the generic gdi framework when a userset is
 * added or modified.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp answer list
 * @param new_us if add it's an empty userset that needs to be filled if mod it's the actual stored userset
 * @param delta_us reduced userset object with new/modified values; its entries are consumed
 * @param add 1 for gdi add 0 for gdi mod
 * @param ruser user who invoked the gdi request
 * @param rhost host where the gid request was invoked
 * @param object structure of the gdi framework
 * @param cmd the command being executed
 * @param sub_command requested sub_commands
 * @param monitor monitoring structure
 *
 * @return 0 on success STATUS_EUNKNOWN if an error occurred
 *
 * @note MT-NOTE: userset_mod() is not MT safe, needs global lock
 */
int userset_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                const ocs::tom::Element<ocs::tom::US> new_us, const ocs::tom::Element<ocs::tom::US> delta_us,
                int add, const char *ruser, const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd,
                ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *userset_name;
   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
   const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();

   /* ---- US_name */
   if (add) {
      // a nullptr name is refused by verify_obj_name() below
      new_us.copy_if_present<US_name>(delta_us);
   }
   userset_name = new_us.get<US_name>();

   if (add && verify_obj_name(
           alpp, userset_name, MAX_VERIFY_STRING, object->object_name) != STATUS_OK) {
      goto ERROR;
   }

   /* ---- US_fshare */
   new_us.copy_if_present<US_fshare>(delta_us);

   /* ---- US_oticket */
   new_us.copy_if_present<US_oticket>(delta_us);

   /* ---- US_type */
   new_us.copy_if_present<US_type>(delta_us);

   /* ensure userset is at least an ACL (qconf -au ) */
   if (new_us.get<US_type>() == 0) {
      new_us.set<US_type>(US_ACL);
   }
   /* make sure acl is valid */
   if (!add) {
      if (acl_is_valid_acl(new_us, alpp) != STATUS_OK) {
         goto ERROR;
      }
   }
   /* ---- US_entries */
   attr_mod_sub_list<US_entries, UE_name>(alpp, new_us, delta_us, sub_command, SGE_ATTR_USER_LISTS,
                                          object->object_name);
   /* interpret user/group names */
   if (userset_validate_entries(new_us, alpp) != STATUS_OK) {
      goto ERROR;
   }

   /* the manager/operator usersets back the manager/operator lists (CS-2394).
    * They may be changed via the userset interface, but only by a manager, they must
    * stay an ACL and must not lose root or the admin user - the same guarantees the
    * (retired) qconf -am/-dm/-ao/-do path always gave. */
   if (strcmp(userset_name, MANAGER_USERSET) == 0 || strcmp(userset_name, OPERATOR_USERSET) == 0) {
      const char *manop_name = (strcmp(userset_name, MANAGER_USERSET) == 0) ? MSG_OBJ_MANAGER : MSG_OBJ_OPERATOR;

      /* Changing the manager/operator list requires manager rights. The US_LIST target
       * itself only requires operator, so enforce it here where the reserved name is
       * known - otherwise an operator could promote users to manager via -au/-mu. */
      if (!manop_is_manager(packet)) {
         ERROR(MSG_SGETEXT_MUSTBEMANAGER_S, packet->user);
         answer_list_add(alpp, SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
         goto ERROR;
      }

      /* NOTE: these rejections must not use STATUS_EEXIST: sge_client_del_user()
       * (qconf -du) maps STATUS_EEXIST to "access list does not exist" and drops
       * our message. STATUS_ESEMANTIC makes the client print the text below. */
      if ((new_us.get<US_type>() & US_ACL) == 0) {
         ERROR(MSG_USERSET_RESERVED_MUSTBEACL_S, userset_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         goto ERROR;
      }

      /* admin_user "none" is the sentinel for "no admin user", it is not seeded either */
      const char *admin_user = ocs::Bootstrap::get_admin_user();
      const auto entries = new_us.get<US_entries>();
      if (!entries.find_first<UE_name>("root")) {
         ERROR(MSG_SGETEXT_MAY_NOT_REMOVE_USER_FROM_LIST_SS, "root", manop_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         goto ERROR;
      }
      if (admin_user != nullptr && sge_strnullcasecmp(admin_user, NONE_STR) != 0 &&
          !entries.find_first<UE_name>(admin_user)) {
         ERROR(MSG_SGETEXT_MAY_NOT_REMOVE_USER_FROM_LIST_SS, admin_user, manop_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         goto ERROR;
      }
   }

   /*
   ** check for users defined in more than one userset if they
   ** are used as departments
   */
   if (sge_verify_department_entries(master_userset_list, new_us, alpp) != STATUS_OK) {
      goto ERROR;
   }

   /*
   ** check advance reservations
   */
   if (!add) {
      ocs::tom::ListPtr<ocs::tom::US> new_master_userset_list;

      for (const auto cqueue : master_cqueue_list) {
         for (const auto qinstance : cqueue.get<CQ_qinstances>()) {
            const char *queue_name = qinstance.get<QU_full_name>();

            if (qinstance_slots_reserved(qinstance) == 0) {
               /* queue not reserved, can not conflict with AR */
               continue;
            }

            if (!qinstance.get<QU_acl>().find_first<US_name>(userset_name) &&
                !qinstance.get<QU_xacl>().find_first<US_name>(userset_name)) {
               /* userset not referenced by queue instance */
               continue;
            }

            for (const auto ar : master_ar_list) {
               if (ar.get<AR_granted_slots>().find_first<JG_qname>(queue_name)) {
                  if (!new_master_userset_list) {
                     new_master_userset_list = master_userset_list.clone("");
                     new_master_userset_list->erase<US_name>(userset_name);
                     new_master_userset_list->push_back(new_us.clone());
                  }

                  if (!sge_ar_have_users_access(nullptr, ar, qinstance.get<QU_full_name>(),
                                                qinstance.get<QU_acl>(),
                                                qinstance.get<QU_xacl>(),
                                                new_master_userset_list.get())) {
                     answer_list_add_sprintf(alpp, STATUS_ESYNTAX,
                                             ANSWER_QUALITY_ERROR,
                                             MSG_PARSE_MOD3_REJECTED_DUE_TO_AR_SU,
                                             "entries",
                                             ar.get<AR_id>());
                     goto ERROR;
                  }
               }
            }
         }
      }
   }

   DRETURN(0);

   ERROR:
DRETURN(STATUS_EUNKNOWN);
}

/**
 * @brief Gdi callback funtion to spool a userset
 *
 * This function is called by the gdi framework after successfully adding or
 * modifing a userset.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp answer list, error messages will be added to this list
 * @param us userset object to spool
 * @param object structure of the gdi framework
 *
 * @return 0 - success, 1 - an error occurred
 *
 * @note MT-NOTE: userset_spool() is not MT safe
 */
int userset_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                  const ocs::tom::ConstElement<ocs::tom::US> us, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   const bool dbret = spool_write_object(&answer_list, spool_get_default_context(), us,
                                         us.get<US_name>(), SGE_TYPE_USERSET);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, us.get<US_name>());
   }

   DRETURN(dbret ? 0 : 1);

}

/**
 * @brief Do something after a successful add/mod
 *
 * This function is called from the framework after successfull add/mod and
 * spool.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param userset new added userset
 * @param old_ep for mod the old userset, an empty handle for add
 * @param object structure of the gdi framework
 * @param post what is left for after the last object of the request, unused
 * @param monitor monitoring structure
 *
 * @return 0 on success
 *
 * @note MT-NOTE: userset_success() is not MT safe
 */
int userset_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::US> userset,
                    const ocs::tom::ConstElement<ocs::tom::US> old_ep, gdi_object_t *object,
                    [[maybe_unused]] ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   dstring ds = DSTRING_INIT;
   const auto master_rqs_list = ocs::DataStore::master_list<SGE_TYPE_RQS>();
   const char *userset_name = userset.get<US_name>();

   /* set consider with categories */
   bool reattach_categories = false;
   sge_dstring_sprintf(&ds, "@%s", userset_name);
   for (const auto rqs : master_rqs_list) {
      if (scope_is_referenced_rqs(rqs, RQR_filter_users, sge_dstring_get_string(&ds))) {
         userset.set<US_consider_with_categories>(true);
         reattach_categories = true;
         break;
      }
   }

   if (old_ep != nullptr) {
      /* change queue versions if userset was modified */
      sge_change_queue_version_acl(packet, task, userset_name);
   }

   if (reattach_categories) {
      const auto master_job_list = ocs::DataStore::master_list_rw<SGE_TYPE_JOB>();
      const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
      const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();
      ocs::CategoryQmaster::reattach_all_jobs(master_job_list, master_userset_list, master_project_list,
                                              master_rqs_list, true, packet->gdi_session);
   }

   sge_add_event(0, old_ep ? sgeE_USERSET_MOD : sgeE_USERSET_ADD, 0, 0,
                 userset_name, nullptr, nullptr, userset, packet->gdi_session);

   sge_dstring_free(&ds);
   DRETURN(0);
}
