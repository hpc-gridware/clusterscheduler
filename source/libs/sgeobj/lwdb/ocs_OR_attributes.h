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
 * This code was generated from file source/libs/sgeobj/json/OR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   OR_type = 1550,
   OR_job_number,
   OR_ja_task_number,
   OR_job_version,
   OR_queuelist,
   OR_granted_resources_list,
   OR_ticket,
   OR_joker,
   OR_pe,
   OR_ntix,
   OR_prio
};

constexpr const int OR_Type[] = {
   OR_type,
   OR_job_number,
   OR_ja_task_number,
   OR_job_version,
   OR_queuelist,
   OR_granted_resources_list,
   OR_ticket,
   OR_joker,
   OR_pe,
   OR_ntix,
   OR_prio,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define OR_ATTRIBUTES \
   {OR_type, "OR_type", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {OR_job_number, "OR_job_number", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {OR_ja_task_number, "OR_ja_task_number", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {OR_job_version, "OR_job_version", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {OR_queuelist, "OR_queuelist", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {OR_granted_resources_list, "OR_granted_resources_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {OR_ticket, "OR_ticket", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {OR_joker, "OR_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {OR_pe, "OR_pe", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {OR_ntix, "OR_ntix", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {OR_prio, "OR_prio", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH} \

} // end namespace

