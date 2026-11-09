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
 * @brief Object containing scheduler configuration attributes: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SC, Object containing scheduler configuration attributes
 *
 * The enumeration is the type of every field number of SC, so a parameter
 * of this type takes the field numbers of SC only.
 */
enum SC_Field {
   /**
    * @brief Scheduler Algorithm
    *
    * Deprecated and may be removed in the future.
    * Was intended to differentiate settings for different scheduling algorithms.
    * There is exactly one algorithm called default.
    */
   SC_algorithm = SC_LOWERBOUND,
   /**
    * @brief Scheduling Interval
    *
    * Fixed interval (time value) defining how often scheduling runs are done.
    */
   SC_schedule_interval,
   /**
    * @brief Maximum User Jobs
    *
    * The maximum number of *running* jobs per user.
    */
   SC_maxujobs,
   /**
    * @brief Queue Sort Method
    *
    * For scheduling jobs the queue instances the sort order can be specified as
    *   - seqno: requires that sequence numbers are defined in the queue definitions
    *   - load: sorting is done by load, @see also load_formula
    */
   SC_queue_sort_method,
   /**
    * @brief Job Load Adjustments
    *
    * Adds virtual load to just started jobs.
    */
   SC_job_load_adjustments,
   /**
    * @brief Load Adjustment Decay Time
    *
    * How fast to decay virtual load added via @see job_load_adjustments.
    */
   SC_load_adjustment_decay_time,
   /**
    * @brief Load Formula
    *
    * Algebraic expression based on load values and weighting factors.
    */
   SC_load_formula,
   /**
    * @brief Schedd Job Info
    *
    * Boolean which defines if information why jobs cannot be schedules shall be made available.
    */
   SC_schedd_job_info,
   /**
    * @brief Flush Submit Sec
    *
    * Forces scheduling run n seconds after job submission.
    */
   SC_flush_submit_sec,
   /**
    * @brief Flush Finish Sec
    *
    * Forces a scheduling run n seconds after a job finished.
    */
   SC_flush_finish_sec,
   /**
    * @brief Params
    *
    * Arbitrary list of scheduler parameters, usually name-value pairs.
    */
   SC_params,
   /**
    * @brief Reprioritize Interval
    *
    * Time value defining how often running jobs will be reprioritized (based on policies).
    */
   SC_reprioritize_interval,
   /**
    * @brief Halftime
    *
    * Halftime for decaying sharetree usage.
    */
   SC_halftime,
   /**
    * @brief Usage Weight List
    *
    * Defines how different usage values (e.g. cpu, mem, io) are weighted in sharetree calculations.
    */
   SC_usage_weight_list,
   /**
    * @brief Compensation Factor
    *
    * Defines how fast too low or too high sharetree usage is compensated.
    */
   SC_compensation_factor,
   /**
    * @brief Weight User
    *
    * Weighting factor for functional policy on user configuration.
    */
   SC_weight_user,
   /**
    * @brief Weight Project
    *
    * Weighting factor for functional policy on project configuration.
    */
   SC_weight_project,
   /**
    * @brief Weight Department
    *
    * Weighting factor for functional policy on department configuration.
    */
   SC_weight_department,
   /**
    * @brief Weight Job
    *
    * Weighting factor for functional policy on job shares (qsub -js).
    */
   SC_weight_job,
   /**
    * @brief Weight Tickets Functional
    *
    * Weight of functional policy as compared to other ticket policies.
    */
   SC_weight_tickets_functional,
   /**
    * @brief Weight Tickets Share
    *
    * Weight of sharetree policy as compared to other ticket policies.
    */
   SC_weight_tickets_share,
   /**
    * @brief Weight Tickets Override
    *
    * Weight of override policy as compared to other ticket policies.
    */
   SC_weight_tickets_override,
   /**
    * @brief Share Override Tickets
    *
    * Boolean defining if every job gets the same amount of tickets from a sharetree node
    * or if tickets shall be distributed over the jobs belonging to a sharetree node.
    */
   SC_share_override_tickets,
   /**
    * @brief Share Functional Shares
    *
    * Boolean defining if functional shares of any object instance are shared among
    * all the jobs associated with the object
    */
   SC_share_functional_shares,
   /**
    * @brief Max Functional Jobs to Schedule
    *
    * The maximum number of functional pending jobs to schedule using the brute-force method.
    */
   SC_max_functional_jobs_to_schedule,
   /**
    * @brief Report Pending Job Tickets
    *
    * Report pending job tickets to the qmaster via scheduler order or not.
    */
   SC_report_pjob_tickets,
   /**
    * @brief Maximum Pending Tasks Per Job
    *
    * The number of array tasks per pending job to schedule. Helps to reduce scheduling overhead.
    */
   SC_max_pending_tasks_per_job,
   /**
    * @brief Halflife Decay List
    *
    * A list of halflife decay values (UA_Type).
    */
   SC_halflife_decay_list,
   /**
    * @brief Policy Hierarchy
    *
    * Defines the Order of the ticket computation.
    */
   SC_policy_hierarchy,
   /**
    * @brief Weight Ticket
    *
    * Weight of the ticket policies (functional, override, sharetree) as compared to other policies.
    */
   SC_weight_ticket,
   /**
    * @brief Weight Waiting Time
    *
    * Weight of the waiting time policy as compared to other policies.
    */
   SC_weight_waiting_time,
   /**
    * @brief Weight Deadline
    *
    * Weight of the deadline policy as compared to other policies.
    */
   SC_weight_deadline,
   /**
    * @brief Weight Urgency
    *
    * Weight of the urgency policy as compared to other policies.
    */
   SC_weight_urgency,
   /**
    * @brief Weight Priority
    *
    * Weight of the posix priority as compared to other policies.
    */
   SC_weight_priority,
   /**
    * @brief Maximum Number of Reservations
    *
    * The maximum number of resource reservations. 0 disables resource reservation.
    */
   SC_max_reservation,
   /**
    * @brief Default Duration
    *
    * Default duration assumed for jobs that specify no h_rt/s_rt.
    */
   SC_default_duration,
};

