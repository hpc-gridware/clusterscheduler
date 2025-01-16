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
 * This code was generated from file source/libs/sgeobj/json/RTIC.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   RTIC_host = 12900,
   RTIC_tickets
};

constexpr const int RTIC_Type[] = {
   RTIC_host,
   RTIC_tickets,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define RTIC_ATTRIBUTES \
   {RTIC_host, "RTIC_host", AttributeStatic::HOST, AttributeStatic::UNORDERED_UNIQUE}, \
   {RTIC_tickets, "RTIC_tickets", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

