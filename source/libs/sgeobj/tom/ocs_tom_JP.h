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
 * This code was generated from file source/libs/sgeobj/json/JP.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Pid: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of JP, Job Pid
 *
 * The enumeration is the type of every field number of JP, so a parameter
 * of this type takes the field numbers of JP only.
 */
enum JP_Field {
   /**
    * @brief Pid
    *
    * Process Id.
    */
   JP_pid = JP_LOWERBOUND,
   /**
    * @brief Background
    *
    * Background flag. @todo looks as if it is not used, remove it.
    */
   JP_background,
};

namespace ocs::tom {

   /**
    * @brief Job Pid
    *
    * This is the list type we use to hold the process ID list for jobs in the PTF O.S. job list.
    */
   struct JP {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JP_pid, "JP_pid", FieldType::Uint32, Flag::Unique | Flag::Hash},
         {JP_background, "JP_background", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JP belongs to JP */
   template <>
   struct TypeOf<JP_Field> {
      using type = JP;   ///< the object type
   };

} // namespace ocs::tom
