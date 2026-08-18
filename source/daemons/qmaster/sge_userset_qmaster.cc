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

static int verify_userset_deletion(lList **alpp, const char *userset_name);

static int dept_is_valid_defaultdepartment(lListElem *dept, lList **answer_list);

static int acl_is_valid_acl(lListElem *acl, lList **answer_list);

/******************************************************************
   sge_del_userset() - Qmaster code

   deletes an userset list from the global userset_list
 ******************************************************************/
/** @brief Delete a user set from the master list
 *
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
sge_del_userset(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **alpp, lList **userset_list, char *ruser, char *rhost) {
   DENTER(TOP_LAYER);

   lListElem *found;
   int pos, ret;
   const char *userset_name;

   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no userset element, if ep has no US_name */
   if ((pos = lGetPosViaElem(ep, US_name, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(US_name), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   userset_name = lGetPosString(ep, pos);
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
   if (!(found = lGetElemStrRW(*userset_list, US_name, userset_name))) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_USERSET, userset_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* ensure there are no (x)acl lists in 
      a queue/pe/project/.. refering to this userset */
   if ((ret = verify_userset_deletion(alpp, userset_name)) != STATUS_OK) {
      /* answerlist gets filled by verify_userset_deletion() in case of errors */
      DRETURN(ret);
   }

   lRemoveElem(*userset_list, &found);

   sge_event_spool(alpp, 0, sgeE_USERSET_DEL, 0, 0, userset_name, nullptr, nullptr,
                   nullptr, nullptr, nullptr, true, true, packet->gdi_session);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, userset_name, MSG_OBJ_USERSET);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}

/*********************************************************************
   Qmaster code

   increase version of queues using given userset as access list (acl or xacl)

   having changed a complex we have to increase the versions
   of all queues containing this complex;
 **********************************************************************/
static void
sge_change_queue_version_acl(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const char *acl_name) {
   DENTER(TOP_LAYER);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   for_each_ep_lv(cqueue, master_cqueue_list) {
      const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);

      for_each_rw_lv(qinstance, qinstance_list) {
         const lList *acl_list = lGetList(qinstance, QU_acl);
         const lList *xacl_list = lGetList(qinstance, QU_xacl);
         const lListElem *acl = lGetElemStr(acl_list, US_name, acl_name);
         const lListElem *xacl = lGetElemStr(xacl_list, US_name, acl_name);
         bool is_used = ((acl != nullptr) || (xacl != nullptr)) ? true : false;

         if (is_used) {
            lList *answer_list = nullptr;

            DPRINTF("increasing version of queue " SFQ " because acl " SFQ
                    " changed\n", lGetString(qinstance, QU_full_name), acl_name);
            qinstance_increase_qversion(qinstance);
            sge_event_spool(&answer_list, 0, sgeE_QINSTANCE_MOD, 0, 0, lGetString(qinstance, QU_qname),
                            lGetHost(qinstance, QU_qhostname), nullptr, qinstance, nullptr, nullptr, true, false, packet->gdi_session);
            answer_list_output(&answer_list);
         }
      }
   }

   DRETURN_VOID;
}

/******************************************************
   sge_verify_department_entries()
      resolves user set/department

   userset_list
      the current master user list (US_Type)
   new_userset
      the new userset element
   alpp
      may be nullptr
      is used to build up an answer
      element in case of error

   returns
      STATUS_OK         - on success
      STATUS_ESEMANTIC  - on error
 ******************************************************/
/** @brief Check that a user set may become, or remain, a department
 *
 * A user may belong to only one department, so a set marked as one must not
 * overlap any other department.
 *
 * @param userset_list the existing user sets
 * @param new_userset the set being added or changed
 * @param alpp receives messages for the caller
 * @return STATUS_OK when the departments stay consistent
 */
