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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Object containing scheduler configuration attributes
*
* In the corresponding master list there is exactly one element holding the scheduler configuration.
* See also man page sge_sched_conf(5).
*
*    SGE_STRING(SC_algorithm) - Scheduler Algorithm
*    Deprecated and may be removed in the future.
*    Was intended to differentiate settings for different scheduling algorithms.
*    There is exactly one algorithm called default.
*
*    SGE_STRING(SC_schedule_interval) - Scheduling Interval
*    Fixed interval (time value) defining how often scheduling runs are done.
*
*    SGE_ULONG(SC_maxujobs) - Maximum User Jobs
*    The maximum number of *running* jobs per user.
*
*    SGE_ULONG(SC_queue_sort_method) - Queue Sort Method
*    For scheduling jobs the queue instances the sort order can be specified as
*      - seqno: requires that sequence numbers are defined in the queue definitions
*      - load: sorting is done by load, @see also load_formula
*
*    SGE_LIST(SC_job_load_adjustments) - Job Load Adjustments
*    Adds virtual load to just started jobs.
*
*    SGE_STRING(SC_load_adjustment_decay_time) - Load Adjustment Decay Time
*    How fast to decay virtual load added via @see job_load_adjustments.
*
*    SGE_STRING(SC_load_formula) - Load Formula
*    Algebraic expression based on load values and weighting factors.
*
*    SGE_STRING(SC_schedd_job_info) - Schedd Job Info
*    Boolean which defines if information why jobs cannot be schedules shall be made available.
*
*    SGE_ULONG(SC_flush_submit_sec) - Flush Submit Sec
*    Forces scheduling run n seconds after job submission.
*
*    SGE_ULONG(SC_flush_finish_sec) - Flush Finish Sec
*    Forces a scheduling run n seconds after a job finished.
*
*    SGE_STRING(SC_params) - Params
*    Arbitrary list of scheduler parameters, usually name-value pairs.
*
*    SGE_STRING(SC_reprioritize_interval) - Reprioritize Interval
*    Time value defining how often running jobs will be reprioritized (based on policies).
*
*    SGE_ULONG(SC_halftime) - Halftime
*    Halftime for decaying sharetree usage.
*
*    SGE_LIST(SC_usage_weight_list) - Usage Weight List
*    Defines how different usage values (e.g. cpu, mem, io) are weighted in sharetree calculations.
*
*    SGE_DOUBLE(SC_compensation_factor) - Compensation Factor
*    Defines how fast too low or too high sharetree usage is compensated.
*
*    SGE_DOUBLE(SC_weight_user) - Weight User
*    Weighting factor for functional policy on user configuration.
*
*    SGE_DOUBLE(SC_weight_project) - Weight Project
*    Weighting factor for functional policy on project configuration.
*
*    SGE_DOUBLE(SC_weight_department) - Weight Department
*    Weighting factor for functional policy on department configuration.
*
*    SGE_DOUBLE(SC_weight_job) - Weight Job
*    Weighting factor for functional policy on job shares (qsub -js).
*
*    SGE_ULONG(SC_weight_tickets_functional) - Weight Tickets Functional
*    Weight of functional policy as compared to other ticket policies.
*
*    SGE_ULONG(SC_weight_tickets_share) - Weight Tickets Share
*    Weight of sharetree policy as compared to other ticket policies.
*
*    SGE_ULONG(SC_weight_tickets_override) - Weight Tickets Override
*    Weight of override policy as compared to other ticket policies.
*
*    SGE_BOOL(SC_share_override_tickets) - Share Override Tickets
*    Boolean defining if every job gets the same amount of tickets from a sharetree node
*    or if tickets shall be distributed over the jobs belonging to a sharetree node.
*
*    SGE_BOOL(SC_share_functional_shares) - Share Functional Shares
*    Boolean defining if functional shares of any object instance are shared among
*    all the jobs associated with the object
*
*    SGE_ULONG(SC_max_functional_jobs_to_schedule) - Max Functional Jobs to Schedule
*    The maximum number of functional pending jobs to schedule using the brute-force method.
*
*    SGE_BOOL(SC_report_pjob_tickets) - Report Pending Job Tickets
*    Report pending job tickets to the qmaster via scheduler order or not.
*
*    SGE_ULONG(SC_max_pending_tasks_per_job) - Maximum Pending Tasks Per Job
*    The number of array tasks per pending job to schedule. Helps to reduce scheduling overhead.
*
*    SGE_STRING(SC_halflife_decay_list) - Halflife Decay List
*    A list of halflife decay values (UA_Type).
*
*    SGE_STRING(SC_policy_hierarchy) - Policy Hierarchy
*    Defines the Order of the ticket computation.
*
*    SGE_DOUBLE(SC_weight_ticket) - Weight Ticket
*    Weight of the ticket policies (functional, override, sharetree) as compared to other policies.
*
*    SGE_DOUBLE(SC_weight_waiting_time) - Weight Waiting Time
*    Weight of the waiting time policy as compared to other policies.
*
*    SGE_DOUBLE(SC_weight_deadline) - Weight Deadline
*    Weight of the deadline policy as compared to other policies.
*
*    SGE_DOUBLE(SC_weight_urgency) - Weight Urgency
*    Weight of the urgency policy as compared to other policies.
*
*    SGE_DOUBLE(SC_weight_priority) - Weight Priority
*    Weight of the posix priority as compared to other policies.
*
*    SGE_ULONG(SC_max_reservation) - Maximum Number of Reservations
*    The maximum number of resource reservations. 0 disables resource reservation.
*
*    SGE_STRING(SC_default_duration) - Default Duration
*    Default duration assumed for jobs that specify no h_rt/s_rt.
*
*/

