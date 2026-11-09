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
 * This code was generated from file source/libs/sgeobj/json/SU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Submit User: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SU, Submit User
 *
 * The enumeration is the type of every field number of SU, so a parameter
 * of this type takes the field numbers of SU only.
 */
enum SU_Field {
   /**
    * @brief Name
    *
    * User Name
    */
   SU_name = SU_LOWERBOUND,
   /**
    * @brief Jobs
    *
    * Currently active jobs for this user (including jobs in finished state).
    */
   SU_jobs,
};

namespace ocs::tom {

   /**
    * @brief Submit User
    *
    * The list of these objects defines which users are allowed to submit jobs into Cluster Scheduler.
    * It is also used to count the number of active jobs of the user.
    */
   struct SU {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SU_name, "SU_name", FieldType::String, Flag::Unique | Flag::Hash},
         {SU_jobs, "SU_jobs", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SU belongs to SU */
   template <>
   struct TypeOf<SU_Field> {
      using type = SU;   ///< the object type
   };

} // namespace ocs::tom

/** @brief The descriptor of SU for the list library: a synonym for ocs::tom::SU::descriptor */
inline constexpr const lDescr *SU_Type = ocs::tom::SU::descriptor.e;
