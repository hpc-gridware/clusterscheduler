#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/QU.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   QU_qhostname = 300,
   QU_qname,
   QU_full_name,
   QU_tag,
   QU_available_at,
   QU_seq_no,
   QU_nsuspend,
   QU_qtype,
   QU_job_slots,
   QU_tmpdir,
   QU_shell,
   QU_calendar,
   QU_priority,
   QU_processors,
   QU_prolog,
   QU_epilog,
   QU_shell_start_mode,
   QU_starter_method,
   QU_suspend_method,
   QU_resume_method,
   QU_terminate_method,
   QU_initial_state,
   QU_rerun,
   QU_s_rt,
   QU_h_rt,
   QU_s_cpu,
   QU_h_cpu,
   QU_s_fsize,
   QU_h_fsize,
   QU_s_data,
   QU_h_data,
   QU_s_stack,
   QU_h_stack,
   QU_s_core,
   QU_h_core,
   QU_s_rss,
   QU_h_rss,
   QU_s_vmem,
   QU_h_vmem,
   QU_suspend_interval,
   QU_min_cpu_interval,
   QU_notify,
   QU_ckpt_list,
   QU_pe_list,
   QU_owner_list,
   QU_acl,
   QU_xacl,
   QU_projects,
   QU_xprojects,
   QU_consumable_config_list,
   QU_load_thresholds,
   QU_suspend_thresholds,
   QU_subordinate_list,
   QU_state,
   QU_tagged4schedule,
   QU_cache_version,
   QU_pending_signal,
   QU_pending_signal_delivery_time,
   QU_version,
   QU_suspended_on_subordinate,
   QU_last_suspend_threshold_ckeck,
   QU_job_cnt,
   QU_pending_job_cnt,
   QU_soft_violation,
   QU_host_seq_no,
   QU_resource_utilization,
   QU_message_list,
   QU_gdi_do_later,
   QU_state_changes,
   QU_joker
};

constexpr const int QU_Type[] = {
   QU_qhostname,
   QU_qname,
   QU_full_name,
   QU_tag,
   QU_available_at,
   QU_seq_no,
   QU_nsuspend,
   QU_qtype,
   QU_job_slots,
   QU_tmpdir,
   QU_shell,
   QU_calendar,
   QU_priority,
   QU_processors,
   QU_prolog,
   QU_epilog,
   QU_shell_start_mode,
   QU_starter_method,
   QU_suspend_method,
   QU_resume_method,
   QU_terminate_method,
   QU_initial_state,
   QU_rerun,
   QU_s_rt,
   QU_h_rt,
   QU_s_cpu,
   QU_h_cpu,
   QU_s_fsize,
   QU_h_fsize,
   QU_s_data,
   QU_h_data,
   QU_s_stack,
   QU_h_stack,
   QU_s_core,
   QU_h_core,
   QU_s_rss,
   QU_h_rss,
   QU_s_vmem,
   QU_h_vmem,
   QU_suspend_interval,
   QU_min_cpu_interval,
   QU_notify,
   QU_ckpt_list,
   QU_pe_list,
   QU_owner_list,
   QU_acl,
   QU_xacl,
   QU_projects,
   QU_xprojects,
   QU_consumable_config_list,
   QU_load_thresholds,
   QU_suspend_thresholds,
   QU_subordinate_list,
   QU_state,
   QU_tagged4schedule,
   QU_cache_version,
   QU_pending_signal,
   QU_pending_signal_delivery_time,
   QU_version,
   QU_suspended_on_subordinate,
   QU_last_suspend_threshold_ckeck,
   QU_job_cnt,
   QU_pending_job_cnt,
   QU_soft_violation,
   QU_host_seq_no,
   QU_resource_utilization,
   QU_message_list,
   QU_gdi_do_later,
   QU_state_changes,
   QU_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define QU_ATTRIBUTES \
   {QU_qhostname, "QU_qhostname", AttributeStatic::HOST, AttributeStatic::UNORDERED_UNIQUE}, \
   {QU_qname, "QU_qname", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_full_name, "QU_full_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {QU_tag, "QU_tag", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_available_at, "QU_available_at", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {QU_seq_no, "QU_seq_no", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_nsuspend, "QU_nsuspend", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_qtype, "QU_qtype", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_job_slots, "QU_job_slots", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_tmpdir, "QU_tmpdir", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_shell, "QU_shell", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_calendar, "QU_calendar", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_priority, "QU_priority", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_processors, "QU_processors", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_prolog, "QU_prolog", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_epilog, "QU_epilog", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_shell_start_mode, "QU_shell_start_mode", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_starter_method, "QU_starter_method", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_suspend_method, "QU_suspend_method", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_resume_method, "QU_resume_method", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_terminate_method, "QU_terminate_method", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_initial_state, "QU_initial_state", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_rerun, "QU_rerun", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {QU_s_rt, "QU_s_rt", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_h_rt, "QU_h_rt", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_s_cpu, "QU_s_cpu", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_h_cpu, "QU_h_cpu", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_s_fsize, "QU_s_fsize", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_h_fsize, "QU_h_fsize", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_s_data, "QU_s_data", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_h_data, "QU_h_data", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_s_stack, "QU_s_stack", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_h_stack, "QU_h_stack", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_s_core, "QU_s_core", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_h_core, "QU_h_core", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_s_rss, "QU_s_rss", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_h_rss, "QU_h_rss", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_s_vmem, "QU_s_vmem", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_h_vmem, "QU_h_vmem", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_suspend_interval, "QU_suspend_interval", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_min_cpu_interval, "QU_min_cpu_interval", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_notify, "QU_notify", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {QU_ckpt_list, "QU_ckpt_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_pe_list, "QU_pe_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_owner_list, "QU_owner_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_acl, "QU_acl", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_xacl, "QU_xacl", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_projects, "QU_projects", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_xprojects, "QU_xprojects", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_consumable_config_list, "QU_consumable_config_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_load_thresholds, "QU_load_thresholds", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_suspend_thresholds, "QU_suspend_thresholds", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_subordinate_list, "QU_subordinate_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_state, "QU_state", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_tagged4schedule, "QU_tagged4schedule", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_cache_version, "QU_cache_version", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_pending_signal, "QU_pending_signal", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_pending_signal_delivery_time, "QU_pending_signal_delivery_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {QU_version, "QU_version", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_suspended_on_subordinate, "QU_suspended_on_subordinate", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_last_suspend_threshold_ckeck, "QU_last_suspend_threshold_ckeck", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {QU_job_cnt, "QU_job_cnt", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_pending_job_cnt, "QU_pending_job_cnt", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_soft_violation, "QU_soft_violation", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_host_seq_no, "QU_host_seq_no", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_resource_utilization, "QU_resource_utilization", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_message_list, "QU_message_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_gdi_do_later, "QU_gdi_do_later", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {QU_state_changes, "QU_state_changes", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {QU_joker, "QU_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