enum {
   SC_algorithm = SC_LOWERBOUND,   ///< Scheduler Algorithm
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

LISTDEF(SC_Type)
   SGE_STRING(SC_algorithm, CULL_SPOOL)
   SGE_STRING(SC_schedule_interval, CULL_SPOOL)
   SGE_ULONG(SC_maxujobs, CULL_SPOOL)
   SGE_ULONG(SC_queue_sort_method, CULL_SPOOL)
   SGE_LIST(SC_job_load_adjustments, CE_Type, CULL_SPOOL)
   SGE_STRING(SC_load_adjustment_decay_time, CULL_SPOOL)
   SGE_STRING(SC_load_formula, CULL_SPOOL)
   SGE_STRING(SC_schedd_job_info, CULL_SPOOL)
   SGE_ULONG(SC_flush_submit_sec, CULL_SPOOL)
   SGE_ULONG(SC_flush_finish_sec, CULL_SPOOL)
   SGE_STRING(SC_params, CULL_SPOOL)
   SGE_STRING(SC_reprioritize_interval, CULL_SPOOL)
   SGE_ULONG(SC_halftime, CULL_SPOOL)
   SGE_LIST(SC_usage_weight_list, UA_Type, CULL_SPOOL)
   SGE_DOUBLE(SC_compensation_factor, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_user, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_project, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_department, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_job, CULL_SPOOL)
   SGE_ULONG(SC_weight_tickets_functional, CULL_SPOOL)
   SGE_ULONG(SC_weight_tickets_share, CULL_SPOOL)
   SGE_ULONG(SC_weight_tickets_override, CULL_SPOOL)
   SGE_BOOL(SC_share_override_tickets, CULL_SPOOL)
   SGE_BOOL(SC_share_functional_shares, CULL_SPOOL)
   SGE_ULONG(SC_max_functional_jobs_to_schedule, CULL_SPOOL)
   SGE_BOOL(SC_report_pjob_tickets, CULL_SPOOL)
   SGE_ULONG(SC_max_pending_tasks_per_job, CULL_SPOOL)
   SGE_STRING(SC_halflife_decay_list, CULL_SPOOL)
   SGE_STRING(SC_policy_hierarchy, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_ticket, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_waiting_time, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_deadline, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_urgency, CULL_SPOOL)
   SGE_DOUBLE(SC_weight_priority, CULL_SPOOL)
   SGE_ULONG(SC_max_reservation, CULL_SPOOL)
   SGE_STRING(SC_default_duration, CULL_SPOOL)
LISTEND

NAMEDEF(SCN)
   NAME("SC_algorithm")
   NAME("SC_schedule_interval")
   NAME("SC_maxujobs")
   NAME("SC_queue_sort_method")
   NAME("SC_job_load_adjustments")
   NAME("SC_load_adjustment_decay_time")
   NAME("SC_load_formula")
   NAME("SC_schedd_job_info")
   NAME("SC_flush_submit_sec")
   NAME("SC_flush_finish_sec")
   NAME("SC_params")
   NAME("SC_reprioritize_interval")
   NAME("SC_halftime")
   NAME("SC_usage_weight_list")
   NAME("SC_compensation_factor")
   NAME("SC_weight_user")
   NAME("SC_weight_project")
   NAME("SC_weight_department")
   NAME("SC_weight_job")
   NAME("SC_weight_tickets_functional")
   NAME("SC_weight_tickets_share")
   NAME("SC_weight_tickets_override")
   NAME("SC_share_override_tickets")
   NAME("SC_share_functional_shares")
   NAME("SC_max_functional_jobs_to_schedule")
   NAME("SC_report_pjob_tickets")
   NAME("SC_max_pending_tasks_per_job")
   NAME("SC_halflife_decay_list")
   NAME("SC_policy_hierarchy")
   NAME("SC_weight_ticket")
   NAME("SC_weight_waiting_time")
   NAME("SC_weight_deadline")
   NAME("SC_weight_urgency")
   NAME("SC_weight_priority")
   NAME("SC_max_reservation")
   NAME("SC_default_duration")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define SC_SIZE sizeof(SCN)/sizeof(char *)


