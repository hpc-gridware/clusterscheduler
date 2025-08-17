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
 * This code was generated from file source/libs/sgeobj/json/CQ.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Cluster Queue
*
* An object of this type defines a cluster queue.
* A cluster queue defines the attributes of queue instances (cluster queue on host)
* It contains attribute lists per attribute, which are used to define the value of an attribute
* per host and/or per host group
* @see man page sge_queue_conf.5
* @todo the attribute lists (starting below with seq_no) have a certain type, e.g. AULNG_Type, but
*       but their SubClassName and SubCullPrefix are all ANY.
*
*    SGE_STRING(CQ_name) - Name
*    The name of the cluster queue
*
*    SGE_LIST(CQ_hostlist) - Host List
*    The list of hosts / host groups on which queue instances for the cluster queue shall be created.
*
*    SGE_LIST(CQ_qinstances) - Queue Instances
*    List of queue instances of the cluster queue.
*
*    SGE_LIST(CQ_seq_no) - Sequence Number
*    Queue sequence number, used together with queue_sort_method seqno.
*
*    SGE_LIST(CQ_nsuspend) - NSuspend
*    Number of jobs to suspend on suspend_threshold per time interval.
*
*    SGE_LIST(CQ_job_slots) - Job Slots
*    Number of slots per queue instance.
*
*    SGE_LIST(CQ_rerun) - Rerun
*    Are jobs running in the queue re-runnable?
*
*    SGE_LIST(CQ_s_fsize) - Soft Filesize Limit
*    Soft filesize limit.
*
*    SGE_LIST(CQ_h_fsize) - Hard Filesize Limit
*    Hard filesize limit.
*
*    SGE_LIST(CQ_s_data) - Soft Data Limit
*    Soft data limit.
*
*    SGE_LIST(CQ_h_data) - Hard Data Limit
*    Hard data limit.
*
*    SGE_LIST(CQ_s_stack) - Soft Stack Limit
*    Soft stack size limit.
*
*    SGE_LIST(CQ_h_stack) - Hard Stack Limit
*    Hard stack size limit.
*
*    SGE_LIST(CQ_s_core) - Soft Core Limit
*    Soft core size limit.
*
*    SGE_LIST(CQ_h_core) - Hard Core Limit
*    Hard core size limit.
*
*    SGE_LIST(CQ_s_rss) - Soft RSS Limit
*    Soft RSS limit.
*
*    SGE_LIST(CQ_h_rss) - Hard RSS Limit
*    Hard RSS limit.
*
*    SGE_LIST(CQ_s_vmem) - Soft VMEM Limit
*    Soft VMEM limit.
*
*    SGE_LIST(CQ_h_vmem) - Hard VMEM Limit
*    Hard VMEM limit.
*
*    SGE_LIST(CQ_s_rt) - Soft Runtime Limit
*    Soft runtime limit.
*
*    SGE_LIST(CQ_h_rt) - Hard Runtime Limit
*    Hard runtime limit.
*
*    SGE_LIST(CQ_s_cpu) - Soft CPU Limit
*    Soft CPU time limit.
*
*    SGE_LIST(CQ_h_cpu) - Hard CPU Limit
*    Hard CPU time limit.
*
*    SGE_LIST(CQ_suspend_interval) - Suspend Interval
*    Once every interval nsuspend jobs can be suspended by suspend_threshold.
*
*    SGE_LIST(CQ_min_cpu_interval) - Min CPU Interval
*    Time between two automatic checkpoints.
*
*    SGE_LIST(CQ_notify) - Notify
*    Notify jobs before signalling them?
*
*    SGE_LIST(CQ_tmpdir) - Tmp Directory
*    Toplevel tmp directory holding per job tmp directories on the execution side.
*
*    SGE_LIST(CQ_shell) - Shell
*    Default shell used when starting jobs.
*
*    SGE_LIST(CQ_calendar) - Calendar
*    Queue calendar.
*
*    SGE_LIST(CQ_priority) - Priority
*    Nice value applied to jobs running in the queue.
*
*    SGE_LIST(CQ_processors) - Processors
*    Solaris processor set
*    @todo is it still used at all? We now have hwloc on Solaris as well.
*
*    SGE_LIST(CQ_prolog) - Prolog
*    Prolog script started before a job.
*
*    SGE_LIST(CQ_epilog) - Epilog
*    Epilog script started after a job.
*
*    SGE_LIST(CQ_shell_start_mode) - Shell Start Mode
*    How to start jobs (unix_behavior or posix_compliant, see sge_queue_conf.5 man page.
*
*    SGE_LIST(CQ_starter_method) - Starter Method
*    Job starter script.
*
*    SGE_LIST(CQ_suspend_method) - Suspend Method
*    Signal to be sent to suspend a job or path to a script to be executed when a job shall be suspended.
*
*    SGE_LIST(CQ_resume_method) - Resume Method
*    Signal to be sent to resume a job or path to a script to be executed when a job shall be resumed.
*
*    SGE_LIST(CQ_terminate_method) - Terminate Method
*    Signal to be sent to terminate a job or path to a script to be executed when a job shall be terminated.
*
*    SGE_LIST(CQ_initial_state) - Initial State
*    Initial state of a newly created queue instance (default, enabled, disabled).
*
*    SGE_LIST(CQ_pe_list) - PE List
*    List of parallel environments available for a queue instance.
*
*    SGE_LIST(CQ_ckpt_list) - Checkpoint List
*    List of  checkpointing environments available for a queue instance.
*
*    SGE_LIST(CQ_owner_list) - Owner List
*    List of queue owners (has administrative rights like disable/enable, suspend/resume.
*
*    SGE_LIST(CQ_acl) - Access List
*    ACL defining whose jobs may run in the queue.
*
*    SGE_LIST(CQ_xacl) - X Access List
*    ACL defining whose jobs may not run in the queue.
*
*    SGE_LIST(CQ_projects) - Projects
*    List of projects whose jobs may run in the queue.
*
*    SGE_LIST(CQ_xprojects) - X Projects
*    List of projects whose jobs may not run in the queue.
*
*    SGE_LIST(CQ_load_thresholds) - Load Thresholds
*    List of load thresholds which will disable queue instances when exceeded.
*
*    SGE_LIST(CQ_suspend_thresholds) - Suspend Thresholds
*    List of load thresholds which will suspend queue instances (jobs running in the qinstance) when exceeded.
*
*    SGE_LIST(CQ_consumable_config_list) - Consumable Config List
*    Capacities of consumable resources.
*
*    SGE_LIST(CQ_subordinate_list) - Subordinate List
*    List of subordinate queues.
*
*    SGE_LIST(CQ_qtype) - Queue Type
*    Queue type (batch or interactive).
*
*    SGE_ULONG(CQ_tag) - Tag
*    Queue tag used in scheduler thread to mark certain queues.
*
*    SGE_LIST(CQ_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of VA type and it is spooled.
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
   CQ_tag,
   CQ_joker
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
   SGE_LIST(CQ_joker, VA_Type, CULL_SPOOL)
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
   NAME("CQ_joker")
NAMEEND

#define CQ_SIZE sizeof(CQN)/sizeof(char *)


