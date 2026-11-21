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
 * This code was generated from file source/libs/sgeobj/json/ACELIST.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute Centry List: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of ACELIST, Attribute Centry List
 *
 * The enumeration is the type of every field number of ACELIST, so a parameter
 * of this type takes the field numbers of ACELIST only.
 */
enum ACELIST_Field {
   /**
    * @brief Host Reference
    *
    * Name of a host or a host group.
    */
   ACELIST_href = ACELIST_LOWERBOUND,
   /**
    * @brief Value
    *
    * The complex entry list (list of CE).
    */
   ACELIST_value,
};

namespace ocs::tom {

   struct CE;

   /**
    * @brief Attribute Centry List
    *
    * Used for complex entry list attributes in the cluster queues.
    * E.g. used for the queue complex_values, load_thresholds, suspend_thresholds.
    */
   struct ACELIST {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ACELIST_href, "ACELIST_href", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {ACELIST_value, "ACELIST_value", FieldType::List, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ACELIST belongs to ACELIST */
   template <>
   struct TypeOf<ACELIST_Field> {
      using type = ACELIST;   ///< the object type
   };

   /** @brief ACELIST_value holds elements of CE */
   template <>
   struct SubType<ACELIST_value> {
      using type = CE;   ///< the sub-type
   };

} // namespace ocs::tom
