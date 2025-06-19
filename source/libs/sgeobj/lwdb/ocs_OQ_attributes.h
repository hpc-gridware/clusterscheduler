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
 * This code was generated from file source/libs/sgeobj/json/OQ.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   OQ_slots = 1650,
   OQ_dest_queue,
   OQ_dest_version,
   OQ_ticket,
   OQ_oticket,
   OQ_fticket,
   OQ_sticket,
   OQ_binding_to_use
};

constexpr const int OQ_Type[] = {
   OQ_slots,
   OQ_dest_queue,
   OQ_dest_version,
   OQ_ticket,
   OQ_oticket,
   OQ_fticket,
   OQ_sticket,
   OQ_binding_to_use,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define OQ_ATTRIBUTES \
   {OQ_slots, "OQ_slots", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {OQ_dest_queue, "OQ_dest_queue", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {OQ_dest_version, "OQ_dest_version", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {OQ_ticket, "OQ_ticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {OQ_oticket, "OQ_oticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {OQ_fticket, "OQ_fticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {OQ_sticket, "OQ_sticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {OQ_binding_to_use, "OQ_binding_to_use", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

