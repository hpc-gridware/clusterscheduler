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
 * This code was generated from file source/libs/sgeobj/json/NSV.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Named String Vector: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of NSV, Named String Vector
 *
 * The enumeration is the type of every field number of NSV, so a parameter
 * of this type takes the field numbers of NSV only.
 */
enum NSV_Field {
   /**
    * @brief Name
    *
    * Name of the string vector.
    */
   NSV_name = NSV_LOWERBOUND,
   /**
    * @brief Strings
    *
    * Strings of this string vector.
    */
   NSV_strings,
};

namespace ocs::tom {

   struct ST;

   /**
    * @brief Named String Vector
    *
    * Used in the DRMAA library to implement vector job template attributes.
    */
   struct NSV {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {NSV_name, "NSV_name", FieldType::String, Flag::None},
         {NSV_strings, "NSV_strings", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of NSV belongs to NSV */
   template <>
   struct TypeOf<NSV_Field> {
      using type = NSV;   ///< the object type
   };

   /** @brief NSV_strings holds elements of ST */
   template <>
   struct SubType<NSV_strings> {
      using type = ST;   ///< the sub-type
   };

} // namespace ocs::tom
