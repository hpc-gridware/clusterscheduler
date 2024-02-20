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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstring>

#include "uti/config_file.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_pe.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_utility.h"

#include "sched/sge_job_schedd.h"

#include "spool/sge_spooling.h"

#include "sge.h"
#include "sge_pe_qmaster.h"
#include "evm/sge_event_master.h"
#include "sge_userset_qmaster.h"
#include "sge_utility_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"


static char object_name[] = "parallel environment";

static void pe_update_categories(const lListElem *new_pe, const lListElem *old_pe);

int
pe_mod(lList **alpp, lListElem *new_pe, lListElem *pe, /* reduced */
       int add, const char *ruser, const char *rhost, gdi_object_t *object, int sub_command,
       monitoring_t *monitor) {
   int ret;
   const char *s, *pe_name;
   const lList *master_userset_list = *object_type_get_master_list(SGE_TYPE_USERSET);
   const lList *master_ar_list = *object_type_get_master_list(SGE_TYPE_AR);

   DENTER(TOP_LAYER);

   /* ---- PE_name */
   if (add) {
      if (attr_mod_str(alpp, pe, new_pe, PE_name, object->object_name)) {
         goto ERROR;
      }
   }
   pe_name = lGetString(new_pe, PE_name);

   /* Name has to be a valid filename without pathchanges */
   if (add && verify_str_key(alpp, pe_name, MAX_VERIFY_STRING, MSG_OBJ_PE, KEY_TABLE) != STATUS_OK) {
      DRETURN(STATUS_EUNKNOWN);
   }

   /* ---- PE_slots */
   if (lGetPosViaElem(pe, PE_slots, SGE_NO_ABORT) >= 0) {
      u_long32 pe_slots = lGetUlong(pe, PE_slots);

      if (pe_validate_slots(alpp, pe_slots) != STATUS_OK) {
         goto ERROR;
      }

      if (ar_list_has_reservation_for_pe_with_slots(master_ar_list, alpp, pe_name, pe_slots)) {
         goto ERROR;
      }
   }
   attr_mod_ulong(pe, new_pe, PE_slots, "slots");

   /* ---- PE_control_slaves */
   attr_mod_bool(pe, new_pe, PE_control_slaves, "control_slaves");

   /* ---- PE_job_is_first_task */
   attr_mod_bool(pe, new_pe, PE_job_is_first_task, "job_is_first_task");

   /* ---- PE_user_list */
   if (lGetPosViaElem(pe, PE_user_list, SGE_NO_ABORT) >= 0) {
      DPRINTF(("got new PE_user_list\n"));
      /* check user_lists */
      normalize_sublist(pe, PE_user_list);
      if (userset_list_validate_acl_list(lGetList(pe, PE_user_list), alpp, master_userset_list) != STATUS_OK) {
         goto ERROR;
      }

      attr_mod_sub_list(alpp, new_pe, PE_user_list, US_name, pe, sub_command,
                        SGE_ATTR_USER_LISTS, SGE_OBJ_PE, 0, nullptr);
   }

   /* ---- PE_xuser_list */
   if (lGetPosViaElem(pe, PE_xuser_list, SGE_NO_ABORT) >= 0) {
      DPRINTF(("got new QU_axcl\n"));
      /* check xuser_lists */
      normalize_sublist(pe, PE_xuser_list);
      if (userset_list_validate_acl_list(lGetList(pe, PE_xuser_list), alpp, master_userset_list) != STATUS_OK) {
         goto ERROR;
      }
      attr_mod_sub_list(alpp, new_pe, PE_xuser_list, US_name, pe, sub_command,
                        SGE_ATTR_XUSER_LISTS, SGE_OBJ_PE, 0, nullptr);
   }

   if (lGetPosViaElem(pe, PE_xuser_list, SGE_NO_ABORT) >= 0 ||
       lGetPosViaElem(pe, PE_user_list, SGE_NO_ABORT) >= 0) {
      if (multiple_occurances(alpp, lGetList(new_pe, PE_user_list), lGetList(new_pe, PE_xuser_list), US_name, pe_name,
                              object_name)) {
         goto ERROR;
      }
   }

   if (attr_mod_procedure(alpp, pe, new_pe, PE_start_proc_args, "start_proc_args", pe_variables)) {
      goto ERROR;
   }
   if (attr_mod_procedure(alpp, pe, new_pe, PE_stop_proc_args, "stop_proc_args", pe_variables)) {
      goto ERROR;
   }

   /* -------- PE_allocation_rule */
   if (lGetPosViaElem(pe, PE_allocation_rule, SGE_NO_ABORT) >= 0) {
      s = lGetString(pe, PE_allocation_rule);
      if (s == nullptr) {
         ERROR((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(PE_allocation_rule), "validate_pe"));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }

      if (replace_params(s, nullptr, 0, pe_alloc_rule_variables)) {
         ERROR((SGE_EVENT, MSG_PE_ALLOCRULE_SS, pe_name, err_msg));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }
      lSetString(new_pe, PE_allocation_rule, s);
   }

   /* -------- PE_urgency_slots */
   if (lGetPosViaElem(pe, PE_urgency_slots, SGE_NO_ABORT) >= 0) {
      s = lGetString(pe, PE_urgency_slots);
      if (s == nullptr) {
         ERROR((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(PE_allocation_rule), "validate_pe"));
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EEXIST);
      }

      if ((ret = pe_validate_urgency_slots(alpp, s)) != STATUS_OK) {
         DRETURN(ret);
      }
      lSetString(new_pe, PE_urgency_slots, s);
   }

