/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief Base view of `qstat -j`, and the interface the three output formats implement
 */

#include <algorithm>
#include <sstream>
#include <vector>

#include "uti/sge_bitfield.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_usage.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_SME.h"
#include "sgeobj/tom/ocs_tom_UA.h"

#include "qstat/job/ocs_QStatJobViewBase.h"

/** @brief Report the selected jobs, and why the pending ones are waiting
 * @param os stream to write to
 * @param parameter the call's parameters
 * @param model the fetched lists
 * @return none
 */
void ocs::QStatJobViewBase::show_jobs_and_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) {
   DENTER(TOP_LAYER);
   bool first_job = true;
#if 0
   gdi::Packet *packet = parameter.get_packet();
#endif

   /* print scheduler job information and global scheduler info */
   const ocs::tom::ConstList<ocs::tom::SME> ilp = *model.ilp;
   for (const ocs::tom::ConstElement<ocs::tom::JB> job : *model.jlp) {
#if 0
      // Do not show jobs that should not be visible
      if (!job_is_visible(packet, ocs::tom::get<JB_owner>(job), model.is_manager())) {
         continue;
      }
#endif

      if (first_job) {
         report_jobs_started(os, parameter);
         first_job = false;
      }

      report_job_separator(os, parameter);

      report_job_started(os, parameter);
      show_job(os, ilp, job, 0);
      report_job_finished(os, parameter);
   }
   if (!first_job) {
      report_jobs_finished(os, parameter);
      first_job = false;
   }

   DRETURN_VOID;
}


/** @brief Report one job, one hook call per row
 * @param os stream to write to
 * @param ilp the scheduler job info list, holding the reasons a job is not running
 * @param job the job (`JB_Type`)
 * @param flags which parts of the listing were requested
 */
void ocs::QStatJobViewBase::show_job(std::ostream &os, const ocs::tom::ConstList<ocs::tom::SME> ilp,
                                     const ocs::tom::ConstElement<ocs::tom::JB> job, int flags) {
   DENTER(TOP_LAYER);

   if (job == nullptr) {
      DRETURN_VOID;
   }

   report_job_id(os, job, flags);
   report_category_id(os, job);
   report_exec_file(os, job);
   report_submission_time(os, job);
   report_deadline_time(os, job);
   report_submit_cmd_line(os, job);
   report_effective_submit_cmd_line(os, job);
   report_ownership(os, job);
   report_env_core(os, job);
   report_execution_time(os, job);
   report_account(os, job);
   report_checkpoint(os, job);
   report_cwd(os, job);
   report_path_aliases(os, job);
   report_directive_prefix(os, job);
   report_stdin_path_list(os, job);
   report_stdout_path_list(os, job);
   report_stderr_path_list(os, job);
   report_reserve(os, job);
   report_merge_stderr(os, job);
   report_request_set_list(os, job);
   report_mail_options(os, job);
   report_mail_list(os, job);
   report_notify(os, job);
   report_name(os, job);
   report_priority(os, job);
   report_job_share(os, job);
   report_restart(os, job);
   report_shell_list(os, job);
   report_env_list(os, job);
   report_verify(os, job);
   report_job_args(os, job);
#if 0
   report_job_identifier_list(os, job); // What is this?
#endif
   report_script_size(os, job);
   report_script_file(os, job);
#if 0 // data is not subscribed and also not available in worker
   report_script_ptr(os, job);
#endif
   report_pe(os, job);
   report_jid_request_list(os, job);
   report_jid_predecessor_list(os, job);
   report_jid_successor_list(os, job);
   report_ja_ad_request_list(os, job);
   report_ja_ad_predecessor_list(os, job);
   report_ja_ad_successor_list(os, job);
   report_verify_suitable_queues(os, job);
#if 0 // this is data only available in execd. why should this be shown here. data also subscribed unnecessarily
   report_soft_wallclock_gmt(os, job);
   report_hard_wallclock_gmt(os, job);
#endif
   report_version(os, job);
   report_override_tickets(os, job);
   report_ar(os, job);
   report_project(os, job);
   report_department(os, job);
   report_sync_options(os, job);
   report_ja_structure(os, job);
   report_pending_tasks(os, job);
   report_ja_task_concurrency(os, job);
   report_ctx_list(os, job);
   report_binding(os, job);

   // start task specific output
   report_task_list_started(os, job);
   if (ocs::tom::has<JB_ja_tasks>(job)) {
      // the tasks in the order of their numbers; the job is the caller's and is not sorted itself
      std::vector<ocs::tom::ConstElement<ocs::tom::JAT>> ja_tasks;
      for (const ocs::tom::ConstElement<ocs::tom::JAT> ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
         ja_tasks.push_back(ja_task);
      }
      std::ranges::sort(ja_tasks, {}, [](const ocs::tom::ConstElement<ocs::tom::JAT> ja_task) {
         return ocs::tom::get<JAT_task_number>(ja_task);
      });

      for (const ocs::tom::ConstElement<ocs::tom::JAT> ja_task : ja_tasks) {
         report_task_started(os, job, ja_task);

         report_task_id(os, job, ja_task);
         report_task_state(os, job, ja_task);
         report_task_usage(os, job, ja_task);
         report_task_exec_binding_list(os, job, ja_task);
         report_task_exec_queue_list(os, job, ja_task);
         report_task_exec_host_list(os, job, ja_task);
         report_task_start_time(os, job, ja_task);
         report_task_end_time(os, job, ja_task);
         report_task_resource_map(os, job, ja_task);
         report_task_error_reason(os, job, ja_task);

         report_task_finished(os, job, ja_task);
      }
   }
   report_task_list_finished(os, job);

   // switch back to job specific output
   report_schedd_job_info(os, ilp, job);

   DRETURN_VOID;
}

