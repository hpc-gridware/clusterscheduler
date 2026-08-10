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
 * This code was generated from file source/libs/sgeobj/json/US.json
 * DO NOT CHANGE
 */

/** @file
 * @brief User Set
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of US
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   US_name = 1550,   ///< Name
   US_type,   ///< Type
   US_fshare,   ///< Functional Share
   US_oticket,   ///< Override Tickets
   US_job_cnt,   ///< Running Job Count
   US_pending_job_cnt,   ///< Pending Job Count
   US_entries,   ///< Members
   US_consider_with_categories,   ///< Category Relevant
   US_joker   ///< Joker
};

/** @brief The attribute ids of US, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
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

/** @brief The compile-time description of every attribute of US
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define US_ATTRIBUTES \
   {US_name, "US_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {US_type, "US_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {US_fshare, "US_fshare", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {US_oticket, "US_oticket", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {US_job_cnt, "US_job_cnt", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {US_pending_job_cnt, "US_pending_job_cnt", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {US_entries, "US_entries", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {US_consider_with_categories, "US_consider_with_categories", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {US_joker, "US_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

