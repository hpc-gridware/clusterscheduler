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
 * This code was generated from file source/libs/sgeobj/json/ATIME.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Attribute Time: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of ATIME, Attribute Time
 *
 * The enumeration is the type of every field number of ATIME, so a parameter
 * of this type takes the field numbers of ATIME only.
 */
enum ATIME_Field {
   /**
    * @brief Host Reference
    *
    * Name of a host or a host group.
    */
   ATIME_href = ATIME_LOWERBOUND,
   /**
    * @brief Value
    *
    * The date/time value (represented as string).
    */
   ATIME_value,
};

namespace ocs::tom {

   /**
    * @brief Attribute Time
    *
    * Used for date/time attributes in the cluster queues.
    * E.g. used for the queue h_rt, s_rt, h_cpu, ...
    */
   struct ATIME {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {ATIME_href, "ATIME_href", FieldType::Host, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {ATIME_value, "ATIME_value", FieldType::String, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of ATIME belongs to ATIME */
   template <>
   struct TypeOf<ATIME_Field> {
      using type = ATIME;   ///< the object type
   };

} // namespace ocs::tom
