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
 * This code was generated from file source/libs/sgeobj/json/QETI.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   QETI_total = 11200,
   QETI_resource_instance,
   QETI_queue_end_next
};

constexpr const int QETI_Type[] = {
   QETI_total,
   QETI_resource_instance,
   QETI_queue_end_next,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define QETI_ATTRIBUTES \
   {QETI_total, "QETI_total", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {QETI_resource_instance, "QETI_resource_instance", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {QETI_queue_end_next, "QETI_queue_end_next", AttributeStatic::REF, AttributeStatic::NO_HASH} \

} // end namespace

