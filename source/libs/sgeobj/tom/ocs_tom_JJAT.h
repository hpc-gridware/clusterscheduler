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
 * This code was generated from file source/libs/sgeobj/json/JJAT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief JAPI Array Task: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of JJAT, JAPI Array Task
 *
 * The enumeration is the type of every field number of JJAT, so a parameter
 * of this type takes the field numbers of JJAT only.
 */
enum JJAT_Field {
   /**
    * @brief Task Id
    *
    * Array Task Id (equal to JAT_task_number).
    */
   JJAT_task_id = JJAT_LOWERBOUND,
   /**
    * @brief Stat
    *
    * Stat information provided by japi_wait().
    */
   JJAT_stat,
   /**
    * @brief RUsage
    *
    * Resource usage information provided by japi_wait()
    */
   JJAT_rusage,
   /**
    * @brief Failed Text
    *
    * Printable error text describing reason of job error.
    */
   JJAT_failed_text,
};

namespace ocs::tom {

   struct UA;

   /**
    * @brief JAPI Array Task
    *
    * Element holding per job array task information about JAPI session
    */
   struct JJAT {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JJAT_task_id, "JJAT_task_id", FieldType::Uint32, Flag::None},
         {JJAT_stat, "JJAT_stat", FieldType::Uint32, Flag::None},
         {JJAT_rusage, "JJAT_rusage", FieldType::List, Flag::None},
         {JJAT_failed_text, "JJAT_failed_text", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JJAT belongs to JJAT */
   template <>
   struct TypeOf<JJAT_Field> {
      using type = JJAT;   ///< the object type
   };

   /** @brief JJAT_rusage holds elements of UA */
   template <>
   struct SubType<JJAT_rusage> {
      using type = UA;   ///< the sub-type
   };

} // namespace ocs::tom
