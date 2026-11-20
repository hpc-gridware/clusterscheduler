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
 * This code was generated from file source/libs/sgeobj/json/RQL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Scheduling Cache: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of RQL, Resource Quota Scheduling Cache
 *
 * The enumeration is the type of every field number of RQL, so a parameter
 * of this type takes the field numbers of RQL only.
 */
enum RQL_Field {
   /**
    * @brief Limit Key
    *
    * Identifies the limit this entry caches, built from the rule and resource being evaluated.
    */
   RQL_name = RQL_LOWERBOUND,
   /**
    * @brief Cached Result
    *
    * The `dispatch_t` the limit returned last time it was asked in this run.
    */
   RQL_result,
   /**
    * @brief Earliest Time
    *
    * Earliest time the limit would allow the job to start, for reservation scheduling.
    */
   RQL_time,
   /**
    * @brief Slots
    *
    * How many slots the limit allows, cached alongside the result.
    */
   RQL_slots,
   /**
    * @brief Tagged For Schedule
    *
    * Marks the entry as belonging to the run in progress, so stale entries are not believed.
    */
   RQL_tagged4schedule,
};

namespace ocs::tom {

   /**
    * @brief Resource Quota Scheduling Cache
    *
    * Per-scheduling-run cache of what a resource quota limit answered, so an expensive limit is evaluated once rather than once per queue instance.
    * Not configuration and not spooled: it lives in the scheduler's assignment structure for the duration of one run.
    */
   struct RQL {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RQL_name, "RQL_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash},
         {RQL_result, "RQL_result", FieldType::Int32, Flag::None},
         {RQL_time, "RQL_time", FieldType::Uint64, Flag::None},
         {RQL_slots, "RQL_slots", FieldType::Int32, Flag::None},
         {RQL_tagged4schedule, "RQL_tagged4schedule", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RQL belongs to RQL */
   template <>
   struct TypeOf<RQL_Field> {
      using type = RQL;   ///< the object type
   };

} // namespace ocs::tom
