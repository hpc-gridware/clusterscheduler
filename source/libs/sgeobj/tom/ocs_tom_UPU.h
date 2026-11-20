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
 * This code was generated from file source/libs/sgeobj/json/UPU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Usage: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of UPU, Job Usage
 *
 * The enumeration is the type of every field number of UPU, so a parameter
 * of this type takes the field numbers of UPU only.
 */
enum UPU_Field {
   /**
    * @brief Job Number
    *
    * The job number. Usage of individual array tasks and/or PE tasks is accumulated.
    */
   UPU_job_number = UPU_LOWERBOUND,
   /**
    * @brief Old Usage List
    *
    * UA still debited usage set and used via orders by SGEEE ted_job_usageschedd by qmaster.
    */
   UPU_old_usage_list,
};

namespace ocs::tom {

   struct UA;

   /**
    * @brief Job Usage
    *
    * This is the list type we use to hold the
    * information for user/project. This objects are targets of throwing
    * tickets to them and as usage accumulators. There are no real differences
    * at the moment, so putting them together is convenient.
    */
   struct UPU {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {UPU_job_number, "UPU_job_number", FieldType::Uint32, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {UPU_old_usage_list, "UPU_old_usage_list", FieldType::List, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of UPU belongs to UPU */
   template <>
   struct TypeOf<UPU_Field> {
      using type = UPU;   ///< the object type
   };

   /** @brief UPU_old_usage_list holds elements of UA */
   template <>
   struct SubType<UPU_old_usage_list> {
      using type = UA;   ///< the sub-type
   };

} // namespace ocs::tom
