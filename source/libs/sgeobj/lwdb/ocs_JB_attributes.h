#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

/*
 * This code was generated from file source/libs/sgeobj/json/JB.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   JB_job_number = 50,
   JB_job_name,
   JB_version,
   JB_jid_request_list,
   JB_jid_predecessor_list,
   JB_jid_successor_list,
   JB_ja_ad_request_list,
   JB_ja_ad_predecessor_list,
   JB_ja_ad_successor_list,
   JB_session,
   JB_project,
   JB_department,
   JB_directive_prefix,
   JB_exec_file,
   JB_script_file,
   JB_script_size,
   JB_script_ptr,
   JB_submission_time,
   JB_execution_time,
   JB_deadline,
   JB_owner,
   JB_uid,
   JB_group,
   JB_gid,
   JB_account,
   JB_cwd,
   JB_notify,
   JB_type,
   JB_reserve,
   JB_priority,
   JB_jobshare,
   JB_shell_list,
   JB_verify,
   JB_env_list,
   JB_context,
   JB_job_args,
   JB_checkpoint_attr,
   JB_checkpoint_name,
   JB_checkpoint_object,
   JB_checkpoint_interval,
   JB_restart,
   JB_stdout_path_list,
   JB_stderr_path_list,
   JB_stdin_path_list,
   JB_merge_stderr,
   JB_request_set_list,
   JB_mail_options,
   JB_mail_list,
   JB_pe,
   JB_pe_range,
   JB_tgt,
   JB_cred,
   JB_ja_structure,
   JB_ja_n_h_ids,
   JB_ja_u_h_ids,
   JB_ja_s_h_ids,
   JB_ja_o_h_ids,
   JB_ja_a_h_ids,
   JB_ja_z_ids,
   JB_ja_template,
   JB_ja_tasks,
   JB_host,
   JB_category,
   JB_user_list,
   JB_job_identifier_list,
   JB_verify_suitable_queues,
   JB_soft_wallclock_gmt,
   JB_hard_wallclock_gmt,
   JB_override_tickets,
   JB_qs_args,
   JB_path_aliases,
   JB_urg,
   JB_nurg,
   JB_nppri,
   JB_rrcontr,
   JB_dlcontr,
   JB_wtcontr,
   JB_ar,
   JB_pty,
   JB_ja_task_concurrency,
   JB_binding,
   JB_submission_command_line,
   JB_grp_list,
   JB_joker,
   JB_sync_options,
   JB_category_id
};

constexpr const int JB_Type[] = {
   JB_job_number,
   JB_job_name,
   JB_version,
   JB_jid_request_list,
   JB_jid_predecessor_list,
   JB_jid_successor_list,
   JB_ja_ad_request_list,
   JB_ja_ad_predecessor_list,
   JB_ja_ad_successor_list,
   JB_session,
   JB_project,
   JB_department,
   JB_directive_prefix,
   JB_exec_file,
   JB_script_file,
   JB_script_size,
   JB_script_ptr,
   JB_submission_time,
   JB_execution_time,
   JB_deadline,
   JB_owner,
   JB_uid,
   JB_group,
   JB_gid,
   JB_account,
   JB_cwd,
   JB_notify,
   JB_type,
   JB_reserve,
   JB_priority,
   JB_jobshare,
   JB_shell_list,
   JB_verify,
   JB_env_list,
   JB_context,
   JB_job_args,
   JB_checkpoint_attr,
   JB_checkpoint_name,
   JB_checkpoint_object,
   JB_checkpoint_interval,
   JB_restart,
   JB_stdout_path_list,
   JB_stderr_path_list,
   JB_stdin_path_list,
   JB_merge_stderr,
   JB_request_set_list,
   JB_mail_options,
   JB_mail_list,
   JB_pe,
   JB_pe_range,
   JB_tgt,
   JB_cred,
   JB_ja_structure,
   JB_ja_n_h_ids,
   JB_ja_u_h_ids,
   JB_ja_s_h_ids,
   JB_ja_o_h_ids,
   JB_ja_a_h_ids,
   JB_ja_z_ids,
   JB_ja_template,
   JB_ja_tasks,
   JB_host,
   JB_category,
   JB_user_list,
   JB_job_identifier_list,
   JB_verify_suitable_queues,
   JB_soft_wallclock_gmt,
   JB_hard_wallclock_gmt,
   JB_override_tickets,
   JB_qs_args,
   JB_path_aliases,
   JB_urg,
   JB_nurg,
   JB_nppri,
   JB_rrcontr,
   JB_dlcontr,
   JB_wtcontr,
   JB_ar,
   JB_pty,
   JB_ja_task_concurrency,
   JB_binding,
   JB_submission_command_line,
   JB_grp_list,
   JB_joker,
   JB_sync_options,
   JB_category_id,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define JB_ATTRIBUTES \
   {JB_job_number, "JB_job_number", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {JB_job_name, "JB_job_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_version, "JB_version", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_jid_request_list, "JB_jid_request_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_jid_predecessor_list, "JB_jid_predecessor_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_jid_successor_list, "JB_jid_successor_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_ja_ad_request_list, "JB_ja_ad_request_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_ad_predecessor_list, "JB_ja_ad_predecessor_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_ad_successor_list, "JB_ja_ad_successor_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_session, "JB_session", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_project, "JB_project", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_department, "JB_department", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_directive_prefix, "JB_directive_prefix", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_exec_file, "JB_exec_file", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_script_file, "JB_script_file", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_script_size, "JB_script_size", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_script_ptr, "JB_script_ptr", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_submission_time, "JB_submission_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_execution_time, "JB_execution_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_deadline, "JB_deadline", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_owner, "JB_owner", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_uid, "JB_uid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_group, "JB_group", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_gid, "JB_gid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_account, "JB_account", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_cwd, "JB_cwd", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_notify, "JB_notify", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_type, "JB_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_reserve, "JB_reserve", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_priority, "JB_priority", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_jobshare, "JB_jobshare", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_shell_list, "JB_shell_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_verify, "JB_verify", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_env_list, "JB_env_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_context, "JB_context", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_job_args, "JB_job_args", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_checkpoint_attr, "JB_checkpoint_attr", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_checkpoint_name, "JB_checkpoint_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_checkpoint_object, "JB_checkpoint_object", AttributeStatic::OBJECT, nullptr, 0, AttributeStatic::NO_HASH, false, false}, \
   {JB_checkpoint_interval, "JB_checkpoint_interval", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_restart, "JB_restart", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_stdout_path_list, "JB_stdout_path_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_stderr_path_list, "JB_stderr_path_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_stdin_path_list, "JB_stdin_path_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_merge_stderr, "JB_merge_stderr", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_request_set_list, "JB_request_set_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_mail_options, "JB_mail_options", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_mail_list, "JB_mail_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_pe, "JB_pe", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_pe_range, "JB_pe_range", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_tgt, "JB_tgt", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_cred, "JB_cred", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_ja_structure, "JB_ja_structure", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_n_h_ids, "JB_ja_n_h_ids", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_u_h_ids, "JB_ja_u_h_ids", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_s_h_ids, "JB_ja_s_h_ids", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_o_h_ids, "JB_ja_o_h_ids", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_a_h_ids, "JB_ja_a_h_ids", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_z_ids, "JB_ja_z_ids", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_template, "JB_ja_template", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_tasks, "JB_ja_tasks", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_host, "JB_host", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_category, "JB_category", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_user_list, "JB_user_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_job_identifier_list, "JB_job_identifier_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_verify_suitable_queues, "JB_verify_suitable_queues", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_soft_wallclock_gmt, "JB_soft_wallclock_gmt", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_hard_wallclock_gmt, "JB_hard_wallclock_gmt", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_override_tickets, "JB_override_tickets", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_qs_args, "JB_qs_args", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_path_aliases, "JB_path_aliases", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_urg, "JB_urg", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_nurg, "JB_nurg", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_nppri, "JB_nppri", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_rrcontr, "JB_rrcontr", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_dlcontr, "JB_dlcontr", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_wtcontr, "JB_wtcontr", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JB_ar, "JB_ar", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_pty, "JB_pty", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_ja_task_concurrency, "JB_ja_task_concurrency", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_binding, "JB_binding", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_submission_command_line, "JB_submission_command_line", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_grp_list, "JB_grp_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_joker, "JB_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_sync_options, "JB_sync_options", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JB_category_id, "JB_category_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, true} \

} // end namespace

