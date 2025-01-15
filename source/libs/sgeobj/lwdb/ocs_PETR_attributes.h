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
 * This code was generated from file source/libs/sgeobj/json/PETR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   PETR_jobid = 4550,
   PETR_jataskid,
   PETR_queuename,
   PETR_owner,
   PETR_cwd,
   PETR_path_aliases,
   PETR_environment,
   PETR_submission_time
};

constexpr const int PETR_Type[] = {
   PETR_jobid,
   PETR_jataskid,
   PETR_queuename,
   PETR_owner,
   PETR_cwd,
   PETR_path_aliases,
   PETR_environment,
   PETR_submission_time,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define PETR_ATTRIBUTES \
   {PETR_jobid, "PETR_jobid", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {PETR_jataskid, "PETR_jataskid", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {PETR_queuename, "PETR_queuename", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {PETR_owner, "PETR_owner", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {PETR_cwd, "PETR_cwd", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {PETR_path_aliases, "PETR_path_aliases", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {PETR_environment, "PETR_environment", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {PETR_submission_time, "PETR_submission_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH} \

} // end namespace

