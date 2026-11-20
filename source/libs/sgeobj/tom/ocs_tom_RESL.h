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
 * This code was generated from file source/libs/sgeobj/json/RESL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource List: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of RESL, Resource List
 *
 * The enumeration is the type of every field number of RESL, so a parameter
 * of this type takes the field numbers of RESL only.
 */
enum RESL_Field {
   /**
    * @brief Value
    *
    * The ID value of the RSMAP consumable complex.
    * @todo rename it to id? That's what it is in the RSMAP.
    */
   RESL_value = RESL_LOWERBOUND,
   /**
    * @brief ID Instance
    *
    * We can have multiple RSMAP entries with the same ID. E.g., we have 4 P40 GPUs in a host.
    * To be able to distinguish between them, we store an instance number in this attribute.
    * The first (possibly only) entry has instance 0.
    */
   RESL_id_instance,
   /**
    * @brief PE Task ID
    *
    * When RESL is part of the GDIL or the Granted Resources list of a parallel job,
    * we store here the PE task ID of the parallel task that owns this RSMAP ID.
    * Where we don't need it (in the complex_values list of a host, or if it is a per HOST consumable),
    * the value is 0.
    */
   RESL_pe_task_id,
   /**
    * @brief Resource Amount
    *
    * The number of resources with this name.
    * @todo once we schedule individual RSMAP entries, we can probably remove this attribute - it will always be 1.
    */
   RESL_amount,
   /**
    * @brief Resource Properties
    *
    * Here we store properties of an individual resource, e.g., a GPU or a network device.
    * Properties can be arbitrary attributes of a resource, e.g., the amount of memory of a GPU,
    * the bandwidth of a network device, the affinity to CPU cores, etc.
    * The properties list contains ComplexEntry objects.
    * They can have different types, e.g., string for a GPU model or the affinity mask,
    * double for numerical values like the bandwidth or the memory amount.
    */
   RESL_properties,
};

namespace ocs::tom {

   struct CE;

   /**
    * @brief Resource List
    *
    * Consumable Resource Map Resource List.
    * Holds all consumable resource map identifiers which are
    * available on a host (Referenced in CE / the complex_list of a host).
    * It is also used to store the RSMAP IDs of resources which are
    * granted to a job / array task / pe task; referenced in GRU, which in turn
    * is referenced in gdil (JG) and in ja_tasks' granted_resources list (JAT).
    */
   struct RESL {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RESL_value, "RESL_value", FieldType::String, Flag::Spool},
         {RESL_id_instance, "RESL_id_instance", FieldType::Uint32, Flag::Spool},
         {RESL_pe_task_id, "RESL_pe_task_id", FieldType::Uint32, Flag::Spool},
         {RESL_amount, "RESL_amount", FieldType::Uint32, Flag::Spool},
         {RESL_properties, "RESL_properties", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RESL belongs to RESL */
   template <>
   struct TypeOf<RESL_Field> {
      using type = RESL;   ///< the object type
   };

   /** @brief RESL_properties holds elements of CE */
   template <>
   struct SubType<RESL_properties> {
      using type = CE;   ///< the sub-type
   };

} // namespace ocs::tom
