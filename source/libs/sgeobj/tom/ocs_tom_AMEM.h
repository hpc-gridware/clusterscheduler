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
 * This code was generated from file source/libs/sgeobj/json/AMEM.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute Memory: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of AMEM, Attribute Memory
 *
 * The enumeration is the type of every field number of AMEM, so a parameter
 * of this type takes the field numbers of AMEM only.
 */
enum AMEM_Field {
   /**
    * @brief Host Reference
    *
    * Name of a host or a host group.
    */
   AMEM_href = AMEM_LOWERBOUND,
   /**
    * @brief Value
    *
    * The memory value (represented as string).
    */
   AMEM_value,
};

namespace ocs::tom {

   /**
    * @brief Attribute Memory
    *
    * Used for memory attributes in the cluster queues.
    * E.g. used for the queue s_fsize, h_fsize, s_data, s_stack, ...
    */
   struct AMEM {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {AMEM_href, "AMEM_href", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {AMEM_value, "AMEM_value", FieldType::String, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of AMEM belongs to AMEM */
   template <>
   struct TypeOf<AMEM_Field> {
      using type = AMEM;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of AMEM for the list library: a synonym for ocs::tom::AMEM::descriptor */
inline constexpr const lDescr *AMEM_Type = ocs::tom::AMEM::descriptor.e;
