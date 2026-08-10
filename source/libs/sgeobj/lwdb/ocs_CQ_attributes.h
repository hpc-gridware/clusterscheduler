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
 * This code was generated from file source/libs/sgeobj/json/CQ.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Cluster Queue
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of CQ
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   CQ_name = 9550,   ///< Name
   CQ_hostlist,   ///< Host List
   CQ_qinstances,   ///< Queue Instances
   CQ_seq_no,   ///< Sequence Number
   CQ_nsuspend,   ///< NSuspend
   CQ_job_slots,   ///< Job Slots
   CQ_rerun,   ///< Rerun
   CQ_s_fsize,   ///< Soft Filesize Limit
   CQ_h_fsize,   ///< Hard Filesize Limit
   CQ_s_data,   ///< Soft Data Limit
   CQ_h_data,   ///< Hard Data Limit
   CQ_s_stack,   ///< Soft Stack Limit
   CQ_h_stack,   ///< Hard Stack Limit
   CQ_s_core,   ///< Soft Core Limit
   CQ_h_core,   ///< Hard Core Limit
   CQ_s_rss,   ///< Soft RSS Limit
   CQ_h_rss,   ///< Hard RSS Limit
   CQ_s_vmem,   ///< Soft VMEM Limit
   CQ_h_vmem,   ///< Hard VMEM Limit
   CQ_s_rt,   ///< Soft Runtime Limit
   CQ_h_rt,   ///< Hard Runtime Limit
   CQ_s_cpu,   ///< Soft CPU Limit
   CQ_h_cpu,   ///< Hard CPU Limit
   CQ_suspend_interval,   ///< Suspend Interval
   CQ_min_cpu_interval,   ///< Min CPU Interval
   CQ_notify,   ///< Notify
   CQ_tmpdir,   ///< Tmp Directory
   CQ_shell,   ///< Shell
   CQ_calendar,   ///< Calendar
   CQ_priority,   ///< Priority
   CQ_processors,   ///< Processors
   CQ_prolog,   ///< Prolog
   CQ_epilog,   ///< Epilog
   CQ_shell_start_mode,   ///< Shell Start Mode
   CQ_starter_method,   ///< Starter Method
   CQ_suspend_method,   ///< Suspend Method
   CQ_resume_method,   ///< Resume Method
   CQ_terminate_method,   ///< Terminate Method
   CQ_initial_state,   ///< Initial State
   CQ_pe_list,   ///< PE List
   CQ_ckpt_list,   ///< Checkpoint List
   CQ_owner_list,   ///< Owner List
   CQ_acl,   ///< Access List
   CQ_xacl,   ///< X Access List
   CQ_projects,   ///< Projects
   CQ_xprojects,   ///< X Projects
   CQ_load_thresholds,   ///< Load Thresholds
   CQ_suspend_thresholds,   ///< Suspend Thresholds
   CQ_consumable_config_list,   ///< Consumable Config List
   CQ_subordinate_list,   ///< Subordinate List
   CQ_qtype,   ///< Queue Type
   CQ_tag,   ///< Tag
   CQ_joker   ///< Joker
};

/** @brief The attribute ids of CQ, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int CQ_Type[] = {
   CQ_name,
   CQ_hostlist,
   CQ_qinstances,
   CQ_seq_no,
   CQ_nsuspend,
   CQ_job_slots,
   CQ_rerun,
   CQ_s_fsize,
   CQ_h_fsize,
   CQ_s_data,
   CQ_h_data,
   CQ_s_stack,
   CQ_h_stack,
   CQ_s_core,
   CQ_h_core,
   CQ_s_rss,
   CQ_h_rss,
   CQ_s_vmem,
   CQ_h_vmem,
   CQ_s_rt,
   CQ_h_rt,
   CQ_s_cpu,
   CQ_h_cpu,
   CQ_suspend_interval,
   CQ_min_cpu_interval,
   CQ_notify,
   CQ_tmpdir,
   CQ_shell,
   CQ_calendar,
   CQ_priority,
   CQ_processors,
   CQ_prolog,
   CQ_epilog,
   CQ_shell_start_mode,
   CQ_starter_method,
   CQ_suspend_method,
   CQ_resume_method,
   CQ_terminate_method,
   CQ_initial_state,
   CQ_pe_list,
   CQ_ckpt_list,
   CQ_owner_list,
   CQ_acl,
   CQ_xacl,
   CQ_projects,
   CQ_xprojects,
   CQ_load_thresholds,
   CQ_suspend_thresholds,
   CQ_consumable_config_list,
   CQ_subordinate_list,
   CQ_qtype,
   CQ_tag,
   CQ_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of CQ
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define CQ_ATTRIBUTES \
   {CQ_name, "CQ_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {CQ_hostlist, "CQ_hostlist", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_qinstances, "CQ_qinstances", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CQ_seq_no, "CQ_seq_no", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_nsuspend, "CQ_nsuspend", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_job_slots, "CQ_job_slots", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_rerun, "CQ_rerun", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_s_fsize, "CQ_s_fsize", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_h_fsize, "CQ_h_fsize", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_s_data, "CQ_s_data", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_h_data, "CQ_h_data", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_s_stack, "CQ_s_stack", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_h_stack, "CQ_h_stack", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_s_core, "CQ_s_core", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_h_core, "CQ_h_core", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_s_rss, "CQ_s_rss", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_h_rss, "CQ_h_rss", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_s_vmem, "CQ_s_vmem", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_h_vmem, "CQ_h_vmem", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_s_rt, "CQ_s_rt", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_h_rt, "CQ_h_rt", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_s_cpu, "CQ_s_cpu", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_h_cpu, "CQ_h_cpu", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_suspend_interval, "CQ_suspend_interval", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_min_cpu_interval, "CQ_min_cpu_interval", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_notify, "CQ_notify", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_tmpdir, "CQ_tmpdir", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_shell, "CQ_shell", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_calendar, "CQ_calendar", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_priority, "CQ_priority", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_processors, "CQ_processors", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_prolog, "CQ_prolog", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_epilog, "CQ_epilog", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_shell_start_mode, "CQ_shell_start_mode", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_starter_method, "CQ_starter_method", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_suspend_method, "CQ_suspend_method", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_resume_method, "CQ_resume_method", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_terminate_method, "CQ_terminate_method", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_initial_state, "CQ_initial_state", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_pe_list, "CQ_pe_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_ckpt_list, "CQ_ckpt_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_owner_list, "CQ_owner_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_acl, "CQ_acl", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_xacl, "CQ_xacl", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_projects, "CQ_projects", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_xprojects, "CQ_xprojects", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_load_thresholds, "CQ_load_thresholds", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_suspend_thresholds, "CQ_suspend_thresholds", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_consumable_config_list, "CQ_consumable_config_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_subordinate_list, "CQ_subordinate_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_qtype, "CQ_qtype", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CQ_tag, "CQ_tag", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CQ_joker, "CQ_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

