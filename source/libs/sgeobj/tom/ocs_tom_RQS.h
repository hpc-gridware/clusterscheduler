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
 * This code was generated from file source/libs/sgeobj/json/RQS.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Set: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of RQS, Resource Quota Set
 *
 * The enumeration is the type of every field number of RQS, so a parameter
 * of this type takes the field numbers of RQS only.
 */
enum RQS_Field {
   /**
    * @brief Name
    *
    * The set's name, as used by `qconf -mrqs`.
    */
   RQS_name = RQS_LOWERBOUND,
   /**
    * @brief Description
    *
    * Free text describing what the set is for.
    */
   RQS_description,
   /**
    * @brief Enabled
    *
    * Whether the set is applied. A disabled set stays configured but limits nothing.
    */
   RQS_enabled,
   /**
    * @brief Rules
    *
    * The rules (`RQR`), in the order they are tried.
    */
   RQS_rule,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   RQS_joker,
};

namespace ocs::tom {

   struct RQR;
   struct VA;

   /**
    * @brief Resource Quota Set
    *
    * A named, enableable set of rules that limit resource use across the object model.
    * Where a queue or host limits what it can give, a resource quota set limits what a *combination* may take - "this project may use 100 slots in total, wherever they are". The rules are tried in order and the first that matches decides.
    */
   struct RQS {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RQS_name, "RQS_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {RQS_description, "RQS_description", FieldType::String, Flag::Spool},
         {RQS_enabled, "RQS_enabled", FieldType::Bool, Flag::Spool},
         {RQS_rule, "RQS_rule", FieldType::List, Flag::Spool},
         {RQS_joker, "RQS_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RQS belongs to RQS */
   template <>
   struct TypeOf<RQS_Field> {
      using type = RQS;   ///< the object type
   };

   /** @brief RQS_rule holds elements of RQR */
   template <>
   struct SubType<RQS_rule> {
      using type = RQR;   ///< the sub-type
   };

   /** @brief RQS_joker holds elements of VA */
   template <>
   struct SubType<RQS_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