/** @brief Count the tasks of an array job that have not started yet
 *
 * Sums the ids in the five JB_ja_*_h_ids hold lists and adds the number of
 * enrolled ja_tasks in JB_ja_tasks whose JAT_status is JIDLE. Enrolled tasks
 * that already have the JERROR bit set in JAT_state are excluded. A hold list
 * the (reduced) job does not hold counts nothing.
 *
 * @param job the job (`JB_Type`)
 * @return the number of pending tasks
 */
uint32_t ocs::QStatJobViewBase::count_pending_tasks(const ocs::tom::ConstElement<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);
   uint32_t pending = 0;
   // the ids of one hold list, nothing if the (reduced) job does not hold it
   const auto hold_ids = [job]<auto F>() -> uint32_t {
      return ocs::tom::has<F>(job) ? range_list_get_number_of_ids(ocs::tom::get<F>(job)) : 0;
   };
   pending += hold_ids.template operator()<JB_ja_n_h_ids>();
   pending += hold_ids.template operator()<JB_ja_u_h_ids>();
   pending += hold_ids.template operator()<JB_ja_s_h_ids>();
   pending += hold_ids.template operator()<JB_ja_o_h_ids>();
   pending += hold_ids.template operator()<JB_ja_a_h_ids>();

   if (ocs::tom::has<JB_ja_tasks>(job)) {
      for (const ocs::tom::ConstElement<ocs::tom::JAT> ja_task : ocs::tom::get<JB_ja_tasks>(job)) {
         if (ocs::tom::get<JAT_status>(ja_task) == JIDLE && !ISSET(ocs::tom::get<JAT_state>(ja_task), JERROR)) {
            pending++;
         }
      }
   }

   DRETURN(pending);
}

/** @brief Add one task's usage to a running total
 * @param task the task whose usage is added
 * @param usage the total, updated in place
 */
