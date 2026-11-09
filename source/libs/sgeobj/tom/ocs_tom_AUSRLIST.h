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
 * This code was generated from file source/libs/sgeobj/json/AUSRLIST.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute User Set List: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of AUSRLIST, Attribute User Set List
 *
 * The enumeration is the type of every field number of AUSRLIST, so a parameter
 * of this type takes the field numbers of AUSRLIST only.
 */
enum AUSRLIST_Field {
   /**
    * @brief Host Reference
    *
    * Name of a host or a host group.
    */
   AUSRLIST_href = AUSRLIST_LOWERBOUND,
   /**
    * @brief Value
    *
    * The user set list (list of US_Type).
    */
   AUSRLIST_value,
};

namespace ocs::tom {

   struct US;

   /**
    * @brief Attribute User Set List
    *
    * Used for user set list attributes in the cluster queues.
    * E.g. used for the queue owner, acl, xacl.
    */
   struct AUSRLIST {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {AUSRLIST_href, "AUSRLIST_href", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {AUSRLIST_value, "AUSRLIST_value", FieldType::List, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of AUSRLIST belongs to AUSRLIST */
   template <>
   struct TypeOf<AUSRLIST_Field> {
      using type = AUSRLIST;   ///< the object type
   };

   /** @brief AUSRLIST_value holds elements of US */
   template <>
   struct SubType<AUSRLIST_value> {
      using type = US;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of AUSRLIST for the list library: a synonym for ocs::tom::AUSRLIST::descriptor */
inline constexpr const lDescr *AUSRLIST_Type = ocs::tom::AUSRLIST::descriptor.e;
