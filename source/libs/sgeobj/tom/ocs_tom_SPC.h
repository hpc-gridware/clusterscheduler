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
 * This code was generated from file source/libs/sgeobj/json/SPC.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Spooling Context: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SPC, Spooling Context
 *
 * The enumeration is the type of every field number of SPC, so a parameter
 * of this type takes the field numbers of SPC only.
 */
enum SPC_Field {
   /**
    * @brief Name
    *
    * Unique name of the spooling context.
    */
   SPC_name = SPC_LOWERBOUND,
   /**
    * @brief Rules
    *
    * List of spooling rules.
    */
   SPC_rules,
   /**
    * @brief Types
    *
    * List of spoolable object types with references to rules.
    */
   SPC_types,
};

namespace ocs::tom {

   struct SPR;
   struct SPT;

   /**
    * @brief Spooling Context
    *
    * A spooling context describes the way how objects
    * are spooled (read and written).
    *  
    * A spooling context contains one or multiple rules for
    * spooling. A rule can for example describe a database connection.
    *  
    * It also contains a list of types that can be spooled.
    * A default entry for all types can be created; if type entries
    * for individual types exist, these entries will be used for spooling.
    * A type references one or multiple rules which will
    * be executed for writing or deleting data.
    * Exactly one rule can be defined to be the default rule
    * for reading objects.
    * +----------+       1:n       +----------+
    * | SPC_Type |----------------<| SPT_Type |
    * +----------+                 +----------+
    *      |                             |
    *      | 1                           |
    *      | :                           |
    *      | n                           |
    *      |                             |
    *      ^                             |
    * +----------+   1:n, one is default |
    * | SPR_Type |>----------------------+
    * +----------+
    */
   struct SPC {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SPC_name, "SPC_name", FieldType::String, Flag::Unique | Flag::Hash},
         {SPC_rules, "SPC_rules", FieldType::List, Flag::None},
         {SPC_types, "SPC_types", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SPC belongs to SPC */
   template <>
   struct TypeOf<SPC_Field> {
      using type = SPC;   ///< the object type
   };

   /** @brief SPC_rules holds elements of SPR */
   template <>
   struct SubType<SPC_rules> {
      using type = SPR;   ///< the sub-type
   };

   /** @brief SPC_types holds elements of SPT */
   template <>
   struct SubType<SPC_types> {
      using type = SPT;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of SPC for the list library: a synonym for ocs::tom::SPC::descriptor */
inline constexpr const lDescr *SPC_Type = ocs::tom::SPC::descriptor.e;
