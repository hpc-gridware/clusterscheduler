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
 * This code was generated from file source/libs/sgeobj/json/AN.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   AN_status = 750,
   AN_text,
   AN_quality
};

constexpr const int AN_Type[] = {
   AN_status,
   AN_text,
   AN_quality,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define AN_ATTRIBUTES \
   {AN_status, "AN_status", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {AN_text, "AN_text", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {AN_quality, "AN_quality", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

