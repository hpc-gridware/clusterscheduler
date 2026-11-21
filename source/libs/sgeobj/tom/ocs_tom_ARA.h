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
 * This code was generated from file source/libs/sgeobj/json/ARA.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Advance Reservation Access List Entry: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of ARA, Advance Reservation Access List Entry
 *
 * The enumeration is the type of every field number of ARA, so a parameter
 * of this type takes the field numbers of ARA only.
 */
enum ARA_Field {
   /**
    * @brief Name
    *
    * The user or userset allowed, or denied, access to the reservation.
    */
   ARA_name = ARA_LOWERBOUND,
   /**
    * @brief Group
    *
    * The UNIX group the entry stands for, where the entry names a group.
    */
   ARA_group,
};

namespace ocs::tom {

   /**
    * @brief Advance Reservation Access List Entry
    *
    * One user or group entry in an advance reservation's access or no-access list.
    */
   struct ARA {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ARA_name, "ARA_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {ARA_group, "ARA_group", FieldType::String, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ARA belongs to ARA */
   template <>
   struct TypeOf<ARA_Field> {
      using type = ARA;   ///< the object type
   };

} // namespace ocs::tom
