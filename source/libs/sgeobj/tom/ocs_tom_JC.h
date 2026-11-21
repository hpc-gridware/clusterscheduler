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
 * This code was generated from file source/libs/sgeobj/json/JC.json
 * DO NOT CHANGE
 */

/** @file
 * @brief User Job Count: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of JC, User Job Count
 *
 * The enumeration is the type of every field number of JC, so a parameter
 * of this type takes the field numbers of JC only.
 */
enum JC_Field {
   /**
    * @brief User Name
    *
    * User or user group name
    */
   JC_name = JC_LOWERBOUND,
   /**
    * @brief Number of Jobs
    *
    * Number of running jobs of this user / this user group.
    */
   JC_jobs,
};

namespace ocs::tom {

   /**
    * @brief User Job Count
    *
    * Job count per user or user group.
    * this list is used by scheduler thread to keep the number
    * of running jobs per user/group efficiently
    */
   struct JC {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JC_name, "JC_name", FieldType::String, Flag::Unique | Flag::Hash},
         {JC_jobs, "JC_jobs", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JC belongs to JC */
   template <>
   struct TypeOf<JC_Field> {
      using type = JC;   ///< the object type
   };

} // namespace ocs::tom
