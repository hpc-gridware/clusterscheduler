#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/JAT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Array Task
*
* JAT_Type elements are a sub element of JB_Type elements.
* One element of each type is necessary to hold all data for the execution of one job.
* One JB_Type element and x JAT_Type elements are needed to execute an array job with x tasks.
* 
*          -----------       1:x        ------------
*          | JB_Type |<---------------->| JAT_Type |
*          -----------                  ------------
* 
* The relation between these two elements is defined in the
* 'JB_ja_tasks' sublist of a 'JB_Type' element. This list will
* contain all belonging JAT_Type elements.
* 
* The 'JAT_Type' CULL element containes all attributes in which
* one array task may differ from another array task of the
* same array job. The 'JB_Type' element defines all attributes
* which are equivalent for all tasks of an array job.
* A job and an array job with one task are equivalent
* concerning their data structures. Both consist of one 'JB_Type'
* and one 'JAT_Type' element.
* 
* 'JAT_Type' elements contain dynamic data which accrue during the
* execution of a job. Therefore it is not necessary to create
* these elements during the submition of a (array) job but
* after the job has been dispatched.
*
*    SGE_ULONG(JAT_task_number) - Task Number
*    Unique task number assigned during task creation.
*
*    SGE_ULONG(JAT_status) - Status
*    First part of the state (see also JAT_hold, JAT_state).
*    @todo merge status, state and hold
*
*    SGE_ULONG64(JAT_start_time) - Start Time
*    Start time of the array task in microseconds since epoch.
*
*    SGE_ULONG64(JAT_end_time) - End Time
*    End time of the array task in microseconds since epoch.
*    Only used in sge_execd to simulate job runs.
*
*    SGE_ULONG(JAT_hold) - Hold
*    Second part of the state (user, operator, system hold)
*    @todo merge status, state and hold
*
*    SGE_STRING(JAT_granted_pe) - Granted PE
*    Name of a granted parallel environment (in case of parallel jobs).
*
*    SGE_ULONG(JAT_job_restarted) - Job Restarted
*    Was the task restarted (due to reschedule/migrate)?
*    @todo it is no boolean, but misused for other information!
*
*    SGE_LIST(JAT_granted_destin_identifier_list) - Granted Destination Identifier List
*    Granted destination identifier list (JG_Type).
*    Has one element per queue instance the job is running on.
*
*    SGE_LIST(JAT_granted_resources_list) - Granted Resources
*    List of granted resources, currently these are granted RSMAPs only.
*
*    SGE_STRING(JAT_master_queue) - Master Queue
*    Name of the master queue.
*    @todo redundant information, it is the name of the first element in the GDIL.
*
*    SGE_ULONG(JAT_state) - State
*    Third part of state (see also JAT_hold, JAT_status).
*    @todo merge status, state and hold
*
*    SGE_ULONG(JAT_pvm_ckpt_pid) - PVM Checkpoint Pid
*    Pid of a previous, checkpointed run of the job/ja_task.
*    @todo was apparently required for checkpointing/restart of PVM jobs - still needed?
*
*    SGE_ULONG(JAT_pending_signal) - Pending Signal
*    Pending signal (not yet acknowledged by execd).
*
*    SGE_ULONG64(JAT_pending_signal_delivery_time) - Pending Signal Delivery Time
*    Time when a signal shall be delivered by sge_qmaster to sge_execd (repeated) in µs since epoch.
*    Also used in sge_execd to repeat sending of signals.
*
*    SGE_ULONG(JAT_pid) - Pid
*    Pid of the sge_shepherd of a job (child of sge_execd). Used for signal delivery.
*
*    SGE_STRING(JAT_osjobid) - OS Job Id
*    Unique id which applies to all os processes started
*    on behalf of this task. Set during the startup phase of the
*    job. Meaning depends on the architecture of the
*    host were the task is started.
*    SOLARIS/LINUX: additional group id
*    Could be / was a OS supported job id on no longer supported platforms
*    like Cray, Nec, Irix.
*
*    SGE_LIST(JAT_usage_list) - Usage List
*    Raw usage from data collector. Scaled by sge_qmaster to JAT_scaled_usage_list.
*    Scheduling is using the scaled usage.
*
*    SGE_LIST(JAT_scaled_usage_list) - Scaled Usage List
*    Scaled usage set by qmaster, used by scheduler.
*    Usage scaling is defined in the exechost objects, see sge_host_conf.5
*
*    SGE_LIST(JAT_reported_usage_list) - Reported Usage List
*    Usage which has already been reported in intermediate accounting records
*    in the reporting file. For long running jobs, written around midnight
*    to provide daily accounting information.
*
*    SGE_ULONG(JAT_fshare) - Functional Share
*    Functional shares associated with the job.
*
*    SGE_DOUBLE(JAT_tix) - Tickets
*    Total number of tickets.
*
*    SGE_DOUBLE(JAT_oticket) - Override Tickets
*    Override tickets.
*
*    SGE_DOUBLE(JAT_fticket) - Functional Tickets
*    Functional tickets.
*
*    SGE_DOUBLE(JAT_sticket) - Sharetree Tickets
*    Sharetree tickets.
*
*    SGE_DOUBLE(JAT_share) - Share
*    Job targeted proportion set by scheduler.
*
*    SGE_ULONG(JAT_suitable) - Suitable
*    Tag used in filtering in client job output (qstat, qhost).
*
*    SGE_LIST(JAT_task_list) - Task List
*    List of running parallel tasks.
*
*    SGE_LIST(JAT_finished_task_list) - Finished Task List
*    List of finished parallel tasks - contains only task names.
*
*    SGE_LIST(JAT_previous_usage_list) - Previous Usage List
*    Copy of the scaled usage list created when a job gets rescheduled.
*    @todo If a job is rescheduled multiple times, should probably be aggregated.
*
*    SGE_OBJECT(JAT_pe_object) - PE Object
*    PE object granted to this task (PE_Type), only used in execd.
*
*    SGE_ULONG(JAT_next_pe_task_id) - Next PE Task Id
*    Used locally in execd to store next pe task id for this jatask on this execd.
*
*    SGE_ULONG64(JAT_stop_initiate_time) - Stop Initiate Time
*    The time in µs since epoch when the action to initiate a job finish
*    (i.e. job termination, or checkpoint creation) was triggered or re-triggered.
*
*    SGE_DOUBLE(JAT_prio) - Priority
*    The priority derived from weighted normalized tickets and weighted normalized
*    static urgency. Changes with task due to GEEE ticket dependency.
*
*    SGE_DOUBLE(JAT_ntix) - Normalized Tickets
*    Relative importance due to JAT_tix amount in the range between 0.0 and 1.0.
*
*    SGE_ULONG64(JAT_wallclock_limit) - Wallclock Limit
*    Wallclock_limit. Either requested (qsub -l h_rt=...) or the minimum of queue h_rt
*    in microseconds since epoch
*
*    SGE_LIST(JAT_message_list) - Message List
*    Task specific messages.
*    Currently only used for storing the reason why a job is in error state.
*
*    SGE_LIST(JAT_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   JAT_task_number = JAT_LOWERBOUND,
   JAT_status,
   JAT_start_time,
   JAT_end_time,
   JAT_hold,
   JAT_granted_pe,
   JAT_job_restarted,
   JAT_granted_destin_identifier_list,
   JAT_granted_resources_list,
   JAT_master_queue,
   JAT_state,
   JAT_pvm_ckpt_pid,
   JAT_pending_signal,
   JAT_pending_signal_delivery_time,
   JAT_pid,
   JAT_osjobid,
   JAT_usage_list,
   JAT_scaled_usage_list,
   JAT_reported_usage_list,
   JAT_fshare,
   JAT_tix,
   JAT_oticket,
   JAT_fticket,
   JAT_sticket,
   JAT_share,
   JAT_suitable,
   JAT_task_list,
   JAT_finished_task_list,
   JAT_previous_usage_list,
   JAT_pe_object,
   JAT_next_pe_task_id,
   JAT_stop_initiate_time,
   JAT_prio,
   JAT_ntix,
   JAT_wallclock_limit,
   JAT_message_list,
   JAT_joker
};

