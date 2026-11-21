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
 * This code was generated from file source/libs/sgeobj/json/CCT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Category Cache Entry: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of CCT, Category Cache Entry
 *
 * The enumeration is the type of every field number of CCT, so a parameter
 * of this type takes the field numbers of CCT only.
 */
enum CCT_Field {
   /**
    * @brief Parallel Environment
    *
    * The parallel environment this cache is for; a category is cached once per PE it was tried with.
    */
   CCT_pe_name = CCT_LOWERBOUND,
   /**
    * @brief Rejected Queues
    *
    * Cluster queues already known not to suit this category (`CTI`), so later jobs skip them.
    */
   CCT_ignore_queues,
   /**
    * @brief Rejected Hosts
    *
    * Hosts already known not to suit this category.
    */
   CCT_ignore_hosts,
   /**
    * @brief Scheduler Messages
    *
    * The "why not" messages (`MES`) produced for this category, reused rather than regenerated per job.
    */
   CCT_job_messages,
   /**
    * @brief Possible Slot Counts
    *
    * Reference to the set of slot counts the parallel environment could grant, cached across the category's jobs.
    */
   CCT_pe_job_slots,
   /**
    * @brief Slot Count Size
    *
    * How many entries CCT_pe_job_slots holds.
    */
   CCT_pe_job_slot_count,
};

namespace ocs::tom {

   struct CTI;
   struct MES;

   /**
    * @brief Category Cache Entry
    *
    * What the scheduler learned about one category while scheduling for one parallel environment.
    * Jobs in a category make identical requests, so a queue or host that rejected the first job rejects them all. Remembering those rejections turns a per-job search into a per-category one, which is where most of the scheduler's speed on a large pending list comes from.
    */
   struct CCT {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CCT_pe_name, "CCT_pe_name", FieldType::String, Flag::None},
         {CCT_ignore_queues, "CCT_ignore_queues", FieldType::List, Flag::None},
         {CCT_ignore_hosts, "CCT_ignore_hosts", FieldType::List, Flag::None},
         {CCT_job_messages, "CCT_job_messages", FieldType::List, Flag::None},
         {CCT_pe_job_slots, "CCT_pe_job_slots", FieldType::Ref, Flag::NoTransfer},
         {CCT_pe_job_slot_count, "CCT_pe_job_slot_count", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CCT belongs to CCT */
   template <>
   struct TypeOf<CCT_Field> {
      using type = CCT;   ///< the object type
   };

   /** @brief CCT_ignore_queues holds elements of CTI */
   template <>
   struct SubType<CCT_ignore_queues> {
      using type = CTI;   ///< the sub-type
   };

   /** @brief CCT_ignore_hosts holds elements of CTI */
   template <>
   struct SubType<CCT_ignore_hosts> {
      using type = CTI;   ///< the sub-type
   };

   /** @brief CCT_job_messages holds elements of MES */
   template <>
   struct SubType<CCT_job_messages> {
      using type = MES;   ///< the sub-type
   };

} // namespace ocs::tom
