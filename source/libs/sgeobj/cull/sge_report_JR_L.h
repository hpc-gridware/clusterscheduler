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
 * This code was generated from file source/libs/sgeobj/json/JR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Report
*
* Definition of a job report sent from sge_execd to sge_qmaster.
* Job reports are sent for every running array task and every task of a tightly integrated parallel job.
*
*    SGE_ULONG(JR_job_number) - Job Number
*    The job number / job id of the corresponding job.
*
*    SGE_ULONG(JR_ja_task_number) - Array Task Number
*    The array task number / id of the corresponding array task.
*
*    SGE_STRING(JR_queue_name) - Queue Name
*    Queue this job is running in / tried to run in.
*
*    SGE_ULONG(JR_state) - State
*    Either JRUNNING or JEXITING, JRUNNING is sent as ack for jobdelivery and in the load_report_interval.
*
*    SGE_ULONG(JR_failed) - Failed
*    Failure code, e.g. EESTATE_NO_CONFIG, EESTATE_NO_PID, ...
*
*    SGE_ULONG(JR_general_failure) - General Failure
*    General failure code, e.g. GFSTATE_NO_HALT, GFSTATE_QUEUE, ...
*
*    SGE_STRING(JR_err_str) - Error String
*    Error description for failed jobs.
*
*    SGE_LIST(JR_usage) - Usage
*    List of resource usage by the job/task, e.g. wallclock, cpu, mem, ...
*
*    SGE_ULONG(JR_job_pid) - Job Pid
*    Process Id of the job script / the toplevel process of the job.
*
*    SGE_ULONG(JR_ckpt_arena) - Checkpoint Arena
*    If there is a checkpoint in the arena.
*
*    SGE_STRING(JR_pe_task_id_str) - Pe Task Id
*    For tasks of tightly integrated parallel jobs: The tasks unique id.
*    It consists of the host name and a sequential number, e.g. myhostname.5
*
*    SGE_STRING(JR_osjobid) - OS Job Id
*    String containing osjobid for ckpt jobs.
*
*    SGE_ULONG(JR_wait_status) - Wait Status
*    japi_wait() 'status' information.
*
*    SGE_BOOL(JR_flush) - Flush
*    Used in sge_execd: Whether to send the job report immediately / as early as possible.
*
*    SGE_BOOL(JR_no_send) - No Send
*    Used in sge_execd: Do not send the job report, e.g. for pe tasks when accounting_summary is configured.
*
*    SGE_BOOL(JR_delay_report) - Delay Report
*    Used in sge_execd: Delay sending of the job report. Used for qsub -sync/DRMAA jobs after a qmaster failover.
*
*/

enum {
   JR_job_number = JR_LOWERBOUND,
   JR_ja_task_number,
   JR_queue_name,
   JR_state,
   JR_failed,
   JR_general_failure,
   JR_err_str,
   JR_usage,
   JR_job_pid,
   JR_ckpt_arena,
   JR_pe_task_id_str,
   JR_osjobid,
   JR_wait_status,
   JR_flush,
   JR_no_send,
   JR_delay_report
};

LISTDEF(JR_Type)
   SGE_ULONG(JR_job_number, CULL_HASH)
   SGE_ULONG(JR_ja_task_number, CULL_DEFAULT)
   SGE_STRING(JR_queue_name, CULL_DEFAULT)
   SGE_ULONG(JR_state, CULL_DEFAULT)
   SGE_ULONG(JR_failed, CULL_DEFAULT)
   SGE_ULONG(JR_general_failure, CULL_DEFAULT)
   SGE_STRING(JR_err_str, CULL_DEFAULT)
   SGE_LIST(JR_usage, UA_Type, CULL_DEFAULT)
   SGE_ULONG(JR_job_pid, CULL_DEFAULT)
   SGE_ULONG(JR_ckpt_arena, CULL_DEFAULT)
   SGE_STRING(JR_pe_task_id_str, CULL_DEFAULT)
   SGE_STRING(JR_osjobid, CULL_DEFAULT)
   SGE_ULONG(JR_wait_status, CULL_DEFAULT)
   SGE_BOOL(JR_flush, CULL_DEFAULT)
   SGE_BOOL(JR_no_send, CULL_DEFAULT)
   SGE_BOOL(JR_delay_report, CULL_DEFAULT)
LISTEND

NAMEDEF(JRN)
   NAME("JR_job_number")
   NAME("JR_ja_task_number")
   NAME("JR_queue_name")
   NAME("JR_state")
   NAME("JR_failed")
   NAME("JR_general_failure")
   NAME("JR_err_str")
   NAME("JR_usage")
   NAME("JR_job_pid")
   NAME("JR_ckpt_arena")
   NAME("JR_pe_task_id_str")
   NAME("JR_osjobid")
   NAME("JR_wait_status")
   NAME("JR_flush")
   NAME("JR_no_send")
   NAME("JR_delay_report")
NAMEEND

#define JR_SIZE sizeof(JRN)/sizeof(char *)


