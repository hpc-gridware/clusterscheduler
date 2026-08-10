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
 * This code was generated from file source/libs/sgeobj/json/PET.json
 * DO NOT CHANGE
 */

/** @file
 * @brief PE Task
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of PET
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   PET_id = 4050,   ///< PE Task Id
   PET_name,   ///< Task Name
   PET_status,   ///< Status
   PET_granted_destin_identifier_list,   ///< Granted Destination Identifier List
   PET_pid,   ///< Process Id
   PET_osjobid,   ///< OS JobId
   PET_systemd_scope,   ///< Systemd Scope
   PET_usage_collection,   ///< Usage Collection Mode
   PET_usage,   ///< Usage
   PET_scaled_usage,   ///< Scaled Usage
   PET_reported_usage,   ///< Reported Usage
   PET_previous_usage,   ///< Previous Usage
   PET_submission_time,   ///< Submission Time
   PET_start_time,   ///< Start Time
   PET_end_time,   ///< End Time
   PET_cwd,   ///< Current Working Directory
   PET_path_aliases,   ///< Path Aliases
   PET_environment,   ///< Environment
   PET_do_contact,   ///< Do Contact
   PET_cred,   ///< TLS Credentials
   PET_joker   ///< Joker
};

/** @brief The attribute ids of PET, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int PET_Type[] = {
   PET_id,
   PET_name,
   PET_status,
   PET_granted_destin_identifier_list,
   PET_pid,
   PET_osjobid,
   PET_systemd_scope,
   PET_usage_collection,
   PET_usage,
   PET_scaled_usage,
   PET_reported_usage,
   PET_previous_usage,
   PET_submission_time,
   PET_start_time,
   PET_end_time,
   PET_cwd,
   PET_path_aliases,
   PET_environment,
   PET_do_contact,
   PET_cred,
   PET_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of PET
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define PET_ATTRIBUTES \
   {PET_id, "PET_id", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {PET_name, "PET_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_status, "PET_status", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_granted_destin_identifier_list, "PET_granted_destin_identifier_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_pid, "PET_pid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_osjobid, "PET_osjobid", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_systemd_scope, "PET_systemd_scope", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_usage_collection, "PET_usage_collection", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_usage, "PET_usage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_scaled_usage, "PET_scaled_usage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_reported_usage, "PET_reported_usage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_previous_usage, "PET_previous_usage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_submission_time, "PET_submission_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_start_time, "PET_start_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_end_time, "PET_end_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_cwd, "PET_cwd", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_path_aliases, "PET_path_aliases", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_environment, "PET_environment", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_do_contact, "PET_do_contact", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_cred, "PET_cred", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_joker, "PET_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

