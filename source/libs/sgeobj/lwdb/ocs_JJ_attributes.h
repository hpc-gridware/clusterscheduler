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
 * This code was generated from file source/libs/sgeobj/json/JJ.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   JJ_jobid = 8450,
   JJ_type,
   JJ_finished_tasks,
   JJ_not_yet_finished_ids,
   JJ_started_task_ids
};

constexpr const int JJ_Type[] = {
   JJ_jobid,
   JJ_type,
   JJ_finished_tasks,
   JJ_not_yet_finished_ids,
   JJ_started_task_ids,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define JJ_ATTRIBUTES \
   {JJ_jobid, "JJ_jobid", AttributeStatic::UINT32, AttributeStatic::UNORDERED_UNIQUE}, \
   {JJ_type, "JJ_type", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {JJ_finished_tasks, "JJ_finished_tasks", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JJ_not_yet_finished_ids, "JJ_not_yet_finished_ids", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {JJ_started_task_ids, "JJ_started_task_ids", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

