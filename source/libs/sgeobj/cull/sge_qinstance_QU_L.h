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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Queue Instance
*
* Queue Instance: Instance of a cluster queue on an execution host
*
*    SGE_HOST(QU_qhostname) - Execution Host
*    The host this queue instance runs on. Together with QU_qname it identifies the instance.
*
*    SGE_STRING(QU_qname) - cluster queue name
*    @todo add description
*
*    SGE_STRING(QU_full_name) - Full Name
*    The instance name as users see it, "cluster_queue@host". Derived from QU_qname and QU_qhostname; do not set it directly.
*
*    SGE_ULONG(QU_tag) - Modification Tag
*    Scratch marker used while a cluster queue change is applied, holding SGE_QI_TAG_ADD, _MOD or _DEL for this instance. Not persistent.
*
*    SGE_ULONG64(QU_available_at) - Available At
*    Earliest time this instance could run the job being scheduled, set during resource matching and used to order the candidates.
*
*    SGE_ULONG(QU_seq_no) - Sequence Number
*    Position of this queue in the scheduling order, used when the scheduler is configured to sort queues by sequence number rather than by load.
*
*    SGE_ULONG(QU_nsuspend) - Suspend Count
*    How many jobs are suspended at once when a suspend threshold is exceeded, and resumed at once when it falls back.
*
*    SGE_ULONG(QU_qtype) - Queue Type
*    Whether the queue accepts batch jobs, interactive jobs, or both.
*
*    SGE_ULONG(QU_job_slots) - Slots
*    How many jobs may run in this instance at once. The scheduler compares it against the slots already used.
*
*    SGE_STRING(QU_tmpdir) - Temporary Directory
*    Where the execution daemon creates the per-job temporary directory it advertises as $TMPDIR.
*
*    SGE_STRING(QU_shell) - Login Shell
*    The shell a job script is started with, when the start mode calls for one.
*
*    SGE_STRING(QU_calendar) - Calendar
*    The calendar that enables, disables or suspends this queue by time of day and day of week.
*
*    SGE_STRING(QU_priority) - Nice Value
*    The nice value jobs in this queue are started with.
*
*    SGE_STRING(QU_processors) - Processor Set
*    The processor set jobs are bound to, on the platforms that support one.
*
*    SGE_STRING(QU_prolog) - Prolog
*    A command run as a preamble before the job, in the job's environment. A non-zero exit puts the queue into error state.
*
*    SGE_STRING(QU_epilog) - Epilog
*    A command run after the job, in the job's environment, whatever the job's own exit status was.
*
*    SGE_STRING(QU_shell_start_mode) - Shell Start Mode
*    How the job script is handed to the shell: as a script argument, on stdin, or by exec.
*
*    SGE_STRING(QU_starter_method) - Starter Method
*    Replaces the default way of starting the job, for jobs that need a wrapper.
*
*    SGE_STRING(QU_suspend_method) - Suspend Method
*    Replaces the default SIGSTOP when the job is suspended.
*
*    SGE_STRING(QU_resume_method) - Resume Method
*    Replaces the default SIGCONT when the job is resumed.
*
*    SGE_STRING(QU_terminate_method) - Terminate Method
*    Replaces the default SIGKILL when the job is deleted.
*
*    SGE_STRING(QU_initial_state) - Initial State
*    The state the instance takes when it is first created or when its execution daemon restarts: enabled, disabled, or whatever it was.
*
*    SGE_BOOL(QU_rerun) - Rerunnable
*    Default for jobs whose execution host crashed: rerun them elsewhere, or report them as failed. A job may override it with qsub -r.
*
*    SGE_STRING(QU_s_rt) - Soft Real Time Limit
*    Wall clock time after which the job is warned with SIGUSR1 and then killed once QU_notify has passed.
*
*    SGE_STRING(QU_h_rt) - Hard Real Time Limit
*    Wall clock time after which the job is killed outright.
*
*    SGE_STRING(QU_s_cpu) - Soft CPU Limit
*    Combined CPU time of all the job's processes after which it is sent SIGXCPU.
*
*    SGE_STRING(QU_h_cpu) - Hard CPU Limit
*    Combined CPU time after which the job is killed.
*
*    SGE_STRING(QU_s_fsize) - Soft File Size Limit
*    Largest file the job may create before it is warned.
*
*    SGE_STRING(QU_h_fsize) - Hard File Size Limit
*    Largest file the job may create.
*
*    SGE_STRING(QU_s_data) - Soft Data Segment Limit
*    Data segment size after which the job is warned.
*
*    SGE_STRING(QU_h_data) - Hard Data Segment Limit
*    Largest data segment the job may allocate.
*
*    SGE_STRING(QU_s_stack) - Soft Stack Limit
*    Stack size after which the job is warned.
*
*    SGE_STRING(QU_h_stack) - Hard Stack Limit
*    Largest stack the job may use.
*
*    SGE_STRING(QU_s_core) - Soft Core File Limit
*    Core file size after which the job is warned.
*
*    SGE_STRING(QU_h_core) - Hard Core File Limit
*    Largest core file the job may write.
*
*    SGE_STRING(QU_s_rss) - Soft Resident Set Limit
*    Resident set size after which the job is warned.
*
*    SGE_STRING(QU_h_rss) - Hard Resident Set Limit
*    Largest resident set the job may hold.
*
*    SGE_STRING(QU_s_vmem) - Soft Virtual Memory Limit
*    Virtual memory after which the job is warned. For a parallel job the limit applies per slot on the host.
*
*    SGE_STRING(QU_h_vmem) - Hard Virtual Memory Limit
*    Virtual memory after which the job is killed. For a parallel job the limit applies per slot on the host.
*
*    SGE_STRING(QU_suspend_interval) - Suspend Interval
*    How long to wait between two suspend rounds, so a threshold is not acted on faster than the load can react.
*
*    SGE_STRING(QU_min_cpu_interval) - Minimum Checkpoint Interval
*    Shortest time between two automatic checkpoints, whatever shorter interval the checkpointing environment or the job asks for.
*
*    SGE_STRING(QU_notify) - Notify Time
*    How long to wait between the warning signal and the real suspend or kill signal, for jobs submitted with qsub -notify.
*
*    SGE_LIST(QU_ckpt_list) - Checkpointing Environments
*    The checkpointing environments a job may request here.
*
*    SGE_LIST(QU_pe_list) - Parallel Environments
*    The parallel environments a job may request here.
*
*    SGE_LIST(QU_owner_list) - Owners
*    Users allowed to suspend, resume and disable this queue without being managers or operators.
*
*    SGE_LIST(QU_acl) - Access List
*    The usersets allowed to run jobs here. Empty means everyone who is not in QU_xacl.
*
*    SGE_LIST(QU_xacl) - Excluded Access List
*    The usersets not allowed to run jobs here. Checked before QU_acl.
*
*    SGE_LIST(QU_projects) - Projects
*    The projects whose jobs may run here.
*
*    SGE_LIST(QU_xprojects) - Excluded Projects
*    The projects whose jobs may not run here.
*
*    SGE_LIST(QU_consumable_config_list) - Configured Consumables
*    The consumable capacities of this instance, as configured. QU_resource_utilization tracks what is actually booked against them.
*
*    SGE_LIST(QU_load_thresholds) - Load Thresholds
*    Load values above which the queue stops accepting jobs. Reaching one puts the instance into alarm state (the "a" in qstat).
*
*    SGE_LIST(QU_suspend_thresholds) - Suspend Thresholds
*    Load values above which jobs already running here are suspended, QU_nsuspend at a time.
*
*    SGE_LIST(QU_subordinate_list) - Subordinate Queues
*    Queues to suspend while this one is busy, either wholesale or per slot. What implements the "S" state on the other queue.
*
*    SGE_ULONG(QU_state) - State Bits
*    The QI_* state bits, and the letters qstat prints for them. Eleven independent flags; see sge_qinstance_state.cc.
*
*    SGE_ULONG(QU_tagged4schedule) - Queue Instance Tagged for Schedule
*    used for parallel job scheduling only
*    0 - can be used only as slave queue
*    1 - can be used as slave queue for now assignment,
*        however as master for reservation
*    2 - can be used as master for now and reservation
*
*    SGE_ULONG(QU_cache_version) - Unused
*    @warning Read and written nowhere in either repository. Kept because removing an attribute changes the spooled format.
*
*    SGE_ULONG(QU_pending_signal) - Pending Signal
*    A signal sent to the queue that has not taken effect yet. Kept until the state changes, so it can be repeated.
*
*    SGE_ULONG64(QU_pending_signal_delivery_time) - Pending Signal Retry Time
*    When to send QU_pending_signal again. Spooled, so a qmaster restart does not lose an in-flight signal.
*
*    SGE_ULONG(QU_version) - Queue Version
*    Incremented by qinstance_increase_qversion() on every change. A scheduler order carries the version it was computed against, and sge_follow() rejects the order if the queue has moved on.
*
*    SGE_ULONG(QU_suspended_on_subordinate) - Subordinate Suspend Count
*    How many superordinate queues currently demand that this one be suspended. A count, not a flag, so releasing one does not resume a queue another still holds down.
*
*    SGE_ULONG64(QU_last_suspend_threshold_ckeck) - Last Suspend Threshold Check
*    When the suspend thresholds were last evaluated, so they are not acted on more often than QU_suspend_interval.
*
*    SGE_ULONG(QU_job_cnt) - Unused
*    @warning Read and written nowhere in either repository. Kept because removing an attribute changes the spooled format.
*
*    SGE_ULONG(QU_pending_job_cnt) - Unused
*    @warning Read and written nowhere in either repository. Kept because removing an attribute changes the spooled format.
*
*    SGE_ULONG(QU_soft_violation) - Soft Request Violations
*    How many of the job's soft requests this instance cannot satisfy. The scheduler sorts candidates by this ascending, so a queue that grants more soft requests wins.
*
*    SGE_ULONG(QU_host_seq_no) - Host Sequence Number
*    The host's position in the scheduler's host sort order, copied onto the instance while tagging so queues on a better host sort first.
*
*    SGE_LIST(QU_resource_utilization) - Resource Utilization
*    What is actually booked against each consumable over time, including future bookings made by advance reservations. The counterpart of QU_consumable_config_list.
*
*    SGE_LIST(QU_message_list) - Messages
*    Messages explaining the instance's state, shown by qstat -explain and qrstat.
*
*    SGE_ULONG(QU_gdi_do_later) - Deferred GDI Change
*    Marks the instance as having a change that is applied after the current request rather than during it.
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
   QU_qhostname = QU_LOWERBOUND,   ///< Execution Host
   QU_qname,   ///< cluster queue name
   QU_full_name,   ///< Full Name
   QU_tag,   ///< Modification Tag
   QU_available_at,   ///< Available At
   QU_seq_no,   ///< Sequence Number
   QU_nsuspend,   ///< Suspend Count
   QU_qtype,   ///< Queue Type
   QU_job_slots,   ///< Slots
   QU_tmpdir,   ///< Temporary Directory
   QU_shell,   ///< Login Shell
   QU_calendar,   ///< Calendar
   QU_priority,   ///< Nice Value
   QU_processors,   ///< Processor Set
   QU_prolog,   ///< Prolog
   QU_epilog,   ///< Epilog
   QU_shell_start_mode,   ///< Shell Start Mode
   QU_starter_method,   ///< Starter Method
   QU_suspend_method,   ///< Suspend Method
   QU_resume_method,   ///< Resume Method
   QU_terminate_method,   ///< Terminate Method
   QU_initial_state,   ///< Initial State
   QU_rerun,   ///< Rerunnable
   QU_s_rt,   ///< Soft Real Time Limit
   QU_h_rt,   ///< Hard Real Time Limit
   QU_s_cpu,   ///< Soft CPU Limit
   QU_h_cpu,   ///< Hard CPU Limit
   QU_s_fsize,   ///< Soft File Size Limit
   QU_h_fsize,   ///< Hard File Size Limit
   QU_s_data,   ///< Soft Data Segment Limit
   QU_h_data,   ///< Hard Data Segment Limit
   QU_s_stack,   ///< Soft Stack Limit
   QU_h_stack,   ///< Hard Stack Limit
   QU_s_core,   ///< Soft Core File Limit
   QU_h_core,   ///< Hard Core File Limit
   QU_s_rss,   ///< Soft Resident Set Limit
   QU_h_rss,   ///< Hard Resident Set Limit
   QU_s_vmem,   ///< Soft Virtual Memory Limit
   QU_h_vmem,   ///< Hard Virtual Memory Limit
   QU_suspend_interval,   ///< Suspend Interval
   QU_min_cpu_interval,   ///< Minimum Checkpoint Interval
   QU_notify,   ///< Notify Time
   QU_ckpt_list,   ///< Checkpointing Environments
   QU_pe_list,   ///< Parallel Environments
   QU_owner_list,   ///< Owners
   QU_acl,   ///< Access List
   QU_xacl,   ///< Excluded Access List
   QU_projects,   ///< Projects
   QU_xprojects,   ///< Excluded Projects
   QU_consumable_config_list,   ///< Configured Consumables
   QU_load_thresholds,   ///< Load Thresholds
   QU_suspend_thresholds,   ///< Suspend Thresholds
   QU_subordinate_list,   ///< Subordinate Queues
   QU_state,   ///< State Bits
   QU_tagged4schedule,   ///< Queue Instance Tagged for Schedule
   QU_cache_version,   ///< Unused
   QU_pending_signal,   ///< Pending Signal
   QU_pending_signal_delivery_time,   ///< Pending Signal Retry Time
   QU_version,   ///< Queue Version
   QU_suspended_on_subordinate,   ///< Subordinate Suspend Count
   QU_last_suspend_threshold_ckeck,   ///< Last Suspend Threshold Check
   QU_job_cnt,   ///< Unused
   QU_pending_job_cnt,   ///< Unused
   QU_soft_violation,   ///< Soft Request Violations
   QU_host_seq_no,   ///< Host Sequence Number
   QU_resource_utilization,   ///< Resource Utilization
   QU_message_list,   ///< Messages
   QU_gdi_do_later,   ///< Deferred GDI Change
   QU_state_changes,   ///< New State Variables
   QU_joker   ///< Joker
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

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define QU_SIZE sizeof(QUN)/sizeof(char *)


