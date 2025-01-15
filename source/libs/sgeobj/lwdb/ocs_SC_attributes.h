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
 * This code was generated from file source/libs/sgeobj/json/SC.json
 * DO NOT CHANGE
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {

enum {
   SC_algorithm = 4350,
   SC_schedule_interval,
   SC_maxujobs,
   SC_queue_sort_method,
   SC_job_load_adjustments,
   SC_load_adjustment_decay_time,
   SC_load_formula,
   SC_schedd_job_info,
   SC_flush_submit_sec,
   SC_flush_finish_sec,
   SC_params,
   SC_reprioritize_interval,
   SC_halftime,
   SC_usage_weight_list,
   SC_compensation_factor,
   SC_weight_user,
   SC_weight_project,
   SC_weight_department,
   SC_weight_job,
   SC_weight_tickets_functional,
   SC_weight_tickets_share,
   SC_weight_tickets_override,
   SC_share_override_tickets,
   SC_share_functional_shares,
   SC_max_functional_jobs_to_schedule,
   SC_report_pjob_tickets,
   SC_max_pending_tasks_per_job,
   SC_halflife_decay_list,
   SC_policy_hierarchy,
   SC_weight_ticket,
   SC_weight_waiting_time,
   SC_weight_deadline,
   SC_weight_urgency,
   SC_weight_priority,
   SC_max_reservation,
   SC_default_duration
};

constexpr const int SC_Type[] = {
   SC_algorithm,
   SC_schedule_interval,
   SC_maxujobs,
   SC_queue_sort_method,
   SC_job_load_adjustments,
   SC_load_adjustment_decay_time,
   SC_load_formula,
   SC_schedd_job_info,
   SC_flush_submit_sec,
   SC_flush_finish_sec,
   SC_params,
   SC_reprioritize_interval,
   SC_halftime,
   SC_usage_weight_list,
   SC_compensation_factor,
   SC_weight_user,
   SC_weight_project,
   SC_weight_department,
   SC_weight_job,
   SC_weight_tickets_functional,
   SC_weight_tickets_share,
   SC_weight_tickets_override,
   SC_share_override_tickets,
   SC_share_functional_shares,
   SC_max_functional_jobs_to_schedule,
   SC_report_pjob_tickets,
   SC_max_pending_tasks_per_job,
   SC_halflife_decay_list,
   SC_policy_hierarchy,
   SC_weight_ticket,
   SC_weight_waiting_time,
   SC_weight_deadline,
   SC_weight_urgency,
   SC_weight_priority,
   SC_max_reservation,
   SC_default_duration,
   AttributeStatic::END_OF_ATTRIBUTES
};

#define SC_ATTRIBUTES \
   {SC_algorithm, "SC_algorithm", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SC_schedule_interval, "SC_schedule_interval", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SC_maxujobs, "SC_maxujobs", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_queue_sort_method, "SC_queue_sort_method", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_job_load_adjustments, "SC_job_load_adjustments", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {SC_load_adjustment_decay_time, "SC_load_adjustment_decay_time", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SC_load_formula, "SC_load_formula", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SC_schedd_job_info, "SC_schedd_job_info", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SC_flush_submit_sec, "SC_flush_submit_sec", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_flush_finish_sec, "SC_flush_finish_sec", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_params, "SC_params", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SC_reprioritize_interval, "SC_reprioritize_interval", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SC_halftime, "SC_halftime", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_usage_weight_list, "SC_usage_weight_list", AttributeStatic::LIST, AttributeStatic::NO_HASH}, \
   {SC_compensation_factor, "SC_compensation_factor", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_weight_user, "SC_weight_user", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_weight_project, "SC_weight_project", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_weight_department, "SC_weight_department", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_weight_job, "SC_weight_job", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_weight_tickets_functional, "SC_weight_tickets_functional", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_weight_tickets_share, "SC_weight_tickets_share", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_weight_tickets_override, "SC_weight_tickets_override", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_share_override_tickets, "SC_share_override_tickets", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {SC_share_functional_shares, "SC_share_functional_shares", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {SC_max_functional_jobs_to_schedule, "SC_max_functional_jobs_to_schedule", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_report_pjob_tickets, "SC_report_pjob_tickets", AttributeStatic::BOOL, AttributeStatic::NO_HASH}, \
   {SC_max_pending_tasks_per_job, "SC_max_pending_tasks_per_job", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_halflife_decay_list, "SC_halflife_decay_list", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SC_policy_hierarchy, "SC_policy_hierarchy", AttributeStatic::STRING, AttributeStatic::NO_HASH}, \
   {SC_weight_ticket, "SC_weight_ticket", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_weight_waiting_time, "SC_weight_waiting_time", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_weight_deadline, "SC_weight_deadline", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_weight_urgency, "SC_weight_urgency", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_weight_priority, "SC_weight_priority", AttributeStatic::DOUBLE, AttributeStatic::NO_HASH}, \
   {SC_max_reservation, "SC_max_reservation", AttributeStatic::UINT32, AttributeStatic::NO_HASH}, \
   {SC_default_duration, "SC_default_duration", AttributeStatic::STRING, AttributeStatic::NO_HASH} \

} // end namespace

