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
 * This code was generated from file source/libs/sgeobj/json/TE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Time Event
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of TE
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   TE_when = 5550,   ///< When
   TE_type,   ///< Type
   TE_mode,   ///< Mode
   TE_interval,   ///< Interval
   TE_uval0,   ///< UVal0
   TE_uval1,   ///< UVal1
   TE_sval,   ///< String Key
   TE_seqno   ///< Sequence Number
};

/** @brief The attribute ids of TE, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int TE_Type[] = {
   TE_when,
   TE_type,
   TE_mode,
   TE_interval,
   TE_uval0,
   TE_uval1,
   TE_sval,
   TE_seqno,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of TE
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define TE_ATTRIBUTES \
   {TE_when, "TE_when", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {TE_type, "TE_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {TE_mode, "TE_mode", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {TE_interval, "TE_interval", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {TE_uval0, "TE_uval0", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {TE_uval1, "TE_uval1", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {TE_sval, "TE_sval", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {TE_seqno, "TE_seqno", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

