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
 * This code was generated from file source/libs/sgeobj/json/ET.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Event Type
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of ET
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   ET_unique_id = 850,   ///< unique ID for an event within qmaster.
   ET_number,   ///< serial number
   ET_timestamp,   ///< event generation time
   ET_type,   ///< event type
   ET_intkey,   ///< first integer key
   ET_intkey2,   ///< second integer key
   ET_strkey,   ///< first string key
   ET_strkey2,   ///< second string key
   ET_new_version   ///< new version of the data
};

/** @brief The attribute ids of ET, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int ET_Type[] = {
   ET_unique_id,
   ET_number,
   ET_timestamp,
   ET_type,
   ET_intkey,
   ET_intkey2,
   ET_strkey,
   ET_strkey2,
   ET_new_version,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of ET
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define ET_ATTRIBUTES \
   {ET_unique_id, "ET_unique_id", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, true, false}, \
   {ET_number, "ET_number", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {ET_timestamp, "ET_timestamp", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {ET_type, "ET_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {ET_intkey, "ET_intkey", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {ET_intkey2, "ET_intkey2", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {ET_strkey, "ET_strkey", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {ET_strkey2, "ET_strkey2", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {ET_new_version, "ET_new_version", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

