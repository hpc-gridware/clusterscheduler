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
 * This code was generated from file source/libs/sgeobj/json/ASTRLIST.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute String List: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of ASTRLIST, Attribute String List
 *
 * The enumeration is the type of every field number of ASTRLIST, so a parameter
 * of this type takes the field numbers of ASTRLIST only.
 */
enum ASTRLIST_Field {
   /**
    * @brief Host Reference
    *
    * Name of a host or a host group.
    */
   ASTRLIST_href = ASTRLIST_LOWERBOUND,
   /**
    * @brief Value
    *
    * The string list (list of ST_Type objects)
    */
   ASTRLIST_value,
};

namespace ocs::tom {

   struct ST;

   /**
    * @brief Attribute String List
    *
    * Used for string list attributes in the cluster queues.
    * E.g. used for the queue pe_list and ckpt_list.
    */
   struct ASTRLIST {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ASTRLIST_href, "ASTRLIST_href", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {ASTRLIST_value, "ASTRLIST_value", FieldType::List, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ASTRLIST belongs to ASTRLIST */
   template <>
   struct TypeOf<ASTRLIST_Field> {
      using type = ASTRLIST;   ///< the object type
   };

   /** @brief ASTRLIST_value holds elements of ST */
   template <>
   struct SubType<ASTRLIST_value> {
      using type = ST;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of ASTRLIST for the list library: a synonym for ocs::tom::ASTRLIST::descriptor */
inline constexpr const lDescr *ASTRLIST_Type = ocs::tom::ASTRLIST::descriptor.e;
