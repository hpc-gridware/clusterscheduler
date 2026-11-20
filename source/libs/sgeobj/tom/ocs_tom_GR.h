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
 * This code was generated from file source/libs/sgeobj/json/GR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Group Id: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of GR, Group Id
 *
 * The enumeration is the type of every field number of GR, so a parameter
 * of this type takes the field numbers of GR only.
 */
enum GR_Field {
   /**
    * @brief Group Id
    *
    * The numeric group id. Read positionally by the process data collector.
    */
   GR_group = GR_LOWERBOUND,
};

namespace ocs::tom {

   /**
    * @brief Group Id
    *
    * A single UNIX group id, used where a list of supplementary groups is needed.
    */
   struct GR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {GR_group, "GR_group", FieldType::Uint32, Flag::Hash},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of GR belongs to GR */
   template <>
   struct TypeOf<GR_Field> {
      using type = GR;   ///< the object type
   };

} // namespace ocs::tom
