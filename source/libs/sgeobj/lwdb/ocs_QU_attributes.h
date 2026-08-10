#pragma once
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

/*
 * This code was generated from file source/libs/sgeobj/json/QU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Queue Instance
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of QU
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   QU_qhostname = 300,   ///< @todo add summary
   QU_qname,   ///< cluster queue name
   QU_full_name,   ///< @todo add summary
   QU_tag,   ///< @todo add summary
   QU_available_at,   ///< @todo add summary
   QU_seq_no,   ///< @todo add summary
   QU_nsuspend,   ///< @todo add summary
   QU_qtype,   ///< @todo add summary
   QU_job_slots,   ///< @todo add summary
   QU_tmpdir,   ///< @todo add summary
   QU_shell,   ///< @todo add summary
   QU_calendar,   ///< @todo add summary
   QU_priority,   ///< @todo add summary
   QU_processors,   ///< @todo add summary
   QU_prolog,   ///< @todo add summary
   QU_epilog,   ///< @todo add summary
   QU_shell_start_mode,   ///< @todo add summary
   QU_starter_method,   ///< @todo add summary
   QU_suspend_method,   ///< @todo add summary
   QU_resume_method,   ///< @todo add summary
   QU_terminate_method,   ///< @todo add summary
   QU_initial_state,   ///< @todo add summary
   QU_rerun,   ///< @todo add summary
   QU_s_rt,   ///< @todo add summary
   QU_h_rt,   ///< @todo add summary
   QU_s_cpu,   ///< @todo add summary
   QU_h_cpu,   ///< @todo add summary
   QU_s_fsize,   ///< @todo add summary
   QU_h_fsize,   ///< @todo add summary
   QU_s_data,   ///< @todo add summary
   QU_h_data,   ///< @todo add summary
   QU_s_stack,   ///< @todo add summary
   QU_h_stack,   ///< @todo add summary
   QU_s_core,   ///< @todo add summary
   QU_h_core,   ///< @todo add summary
   QU_s_rss,   ///< @todo add summary
   QU_h_rss,   ///< @todo add summary
   QU_s_vmem,   ///< @todo add summary
   QU_h_vmem,   ///< @todo add summary
   QU_suspend_interval,   ///< @todo add summary
   QU_min_cpu_interval,   ///< @todo add summary
   QU_notify,   ///< @todo add summary
   QU_ckpt_list,   ///< @todo add summary
   QU_pe_list,   ///< @todo add summary
   QU_owner_list,   ///< @todo add summary
   QU_acl,   ///< @todo add summary
   QU_xacl,   ///< @todo add summary
   QU_projects,   ///< @todo add summary
   QU_xprojects,   ///< @todo add summary
   QU_consumable_config_list,   ///< @todo add summary
   QU_load_thresholds,   ///< @todo add summary
   QU_suspend_thresholds,   ///< @todo add summary
   QU_subordinate_list,   ///< @todo add summary
   QU_state,   ///< @todo add summary
   QU_tagged4schedule,   ///< Queue Instance Tagged for Schedule
   QU_cache_version,   ///< @todo add summary
   QU_pending_signal,   ///< @todo add summary
   QU_pending_signal_delivery_time,   ///< @todo add summary
   QU_version,   ///< @todo add summary
   QU_suspended_on_subordinate,   ///< @todo add summary
   QU_last_suspend_threshold_ckeck,   ///< @todo add summary
   QU_job_cnt,   ///< @todo add summary
   QU_pending_job_cnt,   ///< @todo add summary
   QU_soft_violation,   ///< @todo add summary
   QU_host_seq_no,   ///< @todo add summary
   QU_resource_utilization,   ///< @todo add summary
   QU_message_list,   ///< @todo add summary
   QU_gdi_do_later,   ///< @todo add summary
   QU_state_changes,   ///< New State Variables
   QU_joker   ///< Joker
};

/** @brief The attribute ids of QU, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
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

/** @brief The compile-time description of every attribute of QU
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define QU_ATTRIBUTES \
   {QU_qhostname, "QU_qhostname", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, true}, \
   {QU_qname, "QU_qname", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, true}, \
   {QU_full_name, "QU_full_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {QU_tag, "QU_tag", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_available_at, "QU_available_at", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_seq_no, "QU_seq_no", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_nsuspend, "QU_nsuspend", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_qtype, "QU_qtype", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_job_slots, "QU_job_slots", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_tmpdir, "QU_tmpdir", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_shell, "QU_shell", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_calendar, "QU_calendar", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_priority, "QU_priority", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_processors, "QU_processors", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_prolog, "QU_prolog", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_epilog, "QU_epilog", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_shell_start_mode, "QU_shell_start_mode", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_starter_method, "QU_starter_method", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_suspend_method, "QU_suspend_method", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_resume_method, "QU_resume_method", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_terminate_method, "QU_terminate_method", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_initial_state, "QU_initial_state", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_rerun, "QU_rerun", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_s_rt, "QU_s_rt", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_h_rt, "QU_h_rt", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_s_cpu, "QU_s_cpu", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_h_cpu, "QU_h_cpu", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_s_fsize, "QU_s_fsize", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_h_fsize, "QU_h_fsize", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_s_data, "QU_s_data", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_h_data, "QU_h_data", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_s_stack, "QU_s_stack", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_h_stack, "QU_h_stack", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_s_core, "QU_s_core", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_h_core, "QU_h_core", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_s_rss, "QU_s_rss", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_h_rss, "QU_h_rss", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_s_vmem, "QU_s_vmem", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_h_vmem, "QU_h_vmem", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_suspend_interval, "QU_suspend_interval", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_min_cpu_interval, "QU_min_cpu_interval", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_notify, "QU_notify", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_ckpt_list, "QU_ckpt_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_pe_list, "QU_pe_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_owner_list, "QU_owner_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_acl, "QU_acl", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_xacl, "QU_xacl", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_projects, "QU_projects", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_xprojects, "QU_xprojects", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_consumable_config_list, "QU_consumable_config_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_load_thresholds, "QU_load_thresholds", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_suspend_thresholds, "QU_suspend_thresholds", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_subordinate_list, "QU_subordinate_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_state, "QU_state", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {QU_tagged4schedule, "QU_tagged4schedule", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_cache_version, "QU_cache_version", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_pending_signal, "QU_pending_signal", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {QU_pending_signal_delivery_time, "QU_pending_signal_delivery_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {QU_version, "QU_version", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {QU_suspended_on_subordinate, "QU_suspended_on_subordinate", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_last_suspend_threshold_ckeck, "QU_last_suspend_threshold_ckeck", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_job_cnt, "QU_job_cnt", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_pending_job_cnt, "QU_pending_job_cnt", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_soft_violation, "QU_soft_violation", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_host_seq_no, "QU_host_seq_no", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_resource_utilization, "QU_resource_utilization", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_message_list, "QU_message_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {QU_gdi_do_later, "QU_gdi_do_later", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_state_changes, "QU_state_changes", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {QU_joker, "QU_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

