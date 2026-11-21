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
 * This code was generated from file source/libs/sgeobj/json/OR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Scheduler Order: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of OR, Scheduler Order
 *
 * The enumeration is the type of every field number of OR, so a parameter
 * of this type takes the field numbers of OR only.
 */
enum OR_Field {
   /**
    * @brief Order Type
    *
    * The type of the order. Defined in an enum in libs/sgeobj/sge_order.h, e.g.
    *   - ORT_start_job
    *   - ORT_tickets
    *   - ORT_ptickets
    *   - ORT_...
    */
   OR_type = OR_LOWERBOUND,
   /**
    * @brief Job Number
    *
    * The job id in case of job related orders.
    */
   OR_job_number,
   /**
    * @brief Array Task Number
    *
    * The array task id in case of array job related orders.
    */
   OR_ja_task_number,
   /**
    * @brief Job Version
    *
    * A job version number. Used to detect if a scheduler order is based on an older version of the job.
    * When a job is modified, the job version (in the job object) is increased.
    * If a scheduling decision has been done on an older version of the job the order is ignored.
    */
   OR_job_version,
   /**
    * @brief Queue Instances
    *
    * List of queue instances a job has been scheduled to
    */
   OR_queuelist,
   /**
    * @brief Granted Resources List
    *
    * List of RSMAP (future: and resource) requests.
    */
   OR_granted_resources_list,
   /**
    * @brief Number of Tickets
    *
    * Number of tickets a job got during scheduling.
    */
   OR_ticket,
   /**
    * @brief Order Specific Data
    *
    * Sublist with order specific data, depending on the order type:
    *   - ORT_start_job:              empty
    *   - ORT_tickets:                reduced job element
    *   - ORT_remove_immediate_job:   empty
    *   - ORT_job_schedd_info:        scheduler messages (SME)
    *   - ORT_ptickets:               reduced job element
    */
   OR_joker,
   /**
    * @brief PE Name
    *
    * In case of start order for parallel jobs: The name of the PE the job has been scheduled to.
    */
   OR_pe,
   /**
    * @brief Normalized Tickets
    *
    * Number of normalized job tickets.
    */
   OR_ntix,
   /**
    * @brief Job Priority
    *
    * Priority of a scheduled job after applying all policies.
    */
   OR_prio,
};

namespace ocs::tom {

   struct GRU;
   struct OQ;

   /**
    * @brief Scheduler Order
    *
    * A single scheduler order, e.g. to start a job
    */
   struct OR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {OR_type, "OR_type", FieldType::Uint32, Flag::None},
         {OR_job_number, "OR_job_number", FieldType::Uint32, Flag::None},
         {OR_ja_task_number, "OR_ja_task_number", FieldType::Uint32, Flag::None},
         {OR_job_version, "OR_job_version", FieldType::Uint32, Flag::None},
         {OR_queuelist, "OR_queuelist", FieldType::List, Flag::None},
         {OR_granted_resources_list, "OR_granted_resources_list", FieldType::List, Flag::None},
         {OR_ticket, "OR_ticket", FieldType::Double, Flag::None},
         {OR_joker, "OR_joker", FieldType::List, Flag::None},
         {OR_pe, "OR_pe", FieldType::String, Flag::None},
         {OR_ntix, "OR_ntix", FieldType::Double, Flag::None},
         {OR_prio, "OR_prio", FieldType::Double, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of OR belongs to OR */
   template <>
   struct TypeOf<OR_Field> {
      using type = OR;   ///< the object type
   };

   /** @brief OR_queuelist holds elements of OQ */
   template <>
   struct SubType<OR_queuelist> {
      using type = OQ;   ///< the sub-type
   };

   /** @brief OR_granted_resources_list holds elements of GRU */
   template <>
   struct SubType<OR_granted_resources_list> {
      using type = GRU;   ///< the sub-type
   };

} // namespace ocs::tom
