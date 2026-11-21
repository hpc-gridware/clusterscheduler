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
 * This code was generated from file source/libs/sgeobj/json/FPET.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Finished PE Task: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of FPET, Finished PE Task
 *
 * The enumeration is the type of every field number of FPET, so a parameter
 * of this type takes the field numbers of FPET only.
 */
enum FPET_Field {
   /**
    * @brief Finished PE Task Id
    *
    * PE Task Id identifying a finished PE Task.
    */
   FPET_id = FPET_LOWERBOUND,
};

namespace ocs::tom {

   /**
    * @brief Finished PE Task
    *
    * We need to store information about finished pe tasks to avoid
    * duplicate accounting records (see IZ 438).
    * A ja task will contain a list of finished pe tasks.
    * Only the task id of finished tasks will be stored.
    */
   struct FPET {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {FPET_id, "FPET_id", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of FPET belongs to FPET */
   template <>
   struct TypeOf<FPET_Field> {
      using type = FPET;   ///< the object type
   };

} // namespace ocs::tom
