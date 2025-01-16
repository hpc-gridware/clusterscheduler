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
 * This code was generated from file source/libs/sgeobj/json/EVR.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   EVR_operation = 12700,
   EVR_timestamp,
   EVR_event_client_id,
   EVR_event_number,
   EVR_session,
   EVR_event_client,
   EVR_event_list
};

constexpr const int EVR_Type[] = {
   EVR_operation,
   EVR_timestamp,
   EVR_event_client_id,
   EVR_event_number,
   EVR_session,
   EVR_event_client,
   EVR_event_list,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define EVR_ATTRIBUTES \
   {EVR_operation, "EVR_operation", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EVR_timestamp, "EVR_timestamp", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {EVR_event_client_id, "EVR_event_client_id", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EVR_event_number, "EVR_event_number", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EVR_session, "EVR_session", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {EVR_event_client, "EVR_event_client", AttributeStatic::OBJECT, AttributeStatic::NO_HASH}, \
   {EVR_event_list, "EVR_event_list", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

