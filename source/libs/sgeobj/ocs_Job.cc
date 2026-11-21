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
 *  Portions of this software are Copyright (c) 2025-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The job object and the operations on it
 */

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
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_SGEJ.h"

#include "msg_common.h"
#include "ocs_BindingInstance.h"
#include "ocs_BindingObj.h"

/**
 * @brief Sort jobs in the job list based on prio, submit time and job number
 *
 * This function sorts the jobs in the provided job list based on the
 * task priority, submit time and job number. The sorting is done in descending order
 * of task priority, and in ascending order of submit time and job number for jobs with the same priority.
 *
 * @param job_list the list of jobs to be sorted (JB), may be an empty handle
 */
void ocs::Job::sgeee_sort_jobs(const ocs::tom::List<ocs::tom::JB> job_list) {
   DENTER(TOP_LAYER);

   if (!job_list) {
      DRETURN_VOID;
   }

   // Create a temporary list and store the job reference and all data required to sort to list
   auto tmp_list = ocs::tom::ListPtr<ocs::tom::SGEJ>::create("tmp list");
   auto nxt_job = job_list.front();
   while (nxt_job) {
      const auto job = nxt_job;
      nxt_job = nxt_job.next();

      // First try to find an enrolled task. Those will have the highest priority
      // If there is no enrolled task then take the template element
      ocs::tom::ConstElement<ocs::tom::JAT> tmp_task = job.get<JB_ja_tasks>().front();
      if (!tmp_task) {
         tmp_task = job.get<JB_ja_template>().front();
      }

      // Store sort-criteria and job reference in the temporary list
      auto tmp_sge_job = ocs::tom::ElementPtr<ocs::tom::SGEJ>::create();
      tmp_sge_job->set<SGEJ_priority>(tmp_task.get<JAT_prio>());
      tmp_sge_job->set<SGEJ_submission_time>(job.get<JB_submission_time>());
      tmp_sge_job->set<SGEJ_job_number>(job.get<JB_job_number>());
      tmp_sge_job->set<SGEJ_job_reference>(job.raw());
      tmp_list->push_back(std::move(tmp_sge_job));

      // Remove the job from the original list; the temporary list holds the only reference now
      static_cast<void>(job_list.extract(job).release());
   }

   // Sort the temporary list according to the following criteria:
   tmp_list->sort<ocs::tom::desc<SGEJ_priority>, ocs::tom::asc<SGEJ_submission_time>,
                  ocs::tom::asc<SGEJ_job_number>>();

   // The job list is empty at this point, so we can just append the sorted jobs
   for (const auto tmp_sge_job : *tmp_list) {
      job_list.push_back(ocs::tom::ElementPtr<ocs::tom::JB>::from_typed(
         static_cast<lListElem *>(tmp_sge_job.get<SGEJ_job_reference>())));
   }

   // we need to free the tmp list, but not the job references
   tmp_list.reset();

   DRETURN_VOID;
}

/**
 * @brief The systemd slice and scope names a job's processes run under
 *
 * @param job the job (JB)
 * @param ja_task the array task (JAT), may be an empty handle
 * @param pe_task the parallel task (PET), an empty handle for the master task
 * @param[out] slice receives the slice name
 * @param[out] scope receives the scope name
 * @param[out] error_dstr receives the reason on failure
 * @return true when both names could be built
 */
bool
ocs::Job::job_get_systemd_slice_and_scope(const ocs::tom::ConstElement<ocs::tom::JB> job,
                                          const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                                          const ocs::tom::ConstElement<ocs::tom::PET> pe_task,
                                          std::string &slice, std::string &scope, dstring *error_dstr) {
   DENTER(TOP_LAYER);

   bool ret = true;

#if defined(OCS_WITH_SYSTEMD)

   // If the systemd integration could not be initialized
   // (e.g., symbols not being loaded, or there is no toplevel systemd slice)
   // then we cannot build the job slice name and scope.
   if (!ocs::uti::Systemd::is_systemd_available()) {
      DPRINTF("Systemd is not available");
      ret = false;
   }

   if (ret) {
      const bool is_array = job_is_array(job);
      bool is_tightly_integrated = false;
      if (ja_task) {
         if (const auto pe = ja_task.get<JAT_pe_object>(); pe) {
            is_tightly_integrated = pe.get<PE_control_slaves>();
         }
      }

      std::string toplevel_slice = ocs::uti::Systemd::get_slice_name();
      slice = toplevel_slice + "-jobs";
      scope = toplevel_slice + ".";
      if (is_array) {
         std::string jobtask_id = std::to_string(job.get<JB_job_number>()) + "." +
                                  std::to_string(ja_task.get<JAT_task_number>());
         // array job
         if (is_tightly_integrated) {
            // array PE job, we have master and slave tasks
            // ocs8012-jobs-1234.1.slice, ocs8012.1234.1.master.scope or ocs8012.1234.1.<num>.<hostname>.scope
            slice += "-" + jobtask_id;
            if (!pe_task) {
               scope += jobtask_id + ".master";
            } else {
               scope += jobtask_id + '.' + pe_task.get<PET_id>();
            }
         } else {
            // just an array job
            // ocs8012-jobs.slice, ocs8012.1234.1.scope
            scope += jobtask_id;
         }
      } else {
         std::string job_id = std::to_string(job.get<JB_job_number>());
         if (is_tightly_integrated) {
            // sequential PE job, we have master and slave tasks
            // ocs8012-jobs-1234.slice, ocs8012.master.scope or ocs8012.<num>.<hostname>.scope
            slice += "-" + job_id;
            if (!pe_task) {
               scope += job_id + ".master";
            } else {
               scope += job_id + '-' + pe_task.get<PET_id>();
            }
         } else {
            // just a sequential job
            // ocs8012-jobs.slice, ocs8012.1234.scope
            scope += job_id;
         }
      }

      slice += ".slice";
      scope += ".scope";
   }
#else
   ret = false;
#endif

   if (!ret) {
      slice.clear();
      scope.clear();
   }

   DRETURN(ret);
}

