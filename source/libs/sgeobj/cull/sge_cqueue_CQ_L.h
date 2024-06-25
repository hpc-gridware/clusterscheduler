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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(CQ_name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_hostlist) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_qinstances) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_seq_no) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_nsuspend) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_job_slots) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_rerun) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_s_fsize) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_h_fsize) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_s_data) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_h_data) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_s_stack) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_h_stack) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_s_core) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_h_core) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_s_rss) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_h_rss) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_s_vmem) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_h_vmem) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_s_rt) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_h_rt) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_s_cpu) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_h_cpu) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_suspend_interval) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_min_cpu_interval) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_notify) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_tmpdir) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_shell) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_calendar) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_priority) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_processors) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_prolog) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_epilog) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_shell_start_mode) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_starter_method) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_suspend_method) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_resume_method) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_terminate_method) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_initial_state) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_pe_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_ckpt_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_owner_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_acl) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_xacl) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_projects) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_xprojects) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_load_thresholds) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_suspend_thresholds) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_consumable_config_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_subordinate_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CQ_qtype) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CQ_tag) - @todo add summary
*    @todo add description
*
*/

enum {
   CQ_name = CQ_LOWERBOUND,
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
   CQ_tag
};

LISTDEF(CQ_Type)
   SGE_STRING(CQ_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_LIST(CQ_hostlist, HR_Type, CULL_SPOOL)
   SGE_LIST(CQ_qinstances, QU_Type, CULL_DEFAULT)
   SGE_LIST(CQ_seq_no, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_nsuspend, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_job_slots, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_rerun, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_s_fsize, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_h_fsize, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_s_data, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_h_data, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_s_stack, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_h_stack, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_s_core, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_h_core, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_s_rss, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_h_rss, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_s_vmem, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_h_vmem, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_s_rt, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_h_rt, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_s_cpu, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_h_cpu, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_suspend_interval, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_min_cpu_interval, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_notify, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_tmpdir, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_shell, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_calendar, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_priority, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_processors, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_prolog, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_epilog, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_shell_start_mode, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_starter_method, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_suspend_method, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_resume_method, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_terminate_method, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_initial_state, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_pe_list, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_ckpt_list, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_owner_list, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_acl, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_xacl, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_projects, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_xprojects, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_load_thresholds, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_suspend_thresholds, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_consumable_config_list, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_subordinate_list, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_LIST(CQ_qtype, CULL_ANY_SUBTYPE, CULL_SPOOL)
   SGE_ULONG(CQ_tag, CULL_DEFAULT)
LISTEND

NAMEDEF(CQN)
   NAME("CQ_name")
   NAME("CQ_hostlist")
   NAME("CQ_qinstances")
   NAME("CQ_seq_no")
   NAME("CQ_nsuspend")
   NAME("CQ_job_slots")
   NAME("CQ_rerun")
   NAME("CQ_s_fsize")
   NAME("CQ_h_fsize")
   NAME("CQ_s_data")
   NAME("CQ_h_data")
   NAME("CQ_s_stack")
   NAME("CQ_h_stack")
   NAME("CQ_s_core")
   NAME("CQ_h_core")
   NAME("CQ_s_rss")
   NAME("CQ_h_rss")
   NAME("CQ_s_vmem")
   NAME("CQ_h_vmem")
   NAME("CQ_s_rt")
   NAME("CQ_h_rt")
   NAME("CQ_s_cpu")
   NAME("CQ_h_cpu")
   NAME("CQ_suspend_interval")
   NAME("CQ_min_cpu_interval")
   NAME("CQ_notify")
   NAME("CQ_tmpdir")
   NAME("CQ_shell")
   NAME("CQ_calendar")
   NAME("CQ_priority")
   NAME("CQ_processors")
   NAME("CQ_prolog")
   NAME("CQ_epilog")
   NAME("CQ_shell_start_mode")
   NAME("CQ_starter_method")
   NAME("CQ_suspend_method")
   NAME("CQ_resume_method")
   NAME("CQ_terminate_method")
   NAME("CQ_initial_state")
   NAME("CQ_pe_list")
   NAME("CQ_ckpt_list")
   NAME("CQ_owner_list")
   NAME("CQ_acl")
   NAME("CQ_xacl")
   NAME("CQ_projects")
   NAME("CQ_xprojects")
   NAME("CQ_load_thresholds")
   NAME("CQ_suspend_thresholds")
   NAME("CQ_consumable_config_list")
   NAME("CQ_subordinate_list")
   NAME("CQ_qtype")
   NAME("CQ_tag")
NAMEEND

#define CQ_SIZE sizeof(CQN)/sizeof(char *)


