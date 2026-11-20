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
 * This code was generated from file source/libs/sgeobj/json/SGEJ.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Sort Object: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SGEJ, Job Sort Object
 *
 * The enumeration is the type of every field number of SGEJ, so a parameter
 * of this type takes the field numbers of SGEJ only.
 */
enum SGEJ_Field {
   /**
    * @brief Priority
    *
    * Normalized priority of the job/array task after applying all policies.
    * The job list is sorted by priority descending as primary sort criterion.
    */
   SGEJ_priority = SGEJ_LOWERBOUND,
   /**
    * @brief Job Number
    *
    * The job number. Third sort criterion.
    */
   SGEJ_job_number,
   /**
    * @brief Job Name
    *
    * The job name.
    * @todo can be removed, it is only set, never read.
    */
   SGEJ_job_name,
   /**
    * @brief Owner
    *
    * The job owner.
    * @todo can be removed, it is only set, never read.
    */
   SGEJ_owner,
   /**
    * @brief State
    *
    * The state of an enrolled array task (JAT_state).
    * @todo can be removed, it is only set, never read.
    */
   SGEJ_state,
   /**
    * @brief Master Queue
    *
    * The master queue of an enrolled array task (JAT_master_queue).
    * @todo can be removed, it is only set, never read.
    */
   SGEJ_master_queue,
   /**
    * @brief Job Reference
    *
    * Reference (Pointer) to the job in the scheduler's job list.
    */
   SGEJ_job_reference,
   /**
    * @brief Submission Time
    *
    * The job submission time. Secondary sort criterion.
    */
   SGEJ_submission_time,
};

namespace ocs::tom {

   /**
    * @brief Job Sort Object
    *
    * This is the list type we use to sort the joblist in the sge scheduler.
    * The list will be build before the actual scheduling.
    * It is sorted by priorty ASC, submission time ASC, job number DESC.
    * Jobs in this list will then be scheduled one after the other.
    */
   struct SGEJ {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SGEJ_priority, "SGEJ_priority", FieldType::Double, Flag::None},
         {SGEJ_job_number, "SGEJ_job_number", FieldType::Uint32, Flag::None},
         {SGEJ_job_name, "SGEJ_job_name", FieldType::String, Flag::None},
         {SGEJ_owner, "SGEJ_owner", FieldType::String, Flag::None},
         {SGEJ_state, "SGEJ_state", FieldType::Uint32, Flag::None},
         {SGEJ_master_queue, "SGEJ_master_queue", FieldType::String, Flag::None},
         {SGEJ_job_reference, "SGEJ_job_reference", FieldType::Ref, Flag::NoTransfer},
         {SGEJ_submission_time, "SGEJ_submission_time", FieldType::Uint64, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SGEJ belongs to SGEJ */
   template <>
   struct TypeOf<SGEJ_Field> {
      using type = SGEJ;   ///< the object type
   };

} // namespace ocs::tom
