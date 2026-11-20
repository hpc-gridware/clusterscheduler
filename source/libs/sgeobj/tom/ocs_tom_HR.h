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
 * This code was generated from file source/libs/sgeobj/json/HR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Host Reference: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of HR, Host Reference
 *
 * The enumeration is the type of every field number of HR, so a parameter
 * of this type takes the field numbers of HR only.
 */
enum HR_Field {
   /**
    * @brief Name
    *
    * The host name.
    */
   HR_name = HR_LOWERBOUND,
};

namespace ocs::tom {

   /**
    * @brief Host Reference
    *
    * Used to reference a host by name.
    */
   struct HR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {HR_name, "HR_name", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of HR belongs to HR */
   template <>
   struct TypeOf<HR_Field> {
      using type = HR;   ///< the object type
   };

} // namespace ocs::tom
