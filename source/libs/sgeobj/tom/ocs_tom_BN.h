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
 * This code was generated from file source/libs/sgeobj/json/BN.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Binding: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of BN, Binding
 *
 * The enumeration is the type of every field number of BN, so a parameter
 * of this type takes the field numbers of BN only.
 */
enum BN_Field {
   /**
    * @brief type of binding
    *
    * host or slot binding
    */
   BN_new_type = BN_LOWERBOUND,
   /**
    * @brief Instance that applies the binding
    *
    * Set, PE or env
    */
   BN_instance,
   /**
    * @brief Amount of units
    *
    * Number of units to bind to
    */
   BN_amount,
   /**
    * @brief Unit type that should be bound
    *
    * Defines if threads, cores, sockets ... should be bound
    */
   BN_unit,
   /**
    * @brief Mask that defines which parts of a topology should not be bound
    *
    * Masked parts have to be lowercase in the topology string.
    */
   BN_filter,
   /**
    * @brief Sort order of binding
    *
    * Defines how units within the topology string should be sorted before binding.
    */
   BN_sort,
   /**
    * @brief Start position
    *
    * Defines the start position for binding within the topology string.
    */
   BN_start,
   /**
    * @brief Stop position
    *
    * Defines the stop position for binding within the topology string.
    */
   BN_stop,
   /**
    * @brief Binding strategy ...
    *
    * Defines the strategy for binding, like linear, striding, packed or explicit
    */
   BN_strategy,
   /**
    * @brief hostname of where the attributes specific_binding is valid for
    *
    * Used in scheduler only to identify where a specific binding decision was done for.
    */
   BN_specific_hostname,
   /**
    * @brief A specific binding decision for a job on a host
    *
    * Used in scheduler only to hold a binding decision for a specific job on a host.
    */
   BN_specific_binding,
   /**
    * @brief Sublist of individual task specific bindings.
    *
    * Id of sublist specifies a task ID and the string the binding done for that task.
    * Exists only if task specific binding decisions have to be done.
    */
   BN_specific_binding_list,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   BN_joker,
};

namespace ocs::tom {

   struct ST;
   struct VA;

   /**
    * @brief Binding
    *
    * How a job is to be bound to the cores of its execution host.
    * Holds what the user asked for with `qsub -binding`: the strategy, how much to bind, and the starting point. What was actually granted is recorded on the granted destination identifier.
    */
   struct BN {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {BN_new_type, "BN_new_type", FieldType::Uint32, Flag::SpoolSublist},
         {BN_instance, "BN_instance", FieldType::Uint32, Flag::SpoolSublist},
         {BN_amount, "BN_amount", FieldType::Uint32, Flag::SpoolSublist},
         {BN_unit, "BN_unit", FieldType::Uint32, Flag::SpoolSublist},
         {BN_filter, "BN_filter", FieldType::String, Flag::SpoolSublist},
         {BN_sort, "BN_sort", FieldType::String, Flag::SpoolSublist},
         {BN_start, "BN_start", FieldType::Uint32, Flag::SpoolSublist},
         {BN_stop, "BN_stop", FieldType::Uint32, Flag::SpoolSublist},
         {BN_strategy, "BN_strategy", FieldType::Uint32, Flag::SpoolSublist},
         {BN_specific_hostname, "BN_specific_hostname", FieldType::Host, Flag::SpoolSublist},
         {BN_specific_binding, "BN_specific_binding", FieldType::String, Flag::SpoolSublist},
         {BN_specific_binding_list, "BN_specific_binding_list", FieldType::List, Flag::Spool},
         {BN_joker, "BN_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of BN belongs to BN */
   template <>
   struct TypeOf<BN_Field> {
      using type = BN;   ///< the object type
   };

   /** @brief BN_specific_binding_list holds elements of ST */
   template <>
   struct SubType<BN_specific_binding_list> {
      using type = ST;   ///< the sub-type
   };

   /** @brief BN_joker holds elements of VA */
   template <>
   struct SubType<BN_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
