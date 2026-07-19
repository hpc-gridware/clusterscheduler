#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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

#include <ostream>

#include "cull/cull_list.h"

#include "ocs_ProcedureView.h"
#include "qstat/ocs_QStatModelBase.h"

namespace ocs {
   class QStatJobViewBase : public ProcedureView {
   public:
      struct Usage {
         double wallclock{0.0};
         double cpu{0.0};
         double mem{0.0};
         double io{0.0};
         double ioops{0.0};
         double iow{0.0};
         double vmem{0.0};
         double maxvmem{0.0};
         double rss{0.0};
         double maxrss{0.0};

         // In case we have execd_params ENABLE_MEM_DETAILS set, output these values as well.
         bool have_mem_details{false};
         double pss{0.0};
         double maxpss{0.0};
         double pmem{0.0};
         double smem{0.0};
      };

   protected:
      static void accumulate_usage(const lListElem *task, Usage &usage);
      static uint32_t count_pending_tasks(const lListElem *job);
   public:
      explicit QStatJobViewBase(const ProcedureParameter &parameter) : ProcedureView(parameter) {};
      ~QStatJobViewBase() override = default;

      virtual void show_jobs_and_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model);
      virtual void show_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) = 0;
      virtual void show_job(std::ostream &os, const lList *ilp, const lListElem *job, int flags);

      virtual void report_started(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_finished(std::ostream &os, QStatParameter &parameter) = 0;

      virtual void report_jobs_started(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_jobs_finished(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_job_separator(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_job_started(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_job_finished(std::ostream &os, QStatParameter &parameter) = 0;

      virtual void report_job_id(std::ostream &os, const lListElem *job, int flags) = 0;
      virtual void report_category_id(std::ostream &os, const lListElem *job) = 0;
      virtual void report_exec_file(std::ostream &os, const lListElem *job) = 0;
      virtual void report_submission_time(std::ostream &os, const lListElem *job) = 0;
      virtual void report_deadline_time(std::ostream &os, const lListElem *job) = 0;
      virtual void report_submit_cmd_line(std::ostream &os, const lListElem *job) = 0;
      virtual void report_effective_submit_cmd_line(std::ostream &os, const lListElem *job) = 0;
      virtual void report_ownership(std::ostream &os, const lListElem *job) = 0;
      virtual void report_env_core(std::ostream &os, const lListElem *job) = 0;
      virtual void report_execution_time(std::ostream &os, const lListElem *job) = 0;
      virtual void report_account(std::ostream &os, const lListElem *job) = 0;
      virtual void report_checkpoint(std::ostream &os, const lListElem *job) = 0;
      virtual void report_cwd(std::ostream &os, const lListElem *job) = 0;
      virtual void report_path_aliases(std::ostream &os, const lListElem *job) = 0;
      virtual void report_directive_prefix(std::ostream &os, const lListElem *job) = 0;
      virtual void report_stdin_path_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_stdout_path_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_stderr_path_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_reserve(std::ostream &os, const lListElem *job) = 0;
      virtual void report_merge_stderr(std::ostream &os, const lListElem *job) = 0;
      virtual void report_request_set_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_mail_options(std::ostream &os, const lListElem *job) = 0;
      virtual void report_mail_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_notify(std::ostream &os, const lListElem *job) = 0;
      virtual void report_name(std::ostream &os, const lListElem *job) = 0;
      virtual void report_priority(std::ostream &os, const lListElem *job) = 0;
      virtual void report_job_share(std::ostream &os, const lListElem *job) = 0;
      virtual void report_restart(std::ostream &os, const lListElem *job) = 0;
      virtual void report_shell_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_env_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_verify(std::ostream &os, const lListElem *job) = 0;
      virtual void report_job_args(std::ostream &os, const lListElem *job) = 0;
      virtual void report_job_identifier_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_script_size(std::ostream &os, const lListElem *job) = 0;
      virtual void report_script_file(std::ostream &os, const lListElem *job) = 0;
      virtual void report_script_ptr(std::ostream &os, const lListElem *job) = 0;
      virtual void report_pe(std::ostream &os, const lListElem *job) = 0;
      virtual void report_jid_request_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_jid_predecessor_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_jid_successor_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_ja_ad_request_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_ja_ad_predecessor_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_ja_ad_successor_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_verify_suitable_queues(std::ostream &os, const lListElem *job) = 0;
      virtual void report_soft_wallclock_gmt(std::ostream &os, const lListElem *job) = 0;
      virtual void report_hard_wallclock_gmt(std::ostream &os, const lListElem *job) = 0;
      virtual void report_version(std::ostream &os, const lListElem *job) = 0;
      virtual void report_override_tickets(std::ostream &os, const lListElem *job) = 0;
      virtual void report_ar(std::ostream &os, const lListElem *job) = 0;
      virtual void report_project(std::ostream &os, const lListElem *job) = 0;
      virtual void report_department(std::ostream &os, const lListElem *job) = 0;
      virtual void report_sync_options(std::ostream &os, const lListElem *job) = 0;
      virtual void report_ja_structure(std::ostream &os, const lListElem *job) = 0;
      virtual void report_pending_tasks(std::ostream &os, const lListElem *job) = 0;
      virtual void report_ja_task_concurrency(std::ostream &os, const lListElem *job) = 0;
      virtual void report_ctx_list(std::ostream &os, const lListElem *job) = 0;
      virtual void report_binding(std::ostream &os, const lListElem *job) = 0;
      virtual void report_schedd_job_info(std::ostream &os, const lList *ilp, const lListElem *job) = 0;

      virtual void report_task_list_started(std::ostream &os, const lListElem *job) = 0;
      virtual void report_task_list_finished(std::ostream &os, const lListElem *job) = 0;
      virtual void report_task_started(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_finished(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_id(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_state(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_usage(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_exec_binding_list(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_exec_queue_list(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_exec_host_list(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_start_time(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /* CS-1908 retention: end_time row for JAT_status == JFINISHED rows. */
      virtual void report_task_end_time(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_resource_map(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      virtual void report_task_error_reason(std::ostream &os, const lListElem *job, const lListElem *task) = 0;

   private:
      double sum_up_jatask_usage(const lListElem *ja_task, const char *attr) const;
      double sum_up_petask_usage(const lListElem *pe_task, const char *attr) const;
   };
}
