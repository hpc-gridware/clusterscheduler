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
 * This code was generated from file source/libs/sgeobj/json/CQ.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   CQ_name = 10050,
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
   CQ_joker
};

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

#define CQ_ATTRIBUTES \
   {CQ_name, "CQ_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {CQ_hostlist, "CQ_hostlist", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_qinstances, "CQ_qinstances", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_seq_no, "CQ_seq_no", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_nsuspend, "CQ_nsuspend", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_job_slots, "CQ_job_slots", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_rerun, "CQ_rerun", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_s_fsize, "CQ_s_fsize", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_h_fsize, "CQ_h_fsize", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_s_data, "CQ_s_data", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_h_data, "CQ_h_data", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_s_stack, "CQ_s_stack", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_h_stack, "CQ_h_stack", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_s_core, "CQ_s_core", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_h_core, "CQ_h_core", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_s_rss, "CQ_s_rss", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_h_rss, "CQ_h_rss", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_s_vmem, "CQ_s_vmem", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_h_vmem, "CQ_h_vmem", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_s_rt, "CQ_s_rt", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_h_rt, "CQ_h_rt", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_s_cpu, "CQ_s_cpu", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_h_cpu, "CQ_h_cpu", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_suspend_interval, "CQ_suspend_interval", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_min_cpu_interval, "CQ_min_cpu_interval", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_notify, "CQ_notify", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_tmpdir, "CQ_tmpdir", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_shell, "CQ_shell", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_calendar, "CQ_calendar", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_priority, "CQ_priority", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_processors, "CQ_processors", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_prolog, "CQ_prolog", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_epilog, "CQ_epilog", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_shell_start_mode, "CQ_shell_start_mode", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_starter_method, "CQ_starter_method", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_suspend_method, "CQ_suspend_method", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_resume_method, "CQ_resume_method", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_terminate_method, "CQ_terminate_method", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_initial_state, "CQ_initial_state", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_pe_list, "CQ_pe_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_ckpt_list, "CQ_ckpt_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_owner_list, "CQ_owner_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_acl, "CQ_acl", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_xacl, "CQ_xacl", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_projects, "CQ_projects", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_xprojects, "CQ_xprojects", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_load_thresholds, "CQ_load_thresholds", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_suspend_thresholds, "CQ_suspend_thresholds", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_consumable_config_list, "CQ_consumable_config_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_subordinate_list, "CQ_subordinate_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_qtype, "CQ_qtype", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {CQ_tag, "CQ_tag", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {CQ_joker, "CQ_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

