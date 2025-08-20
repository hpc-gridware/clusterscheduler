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
 * This code was generated from file source/libs/sgeobj/json/EH.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   EH_name = 450,
   EH_scaling_list,
   EH_consumable_config_list,
   EH_usage_scaling_list,
   EH_load_list,
   EH_lt_heard_from,
   EH_processors,
   EH_acl,
   EH_xacl,
   EH_prj,
   EH_xprj,
   EH_sort_value,
   EH_reuse_me,
   EH_tagged,
   EH_load_correction_factor,
   EH_seq_no,
   EH_real_name,
   EH_sge_load,
   EH_sge_ticket_pct,
   EH_sge_load_pct,
   EH_featureset_id,
   EH_scaled_usage_list,
   EH_scaled_usage_pct_list,
   EH_num_running_jobs,
   EH_load_report_interval,
   EH_resource_utilization,
   EH_cached_complexes,
   EH_cache_version,
   EH_master_host,
   EH_reschedule_unknown,
   EH_reschedule_unknown_list,
   EH_report_seqno,
   EH_report_variables,
   EH_merged_report_variables,
   EH_joker,
   EH_internal_topology
};

constexpr const int EH_Type[] = {
   EH_name,
   EH_scaling_list,
   EH_consumable_config_list,
   EH_usage_scaling_list,
   EH_load_list,
   EH_lt_heard_from,
   EH_processors,
   EH_acl,
   EH_xacl,
   EH_prj,
   EH_xprj,
   EH_sort_value,
   EH_reuse_me,
   EH_tagged,
   EH_load_correction_factor,
   EH_seq_no,
   EH_real_name,
   EH_sge_load,
   EH_sge_ticket_pct,
   EH_sge_load_pct,
   EH_featureset_id,
   EH_scaled_usage_list,
   EH_scaled_usage_pct_list,
   EH_num_running_jobs,
   EH_load_report_interval,
   EH_resource_utilization,
   EH_cached_complexes,
   EH_cache_version,
   EH_master_host,
   EH_reschedule_unknown,
   EH_reschedule_unknown_list,
   EH_report_seqno,
   EH_report_variables,
   EH_merged_report_variables,
   EH_joker,
   EH_internal_topology,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define EH_ATTRIBUTES \
   {EH_name, "EH_name", AttributeStatic::HOST, nullptr, AttributeStatic::NO_POS, AttributeStatic::UNORDERED_UNIQUE, true, true}, \
   {EH_scaling_list, "EH_scaling_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_consumable_config_list, "EH_consumable_config_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_usage_scaling_list, "EH_usage_scaling_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_load_list, "EH_load_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_lt_heard_from, "EH_lt_heard_from", AttributeStatic::UINT64, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_processors, "EH_processors", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_acl, "EH_acl", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_xacl, "EH_xacl", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_prj, "EH_prj", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_xprj, "EH_xprj", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_sort_value, "EH_sort_value", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_reuse_me, "EH_reuse_me", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_tagged, "EH_tagged", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_load_correction_factor, "EH_load_correction_factor", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_seq_no, "EH_seq_no", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_real_name, "EH_real_name", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_sge_load, "EH_sge_load", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_sge_ticket_pct, "EH_sge_ticket_pct", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_sge_load_pct, "EH_sge_load_pct", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_featureset_id, "EH_featureset_id", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_scaled_usage_list, "EH_scaled_usage_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_scaled_usage_pct_list, "EH_scaled_usage_pct_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_num_running_jobs, "EH_num_running_jobs", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_load_report_interval, "EH_load_report_interval", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_resource_utilization, "EH_resource_utilization", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_cached_complexes, "EH_cached_complexes", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_cache_version, "EH_cache_version", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_master_host, "EH_master_host", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_reschedule_unknown, "EH_reschedule_unknown", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_reschedule_unknown_list, "EH_reschedule_unknown_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_report_seqno, "EH_report_seqno", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_report_variables, "EH_report_variables", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_merged_report_variables, "EH_merged_report_variables", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false}, \
   {EH_joker, "EH_joker", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {EH_internal_topology, "EH_internal_topology", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, false} \

} // end namespace

