#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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

#include "ocs_QStatJobViewBase.h"
#include "qstat/ocs_QStatModelBase.h"

namespace ocs {
   class QStatJobViewJSON : public QStatJobViewBase {
      int indent{0};
      bool first_attribute{true};
      bool first_task{true};
      bool first_task_attribute{true};

      void report_X_path_list(std::ostream &os, const lListElem *job, int nm, const char *name);
      void report_X_boolean(std::ostream &os, const lListElem *job, int nm, const char *name);
      void report_X_uint32(std::ostream &os, const lListElem *job, int nm, const char *name);
      void report_X_string(std::ostream &os, const lListElem *job, int nm, const char *name);
      void report_X_ISO_8601_timestamp(std::ostream &os, const lListElem *job, int nm, const char *name);
      void report_X_task_ISO_8601_timestamp(std::ostream &os, const lListElem *job, int nm, const char *name);
      void report_X_resource_list(std::ostream &os, const lListElem *job, int nm, const char *name);
      void report_X_queue_list(std::ostream &os, const lListElem *jrs, int nm, const char *name);
      void report_X_string_list(std::ostream &os, const lListElem *job, int list_nm, int value_nm, const char *name);
      void report_X_uint32_list(std::ostream &os, const lListElem *job, int list_nm, int value_nm, const char *name);
      void report_X_env_list(std::ostream &os, const lListElem *job, int nm, const char *name);
      void report_X_task_str_uint32_list(std::ostream &os, const lListElem *task, int nm, const char *name,
                                         int str_nm, const char *str_name, int uint32_nm, const char *uint32_name);
   public:
      explicit QStatJobViewJSON(const ProcedureParameter &parameter) : QStatJobViewBase(parameter) {
      } ;

      ~QStatJobViewJSON() override = default;

      void show_reasons(std::ostream &os, QStatParameter &parameter, QStatModelBase &model) override;

      void report_started(std::ostream &os, QStatParameter &parameter) override;
      void report_finished(std::ostream &os, QStatParameter &parameter) override;

      void report_jobs_started(std::ostream &os, QStatParameter &parameter) override;
      void report_jobs_finished(std::ostream &os, QStatParameter &parameter) override;
      void report_job_separator(std::ostream &os, QStatParameter &parameter) override;
      void report_job_started(std::ostream &os, QStatParameter &parameter) override;
      void report_job_finished(std::ostream &os, QStatParameter &parameter) override;

      void report_job_id(std::ostream &os, const lListElem *job, int flags) override;
      void report_category_id(std::ostream &os, const lListElem *job) override;
      void report_exec_file(std::ostream &os, const lListElem *job) override;
      void report_submission_time(std::ostream &os, const lListElem *job) override;
      void report_deadline_time(std::ostream &os, const lListElem *job) override;
      void report_submit_cmd_line(std::ostream &os, const lListElem *job) override;
      void report_effective_submit_cmd_line(std::ostream &os, const lListElem *job) override;
      void report_ownership(std::ostream &os, const lListElem *job) override;
      void report_env_core(std::ostream &os, const lListElem *job) override;
      void report_execution_time(std::ostream &os, const lListElem *job) override;
      void report_account(std::ostream &os, const lListElem *job) override;
      void report_checkpoint(std::ostream &os, const lListElem *job) override;
      void report_cwd(std::ostream &os, const lListElem *job) override;
      void report_path_aliases(std::ostream &os, const lListElem *job) override;
      void report_directive_prefix(std::ostream &os, const lListElem *job) override;
      void report_stdin_path_list(std::ostream &os, const lListElem *job) override;
      void report_stdout_path_list(std::ostream &os, const lListElem *job) override;
      void report_stderr_path_list(std::ostream &os, const lListElem *job) override;
      void report_reserve(std::ostream &os, const lListElem *job) override;
      void report_merge_stderr(std::ostream &os, const lListElem *job) override;
      void report_request_set_list(std::ostream &os, const lListElem *job) override;
      void report_mail_options(std::ostream &os, const lListElem *job) override;
      void report_mail_list(std::ostream &os, const lListElem *job) override;
      void report_notify(std::ostream &os, const lListElem *job) override;
      void report_name(std::ostream &os, const lListElem *job) override;
      void report_priority(std::ostream &os, const lListElem *job) override;
      void report_job_share(std::ostream &os, const lListElem *job) override;
      void report_restart(std::ostream &os, const lListElem *job) override;
      void report_shell_list(std::ostream &os, const lListElem *job) override;
      void report_env_list(std::ostream &os, const lListElem *job) override;
      void report_verify(std::ostream &os, const lListElem *job) override;
      void report_job_args(std::ostream &os, const lListElem *job) override;
      void report_job_identifier_list(std::ostream &os, const lListElem *job) override;
      void report_script_size(std::ostream &os, const lListElem *job) override;
      void report_script_file(std::ostream &os, const lListElem *job) override;
      void report_script_ptr(std::ostream &os, const lListElem *job) override;
      void report_pe(std::ostream &os, const lListElem *job) override;
      void report_jid_request_list(std::ostream &os, const lListElem *job) override;
      void report_jid_predecessor_list(std::ostream &os, const lListElem *job) override;
      void report_jid_successor_list(std::ostream &os, const lListElem *job) override;
      void report_ja_ad_request_list(std::ostream &os, const lListElem *job) override;
      void report_ja_ad_predecessor_list(std::ostream &os, const lListElem *job) override;
      void report_ja_ad_successor_list(std::ostream &os, const lListElem *job) override;
      void report_verify_suitable_queues(std::ostream &os, const lListElem *job) override;
      void report_soft_wallclock_gmt(std::ostream &os, const lListElem *job) override;
      void report_hard_wallclock_gmt(std::ostream &os, const lListElem *job) override;
      void report_version(std::ostream &os, const lListElem *job) override;
      void report_override_tickets(std::ostream &os, const lListElem *job) override;
      void report_ar(std::ostream &os, const lListElem *job) override;
      void report_project(std::ostream &os, const lListElem *job) override;
      void report_department(std::ostream &os, const lListElem *job) override;
      void report_sync_options(std::ostream &os, const lListElem *job) override;
      void report_ja_structure(std::ostream &os, const lListElem *job) override;
      void report_ja_task_concurrency(std::ostream &os, const lListElem *job) override;
      void report_ctx_list(std::ostream &os, const lListElem *job) override;
      void report_binding(std::ostream &os, const lListElem *job) override;
      void report_schedd_job_info(std::ostream &os, const lList *ilp, const lListElem *job) override;

      void report_task_list_started(std::ostream &os, const lListElem *job) override;
      void report_task_list_finished(std::ostream &os, const lListElem *job) override;
      void report_task_started(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_finished(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_id(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_state(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_usage(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_exec_binding_list(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_exec_queue_list(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_exec_host_list(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_start_time(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_resource_map(std::ostream &os, const lListElem *job, const lListElem *task) override;
      void report_task_error_reason(std::ostream &os, const lListElem *job, const lListElem *task) override;
   };
}
