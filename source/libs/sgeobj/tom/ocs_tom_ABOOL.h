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
 * This code was generated from file source/libs/sgeobj/json/ABOOL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute Boolean: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of ABOOL, Attribute Boolean
 *
 * The enumeration is the type of every field number of ABOOL, so a parameter
 * of this type takes the field numbers of ABOOL only.
 */
enum ABOOL_Field {
   /**
    * @brief Host Reference
    *
    * Name of a host or a host group.
    */
   ABOOL_href = ABOOL_LOWERBOUND,
   /**
    * @brief Value
    *
    * The boolean value.
    */
   ABOOL_value,
};

namespace ocs::tom {

   /**
    * @brief Attribute Boolean
    *
    * Used for boolean attributes in the cluster queues.
    * Used for the queue rerun attribute.
    */
   struct ABOOL {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ABOOL_href, "ABOOL_href", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {ABOOL_value, "ABOOL_value", FieldType::Bool, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ABOOL belongs to ABOOL */
   template <>
   struct TypeOf<ABOOL_Field> {
      using type = ABOOL;   ///< the object type
   };

} // namespace ocs::tom
