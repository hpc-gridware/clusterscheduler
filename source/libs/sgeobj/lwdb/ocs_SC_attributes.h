#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/** @file
 * @brief Object containing scheduler configuration attributes
 */

#include "lwdb/AttributeStatic.h"

namespace ocs {


/** @brief The attributes of SC
 *
 * Generated from the same JSON as the CULL type, so the two agree by
 * construction rather than by review.
 */
enum {
   SC_algorithm = 3950,   ///< Scheduler Algorithm
   SC_schedule_interval,   ///< Scheduling Interval
   SC_maxujobs,   ///< Maximum User Jobs
   SC_queue_sort_method,   ///< Queue Sort Method
   SC_job_load_adjustments,   ///< Job Load Adjustments
   SC_load_adjustment_decay_time,   ///< Load Adjustment Decay Time
   SC_load_formula,   ///< Load Formula
   SC_schedd_job_info,   ///< Schedd Job Info
   SC_flush_submit_sec,   ///< Flush Submit Sec
   SC_flush_finish_sec,   ///< Flush Finish Sec
   SC_params,   ///< Params
   SC_reprioritize_interval,   ///< Reprioritize Interval
   SC_halftime,   ///< Halftime
   SC_usage_weight_list,   ///< Usage Weight List
   SC_compensation_factor,   ///< Compensation Factor
   SC_weight_user,   ///< Weight User
   SC_weight_project,   ///< Weight Project
   SC_weight_department,   ///< Weight Department
   SC_weight_job,   ///< Weight Job
   SC_weight_tickets_functional,   ///< Weight Tickets Functional
   SC_weight_tickets_share,   ///< Weight Tickets Share
   SC_weight_tickets_override,   ///< Weight Tickets Override
   SC_share_override_tickets,   ///< Share Override Tickets
   SC_share_functional_shares,   ///< Share Functional Shares
   SC_max_functional_jobs_to_schedule,   ///< Max Functional Jobs to Schedule
   SC_report_pjob_tickets,   ///< Report Pending Job Tickets
   SC_max_pending_tasks_per_job,   ///< Maximum Pending Tasks Per Job
   SC_halflife_decay_list,   ///< Halflife Decay List
   SC_policy_hierarchy,   ///< Policy Hierarchy
   SC_weight_ticket,   ///< Weight Ticket
   SC_weight_waiting_time,   ///< Weight Waiting Time
   SC_weight_deadline,   ///< Weight Deadline
   SC_weight_urgency,   ///< Weight Urgency
   SC_weight_priority,   ///< Weight Priority
   SC_max_reservation,   ///< Maximum Number of Reservations
   SC_default_duration   ///< Default Duration
};

/** @brief The attribute ids of SC, terminated by ocs::AttributeStatic::END_OF_ATTRIBUTES
 *
 * This is the array an ocs::Element is instantiated with.
 */
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

/** @brief The compile-time description of every attribute of SC
 *
 * One ocs::AttributeStatic per attribute, in id order.
 */
#define SC_ATTRIBUTES \
   {SC_algorithm, "SC_algorithm", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_schedule_interval, "SC_schedule_interval", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_maxujobs, "SC_maxujobs", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_queue_sort_method, "SC_queue_sort_method", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_job_load_adjustments, "SC_job_load_adjustments", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_load_adjustment_decay_time, "SC_load_adjustment_decay_time", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_load_formula, "SC_load_formula", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_schedd_job_info, "SC_schedd_job_info", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_flush_submit_sec, "SC_flush_submit_sec", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_flush_finish_sec, "SC_flush_finish_sec", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_params, "SC_params", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_reprioritize_interval, "SC_reprioritize_interval", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_halftime, "SC_halftime", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_usage_weight_list, "SC_usage_weight_list", AttributeStatic::LIST, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_compensation_factor, "SC_compensation_factor", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_user, "SC_weight_user", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_project, "SC_weight_project", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_department, "SC_weight_department", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_job, "SC_weight_job", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_tickets_functional, "SC_weight_tickets_functional", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_tickets_share, "SC_weight_tickets_share", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_tickets_override, "SC_weight_tickets_override", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_share_override_tickets, "SC_share_override_tickets", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_share_functional_shares, "SC_share_functional_shares", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_max_functional_jobs_to_schedule, "SC_max_functional_jobs_to_schedule", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_report_pjob_tickets, "SC_report_pjob_tickets", AttributeStatic::BOOL, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_max_pending_tasks_per_job, "SC_max_pending_tasks_per_job", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_halflife_decay_list, "SC_halflife_decay_list", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_policy_hierarchy, "SC_policy_hierarchy", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_ticket, "SC_weight_ticket", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_waiting_time, "SC_weight_waiting_time", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_deadline, "SC_weight_deadline", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_urgency, "SC_weight_urgency", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_weight_priority, "SC_weight_priority", AttributeStatic::DOUBLE, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_max_reservation, "SC_max_reservation", AttributeStatic::UINT32, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true}, \
   {SC_default_duration, "SC_default_duration", AttributeStatic::STRING, nullptr, AttributeStatic::NO_POS, AttributeStatic::NO_HASH, false, true} \

} // end namespace