/**
 * @brief The binding sub-object, created on first access
 *
 * @param job the job to read or extend (JB)
 * @param[out] answer_list receives the reason when it could not be created
 * @return the binding sub-object, or nullptr on error
 */
ocs::tom::Element<ocs::tom::BN>
ocs::Job::binding_get_or_create_elem(const ocs::tom::Element<ocs::tom::JB> job,
                                     ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   // JB_binding holds a BN object
   return Binding::binding_get_or_create_elem<JB_binding>(job, answer_list);
}


/**
 * @brief Who applies the binding
 *
 * @param job the job carrying the binding request (JB)
 * @return the requested @ref ocs::BindingType::Type
 */
ocs::BindingType::Type
ocs::Job::binding_get_type(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return Binding::binding_get_type<JB_binding>(job);
}


/**
 * @brief The hardware unit the binding counts in
 *
 * @param job the job carrying the binding request (JB)
 * @return the requested @ref ocs::BindingUnit::Unit
 */
ocs::BindingUnit::Unit
ocs::Job::binding_get_unit(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return Binding::binding_get_unit<JB_binding>(job);
}


/**
 * @brief How the selected hardware is ordered
 *
 * @param job the job carrying the binding request (JB)
 * @return the sort specification, empty when none was given
 */
std::string
ocs::Job::binding_get_sort(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return Binding::binding_get_sort<JB_binding>(job);
}


/**
 * @brief Where on the topology the binding starts
 *
 * @param job the job carrying the binding request (JB)
 * @return the requested @ref ocs::BindingStart::Start
 */
ocs::BindingStart::Start
ocs::Job::binding_get_start(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return Binding::binding_get_start<JB_binding>(job);
}


/**
 * @brief Where the binding stops
 *
 * @param job the job carrying the binding request (JB)
 * @return the requested @ref ocs::BindingStop::Stop
 */
ocs::BindingStop::Stop
ocs::Job::binding_get_stop(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return Binding::binding_get_end<JB_binding>(job);
}


/**
 * @brief How the binding walks the topology
 *
 * @param job the job carrying the binding request (JB)
 * @return the requested @ref ocs::BindingStrategy::Strategy
 */
ocs::BindingStrategy::Strategy
ocs::Job::binding_get_strategy(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return Binding::binding_get_strategy<JB_binding>(job);
}


/**
 * @brief Which instance of the job the binding applies to
 *
 * @param job the job carrying the binding request (JB)
 * @return the requested @ref ocs::BindingInstance::Instance
 */
ocs::BindingInstance::Instance
ocs::Job::binding_get_instance(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return Binding::binding_get_instance<JB_binding>(job);
}


/**
 * @brief Which hardware the binding is restricted to
 *
 * @param job the job carrying the binding request (JB)
 * @return the filter expression, empty when none was given
 */
std::string
ocs::Job::binding_get_filter(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return Binding::binding_get_filter<JB_binding>(job);
}


/**
 * @brief How many units the binding asks for
 *
 * @param job the job carrying the binding request (JB)
 * @return the requested amount
 */
uint32_t
ocs::Job::binding_get_amount(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   return Binding::binding_get_amount<JB_binding>(job);
}


/**
 * @brief Fill in the binding fields the request left out
 *
 * @param job the job whose binding request is completed (JB)
 * @param[out] answer_list receives the reason on failure
 */
void
ocs::Job::binding_set_missing_defaults(const ocs::tom::Element<ocs::tom::JB> job,
                                       ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   Binding::binding_set_missing_defaults<JB_binding>(job, answer_list);
}

