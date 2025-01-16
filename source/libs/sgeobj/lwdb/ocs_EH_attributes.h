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
   EH_joker
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
   AttributeStatic::END_OF_ATTRIBUTES
};

#define EH_ATTRIBUTES \
   {EH_name, "EH_name", AttributeStatic::HOST, AttributeStatic::UNORDERED_UNIQUE}, \
   {EH_scaling_list, "EH_scaling_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_consumable_config_list, "EH_consumable_config_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_usage_scaling_list, "EH_usage_scaling_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_load_list, "EH_load_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_lt_heard_from, "EH_lt_heard_from", AttributeStatic::UINT64, AttributeStatic::NO_HASH}, \
   {EH_processors, "EH_processors", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_acl, "EH_acl", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_xacl, "EH_xacl", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_prj, "EH_prj", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_xprj, "EH_xprj", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_sort_value, "EH_sort_value", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {EH_reuse_me, "EH_reuse_me", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_tagged, "EH_tagged", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_load_correction_factor, "EH_load_correction_factor", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_seq_no, "EH_seq_no", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_real_name, "EH_real_name", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {EH_sge_load, "EH_sge_load", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_sge_ticket_pct, "EH_sge_ticket_pct", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {EH_sge_load_pct, "EH_sge_load_pct", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {EH_featureset_id, "EH_featureset_id", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_scaled_usage_list, "EH_scaled_usage_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_scaled_usage_pct_list, "EH_scaled_usage_pct_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_num_running_jobs, "EH_num_running_jobs", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_load_report_interval, "EH_load_report_interval", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_resource_utilization, "EH_resource_utilization", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_cached_complexes, "EH_cached_complexes", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_cache_version, "EH_cache_version", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_master_host, "EH_master_host", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_reschedule_unknown, "EH_reschedule_unknown", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_reschedule_unknown_list, "EH_reschedule_unknown_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_report_seqno, "EH_report_seqno", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {EH_report_variables, "EH_report_variables", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_merged_report_variables, "EH_merged_report_variables", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {EH_joker, "EH_joker", AttributeStatic::LIST, AttributeStatic::NO_HASH} \

} // end namespace