LISTDEF(JAT_Type)
   SGE_ULONG(JAT_task_number, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_ULONG(JAT_status, CULL_SUBLIST)
   SGE_ULONG64(JAT_start_time, CULL_SUBLIST)
   SGE_ULONG64(JAT_end_time, CULL_SUBLIST)
   SGE_ULONG(JAT_hold, CULL_SUBLIST)
   SGE_STRING(JAT_granted_pe, CULL_SUBLIST)
   SGE_ULONG(JAT_job_restarted, CULL_SUBLIST)
   SGE_LIST(JAT_granted_destin_identifier_list, JG_Type, CULL_SUBLIST)
   SGE_LIST(JAT_granted_resources_list, GRU_Type, CULL_SPOOL)
   SGE_STRING(JAT_master_queue, CULL_SUBLIST)
   SGE_ULONG(JAT_state, CULL_SUBLIST)
   SGE_ULONG(JAT_pvm_ckpt_pid, CULL_SUBLIST)
   SGE_ULONG(JAT_pending_signal, CULL_SUBLIST)
   SGE_ULONG64(JAT_pending_signal_delivery_time, CULL_SUBLIST)
   SGE_ULONG(JAT_pid, CULL_SUBLIST)
   SGE_STRING(JAT_osjobid, CULL_SUBLIST)
   SGE_LIST(JAT_usage_list, UA_Type, CULL_SUBLIST)
   SGE_LIST(JAT_scaled_usage_list, UA_Type, CULL_SUBLIST)
   SGE_LIST(JAT_reported_usage_list, UA_Type, CULL_SUBLIST)
   SGE_ULONG(JAT_fshare, CULL_SUBLIST)
   SGE_DOUBLE(JAT_tix, CULL_SUBLIST)
   SGE_DOUBLE(JAT_oticket, CULL_SUBLIST)
   SGE_DOUBLE(JAT_fticket, CULL_SUBLIST)
   SGE_DOUBLE(JAT_sticket, CULL_SUBLIST)
   SGE_DOUBLE(JAT_share, CULL_SUBLIST)
   SGE_ULONG(JAT_suitable, CULL_DEFAULT)
   SGE_LIST(JAT_task_list, PET_Type, CULL_SUBLIST)
   SGE_LIST(JAT_finished_task_list, FPET_Type, CULL_SUBLIST)
   SGE_LIST(JAT_previous_usage_list, UA_Type, CULL_DEFAULT)
   SGE_OBJECT(JAT_pe_object, CULL_ANY_SUBTYPE, CULL_SUBLIST)
   SGE_ULONG(JAT_next_pe_task_id, CULL_DEFAULT)
   SGE_ULONG64(JAT_stop_initiate_time, CULL_SUBLIST)
   SGE_DOUBLE(JAT_prio, CULL_DEFAULT)
   SGE_DOUBLE(JAT_ntix, CULL_DEFAULT)
   SGE_ULONG64(JAT_wallclock_limit, CULL_SPOOL)
   SGE_LIST(JAT_message_list, QIM_Type, CULL_SPOOL)
   SGE_LIST(JAT_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(JATN)
   NAME("JAT_task_number")
   NAME("JAT_status")
   NAME("JAT_start_time")
   NAME("JAT_end_time")
   NAME("JAT_hold")
   NAME("JAT_granted_pe")
   NAME("JAT_job_restarted")
   NAME("JAT_granted_destin_identifier_list")
   NAME("JAT_granted_resources_list")
   NAME("JAT_master_queue")
   NAME("JAT_state")
   NAME("JAT_pvm_ckpt_pid")
   NAME("JAT_pending_signal")
   NAME("JAT_pending_signal_delivery_time")
   NAME("JAT_pid")
   NAME("JAT_osjobid")
   NAME("JAT_usage_list")
   NAME("JAT_scaled_usage_list")
   NAME("JAT_reported_usage_list")
   NAME("JAT_fshare")
   NAME("JAT_tix")
   NAME("JAT_oticket")
   NAME("JAT_fticket")
   NAME("JAT_sticket")
   NAME("JAT_share")
   NAME("JAT_suitable")
   NAME("JAT_task_list")
   NAME("JAT_finished_task_list")
   NAME("JAT_previous_usage_list")
   NAME("JAT_pe_object")
   NAME("JAT_next_pe_task_id")
   NAME("JAT_stop_initiate_time")
   NAME("JAT_prio")
   NAME("JAT_ntix")
   NAME("JAT_wallclock_limit")
   NAME("JAT_message_list")
   NAME("JAT_joker")
NAMEEND

#define JAT_SIZE sizeof(JATN)/sizeof(char *)


