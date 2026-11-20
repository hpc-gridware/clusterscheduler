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
 * This code was generated from file source/libs/sgeobj/json/ULNG.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Unsigned Long: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of ULNG, Unsigned Long
 *
 * The enumeration is the type of every field number of ULNG, so a parameter
 * of this type takes the field numbers of ULNG only.
 */
enum ULNG_Field {
   /**
    * @brief Value
    *
    * An ulong value.
    */
   ULNG_value = ULNG_LOWERBOUND,
};

namespace ocs::tom {

   /**
    * @brief Unsigned Long
    *
    * Used for lists / sublists holding 32bit ulong values, e.g. job ids.
    */
   struct ULNG {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ULNG_value, "ULNG_value", FieldType::Uint32, Flag::Unique | Flag::Hash},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ULNG belongs to ULNG */
   template <>
   struct TypeOf<ULNG_Field> {
      using type = ULNG;   ///< the object type
   };

} // namespace ocs::tom