namespace ocs::tom {

   struct CE;
   struct UA;

   /**
    * @brief Object containing scheduler configuration attributes
    *
    * In the corresponding master list there is exactly one element holding the scheduler configuration.
    * See also man page sge_sched_conf(5).
    */
   struct SC {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SC_algorithm, "SC_algorithm", FieldType::String, Flag::Spool},
         {SC_schedule_interval, "SC_schedule_interval", FieldType::String, Flag::Spool},
         {SC_maxujobs, "SC_maxujobs", FieldType::Uint32, Flag::Spool},
         {SC_queue_sort_method, "SC_queue_sort_method", FieldType::Uint32, Flag::Spool},
         {SC_job_load_adjustments, "SC_job_load_adjustments", FieldType::List, Flag::Spool},
         {SC_load_adjustment_decay_time, "SC_load_adjustment_decay_time", FieldType::String, Flag::Spool},
         {SC_load_formula, "SC_load_formula", FieldType::String, Flag::Spool},
         {SC_schedd_job_info, "SC_schedd_job_info", FieldType::String, Flag::Spool},
         {SC_flush_submit_sec, "SC_flush_submit_sec", FieldType::Uint32, Flag::Spool},
         {SC_flush_finish_sec, "SC_flush_finish_sec", FieldType::Uint32, Flag::Spool},
         {SC_params, "SC_params", FieldType::String, Flag::Spool},
         {SC_reprioritize_interval, "SC_reprioritize_interval", FieldType::String, Flag::Spool},
         {SC_halftime, "SC_halftime", FieldType::Uint32, Flag::Spool},
         {SC_usage_weight_list, "SC_usage_weight_list", FieldType::List, Flag::Spool},
         {SC_compensation_factor, "SC_compensation_factor", FieldType::Double, Flag::Spool},
         {SC_weight_user, "SC_weight_user", FieldType::Double, Flag::Spool},
         {SC_weight_project, "SC_weight_project", FieldType::Double, Flag::Spool},
         {SC_weight_department, "SC_weight_department", FieldType::Double, Flag::Spool},
         {SC_weight_job, "SC_weight_job", FieldType::Double, Flag::Spool},
         {SC_weight_tickets_functional, "SC_weight_tickets_functional", FieldType::Uint32, Flag::Spool},
         {SC_weight_tickets_share, "SC_weight_tickets_share", FieldType::Uint32, Flag::Spool},
         {SC_weight_tickets_override, "SC_weight_tickets_override", FieldType::Uint32, Flag::Spool},
         {SC_share_override_tickets, "SC_share_override_tickets", FieldType::Bool, Flag::Spool},
         {SC_share_functional_shares, "SC_share_functional_shares", FieldType::Bool, Flag::Spool},
         {SC_max_functional_jobs_to_schedule, "SC_max_functional_jobs_to_schedule", FieldType::Uint32, Flag::Spool},
         {SC_report_pjob_tickets, "SC_report_pjob_tickets", FieldType::Bool, Flag::Spool},
         {SC_max_pending_tasks_per_job, "SC_max_pending_tasks_per_job", FieldType::Uint32, Flag::Spool},
         {SC_halflife_decay_list, "SC_halflife_decay_list", FieldType::String, Flag::Spool},
         {SC_policy_hierarchy, "SC_policy_hierarchy", FieldType::String, Flag::Spool},
         {SC_weight_ticket, "SC_weight_ticket", FieldType::Double, Flag::Spool},
         {SC_weight_waiting_time, "SC_weight_waiting_time", FieldType::Double, Flag::Spool},
         {SC_weight_deadline, "SC_weight_deadline", FieldType::Double, Flag::Spool},
         {SC_weight_urgency, "SC_weight_urgency", FieldType::Double, Flag::Spool},
         {SC_weight_priority, "SC_weight_priority", FieldType::Double, Flag::Spool},
         {SC_max_reservation, "SC_max_reservation", FieldType::Uint32, Flag::Spool},
         {SC_default_duration, "SC_default_duration", FieldType::String, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SC belongs to SC */
   template <>
   struct TypeOf<SC_Field> {
      using type = SC;   ///< the object type
   };

   /** @brief SC_job_load_adjustments holds elements of CE */
   template <>
   struct SubType<SC_job_load_adjustments> {
      using type = CE;   ///< the sub-type
   };

   /** @brief SC_usage_weight_list holds elements of UA */
   template <>
   struct SubType<SC_usage_weight_list> {
      using type = UA;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of SC for the list library: a synonym for ocs::tom::SC::descriptor */
inline constexpr const lDescr *SC_Type = ocs::tom::SC::descriptor.e;
