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
#include "ocs_QStatJobModel.h"

namespace ocs {
   class QStatJobViewBase : public ProcedureView {
   public:
      explicit QStatJobViewBase(const ProcedureParameter &parameter) : ProcedureView(parameter) {};
      ~QStatJobViewBase() override = default;

      virtual void show_jobs_and_reasons(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) = 0;
      virtual void show_reasons(std::ostream &os, QStatParameter &parameter, QStatJobModel &model) = 0;
      virtual void show_job(std::ostream &os, const lListElem *job, int flags);

      virtual void report_started(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_finished(std::ostream &os, QStatParameter &parameter) = 0;

      virtual void report_jobs_started(std::ostream &os, QStatParameter &parameter) = 0;
      virtual void report_jobs_finished(std::ostream &os, QStatParameter &parameter) = 0;

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

   };
}
