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
 * This code was generated from file source/libs/sgeobj/json/PARA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Parameter: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of PARA, Parameter
 *
 * The enumeration is the type of every field number of PARA, so a parameter
 * of this type takes the field numbers of PARA only.
 */
enum PARA_Field {
   /**
    * @brief Name
    *
    * The parameter name, e.g. `profile`.
    */
   PARA_name = PARA_LOWERBOUND,
   /**
    * @brief Value
    *
    * Its value, always as text; the reader parses it.
    */
   PARA_value,
};

namespace ocs::tom {

   /**
    * @brief Parameter
    *
    * A name/value pair, used for the free-form `params` settings of the scheduler configuration.
    */
   struct PARA {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PARA_name, "PARA_name", FieldType::String, Flag::PrimaryKey},
         {PARA_value, "PARA_value", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PARA belongs to PARA */
   template <>
   struct TypeOf<PARA_Field> {
      using type = PARA;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of PARA for the list library: a synonym for ocs::tom::PARA::descriptor */
inline constexpr const lDescr *PARA_Type = ocs::tom::PARA::descriptor.e;
