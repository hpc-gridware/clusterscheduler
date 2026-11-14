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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Reports: everything an execution host sends back to qmaster
 *
 * Load values, the host's configuration and the state of its jobs all travel
 * in one report object; `REP_type` says which of them `REP_list` holds.
 *
 * @see sge_report.h
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_report.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_JG.h"

#include "execution_states.h"

#if 0
/**
 * @brief Print usage contained in job report
 *
 * Print usage information conatines in "job_report". Print the
 * information to the given file stream "fp" or as debug messages.
 *
 * @param job_report JR_Type element
 * @param fp file stream or nullptr
 */
void job_report_print_usage(const lListElem *job_report, FILE *fp) 
{
   DENTER(TOP_LAYER);

   if (!job_report) {
      DRETURN_VOID;
   }

   for_each_ep_lv(uep, lGetList(job_report, JR_usage)) {
      if (fp) {
         fprintf(fp, "   \"%s\" =   %.99g\n", lGetString(uep, UA_name),
                 lGetDouble(uep, UA_value));
      } else {
         DPRINTF(("   \"%s\" =   %.99g\n", lGetString(uep, UA_name),
                  lGetDouble(uep, UA_value)));
      }
   }

   DRETURN_VOID;
}
#endif

/**
 * @brief Initialize job report
 *
 * Initialize "job_report" from the attributes obtained from
 * "job", "ja_task" and "pe_task".
 *
 * @param job_report JR_Type object
 * @param job JB_Type object
 * @param ja_task JAT_Type object
 * @param pe_task PET_Type object
 */
void job_report_init_from_job(lListElem *job_report,
                              const lListElem *job,
                              const lListElem *ja_task,
                              const lListElem *pe_task) {
   DENTER(TOP_LAYER);

   uint32_t job_id = ocs::tom::get<JB_job_number>(job);
   uint32_t ja_task_id = ocs::tom::get<JAT_task_number>(ja_task);
   ocs::tom::ConstElement<ocs::tom::JG> queue;

   ocs::tom::set<JR_job_number>(job_report, job_id);
   ocs::tom::set<JR_ja_task_number>(job_report, ja_task_id);

   if (pe_task != nullptr) {
      ocs::tom::set<JR_pe_task_id_str>(job_report, ocs::tom::get<PET_id>(pe_task));
      queue = lFirst(ocs::tom::get<PET_granted_destin_identifier_list>(pe_task));
   } else {
      queue = lFirst(ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task));
   }

   if (ocs::tom::get<JAT_status>(ja_task) == JSLAVE){
      if (pe_task == nullptr) {
         ocs::tom::set<JR_state>(job_report, JSLAVE);
      } else {
         ocs::tom::set<JR_state>(job_report, JWRITTEN);
      }
   } else {
      ocs::tom::set<JR_state>(job_report, JWRITTEN);
   }

   if (queue != nullptr) {
      ocs::tom::set<JR_queue_name>(job_report, ocs::tom::get<JG_qname>(queue));
   }

   DRETURN_VOID;
}

/**
 * @brief Fill a job report with the usage a finished job accumulated
 *
 * The usage is taken from the parallel task when one is given, otherwise from
 * the array task.
 *
 * @param[in,out] job_report the report to fill
 * @param job the job the report is about
 * @param ja_task the array task
 * @param pe_task the parallel task, or nullptr
 * @param time_stamp the time the usage was collected
 */
void job_report_init_from_job_with_usage(lListElem *job_report,
                                         const lListElem *job,
                                         lListElem *ja_task,
                                         lListElem *pe_task,
                                         uint64_t time_stamp) {
   DENTER(TOP_LAYER);

   lListElem *obj;
   int nm;

   /*
    * initialize the job jeport like any other job report...
    */
   job_report_init_from_job(job_report, job, ja_task, pe_task);

   /*
    * ... and now add the online usage plus some fileds which are needed for the accounting
    */
   ocs::tom::set<JR_wait_status>(job_report, SGE_SET_WEXITSTATUS(SGE_WEXITED_BIT, 0));
   ocs::tom::set<JR_failed>(job_report, SSTATE_QMASTER_ENFORCED_LIMIT);

   if (pe_task == nullptr) {
      nm = JAT_scaled_usage_list;
      obj = ja_task;
   } else {
      nm = PET_scaled_usage;
      obj = pe_task;

      ocs::tom::set<JR_pe_task_id_str>(job_report, ocs::tom::get<PET_id>(pe_task));
   }

   // the qmaster may enforce a limit on the same task more than once, so the usage can hold these entries already
   lList *usage = lGetOrCreateList(obj, nm, "scaled", UA_Type);
   usage_list_set_double_usage(usage, "submission_time", ocs::tom::get<JB_submission_time>(job));
   usage_list_set_double_usage(usage, "start_time", ocs::tom::get<JAT_start_time>(ja_task));
   usage_list_set_double_usage(usage, "end_time", time_stamp);
   usage_list_set_double_usage(usage, "ru_wallclock", 0.0);

   ocs::tom::set<JR_usage>(job_report, lCopyList("", lGetList(obj, nm)));
   DRETURN_VOID;
}
