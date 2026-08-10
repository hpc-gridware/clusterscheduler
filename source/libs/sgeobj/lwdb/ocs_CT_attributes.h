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
 * This code was generated from file source/libs/sgeobj/json/CT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Category
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of CT
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   CT_id = 6650,   ///< Category ID
   CT_str,   ///< Category String
   CT_refcount,   ///< Reference Count
   CT_rejected,   ///< Rejected
   CT_cache,   ///< Cache
   CT_messages_added,   ///< Messages Added
   CT_resource_contribution,   ///< Resource Contribution
   CT_rc_valid,   ///< Resource Contribution valid
   CT_reservation_rejected   ///< Reservation Rejected
};

/** @brief The attribute ids of CT, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int CT_Type[] = {
   CT_id,
   CT_str,
   CT_refcount,
   CT_rejected,
   CT_cache,
   CT_messages_added,
   CT_resource_contribution,
   CT_rc_valid,
   CT_reservation_rejected,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of CT
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define CT_ATTRIBUTES \
   {CT_id, "CT_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, false}, \
   {CT_str, "CT_str", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, false, false}, \
   {CT_refcount, "CT_refcount", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CT_rejected, "CT_rejected", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CT_cache, "CT_cache", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CT_messages_added, "CT_messages_added", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CT_resource_contribution, "CT_resource_contribution", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CT_rc_valid, "CT_rc_valid", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {CT_reservation_rejected, "CT_reservation_rejected", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

