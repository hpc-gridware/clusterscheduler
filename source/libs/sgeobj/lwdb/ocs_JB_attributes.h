#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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
   JB_joker
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
   AttributeStatic::END_OF_ATTRIBUTES
};

#define JB_ATTRIBUTES \
   {JB_job_number, "JB_job_number", AttributeStatic::UINT32, AttributeStatic::UNORDERED_UNIQUE}, \
   {JB_job_name, "JB_job_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_version, "JB_version", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_jid_request_list, "JB_jid_request_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_jid_predecessor_list, "JB_jid_predecessor_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_jid_successor_list, "JB_jid_successor_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_ad_request_list, "JB_ja_ad_request_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_ad_predecessor_list, "JB_ja_ad_predecessor_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_ad_successor_list, "JB_ja_ad_successor_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_session, "JB_session", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_project, "JB_project", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_department, "JB_department", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_directive_prefix, "JB_directive_prefix", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_exec_file, "JB_exec_file", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_script_file, "JB_script_file", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_script_size, "JB_script_size", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_script_ptr, "JB_script_ptr", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_submission_time, "JB_submission_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {JB_execution_time, "JB_execution_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {JB_deadline, "JB_deadline", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {JB_owner, "JB_owner", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_uid, "JB_uid", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_group, "JB_group", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_gid, "JB_gid", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_account, "JB_account", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_cwd, "JB_cwd", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_notify, "JB_notify", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JB_type, "JB_type", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_reserve, "JB_reserve", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JB_priority, "JB_priority", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_jobshare, "JB_jobshare", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_shell_list, "JB_shell_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_verify, "JB_verify", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_env_list, "JB_env_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_context, "JB_context", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_job_args, "JB_job_args", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_checkpoint_attr, "JB_checkpoint_attr", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_checkpoint_name, "JB_checkpoint_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_checkpoint_object, "JB_checkpoint_object", AttributeStatic::OBJECT, AttributeStatic::NO_HASH}, \
   {JB_checkpoint_interval, "JB_checkpoint_interval", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_restart, "JB_restart", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_stdout_path_list, "JB_stdout_path_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_stderr_path_list, "JB_stderr_path_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_stdin_path_list, "JB_stdin_path_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_merge_stderr, "JB_merge_stderr", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {JB_request_set_list, "JB_request_set_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_mail_options, "JB_mail_options", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_mail_list, "JB_mail_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_pe, "JB_pe", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_pe_range, "JB_pe_range", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_tgt, "JB_tgt", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_cred, "JB_cred", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_ja_structure, "JB_ja_structure", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_n_h_ids, "JB_ja_n_h_ids", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_u_h_ids, "JB_ja_u_h_ids", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_s_h_ids, "JB_ja_s_h_ids", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_o_h_ids, "JB_ja_o_h_ids", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_a_h_ids, "JB_ja_a_h_ids", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_z_ids, "JB_ja_z_ids", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_template, "JB_ja_template", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_ja_tasks, "JB_ja_tasks", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_host, "JB_host", AttributeStatic::HOST, AttributeStatic::NO_HASH}, \
   {JB_category, "JB_category", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {JB_user_list, "JB_user_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_job_identifier_list, "JB_job_identifier_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_verify_suitable_queues, "JB_verify_suitable_queues", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_soft_wallclock_gmt, "JB_soft_wallclock_gmt", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {JB_hard_wallclock_gmt, "JB_hard_wallclock_gmt", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {JB_override_tickets, "JB_override_tickets", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_qs_args, "JB_qs_args", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_path_aliases, "JB_path_aliases", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_urg, "JB_urg", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JB_nurg, "JB_nurg", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JB_nppri, "JB_nppri", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JB_rrcontr, "JB_rrcontr", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JB_dlcontr, "JB_dlcontr", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JB_wtcontr, "JB_wtcontr", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {JB_ar, "JB_ar", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_pty, "JB_pty", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_ja_task_concurrency, "JB_ja_task_concurrency", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JB_binding, "JB_binding", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_submission_command_line, "JB_submission_command_line", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JB_grp_list, "JB_grp_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JB_joker, "JB_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

