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
 * This code was generated from file source/libs/sgeobj/json/MES.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Scheduling Message: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of MES, Job Scheduling Message
 *
 * The enumeration is the type of every field number of MES, so a parameter
 * of this type takes the field numbers of MES only.
 */
enum MES_Field {
   /**
    * @brief Job Number List
    *
    * A list of job ids to which the message applies.
    * These are usually all jobs belonging to a specific job category (sharing the same requests).
    */
   MES_job_number_list = MES_LOWERBOUND,
   /**
    * @brief Message Number
    *
    * Every possible scheduler message has an unique message number.
    */
   MES_message_number,
   /**
    * @brief Message
    *
    * The scheduler message as string.
    */
   MES_message,
};

namespace ocs::tom {

   struct ULNG;

   /**
    * @brief Job Scheduling Message
    *
    * An individual scheduling message,
    * e.g. explaining why a specific request of a certain job cannot be granted.
    */
   struct MES {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {MES_job_number_list, "MES_job_number_list", FieldType::List, Flag::None},
         {MES_message_number, "MES_message_number", FieldType::Uint32, Flag::None},
         {MES_message, "MES_message", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of MES belongs to MES */
   template <>
   struct TypeOf<MES_Field> {
      using type = MES;   ///< the object type
   };

   /** @brief MES_job_number_list holds elements of ULNG */
   template <>
   struct SubType<MES_job_number_list> {
      using type = ULNG;   ///< the sub-type
   };

} // namespace ocs::tom
