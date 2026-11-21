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
 * This code was generated from file source/libs/sgeobj/json/LDR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Load Threshold Reference: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of LDR, Load Threshold Reference
 *
 * The enumeration is the type of every field number of LDR, so a parameter
 * of this type takes the field numbers of LDR only.
 */
enum LDR_Field {
   /**
    * @brief Queue References
    *
    * Part of the scheduler's load threshold reference; see the object description.
    */
   LDR_queue_ref_list = LDR_LOWERBOUND,
   /**
    * @brief Limit
    *
    * Part of the scheduler's load threshold reference; see the object description.
    */
   LDR_limit,
   /**
    * @brief Global Object
    *
    * Part of the scheduler's load threshold reference; see the object description.
    */
   LDR_global,
   /**
    * @brief Host Object
    *
    * Part of the scheduler's load threshold reference; see the object description.
    */
   LDR_host,
   /**
    * @brief Queue Object
    *
    * Part of the scheduler's load threshold reference; see the object description.
    */
   LDR_queue,
};

namespace ocs::tom {

   struct QRL;

   /**
    * @brief Load Threshold Reference
    *
    * A limit with references to the global, host and queue objects it was resolved against.
    * The scheduler builds one per load threshold that depends on a consumable (sge_create_load_list() in libs/sched/sge_select_queue.cc) and checks the queues of the list for load alarm after each dispatch.
    */
   struct LDR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {LDR_queue_ref_list, "LDR_queue_ref_list", FieldType::List, Flag::None},
         {LDR_limit, "LDR_limit", FieldType::String, Flag::None},
         {LDR_global, "LDR_global", FieldType::Ref, Flag::NoTransfer},
         {LDR_host, "LDR_host", FieldType::Ref, Flag::NoTransfer},
         {LDR_queue, "LDR_queue", FieldType::Ref, Flag::NoTransfer},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of LDR belongs to LDR */
   template <>
   struct TypeOf<LDR_Field> {
      using type = LDR;   ///< the object type
   };

   /** @brief LDR_queue_ref_list holds elements of QRL */
   template <>
   struct SubType<LDR_queue_ref_list> {
      using type = QRL;   ///< the sub-type
   };

} // namespace ocs::tom
