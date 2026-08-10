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
 * This code was generated from file source/libs/sgeobj/json/CK.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Checkpoint
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of CK
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   CK_name = 4950,   ///< Name
   CK_interface,   ///< Interface
   CK_ckpt_command,   ///< Checkpoint Command
   CK_migr_command,   ///< Migration Command
   CK_rest_command,   ///< Restart Command
   CK_ckpt_dir,   ///< Checkpoint Directory
   CK_when,   ///< When
   CK_signal,   ///< Signal
   CK_job_pid,   ///< Job Pid
   CK_clean_command,   ///< Clean Command
   CK_joker   ///< Joker
};

/** @brief The attribute ids of CK, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int CK_Type[] = {
   CK_name,
   CK_interface,
   CK_ckpt_command,
   CK_migr_command,
   CK_rest_command,
   CK_ckpt_dir,
   CK_when,
   CK_signal,
   CK_job_pid,
   CK_clean_command,
   CK_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of CK
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define CK_ATTRIBUTES \
   {CK_name, "CK_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {CK_interface, "CK_interface", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CK_ckpt_command, "CK_ckpt_command", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CK_migr_command, "CK_migr_command", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CK_rest_command, "CK_rest_command", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CK_ckpt_dir, "CK_ckpt_dir", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CK_when, "CK_when", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CK_signal, "CK_signal", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CK_job_pid, "CK_job_pid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CK_clean_command, "CK_clean_command", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {CK_joker, "CK_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

