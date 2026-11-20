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
 * This code was generated from file source/libs/sgeobj/json/JRE.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Reference: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of JRE, Job Reference
 *
 * The enumeration is the type of every field number of JRE, so a parameter
 * of this type takes the field numbers of JRE only.
 */
enum JRE_Field {
   /**
    * @brief Job Number
    *
    * Job number of the referenced job.
    */
   JRE_job_number = JRE_LOWERBOUND,
   /**
    * @brief Job Name
    *
    * Job name of the referenced job(s). Multiple jobs can be referenced by the same name.
    */
   JRE_job_name,
};

namespace ocs::tom {

   /**
    * @brief Job Reference
    *
    * Reference to a job by job id or job name.
    * It is for example used to reference successor or predecessor jobs in job dependencies.
    */
   struct JRE {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JRE_job_number, "JRE_job_number", FieldType::Uint32, Flag::PrimaryKey | Flag::SpoolSublist},
         {JRE_job_name, "JRE_job_name", FieldType::String, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JRE belongs to JRE */
   template <>
   struct TypeOf<JRE_Field> {
      using type = JRE;   ///< the object type
   };

} // namespace ocs::tom
