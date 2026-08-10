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
 * This code was generated from file source/libs/sgeobj/json/LS.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Load Sensor
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of LS
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   LS_name = 7250,   ///< Name
   LS_command,   ///< Command
   LS_pid,   ///< Pid
   LS_in,   ///< Stdin File Handle
   LS_out,   ///< Stdout File Handle
   LS_err,   ///< Stderr File Handle
   LS_has_to_restart,   ///< Has to restart
   LS_tag,   ///< Tag
   LS_incomplete,   ///< Incomplete Values
   LS_complete,   ///< Complete Values
   LS_last_mod   ///< Last Modification Time
};

/** @brief The attribute ids of LS, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int LS_Type[] = {
   LS_name,
   LS_command,
   LS_pid,
   LS_in,
   LS_out,
   LS_err,
   LS_has_to_restart,
   LS_tag,
   LS_incomplete,
   LS_complete,
   LS_last_mod,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of LS
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define LS_ATTRIBUTES \
   {LS_name, "LS_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_command, "LS_command", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_pid, "LS_pid", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_in, "LS_in", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_out, "LS_out", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_err, "LS_err", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_has_to_restart, "LS_has_to_restart", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_tag, "LS_tag", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_incomplete, "LS_incomplete", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_complete, "LS_complete", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {LS_last_mod, "LS_last_mod", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

