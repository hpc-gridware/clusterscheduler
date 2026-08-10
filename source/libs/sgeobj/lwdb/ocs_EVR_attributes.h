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
 * This code was generated from file source/libs/sgeobj/json/EVR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief @todo add summary
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of EVR
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   EVR_operation = 12200,   ///< @todo add summary
   EVR_timestamp,   ///< @todo add summary
   EVR_event_client_id,   ///< @todo add summary
   EVR_event_number,   ///< @todo add summary
   EVR_session,   ///< @todo add summary
   EVR_event_client,   ///< @todo add summary
   EVR_event_list   ///< @todo add summary
};

/** @brief The attribute ids of EVR, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int EVR_Type[] = {
   EVR_operation,
   EVR_timestamp,
   EVR_event_client_id,
   EVR_event_number,
   EVR_session,
   EVR_event_client,
   EVR_event_list,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of EVR
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define EVR_ATTRIBUTES \
   {EVR_operation, "EVR_operation", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EVR_timestamp, "EVR_timestamp", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EVR_event_client_id, "EVR_event_client_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EVR_event_number, "EVR_event_number", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EVR_session, "EVR_session", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EVR_event_client, "EVR_event_client", AttributeStatic::OBJECT, nullptr, 0, AttributeStatic::NO_HASH, false, false}, \
   {EVR_event_list, "EVR_event_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

