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
 *  Portions of this software are Copyright (c) 2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "uti/ocs_Systemd.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_log.h"

#include "ocs_Binding.h"
#include "sgeobj/ocs_BindingType.h"
#include "sgeobj/ocs_BindingObj.h"
#include "sgeobj/ocs_BindingUnit.h"
#include "sgeobj/ocs_BindingStart.h"
#include "sgeobj/ocs_BindingStop.h"
#include "sgeobj/ocs_BindingStrategy.h"
#include "ocs_Job.h"
#include "sge_ja_task.h"
#include "sge_job.h"
#include "sge_pe.h"
#include "sge_pe_task.h"
#include "sge_answer.h"

#include "cull/sge_eejob_SGEJ_L.h"

#include "msg_common.h"
#include "ocs_BindingInstance.h"
#include "ocs_BindingObj.h"

/** @brief Sort jobs in the job list based on  prio, submit time and job number
 *
 * This function sorts the jobs in the provided job list based on the
 * task priority, submit time and job number. The sorting is done in descending order
 * of task priority, and in ascending order of job number for jobs with the same priority.
 *
 * @param job_list Pointer to the list of jobs to be sorted.
 */
void ocs::Job::sgeee_sort_jobs(lList **job_list) {
   DENTER(TOP_LAYER);

   if (!job_list || !*job_list) {
      DRETURN_VOID;
   }

   // Create a temporary list and store the job reference and all data required to sort to list
   lList *tmp_list = lCreateList("tmp list", SGEJ_Type);
   lListElem *job = nullptr;
   lListElem *nxt_job = lFirstRW(*job_list);
   while((job = nxt_job) != nullptr) {
      nxt_job = lNextRW(nxt_job);

      // First try to find an enrolled task. Those will have the highest priority
      // If there is no enrolled task then take the template element
      const lListElem *tmp_task = lFirst(lGetList(job, JB_ja_tasks));
      if (tmp_task == nullptr) {
         tmp_task = lFirst(lGetList(job, JB_ja_template));
      }

      // Store sort-criteria and job reference in the temporary list
      lListElem *tmp_sge_job = lCreateElem(SGEJ_Type);
      lSetDouble(tmp_sge_job, SGEJ_priority, lGetDouble(tmp_task, JAT_prio));
      lSetUlong64(tmp_sge_job, SGEJ_submission_time, lGetUlong64(job, JB_submission_time));
      lSetUlong(tmp_sge_job, SGEJ_job_number, lGetUlong(job, JB_job_number));
      lSetRef(tmp_sge_job, SGEJ_job_reference, job);
      lAppendElem(tmp_list, tmp_sge_job);

      // Remove the job from the original list
      lDechainElem(*job_list, job);
   }

   // Sort the temporary list according to the following criteria:
   lPSortList(tmp_list, "%I- %I+ %I+", SGEJ_priority, SGEJ_submission_time, SGEJ_job_number);

   // The job list is empty at this point, so we can just append the sorted jobs
   for_each_rw(job, tmp_list) {
      lAppendElem(*job_list, static_cast<lListElem *>(lGetRef(job, SGEJ_job_reference)));
   }

   // we need to free the tmp list, but not the job references
   lFreeList(&tmp_list);

   DRETURN_VOID;
}

bool
ocs::Job::job_get_systemd_slice_and_scope(const lListElem *job, const lListElem *ja_task, const lListElem *pe_task,
                                          std::string &slice, std::string &scope, dstring *error_dstr) {
   DENTER(TOP_LAYER);

   bool ret = true;

#if defined(OCS_WITH_SYSTEMD)

   bool is_array = job_is_array(job);
   bool is_tightly_integrated = false;
   if (ja_task != nullptr) {
      const lListElem *pe = lGetObject(ja_task, JAT_pe_object);
      if (pe != nullptr) {
         is_tightly_integrated = lGetBool(pe, PE_control_slaves);
      }
   }

   std::string toplevel_slice = ocs::uti::Systemd::get_slice_name();
   slice = toplevel_slice + "-jobs";
   scope = toplevel_slice + ".";
   if (is_array) {
      std::string jobtask_id = std::to_string(lGetUlong(job, JB_job_number)) + "." + std::to_string(lGetUlong(ja_task, JAT_task_number));
      // array job
      if (is_tightly_integrated) {
         // array PE job, we have master and slave tasks
         // ocs8012-jobs-1234.1.slice, ocs8012.1234.1.master.scope or ocs8012.1234.1.<num>.<hostname>.scope
         slice += "-" + jobtask_id;
         if (pe_task == nullptr) {
            scope += jobtask_id + ".master";
         } else {
            scope += jobtask_id + '.' + lGetString(pe_task, PET_id);
         }
      } else {
         // just an array job
         // ocs8012-jobs.slice, ocs8012.1234.1.scope
         scope += jobtask_id;
      }
   } else {
      std::string job_id = std::to_string(lGetUlong(job, JB_job_number));
      if (is_tightly_integrated) {
         // sequential PE job, we have master and slave tasks
         // ocs8012-jobs-1234.slice, ocs8012.master.scope or ocs8012.<num>.<hostname>.scope
         slice += "-" + job_id;
         if (pe_task == nullptr) {
            scope += job_id + ".master";
         } else {
            scope += job_id + '-' + lGetString(pe_task, PET_id);
         }
      } else {
         // just a sequential job
         // ocs8012-jobs.slice, ocs8012.1234.scope
         scope += job_id;
      }
   }

   slice += ".slice";
   scope += ".scope";
#else
   slice.clear();
   scope.clear();
   ret = false;
#endif

   DRETURN(ret);
}

lListElem *
ocs::Job::binding_get_or_create_elem(lListElem *job, lList**answer) {
   return Binding::binding_get_or_create_elem(job, JB_new_binding, answer);
}

bool
ocs::Job::binding_was_requested(const lListElem *job) {
   return Binding::binding_was_requested(job, JB_new_binding);
}

ocs::BindingType::Type
ocs::Job::binding_get_type(const lListElem *job) {
   return Binding::binding_get_type(job, JB_new_binding);
}

ocs::BindingUnit::Unit
ocs::Job::binding_get_unit(const lListElem *job) {
   return Binding::binding_get_unit(job, JB_new_binding);
}

std::string
ocs::Job::binding_get_sort(const lListElem *job) {
   return Binding::binding_get_sort(job, JB_new_binding);
}

ocs::BindingStart::Start
ocs::Job::binding_get_start(const lListElem *job) {
   return Binding::binding_get_start(job, JB_new_binding);
}

ocs::BindingStop::Stop
ocs::Job::binding_get_stop(const lListElem *job) {
   return Binding::binding_get_end(job, JB_new_binding);
}

ocs::BindingStrategy::Strategy
ocs::Job::binding_get_strategy(const lListElem *job) {
   return Binding::binding_get_strategy(job, JB_new_binding);
}

ocs::BindingInstance::Instance
ocs::Job::binding_get_instance(const lListElem *job) {
   return Binding::binding_get_instance(job, JB_new_binding);
}

std::string
ocs::Job::binding_get_filter(const lListElem *job) {
   return Binding::binding_get_filter(job, JB_new_binding);
}

u_long32
ocs::Job::binding_get_amount(const lListElem *job) {
   return Binding::binding_get_amount(job, JB_new_binding);
}

void ocs::Job::binding_set_missing_defaults(lListElem *job) {
   return Binding::binding_set_missing_defaults(job, JB_new_binding);
}

