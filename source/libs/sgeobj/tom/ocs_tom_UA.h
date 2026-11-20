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
 * This code was generated from file source/libs/sgeobj/json/UA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Usage Value: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of UA, Usage Value
 *
 * The enumeration is the type of every field number of UA, so a parameter
 * of this type takes the field numbers of UA only.
 */
enum UA_Field {
   /**
    * @brief Name
    *
    * Name of the usage value, e.g. cpu, mem, vmem, maxvmem, ...
    */
   UA_name = UA_LOWERBOUND,
   /**
    * @brief Value
    *
    * Usage value as a double.
    */
   UA_value,
   /**
    * @brief String Value
    *
    * Usage value as a string, for custom usage values that are not numeric. Nullptr when the value is numeric — presence of svalue is the discriminator between string and numeric custom usage. Standard usage values (USAGE_ATTR_* names) never carry svalue. See CS-849.
    */
   UA_svalue,
};

namespace ocs::tom {

   /**
    * @brief Usage Value
    *
    * Usage values hold the job usage information retrieved by sge_execd per job / array task / pe task.
    */
   struct UA {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {UA_name, "UA_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {UA_value, "UA_value", FieldType::Double, Flag::SpoolSublist},
         {UA_svalue, "UA_svalue", FieldType::String, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of UA belongs to UA */
   template <>
   struct TypeOf<UA_Field> {
      using type = UA;   ///< the object type
   };

} // namespace ocs::tom
