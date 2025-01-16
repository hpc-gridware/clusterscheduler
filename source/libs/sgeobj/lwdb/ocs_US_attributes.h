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
 * This code was generated from file source/libs/sgeobj/json/US.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   US_name = 1750,
   US_type,
   US_fshare,
   US_oticket,
   US_job_cnt,
   US_pending_job_cnt,
   US_entries,
   US_consider_with_categories,
   US_joker
};

constexpr const int US_Type[] = {
   US_name,
   US_type,
   US_fshare,
   US_oticket,
   US_job_cnt,
   US_pending_job_cnt,
   US_entries,
   US_consider_with_categories,
   US_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define US_ATTRIBUTES \
   {US_name, "US_name", AttributeStatic::STRING, AttributeStatic::UNORDERED_UNIQUE}, \
   {US_type, "US_type", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {US_fshare, "US_fshare", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {US_oticket, "US_oticket", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {US_job_cnt, "US_job_cnt", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {US_pending_job_cnt, "US_pending_job_cnt", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {US_entries, "US_entries", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {US_consider_with_categories, "US_consider_with_categories", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {US_joker, "US_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

