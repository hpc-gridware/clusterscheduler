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
 * This code was generated from file source/libs/sgeobj/json/AQTLIST.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute Queue Type List: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of AQTLIST, Attribute Queue Type List
 *
 * The enumeration is the type of every field number of AQTLIST, so a parameter
 * of this type takes the field numbers of AQTLIST only.
 */
enum AQTLIST_Field {
   /**
    * @brief Host Reference
    *
    * Name of a host or a host group.
    */
   AQTLIST_href = AQTLIST_LOWERBOUND,
   /**
    * @brief Value
    *
    * The queue type list (list of ulong values representing types).
    */
   AQTLIST_value,
};

namespace ocs::tom {

   /**
    * @brief Attribute Queue Type List
    *
    * Used for queue type list attributes in the cluster queues.
    * Used for the qtype queue attribute.
    */
   struct AQTLIST {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {AQTLIST_href, "AQTLIST_href", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {AQTLIST_value, "AQTLIST_value", FieldType::Uint32, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of AQTLIST belongs to AQTLIST */
   template <>
   struct TypeOf<AQTLIST_Field> {
      using type = AQTLIST;   ///< the object type
   };

} // namespace ocs::tom
