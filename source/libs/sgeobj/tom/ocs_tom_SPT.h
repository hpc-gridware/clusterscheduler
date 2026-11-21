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
 * This code was generated from file source/libs/sgeobj/json/SPT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Spooling Type: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of SPT, Spooling Type
 *
 * The enumeration is the type of every field number of SPT, so a parameter
 * of this type takes the field numbers of SPT only.
 */
enum SPT_Field {
   /**
    * @brief Type
    *
    * Unique type identifier.
    * See enum sge_object_type in libs/gdi/sge_mirror.h
    * SGE_TYPE_ALL describes a default type entry for all object types.
    */
   SPT_type = SPT_LOWERBOUND,
   /**
    * @brief Name
    *
    * Name of the type - used for informational messages etc.
    */
   SPT_name,
   /**
    * @brief Rules
    *
    * List of rules that can be applied for a certain object type.
    * Does not reference the rules themselves, but contains mapping
    * objects mapping between type and rule.
    */
   SPT_rules,
};

namespace ocs::tom {

   struct SPTR;

   /**
    * @brief Spooling Type
    *
    * Objects to be spooled have a certain type that can be identified by the sge_object_type enum.
    * A spooling context can contain information about individual
    * types and/or define a default behaviour for all (not individually handled) types.
    * The spooling behaviour for a type is defined by a list of references
    * to rules in the spooling context.
    * One of the referenced spooling rules has to be made default rule
    * for reading objects.
    */
   struct SPT {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SPT_type, "SPT_type", FieldType::Uint32, Flag::Unique | Flag::Hash},
         {SPT_name, "SPT_name", FieldType::String, Flag::None},
         {SPT_rules, "SPT_rules", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SPT belongs to SPT */
   template <>
   struct TypeOf<SPT_Field> {
      using type = SPT;   ///< the object type
   };

   /** @brief SPT_rules holds elements of SPTR */
   template <>
   struct SubType<SPT_rules> {
      using type = SPTR;   ///< the sub-type
   };

} // namespace ocs::tom
