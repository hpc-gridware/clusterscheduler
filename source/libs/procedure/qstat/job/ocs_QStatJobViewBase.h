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

/** @file
 * @brief Base view of `qstat -j`, and the interface the three output formats implement
 */

#include <ostream>

#include "cull/cull_list.h"

#include "ocs_ProcedureView.h"
#include "qstat/ocs_QStatModelBase.h"

namespace ocs {
   /** @brief Base view for `qstat -j`, and the interface the three formats implement
    *
    * `qstat -j` prints one row per job attribute, and there is one hook per
    * row. The base decides which rows a job produces and in which order;
    * #QStatJobViewPlain, #QStatJobViewXML and #QStatJobViewJSON decide what a
    * row looks like. Adding an output format means implementing the hooks and
    * touching none of the row selection, which is why all three formats report
    * the same attributes.
    *
    * @ingroup libprocedure
    */
   class QStatJobViewBase : public ProcedureView {
   public:
      /** @brief The usage of a job, summed over its tasks
       *
       * A job's usage is not stored as a total - it is reported per task, and
       * per PE task within a task - so it has to be added up before it can be
       * printed. #accumulate_usage() does that.
       */
      struct Usage {
         double wallclock{0.0};   ///< Wallclock time
         double cpu{0.0};         ///< CPU time
         double mem{0.0};         ///< Integral memory usage
         double io{0.0};          ///< Data transferred
         double ioops{0.0};       ///< I/O operations
         double iow{0.0};         ///< Time spent waiting for I/O
         double vmem{0.0};        ///< Virtual memory in use
         double maxvmem{0.0};     ///< Highest virtual memory seen
         double rss{0.0};         ///< Resident set size
         double maxrss{0.0};      ///< Highest resident set size seen

         // In case we have execd_params ENABLE_MEM_DETAILS set, output these values as well.
         bool have_mem_details{false};   ///< Whether the fields below were collected at all
         double pss{0.0};                ///< Proportional set size
         double maxpss{0.0};             ///< Highest proportional set size seen
         double pmem{0.0};               ///< Private memory
         double smem{0.0};               ///< Shared memory
      };

   protected:
      static void accumulate_usage(const lListElem *task, Usage &usage);

      static uint32_t count_pending_tasks(const lListElem *job);
   public:
      /** @brief Build a view for one `qstat -j` call
       * @param parameter the call's parameters
       */
      explicit QStatJobViewBase(const ProcedureParameter &parameter) : ProcedureView(parameter) {};

      ~QStatJobViewBase() override = default;

