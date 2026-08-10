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
 * This code was generated from file source/libs/sgeobj/json/JR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Report
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of JR
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   JR_job_number = 4350,   ///< Job Number
   JR_ja_task_number,   ///< Array Task Number
   JR_queue_name,   ///< Queue Name
   JR_state,   ///< State
   JR_failed,   ///< Failed
   JR_general_failure,   ///< General Failure
   JR_err_str,   ///< Error String
   JR_usage,   ///< Usage
   JR_job_pid,   ///< Job Pid
   JR_ckpt_arena,   ///< Checkpoint Arena
   JR_pe_task_id_str,   ///< Pe Task Id
   JR_osjobid,   ///< OS Job Id
   JR_wait_status,   ///< Wait Status
   JR_flush,   ///< Flush
   JR_no_send,   ///< No Send
   JR_delay_report,   ///< Delay Report
   JR_deleted_by   ///< Deleted By
};

/** @brief The attribute ids of JR, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int JR_Type[] = {
   JR_job_number,
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
   JR_delay_report,
   JR_deleted_by,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of JR
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define JR_ATTRIBUTES \
   {JR_job_number, "JR_job_number", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, false}, \
   {JR_ja_task_number, "JR_ja_task_number", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_queue_name, "JR_queue_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_state, "JR_state", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_failed, "JR_failed", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_general_failure, "JR_general_failure", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_err_str, "JR_err_str", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_usage, "JR_usage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_job_pid, "JR_job_pid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_ckpt_arena, "JR_ckpt_arena", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_pe_task_id_str, "JR_pe_task_id_str", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_osjobid, "JR_osjobid", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_wait_status, "JR_wait_status", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_flush, "JR_flush", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_no_send, "JR_no_send", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_delay_report, "JR_delay_report", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JR_deleted_by, "JR_deleted_by", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