void ocs::QStatJobViewBase::accumulate_usage(const ocs::tom::ConstElement<ocs::tom::JAT> task, Usage &usage) {
   DENTER(TOP_LAYER);
   usage.have_mem_details =
      static_cast<bool>(ocs::tom::find_first_in<JAT_scaled_usage_list, UA_name>(task, USAGE_ATTR_PSS));

   /* display online job usage separately for each array job but summarized over all pe_tasks */
   // adds one value of a scaled usage list to a total; an attribute the list does not hold adds nothing
   const auto sum_up = [](const ocs::tom::ConstList<ocs::tom::UA> usage_list, double &dst, const char *attr) {
      if (ocs::tom::ConstElement<ocs::tom::UA> uep = ocs::tom::find_first<UA_name>(usage_list, attr)) {
         dst += ocs::tom::get<UA_value>(uep);
      }
   };

   /* master task */
   const auto ja_task_usage = ocs::tom::get<JAT_scaled_usage_list>(task);
   sum_up(ja_task_usage, usage.wallclock, USAGE_ATTR_WALLCLOCK);
   sum_up(ja_task_usage, usage.cpu, USAGE_ATTR_CPU);
   sum_up(ja_task_usage, usage.mem, USAGE_ATTR_MEM);
   sum_up(ja_task_usage, usage.io, USAGE_ATTR_IO);
   sum_up(ja_task_usage, usage.ioops, USAGE_ATTR_IOOPS);
   sum_up(ja_task_usage, usage.iow, USAGE_ATTR_IOW);
   sum_up(ja_task_usage, usage.vmem, USAGE_ATTR_VMEM);
   sum_up(ja_task_usage, usage.maxvmem, USAGE_ATTR_MAXVMEM);
   sum_up(ja_task_usage, usage.rss, USAGE_ATTR_RSS);
   sum_up(ja_task_usage, usage.maxrss, USAGE_ATTR_MAXRSS);
   if (usage.have_mem_details) {
      sum_up(ja_task_usage, usage.pss, USAGE_ATTR_PSS);
      sum_up(ja_task_usage, usage.maxpss, USAGE_ATTR_MAXPSS);
      sum_up(ja_task_usage, usage.pmem, USAGE_ATTR_PMEM);
      sum_up(ja_task_usage, usage.smem, USAGE_ATTR_SMEM);
   }

   /* slave tasks */
   for (const ocs::tom::ConstElement<ocs::tom::PET> pe_task_ep : ocs::tom::get<JAT_task_list>(task)) {
      // we do not sum up wallclock usage per pe task; a PE task keeps its usage in its own list
      const auto pe_task_usage = ocs::tom::get<PET_scaled_usage>(pe_task_ep);
      sum_up(pe_task_usage, usage.cpu, USAGE_ATTR_CPU);
      sum_up(pe_task_usage, usage.mem, USAGE_ATTR_MEM);
      sum_up(pe_task_usage, usage.io, USAGE_ATTR_IO);
      sum_up(pe_task_usage, usage.ioops, USAGE_ATTR_IOOPS);
      sum_up(pe_task_usage, usage.iow, USAGE_ATTR_IOW);
      sum_up(pe_task_usage, usage.vmem, USAGE_ATTR_VMEM);
      sum_up(pe_task_usage, usage.maxvmem, USAGE_ATTR_MAXVMEM);
      sum_up(pe_task_usage, usage.rss, USAGE_ATTR_RSS);
      sum_up(pe_task_usage, usage.maxrss, USAGE_ATTR_MAXRSS);
      if (usage.have_mem_details) {
         sum_up(pe_task_usage, usage.pss, USAGE_ATTR_PSS);
         sum_up(pe_task_usage, usage.maxpss, USAGE_ATTR_MAXPSS);
         sum_up(pe_task_usage, usage.pmem, USAGE_ATTR_PMEM);
         sum_up(pe_task_usage, usage.smem, USAGE_ATTR_SMEM);
      }
   }
   DRETURN_VOID;
}
