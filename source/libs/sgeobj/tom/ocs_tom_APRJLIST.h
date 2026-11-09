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
 * This code was generated from file source/libs/sgeobj/json/APRJLIST.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute Project List: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of APRJLIST, Attribute Project List
 *
 * The enumeration is the type of every field number of APRJLIST, so a parameter
 * of this type takes the field numbers of APRJLIST only.
 */
enum APRJLIST_Field {
   /**
    * @brief Host Reference
    *
    * Name of a host or a host group.
    */
   APRJLIST_href = APRJLIST_LOWERBOUND,
   /**
    * @brief Value
    *
    * The project list (list of PR_Type)
    */
   APRJLIST_value,
};

namespace ocs::tom {

   struct PR;

   /**
    * @brief Attribute Project List
    *
    * Used for project list attributes in the cluster queues.
    * E.g. used for the queue projects, xprojects.
    */
   struct APRJLIST {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {APRJLIST_href, "APRJLIST_href", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {APRJLIST_value, "APRJLIST_value", FieldType::List, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of APRJLIST belongs to APRJLIST */
   template <>
   struct TypeOf<APRJLIST_Field> {
      using type = APRJLIST;   ///< the object type
   };

   /** @brief APRJLIST_value holds elements of PR */
   template <>
   struct SubType<APRJLIST_value> {
      using type = PR;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of APRJLIST for the list library: a synonym for ocs::tom::APRJLIST::descriptor */
inline constexpr const lDescr *APRJLIST_Type = ocs::tom::APRJLIST::descriptor.e;
