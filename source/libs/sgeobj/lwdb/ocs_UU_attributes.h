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
 * This code was generated from file source/libs/sgeobj/json/UU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief User
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of UU
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   UU_name = 3550,   ///< User Name
   UU_oticket,   ///< Override Tickets
   UU_fshare,   ///< Functional Shares
   UU_delete_time,   ///< Delete Time
   UU_job_cnt,   ///< Job Count
   UU_pending_job_cnt,   ///< Pending Job Count
   UU_usage,   ///< Usage
   UU_usage_time_stamp,   ///< Usage Time Stamp
   UU_usage_seqno,   ///< Usage Sequence Number
   UU_long_term_usage,   ///< Long Term Usage
   UU_project,   ///< Project
   UU_debited_job_usage,   ///< Debited Job Usage
   UU_default_project,   ///< Default Project
   UU_version,   ///< User Version
   UU_consider_with_categories,   ///< Consider With Categories
   UU_joker   ///< Joker
};

/** @brief The attribute ids of UU, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
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

/** @brief The compile-time description of every attribute of UU
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define UU_ATTRIBUTES \
   {UU_name, "UU_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {UU_oticket, "UU_oticket", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {UU_fshare, "UU_fshare", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {UU_delete_time, "UU_delete_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {UU_job_cnt, "UU_job_cnt", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {UU_pending_job_cnt, "UU_pending_job_cnt", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {UU_usage, "UU_usage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {UU_usage_time_stamp, "UU_usage_time_stamp", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {UU_usage_seqno, "UU_usage_seqno", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {UU_long_term_usage, "UU_long_term_usage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {UU_project, "UU_project", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {UU_debited_job_usage, "UU_debited_job_usage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {UU_default_project, "UU_default_project", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {UU_version, "UU_version", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {UU_consider_with_categories, "UU_consider_with_categories", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {UU_joker, "UU_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

