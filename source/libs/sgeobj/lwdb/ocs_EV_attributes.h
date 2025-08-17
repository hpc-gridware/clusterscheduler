#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/EV.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   EV_id = 1150,
   EV_name,
   EV_host,
   EV_commproc,
   EV_commid,
   EV_uid,
   EV_d_time,
   EV_flush_delay,
   EV_subscribed,
   EV_changed,
   EV_busy_handling,
   EV_session,
   EV_last_heard_from,
   EV_last_send_time,
   EV_next_send_time,
   EV_next_number,
   EV_busy,
   EV_events,
   EV_sub_array,
   EV_state,
   EV_update_function,
   EV_update_function_arg
};

constexpr const int EV_Type[] = {
   EV_id,
   EV_name,
   EV_host,
   EV_commproc,
   EV_commid,
   EV_uid,
   EV_d_time,
   EV_flush_delay,
   EV_subscribed,
   EV_changed,
   EV_busy_handling,
   EV_session,
   EV_last_heard_from,
   EV_last_send_time,
   EV_next_send_time,
   EV_next_number,
   EV_busy,
   EV_events,
   EV_sub_array,
   EV_state,
   EV_update_function,
   EV_update_function_arg,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define EV_ATTRIBUTES \
   {EV_id, "EV_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {EV_name, "EV_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_host, "EV_host", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_commproc, "EV_commproc", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_commid, "EV_commid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_uid, "EV_uid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_d_time, "EV_d_time", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_flush_delay, "EV_flush_delay", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_subscribed, "EV_subscribed", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_changed, "EV_changed", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_busy_handling, "EV_busy_handling", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_session, "EV_session", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_last_heard_from, "EV_last_heard_from", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_last_send_time, "EV_last_send_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_next_send_time, "EV_next_send_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_next_number, "EV_next_number", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_busy, "EV_busy", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_events, "EV_events", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_sub_array, "EV_sub_array", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_state, "EV_state", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_update_function, "EV_update_function", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EV_update_function_arg, "EV_update_function_arg", AttributeStatic::REF, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

