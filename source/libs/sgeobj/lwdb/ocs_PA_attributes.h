#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/PA.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   PA_origin = 5550,
   PA_submit_host,
   PA_exec_host,
   PA_translation
};

constexpr const int PA_Type[] = {
   PA_origin,
   PA_submit_host,
   PA_exec_host,
   PA_translation,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define PA_ATTRIBUTES \
   {PA_origin, "PA_origin", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {PA_submit_host, "PA_submit_host", AttributeStatic::HOST, AttributeStatic::NO_HASH}, \
   {PA_exec_host, "PA_exec_host", AttributeStatic::HOST, AttributeStatic::NO_HASH}, \
   {PA_translation, "PA_translation", AttributeStatic::STRING, AttributeStatic::NO_HASH} \

} // end namespace

