#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Queue Instance
*
* Queue Instance: Instance of a cluster queue on an execution host
*
*    SGE_HOST(QU_qhostname) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_qname) - cluster queue name
*    @todo add description
*
*    SGE_STRING(QU_full_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_tag) - @todo add summary
*    @todo add description
*
*    SGE_ULONG64(QU_available_at) - @todo add summary
*    scheduling code only
*
*    SGE_ULONG(QU_seq_no) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_nsuspend) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_qtype) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_job_slots) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_tmpdir) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_shell) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_calendar) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_priority) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_processors) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_prolog) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_epilog) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_shell_start_mode) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_starter_method) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_suspend_method) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_resume_method) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_terminate_method) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_initial_state) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(QU_rerun) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_s_rt) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_h_rt) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_s_cpu) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_h_cpu) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_s_fsize) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_h_fsize) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_s_data) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_h_data) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_s_stack) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_h_stack) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_s_core) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_h_core) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_s_rss) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_h_rss) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_s_vmem) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_h_vmem) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_suspend_interval) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_min_cpu_interval) - @todo add summary
*    @todo add description
*
*    SGE_STRING(QU_notify) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_ckpt_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_pe_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_owner_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_acl) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_xacl) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_projects) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_xprojects) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_consumable_config_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_load_thresholds) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_suspend_thresholds) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_subordinate_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_state) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_tagged4schedule) - Queue Instance Tagged for Schedule
*    used for parallel job scheduling only
*    0 - can be used only as slave queue
*    1 - can be used as slave queue for now assignment,
*        however as master for reservation
*    2 - can be used as master for now and reservation
*
*    SGE_ULONG(QU_cache_version) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_pending_signal) - @todo add summary
*    @todo add description
*
*    SGE_ULONG64(QU_pending_signal_delivery_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_version) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_suspended_on_subordinate) - @todo add summary
*    @todo add description
*
*    SGE_ULONG64(QU_last_suspend_threshold_ckeck) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_pending_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_soft_violation) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_host_seq_no) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_resource_utilization) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_message_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(QU_gdi_do_later) - @todo add summary
*    @todo add description
*
*    SGE_LIST(QU_state_changes) - New State Variables
*    new state variables, these are needed for the scheduler, to dispach jobs into suitable queues
*
*    SGE_LIST(QU_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   QU_qhostname = QU_LOWERBOUND,
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

LISTDEF(QU_Type)
   SGE_HOST(QU_qhostname, CULL_HASH | CULL_SPOOL)
   SGE_STRING(QU_qname, CULL_HASH | CULL_SPOOL)
   SGE_STRING(QU_full_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(QU_tag, CULL_DEFAULT)
   SGE_ULONG64(QU_available_at, CULL_DEFAULT)
   SGE_ULONG(QU_seq_no, CULL_CONFIGURE)
   SGE_ULONG(QU_nsuspend, CULL_CONFIGURE)
   SGE_ULONG(QU_qtype, CULL_CONFIGURE)
   SGE_ULONG(QU_job_slots, CULL_CONFIGURE)
   SGE_STRING(QU_tmpdir, CULL_CONFIGURE)
   SGE_STRING(QU_shell, CULL_CONFIGURE)
   SGE_STRING(QU_calendar, CULL_CONFIGURE)
   SGE_STRING(QU_priority, CULL_CONFIGURE)
   SGE_STRING(QU_processors, CULL_CONFIGURE)
   SGE_STRING(QU_prolog, CULL_CONFIGURE)
   SGE_STRING(QU_epilog, CULL_CONFIGURE)
   SGE_STRING(QU_shell_start_mode, CULL_CONFIGURE)
   SGE_STRING(QU_starter_method, CULL_CONFIGURE)
   SGE_STRING(QU_suspend_method, CULL_CONFIGURE)
   SGE_STRING(QU_resume_method, CULL_CONFIGURE)
   SGE_STRING(QU_terminate_method, CULL_CONFIGURE)
   SGE_STRING(QU_initial_state, CULL_CONFIGURE)
   SGE_BOOL(QU_rerun, CULL_CONFIGURE)
   SGE_STRING(QU_s_rt, CULL_CONFIGURE)
   SGE_STRING(QU_h_rt, CULL_CONFIGURE)
   SGE_STRING(QU_s_cpu, CULL_CONFIGURE)
   SGE_STRING(QU_h_cpu, CULL_CONFIGURE)
   SGE_STRING(QU_s_fsize, CULL_CONFIGURE)
   SGE_STRING(QU_h_fsize, CULL_CONFIGURE)
   SGE_STRING(QU_s_data, CULL_CONFIGURE)
   SGE_STRING(QU_h_data, CULL_CONFIGURE)
   SGE_STRING(QU_s_stack, CULL_CONFIGURE)
   SGE_STRING(QU_h_stack, CULL_CONFIGURE)
   SGE_STRING(QU_s_core, CULL_CONFIGURE)
   SGE_STRING(QU_h_core, CULL_CONFIGURE)
   SGE_STRING(QU_s_rss, CULL_CONFIGURE)
   SGE_STRING(QU_h_rss, CULL_CONFIGURE)
   SGE_STRING(QU_s_vmem, CULL_CONFIGURE)
   SGE_STRING(QU_h_vmem, CULL_CONFIGURE)
   SGE_STRING(QU_suspend_interval, CULL_CONFIGURE)
   SGE_STRING(QU_min_cpu_interval, CULL_CONFIGURE)
   SGE_STRING(QU_notify, CULL_CONFIGURE)
   SGE_LIST(QU_ckpt_list, ST_Type, CULL_CONFIGURE)
   SGE_LIST(QU_pe_list, ST_Type, CULL_CONFIGURE)
   SGE_LIST(QU_owner_list, US_Type, CULL_CONFIGURE)
   SGE_LIST(QU_acl, US_Type, CULL_CONFIGURE)
   SGE_LIST(QU_xacl, US_Type, CULL_CONFIGURE)
   SGE_LIST(QU_projects, PR_Type, CULL_CONFIGURE)
   SGE_LIST(QU_xprojects, PR_Type, CULL_CONFIGURE)
   SGE_LIST(QU_consumable_config_list, CE_Type, CULL_CONFIGURE)
   SGE_LIST(QU_load_thresholds, CE_Type, CULL_CONFIGURE)
   SGE_LIST(QU_suspend_thresholds, CE_Type, CULL_CONFIGURE)
   SGE_LIST(QU_subordinate_list, SO_Type, CULL_CONFIGURE)
   SGE_ULONG(QU_state, CULL_SPOOL)
   SGE_ULONG(QU_tagged4schedule, CULL_DEFAULT)
   SGE_ULONG(QU_cache_version, CULL_DEFAULT)
   SGE_ULONG(QU_pending_signal, CULL_SPOOL)
   SGE_ULONG64(QU_pending_signal_delivery_time, CULL_SPOOL)
   SGE_ULONG(QU_version, CULL_SPOOL)
   SGE_ULONG(QU_suspended_on_subordinate, CULL_DEFAULT)
   SGE_ULONG64(QU_last_suspend_threshold_ckeck, CULL_DEFAULT)
   SGE_ULONG(QU_job_cnt, CULL_DEFAULT)
   SGE_ULONG(QU_pending_job_cnt, CULL_DEFAULT)
   SGE_ULONG(QU_soft_violation, CULL_DEFAULT)
   SGE_ULONG(QU_host_seq_no, CULL_DEFAULT)
   SGE_LIST(QU_resource_utilization, RUE_Type, CULL_DEFAULT)
   SGE_LIST(QU_message_list, QIM_Type, CULL_SPOOL)
   SGE_ULONG(QU_gdi_do_later, CULL_DEFAULT)
   SGE_LIST(QU_state_changes, CCT_Type, CULL_DEFAULT)
   SGE_LIST(QU_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(QUN)
   NAME("QU_qhostname")
   NAME("QU_qname")
   NAME("QU_full_name")
   NAME("QU_tag")
   NAME("QU_available_at")
   NAME("QU_seq_no")
   NAME("QU_nsuspend")
   NAME("QU_qtype")
   NAME("QU_job_slots")
   NAME("QU_tmpdir")
   NAME("QU_shell")
   NAME("QU_calendar")
   NAME("QU_priority")
   NAME("QU_processors")
   NAME("QU_prolog")
   NAME("QU_epilog")
   NAME("QU_shell_start_mode")
   NAME("QU_starter_method")
   NAME("QU_suspend_method")
   NAME("QU_resume_method")
   NAME("QU_terminate_method")
   NAME("QU_initial_state")
   NAME("QU_rerun")
   NAME("QU_s_rt")
   NAME("QU_h_rt")
   NAME("QU_s_cpu")
   NAME("QU_h_cpu")
   NAME("QU_s_fsize")
   NAME("QU_h_fsize")
   NAME("QU_s_data")
   NAME("QU_h_data")
   NAME("QU_s_stack")
   NAME("QU_h_stack")
   NAME("QU_s_core")
   NAME("QU_h_core")
   NAME("QU_s_rss")
   NAME("QU_h_rss")
   NAME("QU_s_vmem")
   NAME("QU_h_vmem")
   NAME("QU_suspend_interval")
   NAME("QU_min_cpu_interval")
   NAME("QU_notify")
   NAME("QU_ckpt_list")
   NAME("QU_pe_list")
   NAME("QU_owner_list")
   NAME("QU_acl")
   NAME("QU_xacl")
   NAME("QU_projects")
   NAME("QU_xprojects")
   NAME("QU_consumable_config_list")
   NAME("QU_load_thresholds")
   NAME("QU_suspend_thresholds")
   NAME("QU_subordinate_list")
   NAME("QU_state")
   NAME("QU_tagged4schedule")
   NAME("QU_cache_version")
   NAME("QU_pending_signal")
   NAME("QU_pending_signal_delivery_time")
   NAME("QU_version")
   NAME("QU_suspended_on_subordinate")
   NAME("QU_last_suspend_threshold_ckeck")
   NAME("QU_job_cnt")
   NAME("QU_pending_job_cnt")
   NAME("QU_soft_violation")
   NAME("QU_host_seq_no")
   NAME("QU_resource_utilization")
   NAME("QU_message_list")
   NAME("QU_gdi_do_later")
   NAME("QU_state_changes")
   NAME("QU_joker")
NAMEEND

#define QU_SIZE sizeof(QUN)/sizeof(char *)


