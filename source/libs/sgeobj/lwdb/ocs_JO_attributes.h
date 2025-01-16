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
 * This code was generated from file source/libs/sgeobj/json/JO.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   JO_OS_job_ID = 5150,
   JO_OS_job_ID2,
   JO_ja_task_ID,
   JO_task_id_str,
   JO_state,
   JO_usage_list,
   JO_pid_list
};

constexpr const int JO_Type[] = {
   JO_OS_job_ID,
   JO_OS_job_ID2,
   JO_ja_task_ID,
   JO_task_id_str,
   JO_state,
   JO_usage_list,
   JO_pid_list,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define JO_ATTRIBUTES \
   {JO_OS_job_ID, "JO_OS_job_ID", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JO_OS_job_ID2, "JO_OS_job_ID2", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JO_ja_task_ID, "JO_ja_task_ID", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JO_task_id_str, "JO_task_id_str", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {JO_state, "JO_state", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JO_usage_list, "JO_usage_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JO_pid_list, "JO_pid_list", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

