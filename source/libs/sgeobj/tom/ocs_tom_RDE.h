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
 * This code was generated from file source/libs/sgeobj/json/RDE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Diagram Entry: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of RDE, Resource Diagram Entry
 *
 * The enumeration is the type of every field number of RDE, so a parameter
 * of this type takes the field numbers of RDE only.
 */
enum RDE_Field {
   /**
    * @brief Time
    *
    * Time stamp (microseconds since epoch).
    */
   RDE_time = RDE_LOWERBOUND,
   /**
    * @brief Amount
    *
    * Amount of the resource which is used.
    */
   RDE_amount,
   /**
    * @brief Resource Map List
    *
    * Amount of Resource Maps which are used.
    */
   RDE_resource_map_list,
   /**
    * @brief Binding
    *
    * Cores/threads that are used by jobs. Filled only if the resource is slots and when bookkeeping is done on host level.
    */
   RDE_binding_inuse,
};

namespace ocs::tom {

   struct RESL;

   /**
    * @brief Resource Diagram Entry
    *
    * A resource diagram lists future resource utilization (it starts at the current time).
    * An object of this type is related to the usage of a specific resource,
    * whose name is defined in the parent object (RUE).
    * The list of resource diaram entries represents a diagram showing
    * resource progression over time.
    *  
    * E.g. the following resource diagram
    *    N ^
    *      |             +-------+
    *      |   +-----+   |  J3   |
    *      |   |     +---+-------+--+
    *      |   | J1  |      J2      |
    *      +---+-----+--------------+-----> t
    *      0   4     10  14      22 25
    *  
    * is respresented by the the following table
    *     t | N
    *    ---+---
    *     0 | 0
    *     4 | 3
    *    10 | 2
    *    14 | 4
    *    22 | 2
    *    25 | 2
    *  
    *  
    *  
    */
   struct RDE {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RDE_time, "RDE_time", FieldType::Uint64, Flag::None},
         {RDE_amount, "RDE_amount", FieldType::Double, Flag::None},
         {RDE_resource_map_list, "RDE_resource_map_list", FieldType::List, Flag::None},
         {RDE_binding_inuse, "RDE_binding_inuse", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RDE belongs to RDE */
   template <>
   struct TypeOf<RDE_Field> {
      using type = RDE;   ///< the object type
   };

   /** @brief RDE_resource_map_list holds elements of RESL */
   template <>
   struct SubType<RDE_resource_map_list> {
      using type = RESL;   ///< the sub-type
   };

} // namespace ocs::tom