      virtual void show_jobs_and_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model);
      /** @brief Report why the pending jobs are not running
       * @param os stream to write to
       * @param parameter the call's parameters
       * @param model the fetched lists
       */
      virtual void show_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) = 0;
      virtual void show_job(std::ostream &os, const lList *ilp, const lListElem *job, int flags);

      /** @brief Begin the report

       * @param os stream to write to

       * @param parameter the call's parameters

       */

      virtual void report_started(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief End the report
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_finished(std::ostream &os, QStatParameter &parameter) = 0;

      /** @brief Begin the list of jobs

       * @param os stream to write to

       * @param parameter the call's parameters

       */

      virtual void report_jobs_started(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief End the list of jobs
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_jobs_finished(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief Separate one job from the next
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_job_separator(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief Begin one job
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_job_started(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief End the current job
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_job_finished(std::ostream &os, QStatParameter &parameter) = 0;

      /** @brief Report the job number

       * @param os stream to write to

       * @param job the job (`JB_Type`)

       * @param flags which parts of the listing were requested

       */

      virtual void report_job_id(std::ostream &os, const lListElem *job, int flags) = 0;
      /** @brief Report the scheduling category the job was sorted into
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_category_id(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the job script as it was spooled
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_exec_file(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report when the job was submitted
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_submission_time(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the deadline of a deadline job
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_deadline_time(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the command line the job was submitted with
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_submit_cmd_line(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the command line after defaults and `sge_request` files were applied
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_effective_submit_cmd_line(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the submitting user and group
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_ownership(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the parts of the environment the system itself sets
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_env_core(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the earliest time the job may start
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_execution_time(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the accounting string
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_account(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the checkpointing environment
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_checkpoint(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the working directory
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_cwd(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the path aliases in effect
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_path_aliases(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the prefix that marks directives in the job script
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_directive_prefix(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report where standard input comes from
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_stdin_path_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report where standard output goes
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_stdout_path_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report where standard error goes
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_stderr_path_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report whether the job reserves resources while it waits
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_reserve(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report whether standard error is merged into standard output
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_merge_stderr(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the hard and soft resource requests
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_request_set_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report which job events trigger a mail
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_mail_options(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report who is mailed about the job
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_mail_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report whether the job is warned before it is signalled
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_notify(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the job name
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_name(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the priority the job was submitted with
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_priority(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the share the job was submitted with
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_job_share(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report whether the job may be restarted after a failure
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_restart(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the shell the job runs under, per host
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_shell_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the environment variables passed to the job
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_env_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the verification mode the job was submitted with
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_verify(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the arguments passed to the job script
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_job_args(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the tasks of an array job that were selected
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_job_identifier_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the size of the spooled job script
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_script_size(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the name of the job script
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_script_file(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the job script itself, for a script read from standard input
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_script_ptr(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the parallel environment and the slot range requested
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_pe(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the jobs this one was asked to wait for, as they were named
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_jid_request_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the jobs this one is still waiting for
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_jid_predecessor_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the jobs waiting for this one
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_jid_successor_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the array dependencies as they were named
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_ja_ad_request_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the array jobs whose tasks this one is waiting for
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_ja_ad_predecessor_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the array jobs whose tasks wait for this one
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_ja_ad_successor_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report which queues the job could run in
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_verify_suitable_queues(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the soft wallclock limit
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_soft_wallclock_gmt(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the hard wallclock limit
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_hard_wallclock_gmt(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the job version, which counts how often the job was modified
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_version(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the tickets a manager granted the job
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_override_tickets(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the advance reservation the job runs in
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_ar(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the project the job is accounted to
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_project(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the department the submitting user belongs to
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_department(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report whether the client waits for the job
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_sync_options(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the task range of an array job
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_ja_structure(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report how many tasks of an array job have not started yet
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_pending_tasks(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report how many tasks of an array job may run at once
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_ja_task_concurrency(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the job context
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_ctx_list(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report the requested core binding
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_binding(std::ostream &os, const lListElem *job) = 0;
      /** @brief Report why the job has not been scheduled yet
       * @param os stream to write to
       * @param ilp the scheduler job info list, holding the reasons a job is not running
       * @param job the job (`JB_Type`)
       */
      virtual void report_schedd_job_info(std::ostream &os, const lList *ilp, const lListElem *job) = 0;

      /** @brief Begin the tasks of the current job

       * @param os stream to write to

       * @param job the job (`JB_Type`)

       */

      virtual void report_task_list_started(std::ostream &os, const lListElem *job) = 0;
      /** @brief End the task list
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       */
      virtual void report_task_list_finished(std::ostream &os, const lListElem *job) = 0;
      /** @brief Begin one task
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_started(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief End the current task
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_finished(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief Report the task number
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_id(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief Report the state of the task
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_state(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief Report the resources the task has consumed
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_usage(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief Report the cores the task was bound to
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_exec_binding_list(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief Report the queue instances the task runs in
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_exec_queue_list(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief Report the hosts the task runs on
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_exec_host_list(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief Report when the task started
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_start_time(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /* CS-1908 retention: end_time row for JAT_status == JFINISHED rows. */
      /** @brief Report when the task finished
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_end_time(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief Report the resource map ids the task was granted
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_resource_map(std::ostream &os, const lListElem *job, const lListElem *task) = 0;
      /** @brief Report why the task failed
       * @param os stream to write to
       * @param job the job (`JB_Type`)
       * @param task the task of the job the row is about
       */
      virtual void report_task_error_reason(std::ostream &os, const lListElem *job, const lListElem *task) = 0;

   private:
      /** @brief Sum one usage attribute over a task and its PE tasks
       * @param ja_task the array task
       * @param attr the usage attribute
       * @return the total
       */
      double sum_up_jatask_usage(const lListElem *ja_task, const char *attr) const;

      /** @brief Read one usage attribute of a PE task
       * @param pe_task the PE task
       * @param attr the usage attribute
       * @return its value, 0 when the task does not report it
       */
      double sum_up_petask_usage(const lListElem *pe_task, const char *attr) const;
   };
}
