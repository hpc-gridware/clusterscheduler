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
 * This code was generated from file source/libs/sgeobj/json/RQRF.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   RQRF_expand = 12100,
   RQRF_scope,
   RQRF_xscope
};

constexpr const int RQRF_Type[] = {
   RQRF_expand,
   RQRF_scope,
   RQRF_xscope,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RQRF_ATTRIBUTES \
   {RQRF_expand, "RQRF_expand", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {RQRF_scope, "RQRF_scope", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {RQRF_xscope, "RQRF_xscope", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

