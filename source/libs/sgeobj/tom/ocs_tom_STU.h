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
 * This code was generated from file source/libs/sgeobj/json/STU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Unique String: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of STU, Unique String
 *
 * The enumeration is the type of every field number of STU, so a parameter
 * of this type takes the field numbers of STU only.
 */
enum STU_Field {
   /**
    * @brief String Name
    *
    * String contents.
    */
   STU_name = STU_LOWERBOUND,
};

namespace ocs::tom {

   /**
    * @brief Unique String
    *
    * An object of this type holds a string which is unique in a List of STU_Type.
    */
   struct STU {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {STU_name, "STU_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of STU belongs to STU */
   template <>
   struct TypeOf<STU_Field> {
      using type = STU;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of STU for the list library: a synonym for ocs::tom::STU::descriptor */
inline constexpr const lDescr *STU_Type = ocs::tom::STU::descriptor.e;