#ifdef SGE_PQS_API
   /* -------- PE_qsort_args */
   if (lGetPosViaElem(pe, PE_qsort_args, SGE_NO_ABORT) >= 0) {
      void *handle=nullptr, *fn=nullptr;

      s = lGetString(pe, PE_qsort_args);

      if ((ret=pe_validate_qsort_args(alpp, s, new_pe, &handle, &fn)) != STATUS_OK) {
         DRETURN(ret);
      }
      lSetString(new_pe, PE_qsort_args, s);
      /* lSetUlong(new_pe, PE_qsort_validated, 1); */
   }
#endif

   /* ---- PE_accounting_summary */
   attr_mod_bool(pe, new_pe, PE_accounting_summary, "accounting_summary");

   /* -------- PE_resource_utilization */
   if (add) {
      if (pe_set_slots_used(new_pe, 0)) {
         ERROR((SGE_EVENT, SFNMAX, MSG_MEM_MALLOC));
         answer_list_add(alpp, SGE_EVENT, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EMALLOC);
      }
   }

   DRETURN(0);

   ERROR:
   DRETURN(STATUS_EUNKNOWN);
}

int
pe_spool(lList **alpp, lListElem *pep, gdi_object_t *object) {
   lList *answer_list = nullptr;
   bool dbret;

   DENTER(TOP_LAYER);

   dbret = spool_write_object(&answer_list, spool_get_default_context(), pep,
                              lGetString(pep, PE_name), SGE_TYPE_PE, true);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, lGetString(pep, PE_name));
   }

   DRETURN(dbret ? 0 : 1);
}

int pe_success(lListElem *ep, lListElem *old_ep, gdi_object_t *object, lList **ppList,
               monitoring_t *monitor) {
   const char *pe_name;

   DENTER(TOP_LAYER);

   pe_name = lGetString(ep, PE_name);

   pe_update_categories(ep, old_ep);

   sge_add_event(0, old_ep ? sgeE_PE_MOD : sgeE_PE_ADD, 0, 0,
                 pe_name, nullptr, nullptr, ep);
   lListElem_clear_changed_info(ep);

   DRETURN(0);
}

