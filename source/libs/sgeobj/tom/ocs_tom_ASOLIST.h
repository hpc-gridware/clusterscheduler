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
 * This code was generated from file source/libs/sgeobj/json/ASOLIST.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute Subordinate List: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of ASOLIST, Attribute Subordinate List
 *
 * The enumeration is the type of every field number of ASOLIST, so a parameter
 * of this type takes the field numbers of ASOLIST only.
 */
enum ASOLIST_Field {
   /**
    * @brief Host Reference
    *
    * Name of a host or a host group.
    */
   ASOLIST_href = ASOLIST_LOWERBOUND,
   /**
    * @brief Value
    *
    * The subordinate list (list of SO).
    */
   ASOLIST_value,
};

namespace ocs::tom {

   struct SO;

   /**
    * @brief Attribute Subordinate List
    *
    * Used for subordinate list attributes in the cluster queues.
    * Used for the queue subordinate_list.
    */
   struct ASOLIST {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ASOLIST_href, "ASOLIST_href", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {ASOLIST_value, "ASOLIST_value", FieldType::List, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ASOLIST belongs to ASOLIST */
   template <>
   struct TypeOf<ASOLIST_Field> {
      using type = ASOLIST;   ///< the object type
   };

   /** @brief ASOLIST_value holds elements of SO */
   template <>
   struct SubType<ASOLIST_value> {
      using type = SO;   ///< the sub-type
   };

} // namespace ocs::tom
