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
 * This code was generated from file source/libs/sgeobj/json/UU.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   UU_name = 3950,
   UU_oticket,
   UU_fshare,
   UU_delete_time,
   UU_job_cnt,
   UU_pending_job_cnt,
   UU_usage,
   UU_usage_time_stamp,
   UU_usage_seqno,
   UU_long_term_usage,
   UU_project,
   UU_debited_job_usage,
   UU_default_project,
   UU_version,
   UU_consider_with_categories,
   UU_joker
};

constexpr const int UU_Type[] = {
   UU_name,
   UU_oticket,
   UU_fshare,
   UU_delete_time,
   UU_job_cnt,
   UU_pending_job_cnt,
   UU_usage,
   UU_usage_time_stamp,
   UU_usage_seqno,
   UU_long_term_usage,
   UU_project,
   UU_debited_job_usage,
   UU_default_project,
   UU_version,
   UU_consider_with_categories,
   UU_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define UU_ATTRIBUTES \
   {UU_name, "UU_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {UU_oticket, "UU_oticket", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {UU_fshare, "UU_fshare", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {UU_delete_time, "UU_delete_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {UU_job_cnt, "UU_job_cnt", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {UU_pending_job_cnt, "UU_pending_job_cnt", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {UU_usage, "UU_usage", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {UU_usage_time_stamp, "UU_usage_time_stamp", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {UU_usage_seqno, "UU_usage_seqno", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {UU_long_term_usage, "UU_long_term_usage", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {UU_project, "UU_project", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {UU_debited_job_usage, "UU_debited_job_usage", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {UU_default_project, "UU_default_project", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {UU_version, "UU_version", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {UU_consider_with_categories, "UU_consider_with_categories", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {UU_joker, "UU_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

