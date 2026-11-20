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
 * This code was generated from file source/libs/sgeobj/json/SPP.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Stored Procedure Parameter: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SPP, Stored Procedure Parameter
 *
 * The enumeration is the type of every field number of SPP, so a parameter
 * of this type takes the field numbers of SPP only.
 */
enum SPP_Field {
   /**
    * @brief Parameter name
    *
    * Name of the parameter list.
    */
   SPP_name = SPP_LOWERBOUND,
   /**
    * @brief List of value for this parameter.
    *
    * One or more entries for this parameter. The content of the list is defined by the user of the parameter list and can be used to store any kind of information, e.g. a list of strings, a list of numbers, a list of references to other objects etc.
    */
   SPP_value_list,
};

namespace ocs::tom {

   /**
    * @brief Stored Procedure Parameter
    *
    * An Object of this type stores parameter list and a name for that list.
    */
   struct SPP {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SPP_name, "SPP_name", FieldType::String, Flag::PrimaryKey | Flag::Hash},
         {SPP_value_list, "SPP_value_list", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SPP belongs to SPP */
   template <>
   struct TypeOf<SPP_Field> {
      using type = SPP;   ///< the object type
   };

} // namespace ocs::tom
