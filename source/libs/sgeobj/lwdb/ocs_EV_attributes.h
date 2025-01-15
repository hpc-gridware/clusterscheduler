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
   {EV_id, "EV_id", AttributeStatic::UINT32, AttributeStatic::UNORDERED_UNIQUE}, \
   {EV_name, "EV_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {EV_host, "EV_host", AttributeStatic::HOST, AttributeStatic::NO_HASH}, \
   {EV_commproc, "EV_commproc", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {EV_commid, "EV_commid", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EV_uid, "EV_uid", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EV_d_time, "EV_d_time", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EV_flush_delay, "EV_flush_delay", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EV_subscribed, "EV_subscribed", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EV_changed, "EV_changed", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {EV_busy_handling, "EV_busy_handling", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EV_session, "EV_session", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {EV_last_heard_from, "EV_last_heard_from", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {EV_last_send_time, "EV_last_send_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {EV_next_send_time, "EV_next_send_time", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {EV_next_number, "EV_next_number", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EV_busy, "EV_busy", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EV_events, "EV_events", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EV_sub_array, "EV_sub_array", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {EV_state, "EV_state", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EV_update_function, "EV_update_function", AttributeStatic::REF, AttributeStatic::NO_HASH}, \
   {EV_update_function_arg, "EV_update_function_arg", AttributeStatic::REF, AttributeStatic::NO_HASH} \

} // end namespace

