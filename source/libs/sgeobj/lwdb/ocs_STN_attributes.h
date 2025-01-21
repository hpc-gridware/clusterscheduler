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
 * This code was generated from file source/libs/sgeobj/json/STN.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   STN_name = 4250,
   STN_type,
   STN_id,
   STN_shares,
   STN_children,
   STN_job_ref_count,
   STN_active_job_ref_count,
   STN_project,
   STN_proportion,
   STN_adjusted_proportion,
   STN_combined_usage,
   STN_pass2_seqno,
   STN_sum_priority,
   STN_actual_proportion,
   STN_m_share,
   STN_last_actual_proportion,
   STN_adjusted_current_proportion,
   STN_temp,
   STN_stt,
   STN_ostt,
   STN_ltt,
   STN_oltt,
   STN_shr,
   STN_sort,
   STN_ref,
   STN_tickets,
   STN_jobid,
   STN_taskid,
   STN_usage_list,
   STN_version,
   STN_joker
};

constexpr const int STN_Type[] = {
   STN_name,
   STN_type,
   STN_id,
   STN_shares,
   STN_children,
   STN_job_ref_count,
   STN_active_job_ref_count,
   STN_project,
   STN_proportion,
   STN_adjusted_proportion,
   STN_combined_usage,
   STN_pass2_seqno,
   STN_sum_priority,
   STN_actual_proportion,
   STN_m_share,
   STN_last_actual_proportion,
   STN_adjusted_current_proportion,
   STN_temp,
   STN_stt,
   STN_ostt,
   STN_ltt,
   STN_oltt,
   STN_shr,
   STN_sort,
   STN_ref,
   STN_tickets,
   STN_jobid,
   STN_taskid,
   STN_usage_list,
   STN_version,
   STN_joker,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define STN_ATTRIBUTES \
   {STN_name, "STN_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {STN_type, "STN_type", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {STN_id, "STN_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_shares, "STN_shares", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {STN_children, "STN_children", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {STN_job_ref_count, "STN_job_ref_count", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_active_job_ref_count, "STN_active_job_ref_count", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_project, "STN_project", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_proportion, "STN_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_adjusted_proportion, "STN_adjusted_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_combined_usage, "STN_combined_usage", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_pass2_seqno, "STN_pass2_seqno", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_sum_priority, "STN_sum_priority", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_actual_proportion, "STN_actual_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_m_share, "STN_m_share", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_last_actual_proportion, "STN_last_actual_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_adjusted_current_proportion, "STN_adjusted_current_proportion", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_temp, "STN_temp", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_stt, "STN_stt", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_ostt, "STN_ostt", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_ltt, "STN_ltt", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_oltt, "STN_oltt", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_shr, "STN_shr", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_sort, "STN_sort", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_ref, "STN_ref", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_tickets, "STN_tickets", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_jobid, "STN_jobid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_taskid, "STN_taskid", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_usage_list, "STN_usage_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_version, "STN_version", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {STN_joker, "STN_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

