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
 * This code was generated from file source/libs/sgeobj/json/JJAT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief JAPI Array Task
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of JJAT
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   JJAT_task_id = 8050,   ///< Task Id
   JJAT_stat,   ///< Stat
   JJAT_rusage,   ///< RUsage
   JJAT_failed_text   ///< Failed Text
};

/** @brief The attribute ids of JJAT, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int JJAT_Type[] = {
   JJAT_task_id,
   JJAT_stat,
   JJAT_rusage,
   JJAT_failed_text,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of JJAT
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define JJAT_ATTRIBUTES \
   {JJAT_task_id, "JJAT_task_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JJAT_stat, "JJAT_stat", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JJAT_rusage, "JJAT_rusage", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JJAT_failed_text, "JJAT_failed_text", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

