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
 * This code was generated from file source/libs/sgeobj/json/RQRF.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Rule Filter
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of RQRF
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   RQRF_expand = 11600,   ///< Expand
   RQRF_scope,   ///< Scope
   RQRF_xscope   ///< Excluded Scope
};

/** @brief The attribute ids of RQRF, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
constexpr const int RQRF_Type[] = {
   RQRF_expand,
   RQRF_scope,
   RQRF_xscope,
   AttributeStatic::END_OF_ATTRIBUTES
};

/** @brief The compile-time description of every attribute of RQRF
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define RQRF_ATTRIBUTES \
   {RQRF_expand, "RQRF_expand", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RQRF_scope, "RQRF_scope", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {RQRF_xscope, "RQRF_xscope", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

