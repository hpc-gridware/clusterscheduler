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
 * This code was generated from file source/libs/sgeobj/json/VA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Variable Object: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of VA, Variable Object
 *
 * The enumeration is the type of every field number of VA, so a parameter
 * of this type takes the field numbers of VA only.
 */
enum VA_Field {
   /**
    * @brief Variable Name
    *
    * Name of the Variable.
    */
   VA_variable = VA_LOWERBOUND,
   /**
    * @brief Variable Value
    *
    * Value of the Variable as String.
    */
   VA_value,
};

namespace ocs::tom {

   /**
    * @brief Variable Object
    *
    * One variable objects holds a name/value pair (variable=[value]).
    * @see libs/sgeobj/sge_var.h
    */
   struct VA {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {VA_variable, "VA_variable", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {VA_value, "VA_value", FieldType::String, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of VA belongs to VA */
   template <>
   struct TypeOf<VA_Field> {
      using type = VA;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of VA for the list library: a synonym for ocs::tom::VA::descriptor */
inline constexpr const lDescr *VA_Type = ocs::tom::VA::descriptor.e;
