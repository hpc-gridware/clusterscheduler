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
 * This code was generated from file source/libs/sgeobj/json/RU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Reschedule Unknown: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of RU, Reschedule Unknown
 *
 * The enumeration is the type of every field number of RU, so a parameter
 * of this type takes the field numbers of RU only.
 */
enum RU_Field {
   /**
    * @brief Job Number
    *
    * The job number (from JB_job_number).
    */
   RU_job_number = RU_LOWERBOUND,
   /**
    * @brief Task Number
    *
    * The array task number (from JAT_task_number).
    */
   RU_task_number,
   /**
    * @brief State
    *
    * The rescheduling state, one of
    * RESCHEDULE_SKIP_JR_REMOVE
    * RESCHEDULE_SKIP_JR_SEND_ACK
    * RESCHEDULE_SKIP_JR
    * RESCHEDULE_HANDLE_JR_REMOVE
    * RESCHEDULE_HANDLE_JR_WAIT
    * @todo add more information, esp. about the meaning of the states.
    */
   RU_state,
};

namespace ocs::tom {

   /**
    * @brief Reschedule Unknown
    *
    * Objects of this types are created for jobs which are rescheduled when a host goes into unknown state.
    */
   struct RU {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RU_job_number, "RU_job_number", FieldType::Uint32, Flag::SpoolSublist},
         {RU_task_number, "RU_task_number", FieldType::Uint32, Flag::SpoolSublist},
         {RU_state, "RU_state", FieldType::Uint32, Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RU belongs to RU */
   template <>
   struct TypeOf<RU_Field> {
      using type = RU;   ///< the object type
   };

} // namespace ocs::tom
