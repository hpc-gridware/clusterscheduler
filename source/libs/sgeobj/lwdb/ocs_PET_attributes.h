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
 * This code was generated from file source/libs/sgeobj/json/PET.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   PET_id = 4450,
   PET_name,
   PET_status,
   PET_granted_destin_identifier_list,
   PET_pid,
   PET_osjobid,
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
   PET_joker
};

constexpr const int PET_Type[] = {
   PET_id,
   PET_name,
   PET_status,
   PET_granted_destin_identifier_list,
   PET_pid,
   PET_osjobid,
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
   PET_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define PET_ATTRIBUTES \
   {PET_id, "PET_id", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {PET_name, "PET_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_status, "PET_status", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_granted_destin_identifier_list, "PET_granted_destin_identifier_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_pid, "PET_pid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {PET_osjobid, "PET_osjobid", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
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
   {PET_joker, "PET_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