int
sge_verify_department_entries(const lList *userset_list, lListElem *new_userset, lList **alpp) {
   DENTER(TOP_LAYER);

   if (!strcmp(lGetString(new_userset, US_name), DEFAULT_DEPARTMENT)) {
      if (!dept_is_valid_defaultdepartment(new_userset, alpp)) {
         DRETURN(STATUS_ESEMANTIC);
      }
   }

   if (!(lGetUlong(new_userset, US_type) & US_DEPT)) {
      DRETURN(STATUS_OK);
   }

   DRETURN(STATUS_OK);
}

/**
 * @brief Is defaultdepartment correct
 *
 * Check if the given defaultdepartment "dept" is valid.
 *
 * @param dept US_Type defaultdepartment
 * @param answer_list AN_Type answer list
 *
 * @return 0 or 1
 */
static int
dept_is_valid_defaultdepartment(lListElem *dept, lList **answer_list) {
   DENTER(TOP_LAYER);

   int ret = 1;
   if (dept != nullptr) {
      /* test 'type' */
      if (!(lGetUlong(dept, US_type) & US_DEPT)) {
         ERROR(SFNMAX, MSG_QMASTER_DEPTFORDEFDEPARTMENT);
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         ret = 0;
      }
      /* test user list */
      if (lGetNumberOfElem(lGetList(dept, US_entries)) > 0) {
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
 * @param acl US_Type acl
 * @param answer_list AN_Type list
 *
 * @return 0 or 1
 */
static int
acl_is_valid_acl(lListElem *acl, lList **answer_list) {
   DENTER(TOP_LAYER);

   int ret = 1;
   if (acl != nullptr) {
      if (!(lGetUlong(acl, US_type) & US_DEPT)) {
         if (lGetUlong(acl, US_fshare) > 0) {
            ERROR(SFNMAX, MSG_QMASTER_ACLNOSHARE);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            ret = 0;
         }
         if (lGetUlong(acl, US_oticket) > 0) {
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
 * @param job the job (`JB_Type`) whose owner and groups are checked
 * @param alpp used to return error messages; may be `nullptr` when the caller
 *             is probing several departments and a non-match is not an error
 * @param dept_name the department to check against
 * @param userset_list the departments (`US_Type`)
 * @return true if the owner is a member of `dept_name`
 */
bool
job_is_valid_department(lListElem *job, lList **alpp, const char *dept_name, const lList *userset_list) {
   DENTER(TOP_LAYER);

   // Do we have a department name?
   if (dept_name == nullptr) {
      ERROR(SFNMAX, MSG_JOB_NODEPTFOUND);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // Does the department exist?
   const lListElem *dept = lGetElemStr(userset_list, US_name, dept_name);
   if (!dept || !(lGetUlong(dept, US_type) & US_DEPT)) {
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
   const char *owner = lGetString(job, JB_owner);
   if (sge_contained_in_access_list(owner, nullptr, nullptr, dept)) {
      DRETURN(true);
   }

   // Does the group or a supplementary group belong to the department?
   const char *group = lGetString(job, JB_group);
   const lList *grp_list = lGetList(job, JB_grp_list);
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
 * @param job the job (`JB_Type`); `JB_department` is set on success
 * @param alpp used to return error messages
 * @param userset_list the departments (`US_Type`)
 * @return true if a department could be assigned
 */
bool
job_set_department(lListElem *job, lList **alpp, const lList *userset_list) {
   DENTER(TOP_LAYER);

   // Find the first department where the user is a member of
   for_each_ep_lv(dept, userset_list) {
      const char *dept_name = lGetString(dept, US_name);
      if (strcmp(dept_name, DEFAULT_DEPARTMENT) == 0) {
         continue;
      }
      if (job_is_valid_department(job, nullptr, dept_name, userset_list)) {
         lSetString(job, JB_department, lGetString(dept, US_name));
         DRETURN(true);
      }
   }

   // No department found => set default department
   if (lGetElemStr(userset_list, US_name, DEFAULT_DEPARTMENT)) {
      lSetString(job, JB_department, DEFAULT_DEPARTMENT);
      DRETURN(true);
   }

   // Also no default department found
   ERROR(SFNMAX, MSG_JOB_NODEPTFOUND);
   answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   DRETURN(false);
}

static int
verify_userset_deletion(lList **alpp, const char *userset_name) {
   DENTER(TOP_LAYER);
   int ret = STATUS_OK;
   const lListElem *ep;
   lList *user_lists = nullptr;
   const lListElem *cl;
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
   const lList *master_ehost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
   const lList *master_role_list = *ocs::DataStore::get_master_list(SGE_TYPE_RL);


   /*
    * fix for bug 6422335
    * check the cq configuration for userset references instead of qinstances
    */
   for_each_ep_lv(cqueue, master_cqueue_list) {
      for_each_ep(cl, lGetList(cqueue, CQ_acl)) {
         if (lGetSubStr(cl, US_name, userset_name, AUSRLIST_value)) {
            ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_QUEUE, lGetString(cqueue, CQ_name));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
      }
      for_each_ep(cl, lGetList(cqueue, CQ_xacl)) {
         if (lGetSubStr(cl, US_name, userset_name, AUSRLIST_value)) {
            ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_QUEUE, lGetString(cqueue, CQ_name));
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            ret = STATUS_EUNKNOWN;
         }
      }
   }

   for_each_ep(ep, master_pe_list) {
      if (lGetElemStr(lGetList(ep, PE_user_list), US_name, userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_PE, lGetString(ep, PE_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
      if (lGetElemStr(lGetList(ep, PE_xuser_list), US_name, userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_PE, lGetString(ep, PE_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   for_each_ep(ep, master_project_list) {
      if (lGetElemStr(lGetList(ep, PR_acl), US_name, userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_PRJ, lGetString(ep, PR_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
      if (lGetElemStr(lGetList(ep, PR_xacl), US_name, userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_PRJ, lGetString(ep, PR_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /* hosts */
   for_each_ep(ep, master_ehost_list) {
      if (lGetElemStr(lGetList(ep, EH_acl), US_name, userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_EH, lGetHost(ep, EH_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
      if (lGetElemStr(lGetList(ep, EH_xacl), US_name, userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_EH, lGetHost(ep, EH_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
   }

   // refuse deletion if any role still references this userset in its user_list
   for_each_ep_lv(role_ep, master_role_list) {
      if (lGetElemStr(lGetList(role_ep, RL_user_list), US_name, userset_name)) {
         ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_ROLE, lGetString(role_ep, RL_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = STATUS_EUNKNOWN;
      }
   }

   /* global configuration */
   user_lists = mconf_get_user_lists();
   if (lGetElemStr(user_lists, US_name, userset_name)) {
      ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_USERLIST, MSG_OBJ_CONF, MSG_OBJ_GLOBAL);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = STATUS_EUNKNOWN;
   }
   lFreeList(&user_lists);

   user_lists = mconf_get_xuser_lists();
   if (lGetElemStr(user_lists, US_name, userset_name)) {
      ERROR(MSG_SGETEXT_USERSETSTILLREFERENCED_SSSS, userset_name, MSG_OBJ_XUSERLIST, MSG_OBJ_CONF, MSG_OBJ_GLOBAL);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      ret = STATUS_EUNKNOWN;
   }
   lFreeList(&user_lists);

   DRETURN(ret);
}

/**
 * @brief True, if userset still used
 *
 * Returns true, if userset is still used as ACL with host_conf(5),
 * queue_conf(5), or sge_pe(5).
 * Use of usersets as ACLs in sge_conf(5) play no role here,
 * since such ACLs are checked in qmaster and thus are not
 * relevant for the scheduling algorithm.
 *
 * @param p the userset
 *
 * @return True, if userset still used
 *
 * @note MT-NOTE: userset_still_used() is not MT safe
 */
static bool userset_still_used(const char *u) {
   const lListElem *qc, *cq, *hep, *rqs;
   dstring ds = DSTRING_INIT;
   const lList *master_rqs_list = *ocs::DataStore::get_master_list(SGE_TYPE_RQS);
   const lList *master_pe_list = *ocs::DataStore::get_master_list(SGE_TYPE_PE);
   const lList *master_ehost_list = *ocs::DataStore::get_master_list(SGE_TYPE_EXECHOST);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);

   sge_dstring_sprintf(&ds, "@%s", u);

   for_each_ep(rqs, master_rqs_list) {
      if (scope_is_referenced_rqs(rqs, RQR_filter_users, sge_dstring_get_string(&ds))) {
         sge_dstring_free(&ds);
         return true;
      }
   }
   sge_dstring_free(&ds);

   for_each_ep(hep, master_pe_list)if (lGetSubStr(hep, US_name, u, PE_user_list) ||
                                       lGetSubStr(hep, US_name, u, PE_xuser_list))
         return true;

   for_each_ep(hep, master_ehost_list)if (lGetSubStr(hep, US_name, u, EH_acl) || lGetSubStr(hep, US_name, u, EH_xacl))
         return true;

   for_each_ep(cq, master_cqueue_list) {
      for_each_ep(qc, lGetList(cq, CQ_acl))if (lGetSubStr(qc, US_name, u, AUSRLIST_value))
            return true;
      for_each_ep(qc, lGetList(cq, CQ_xacl))if (lGetSubStr(qc, US_name, u, AUSRLIST_value))
            return true;
   }

   return false;
}


/**
 * @brief Update all usersets wrts categories
 *
 * Each added/removed userset is verified whether it is used first
 * time/still as ACL for host_conf(5)/queue_conf(5)/sge_pe(5). If
 * so an event is sent.
 *
 * @param added List of added userset references (US_Type)
 * @param removed List of removed userset references (US_Type)
 * @param gdi_session the session the change belongs to
 *
 * @note MT-NOTE: userset_update_categories() is not MT safe
 */
void userset_update_categories(const lList *added, const lList *removed, uint64_t gdi_session) {
   DENTER(TOP_LAYER);
   const lListElem *ep;
   const char *u;
   lListElem *acl;
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   bool reattach_categories = false;

   for_each_ep(ep, added) {
      u = lGetString(ep, US_name);
      DPRINTF("added userset: \"%s\"\n", u);
      acl = lGetElemStrRW(master_userset_list, US_name, u);
      if (acl && !lGetBool(acl, US_consider_with_categories)) {
         lSetBool(acl, US_consider_with_categories, true);
         reattach_categories = true;
         sge_add_event(0, sgeE_USERSET_MOD, 0, 0, u, nullptr, nullptr, acl, gdi_session);
      }
   }

   for_each_ep(ep, removed) {
      u = lGetString(ep, US_name);
      DPRINTF("removed userset: \"%s\"\n", u);
      acl = lGetElemStrRW(master_userset_list, US_name, u);

      if (acl && !userset_still_used(u)) {
         lSetBool(acl, US_consider_with_categories, false);
         reattach_categories = true;
         sge_add_event(0, sgeE_USERSET_MOD, 0, 0, u, nullptr, nullptr, acl, gdi_session);
      }
   }

   // reattach all jobs to categories and consider the new project attributes
   if (reattach_categories) {
      lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
      const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
      const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
      const lList *master_rqs_list = *ocs::DataStore::get_master_list(SGE_TYPE_RQS);
      ocs::CategoryQmaster::reattach_all_jobs(master_job_list, master_userset_list, master_project_list, master_rqs_list, true, gdi_session);
   }

   DRETURN_VOID;
}

/**
 * @brief Gdi callback function for adding/modifing a userset
 *
 * This function is called from the generic gdi framework when a userset is
 * added or modified.
 *
 * @param alpp answer list
 * @param new_userset if add it's an empty userset that needs to be filled if mod it's the actual stored userset
 * @param userset reduced userset object with new/modified values
 * @param add 1 for gdi add 0 for gdi mod
 * @param ruser user who invoked the gdi request
 * @param rhost host where the gid request was invoked
 * @param object structure of the gdi framework
 * @param sub_command requested sub_commands
 * @param monitor monitoring structure
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cmd the command being executed
 *
 * @return 0 on success STATUS_EUNKNOWN if an error occurred
 *
 * @note MT-NOTE: userset_mod() is not MT safe, needs global lock
 */
int userset_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *new_userset,
                lListElem *userset, int add, const char *ruser,
                const char *rhost, gdi_object_t *object,
                ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
                monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *userset_name;
   const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lList *master_ar_list = *ocs::DataStore::get_master_list(SGE_TYPE_AR);

   /* ---- US_name */
   if (add) {
      if (attr_mod_str(alpp, userset, new_userset, US_name, object->object_name)) {
         goto ERROR;
      }
   }
   userset_name = lGetString(new_userset, US_name);

   if (add && verify_obj_name(
           alpp, userset_name, MAX_VERIFY_STRING, object->object_name) != STATUS_OK) {
      goto ERROR;
   }

   /* ---- US_fshare */
   attr_mod_ulong(userset, new_userset, US_fshare, object->object_name);

   /* ---- US_oticket */
   attr_mod_ulong(userset, new_userset, US_oticket, object->object_name);

   /* ---- US_type */
   attr_mod_ulong(userset, new_userset, US_type, object->object_name);

   /* ensure userset is at least an ACL (qconf -au ) */
   if (lGetUlong(new_userset, US_type) == 0) {
      lSetUlong(new_userset, US_type, US_ACL);
   }
   /* make sure acl is valid */
   if (!add) {
      if (acl_is_valid_acl(new_userset, alpp) != STATUS_OK) {
         goto ERROR;
      }
   }
   /* ---- US_entries */
   attr_mod_sub_list(alpp, new_userset, US_entries,
                     UE_name, userset, cmd, sub_command, SGE_ATTR_USER_LISTS, object->object_name, 0, nullptr);
   /* interpret user/group names */
   if (userset_validate_entries(new_userset, alpp) != STATUS_OK) {
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
      if ((lGetUlong(new_userset, US_type) & US_ACL) == 0) {
         ERROR(MSG_USERSET_RESERVED_MUSTBEACL_S, userset_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         goto ERROR;
      }

      /* admin_user "none" is the sentinel for "no admin user", it is not seeded either */
      const char *admin_user = ocs::Bootstrap::get_admin_user();
      if (lGetSubStr(new_userset, UE_name, "root", US_entries) == nullptr) {
         ERROR(MSG_SGETEXT_MAY_NOT_REMOVE_USER_FROM_LIST_SS, "root", manop_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         goto ERROR;
      }
      if (admin_user != nullptr && sge_strnullcasecmp(admin_user, NONE_STR) != 0 &&
          lGetSubStr(new_userset, UE_name, admin_user, US_entries) == nullptr) {
         ERROR(MSG_SGETEXT_MAY_NOT_REMOVE_USER_FROM_LIST_SS, admin_user, manop_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         goto ERROR;
      }
   }

   /*
   ** check for users defined in more than one userset if they
   ** are used as departments
   */
   if (sge_verify_department_entries(master_userset_list, new_userset, alpp) != STATUS_OK) {
      goto ERROR;
   }

   /*
   ** check advance reservations
   */
   if (!add) {
      const lListElem *cqueue;
      lList *new_master_userset_list = nullptr;

      for_each_ep(cqueue, master_cqueue_list) {
         const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
         const lListElem *qinstance;

         for_each_ep(qinstance, qinstance_list) {
            const char *queue_name = lGetString(qinstance, QU_full_name);

            if (qinstance_slots_reserved(qinstance) == 0) {
               /* queue not reserved, can not conflict with AR */
               continue;
            }

            if (lGetElemStr(lGetList(qinstance, QU_acl), US_name, userset_name) == nullptr &&
                lGetElemStr(lGetList(qinstance, QU_xacl), US_name, userset_name) == nullptr) {
               /* userset not referenced by queue instance */
               continue;
            }

            for_each_rw_lv(ar, master_ar_list) {
               if (lGetElemStr(lGetList(ar, AR_granted_slots), JG_qname, queue_name)) {
                  if (new_master_userset_list == nullptr) {
                     lListElem *old_userset;
                     new_master_userset_list = lCopyList("", master_userset_list);
                     old_userset = lGetElemStrRW(new_master_userset_list, US_name, userset_name);
                     lRemoveElem(new_master_userset_list, &old_userset);
                     lAppendElem(new_master_userset_list, lCopyElem(new_userset));
                  }

                  if (!sge_ar_have_users_access(nullptr, ar, lGetString(qinstance, QU_full_name),
                                                lGetList(qinstance, QU_acl),
                                                lGetList(qinstance, QU_xacl),
                                                new_master_userset_list)) {
                     answer_list_add_sprintf(alpp, STATUS_ESYNTAX,
                                             ANSWER_QUALITY_ERROR,
                                             MSG_PARSE_MOD3_REJECTED_DUE_TO_AR_SU,
                                             "entries",
                                             lGetUlong(ar, AR_id));
                     lFreeList(&new_master_userset_list);
                     goto ERROR;
                  }
               }
            }
         }
      }
      lFreeList(&new_master_userset_list);
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
 * @param alpp answer list
 * @param userset userset object to spool
 * @param object structure of the gdi framework
 * @param packet the client request
 * @param task the GDI task being answered
 *
 * @return [alpp] - error messages will be added to this list 0 - success STATUS_EEXIST - an error occurred
 *
 * @note MT-NOTE: userset_spool() is not MT safe
 */
int userset_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *userset, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   lList *answer_list = nullptr;
   bool dbret;

   dbret = spool_write_object(&answer_list, spool_get_default_context(), userset,
                              lGetString(userset, US_name), SGE_TYPE_USERSET);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, lGetString(userset, US_name));
   }

   DRETURN(dbret ? 0 : 1);

}

/**
 * @brief Do something after a successful add/mod
 *
 * This function is called from the framework after successfull add/mod and
 * spool.
 *
 * @param ep new added userset
 * @param old_ep for mod the old userset
 * @param object structure of the gdi framework
 * @param ppList additional list that is returned to the client
 * @param monitor monitoring structure
 * @param packet the client request
 * @param task the GDI task being answered
 *
 * @return 0 on success
 *
 * @note MT-NOTE: userset_success() is not MT safe
 */
int userset_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList,
                    monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   dstring ds = DSTRING_INIT;
   const lListElem *rqs;
   const lList *master_rqs_list = *ocs::DataStore::get_master_list(SGE_TYPE_RQS);
   const char *userset_name = lGetString(ep, US_name);

   /* set consider with categories */
   bool reattach_categories = false;
   sge_dstring_sprintf(&ds, "@%s", userset_name);
   for_each_ep(rqs, master_rqs_list) {
      if (scope_is_referenced_rqs(rqs, RQR_filter_users, sge_dstring_get_string(&ds))) {
         lSetBool(ep, US_consider_with_categories, true);
         reattach_categories = true;
         break;
      }
   }

   if (old_ep != nullptr) {
      /* change queue versions if userset was modified */
      sge_change_queue_version_acl(packet, task, userset_name);
   }

   if (reattach_categories) {
      lList *master_job_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_JOB);
      const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
      const lList *master_project_list = *ocs::DataStore::get_master_list(SGE_TYPE_PROJECT);
      ocs::CategoryQmaster::reattach_all_jobs(master_job_list, master_userset_list, master_project_list, master_rqs_list, true, packet->gdi_session);
   }

   sge_add_event(0, old_ep ? sgeE_USERSET_MOD : sgeE_USERSET_ADD, 0, 0,
                 userset_name, nullptr, nullptr, ep, packet->gdi_session);

   sge_dstring_free(&ds);
   DRETURN(0);
}
