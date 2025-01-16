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
 * This code was generated from file source/libs/sgeobj/json/TM.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   TM_mday = 6350,
   TM_mon,
   TM_year,
   TM_sec,
   TM_min,
   TM_hour,
   TM_wday,
   TM_yday,
   TM_isdst
};

constexpr const int TM_Type[] = {
   TM_mday,
   TM_mon,
   TM_year,
   TM_sec,
   TM_min,
   TM_hour,
   TM_wday,
   TM_yday,
   TM_isdst,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define TM_ATTRIBUTES \
   {TM_mday, "TM_mday", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TM_mon, "TM_mon", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TM_year, "TM_year", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TM_sec, "TM_sec", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TM_min, "TM_min", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TM_hour, "TM_hour", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TM_wday, "TM_wday", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TM_yday, "TM_yday", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {TM_isdst, "TM_isdst", AttributeStatic::UINT32, AttributeStatic::NO_HASH} \

} // end namespace

