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
 * This code was generated from file source/libs/sgeobj/json/PE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Parallel Environment: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of PE, Parallel Environment
 *
 * The enumeration is the type of every field number of PE, so a parameter
 * of this type takes the field numbers of PE only.
 */
enum PE_Field {
   /**
    * @brief Name
    *
    * Name of the pe.
    */
   PE_name = PE_LOWERBOUND,
   /**
    * @brief Total Slots
    *
    * Total number of slots which can be occupied by jobs running in the PE.
    */
   PE_slots,
   /**
    * @brief User List
    *
    * US_type; list of allowed users.
    */
   PE_user_list,
   /**
    * @brief XUser List
    *
    * US; list of not allowed users.
    */
   PE_xuser_list,
   /**
    * @brief Start Procedure
    *
    * Cmd line sequence for starting the pe.
    */
   PE_start_proc_args,
   /**
    * @brief Stop Procedure
    *
    * Cmd line sequence for stopping the pe.
    */
   PE_stop_proc_args,
   /**
    * @brief Allocation Rule
    *
    * Defines how a job is distributed over multiple host.
    * This can be a fixed number of processors (slots) per machine
    * or one of the following special allocation rules:
    * $pe_slots: Allocate only slots on a single host.
    * $fill_up: Fill up slots on one host, then switch to the next host, ...
    * $round_robin. Occupy one slot of a host, then switch to the next one, ...
    */
   PE_allocation_rule,
   /**
    * @brief Control Slaves
    *
    * Defines whether (false) the job is executed in so called loose integration
    * (slave execution daemons do not know about the job, no job control, no accounting)
    * or (true) if it is executed in the tight integration, where execution daemons know about slave tasks
    * and slave tasks are started under OCS control (vial qrsh -inherit).
    * Tight integration provides full job control and we get accounting information for the whole job
    * including slave tasks on remote hosts.
    */
   PE_control_slaves,
   /**
    * @brief Job Is First Task
    *
    *  
    * When set to true then the job script also counts as task (a 4 times parallel job can then
    * comprise of the master task plus 3 slave tasks.
    * When set to true then the master task does not consume a slot. This is used to take into
    * account that the master task in many cases consumes very little resources, e.g.
    * it just does an mpirun call spawning slave tasks and then waits for the slave tasks to finish.
    * A 4 times parallel job can then comprise of the master task plus 4 slave tasks.
    */
   PE_job_is_first_task,
   /**
    * @brief Resource Utilization
    *
    * Sub list of RUE used to store resources (slots) currently in use by running parallel jobs.
    */
   PE_resource_utilization,
   /**
    * @brief Urgency Slots
    *
    * Specifies what slot amount shall be used when computing jobs
    * static urgency in case of jobs with slot range PE requests.
    * The actual problem is that when determining the urgency number
    * the number of slots finally assigned is not yet known. The following
    * settings are supported: `min`, `max`, `avg` or a fixed integer
    */
   PE_urgency_slots,
   /**
    * @brief Accounting Summary
    *
    * For tightly integrated parallel jobs.
    * Specifies if a single accounting record is written for the whole job,
    * or if every task (master task and slave tasks) gets an individual accounting record.
    */
   PE_accounting_summary,
   /**
    * @brief Master Forks Slaves
    *
    * For tightly integrated parallel jobs.
    * Specifies if slave tasks on the master host are started via qrsh -inherit or if they are started
    * by the master task process itself (via fork/exec or as threads).
    */
   PE_master_forks_slaves,
   /**
    * @brief Daemon Forks Slaves
    *
    * For tightly integrated parallel jobs.
    * Specifies if slave tasks on slave host are started individually via qrsh -inherit or if a single
    * process is started via qrsh -inherit which then starts the slave tasks (via fork/exec or as threads).
    */
   PE_daemon_forks_slaves,
   /**
    * @brief Daemon Forks Slaves
    *
    * For tightly integrated parallel jobs.
    * When slave tasks are started on the master host, this flag specifies if slave specific requests
    * shall be applied to these slave tasks or not.
    */
   PE_ignore_slave_requests_on_master_host,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   PE_joker,
};

namespace ocs::tom {

   struct RUE;
   struct US;
   struct VA;

   /**
    * @brief Parallel Environment
    *
    * Defines the runtime environment for running shared memory or distributed memory parallelized applications
    */
   struct PE {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PE_name, "PE_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {PE_slots, "PE_slots", FieldType::Uint32, Flag::Spool},
         {PE_user_list, "PE_user_list", FieldType::List, Flag::Spool},
         {PE_xuser_list, "PE_xuser_list", FieldType::List, Flag::Spool},
         {PE_start_proc_args, "PE_start_proc_args", FieldType::String, Flag::Spool},
         {PE_stop_proc_args, "PE_stop_proc_args", FieldType::String, Flag::Spool},
         {PE_allocation_rule, "PE_allocation_rule", FieldType::String, Flag::Spool},
         {PE_control_slaves, "PE_control_slaves", FieldType::Bool, Flag::Spool},
         {PE_job_is_first_task, "PE_job_is_first_task", FieldType::Bool, Flag::Spool},
         {PE_resource_utilization, "PE_resource_utilization", FieldType::List, Flag::None},
         {PE_urgency_slots, "PE_urgency_slots", FieldType::String, Flag::Spool},
         {PE_accounting_summary, "PE_accounting_summary", FieldType::Bool, Flag::Spool},
         {PE_master_forks_slaves, "PE_master_forks_slaves", FieldType::Bool, Flag::Spool},
         {PE_daemon_forks_slaves, "PE_daemon_forks_slaves", FieldType::Bool, Flag::Spool},
         {PE_ignore_slave_requests_on_master_host, "PE_ignore_slave_requests_on_master_host", FieldType::Bool, Flag::Spool},
         {PE_joker, "PE_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PE belongs to PE */
   template <>
   struct TypeOf<PE_Field> {
      using type = PE;   ///< the object type
   };

   /** @brief PE_user_list holds elements of US */
   template <>
   struct SubType<PE_user_list> {
      using type = US;   ///< the sub-type
   };

   /** @brief PE_xuser_list holds elements of US */
   template <>
   struct SubType<PE_xuser_list> {
      using type = US;   ///< the sub-type
   };

   /** @brief PE_resource_utilization holds elements of RUE */
   template <>
   struct SubType<PE_resource_utilization> {
      using type = RUE;   ///< the sub-type
   };

   /** @brief PE_joker holds elements of VA */
   template <>
   struct SubType<PE_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
