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
 * This code was generated from file source/libs/sgeobj/json/CCT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Category Cache Entry
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of CCT
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   CCT_pe_name = 11000,   ///< Parallel Environment
   CCT_ignore_queues,   ///< Rejected Queues
   CCT_ignore_hosts,   ///< Rejected Hosts
   CCT_job_messages,   ///< Scheduler Messages
   CCT_pe_job_slots,   ///< Possible Slot Counts
   CCT_pe_job_slot_count   ///< Slot Count Size
};

/** @brief The attribute ids of CCT, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int CCT_Type[] = {
   CCT_pe_name,
   CCT_ignore_queues,
   CCT_ignore_hosts,
   CCT_job_messages,
   CCT_pe_job_slots,
   CCT_pe_job_slot_count,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of CCT
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define CCT_ATTRIBUTES \
   {CCT_pe_name, "CCT_pe_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_ignore_queues, "CCT_ignore_queues", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_ignore_hosts, "CCT_ignore_hosts", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_job_messages, "CCT_job_messages", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_pe_job_slots, "CCT_pe_job_slots", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CCT_pe_job_slot_count, "CCT_pe_job_slot_count", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

