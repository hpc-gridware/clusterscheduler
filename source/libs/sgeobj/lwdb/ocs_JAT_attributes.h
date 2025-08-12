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
 * This code was generated from file source/libs/sgeobj/json/JAT.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   JAT_task_number = 6950,
   JAT_status,
   JAT_start_time,
   JAT_end_time,
   JAT_hold,
   JAT_granted_pe,
   JAT_job_restarted,
   JAT_granted_destin_identifier_list,
   JAT_granted_resources_list,
   JAT_master_queue,
   JAT_state,
   JAT_pvm_ckpt_pid,
   JAT_pending_signal,
   JAT_pending_signal_delivery_time,
   JAT_pid,
   JAT_osjobid,
   JAT_systemd_scope,
   JAT_systemd_slice,
   JAT_usage_collection,
   JAT_usage_list,
   JAT_scaled_usage_list,
   JAT_reported_usage_list,
   JAT_fshare,
   JAT_tix,
   JAT_oticket,
   JAT_fticket,
   JAT_sticket,
   JAT_share,
   JAT_suitable,
   JAT_task_list,
   JAT_finished_task_list,
   JAT_previous_usage_list,
   JAT_pe_object,
   JAT_next_pe_task_id,
   JAT_stop_initiate_time,
   JAT_prio,
   JAT_ntix,
   JAT_wallclock_limit,
   JAT_message_list,
   JAT_joker
};

constexpr const int JAT_Type[] = {
   JAT_task_number,
   JAT_status,
   JAT_start_time,
   JAT_end_time,
   JAT_hold,
   JAT_granted_pe,
   JAT_job_restarted,
   JAT_granted_destin_identifier_list,
   JAT_granted_resources_list,
   JAT_master_queue,
   JAT_state,
   JAT_pvm_ckpt_pid,
   JAT_pending_signal,
   JAT_pending_signal_delivery_time,
   JAT_pid,
   JAT_osjobid,
   JAT_systemd_scope,
   JAT_systemd_slice,
   JAT_usage_collection,
   JAT_usage_list,
   JAT_scaled_usage_list,
   JAT_reported_usage_list,
   JAT_fshare,
   JAT_tix,
   JAT_oticket,
   JAT_fticket,
   JAT_sticket,
   JAT_share,
   JAT_suitable,
   JAT_task_list,
   JAT_finished_task_list,
   JAT_previous_usage_list,
   JAT_pe_object,
   JAT_next_pe_task_id,
   JAT_stop_initiate_time,
   JAT_prio,
   JAT_ntix,
   JAT_wallclock_limit,
   JAT_message_list,
   JAT_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define JAT_ATTRIBUTES \
   {JAT_task_number, "JAT_task_number", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, false}, \
   {JAT_status, "JAT_status", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_start_time, "JAT_start_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_end_time, "JAT_end_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_hold, "JAT_hold", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_granted_pe, "JAT_granted_pe", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_job_restarted, "JAT_job_restarted", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_granted_destin_identifier_list, "JAT_granted_destin_identifier_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_granted_resources_list, "JAT_granted_resources_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JAT_master_queue, "JAT_master_queue", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_state, "JAT_state", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_pvm_ckpt_pid, "JAT_pvm_ckpt_pid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_pending_signal, "JAT_pending_signal", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_pending_signal_delivery_time, "JAT_pending_signal_delivery_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_pid, "JAT_pid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_osjobid, "JAT_osjobid", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_systemd_scope, "JAT_systemd_scope", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_systemd_slice, "JAT_systemd_slice", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_usage_collection, "JAT_usage_collection", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_usage_list, "JAT_usage_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_scaled_usage_list, "JAT_scaled_usage_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_reported_usage_list, "JAT_reported_usage_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_fshare, "JAT_fshare", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_tix, "JAT_tix", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_oticket, "JAT_oticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_fticket, "JAT_fticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_sticket, "JAT_sticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_share, "JAT_share", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_suitable, "JAT_suitable", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_task_list, "JAT_task_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_finished_task_list, "JAT_finished_task_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_previous_usage_list, "JAT_previous_usage_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_pe_object, "JAT_pe_object", AttributeStatic::OBJECT, nullptr, 0, AttributeStatic::NO_HASH, false, false}, \
   {JAT_next_pe_task_id, "JAT_next_pe_task_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_stop_initiate_time, "JAT_stop_initiate_time", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_prio, "JAT_prio", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_ntix, "JAT_ntix", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {JAT_wallclock_limit, "JAT_wallclock_limit", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JAT_message_list, "JAT_message_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {JAT_joker, "JAT_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