int
sge_del_pe(lListElem *pep, lList **alpp, char *ruser, char *rhost) {
   int pos;
   lListElem *ep = nullptr;
   const char *pe = nullptr;
   const lList *master_job_list = *object_type_get_master_list(SGE_TYPE_JOB);
   const lList *master_cqueue_list = *object_type_get_master_list(SGE_TYPE_CQUEUE);
   lList *master_pe_list = *object_type_get_master_list_rw(SGE_TYPE_PE);

   DENTER(TOP_LAYER);

   if (!pep || !ruser || !rhost) {
      CRITICAL((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, __func__));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if ((pos = lGetPosViaElem(pep, PE_name, SGE_NO_ABORT)) < 0) {
      ERROR((SGE_EVENT, MSG_SGETEXT_MISSINGCULLFIELD_SS,
              lNm2Str(PE_name), __func__));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   pe = lGetPosString(pep, pos);
   if (!pe) {
      ERROR((SGE_EVENT, MSG_SGETEXT_NULLPTRPASSED_S, __func__));
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if ((ep = pe_list_locate(master_pe_list, pe)) == nullptr) {
      ERROR((SGE_EVENT, MSG_SGETEXT_DOESNOTEXIST_SS, object_name, pe));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* 
    * Try to find references in other objects
    */
   {
      lList *local_answer_list = nullptr;

      if (pe_is_referenced(ep, &local_answer_list, master_job_list, master_cqueue_list)) {
         const lListElem *answer = lFirst(local_answer_list);

         ERROR((SGE_EVENT, "denied: %s", lGetString(answer, AN_text)));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN,
                         ANSWER_QUALITY_ERROR);
         lFreeList(&local_answer_list);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   /* remove host file */
   if (!sge_event_spool(alpp, 0, sgeE_PE_DEL,
                        0, 0, pe, nullptr, nullptr, nullptr, nullptr, nullptr, true, true)) {
      ERROR((SGE_EVENT, MSG_CANTSPOOL_SS, object_name, pe));
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   pe_update_categories(nullptr, ep);

   /* delete found pe element */
   lRemoveElem(master_pe_list, &ep);

   INFO((SGE_EVENT, MSG_SGETEXT_REMOVEDFROMLIST_SSSS,
           ruser, rhost, pe, object_name));
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}

void
debit_all_jobs_from_pes(lList *pe_list) {
   const char *pe_name;
   const lListElem *jep;
   lListElem *pep;
   int slots;
   const lList *master_job_list = *object_type_get_master_list(SGE_TYPE_JOB);

   DENTER(TOP_LAYER);

   for_each_rw(pep, pe_list)
   {

      pe_set_slots_used(pep, 0);
      pe_name = lGetString(pep, PE_name);
      DPRINTF(("debiting from pe %s:\n", pe_name));

      for_each_ep(jep, master_job_list)
      {
         const lListElem *jatep;

         if (lGetString(jep, JB_pe) != nullptr) { /* is job  parallel */
            slots = 0;
            for_each_ep(jatep, lGetList(jep, JB_ja_tasks))
            {
               if ((ISSET(lGetUlong(jatep, JAT_status), JRUNNING) ||      /* is job running  */
                    ISSET(lGetUlong(jatep, JAT_status), JTRANSFERING)) && /* or transfering  */
                   !sge_strnullcmp(pe_name, lGetString(jatep, JAT_granted_pe))) {/* this pe         */
                  slots += sge_granted_slots(lGetList(jatep, JAT_granted_destin_identifier_list));
               }
            }
            pe_debit_slots(pep, slots, lGetUlong(jep, JB_job_number));
         }
      }
   }
   DRETURN_VOID;
}

/****** sge_pe_qmaster/pe_diff_usersets() **************************************
*  NAME
*     pe_diff_usersets() -- Diff old/new PE usersets
*
*  SYNOPSIS
*     void pe_diff_usersets(const lListElem *new_pe, const lListElem *old_pe, lList
*     **new_acl, lList **old_acl)
*
*  FUNCTION
*     A diff new/old is made regarding PE acl/xacl.
*     Userset references are returned in new_acl/old_acl.
*
*  INPUTS
*     const lListElem *new_pe - New PE (PE_Type)
*     const lListElem *old_pe - Old PE (PE_Type)
*     lList **new_acl      - New userset references (US_Type)
*     lList **old_acl      - Old userset references (US_Type)
*
*  NOTES
*     MT-NOTE: pe_diff_usersets() is not MT safe
*******************************************************************************/
void
pe_diff_usersets(const lListElem *new_pe, const lListElem *old_pe, lList **new_acl, lList **old_acl) {
   const lListElem *ep;
   const char *u;

   if (old_pe && old_acl) {
      for_each_ep(ep, lGetList(old_pe, PE_user_list))
      {
         u = lGetString(ep, US_name);
         if (!lGetElemStr(*old_acl, US_name, u))
            lAddElemStr(old_acl, US_name, u, US_Type);
      }
      for_each_ep(ep, lGetList(old_pe, PE_xuser_list))
      {
         u = lGetString(ep, US_name);
         if (!lGetElemStr(*old_acl, US_name, u))
            lAddElemStr(old_acl, US_name, u, US_Type);
      }
   }

   if (new_pe && new_acl) {
      for_each_ep(ep, lGetList(new_pe, PE_user_list))
      {
         u = lGetString(ep, US_name);
         if (!lGetElemStr(*new_acl, US_name, u))
            lAddElemStr(new_acl, US_name, u, US_Type);
      }
      for_each_ep(ep, lGetList(new_pe, PE_xuser_list))
      {
         u = lGetString(ep, US_name);
         if (!lGetElemStr(*new_acl, US_name, u))
            lAddElemStr(new_acl, US_name, u, US_Type);
      }
   }

   lDiffListStr(US_name, new_acl, old_acl);
}


/****** sge_pe_qmaster/pe_update_categories() **********************************
*  NAME
*     pe_update_categories() -- Update categories wrts userset
*
*  SYNOPSIS
*     static void pe_update_categories(const lListElem *new_pe, const lListElem
*     *old_pe)
*
*  FUNCTION
*     The userset information wrts categories is updated based
*      on new/old PE configuration and events are sent upon changes.
*
*  INPUTS
*     const lListElem *new_pe - New PE (PE_Type)
*     const lListElem *old_pe - Old PE (PE_Type)
*
*  NOTES
*     MT-NOTE: pe_update_categories() is not MT safe
*******************************************************************************/
static void
pe_update_categories(const lListElem *new_pe, const lListElem *old_pe) {
   lList *old_lp = nullptr, *new_lp = nullptr;

   pe_diff_usersets(new_pe, old_pe, &new_lp, &old_lp);
   userset_update_categories(new_lp, old_lp);
   lFreeList(&old_lp);
   lFreeList(&new_lp);
}

