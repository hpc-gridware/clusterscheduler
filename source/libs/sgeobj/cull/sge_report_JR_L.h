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
 * This code was generated from file source/libs/sgeobj/json/JR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(JR_job_number) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JR_ja_task_number) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JR_queue_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JR_state) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JR_failed) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JR_general_failure) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JR_err_str) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JR_usage) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JR_job_pid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JR_ckpt_arena) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JR_pe_task_id_str) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JR_osjobid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JR_wait_status) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JR_flush) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JR_no_send) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JR_delay_report) - @todo add summary
*    @todo add description
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


