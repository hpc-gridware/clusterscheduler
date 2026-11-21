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
 * This code was generated from file source/libs/sgeobj/json/SPTR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Spooling Type Rule Mapping: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of SPTR, Spooling Type Rule Mapping
 *
 * The enumeration is the type of every field number of SPTR, so a parameter
 * of this type takes the field numbers of SPTR only.
 */
enum SPTR_Field {
   /**
    * @brief Is Default
    *
    * Defines whether the referenced rule is the default rule
    * for reading the defined object type.
    */
   SPTR_is_default = SPTR_LOWERBOUND,
   /**
    * @brief Rule Name
    *
    * Name of the referenced rule.
    */
   SPTR_rule_name,
   /**
    * @brief Rule
    *
    * Pointer/reference to the rule to be used with the defined object type.
    */
   SPTR_rule,
};

namespace ocs::tom {

   /**
    * @brief Spooling Type Rule Mapping
    *
    * Elements of SPTR define a mapping between object type (SPT)
    * and spooling rules (SPR).
    * One object type can be spooled (written) using multiple spooling rules.
    * One object type will be read using one (the default) spooling rule.
    * One spooling rule can be referenced by multiple object types.
    */
   struct SPTR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SPTR_is_default, "SPTR_is_default", FieldType::Bool, Flag::None},
         {SPTR_rule_name, "SPTR_rule_name", FieldType::String, Flag::Unique},
         {SPTR_rule, "SPTR_rule", FieldType::Ref, Flag::NoTransfer},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SPTR belongs to SPTR */
   template <>
   struct TypeOf<SPTR_Field> {
      using type = SPTR;   ///< the object type
   };

} // namespace ocs::tom
