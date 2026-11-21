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
 * This code was generated from file source/libs/sgeobj/json/RT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Remote Task: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of RT, Remote Task
 *
 * The enumeration is the type of every field number of RT, so a parameter
 * of this type takes the field numbers of RT only.
 */
enum RT_Field {
   /**
    * @brief Task Id
    *
    * Task Id.
    */
   RT_tid = RT_LOWERBOUND,
   /**
    * @brief Hostname
    *
    * Remote host where the task is running on.
    */
   RT_hostname,
   /**
    * @brief Status
    *
    * Status as it comes from waitpid(2).
    */
   RT_status,
   /**
    * @brief State
    *
    * Internal state used by the qrexec module.
    */
   RT_state,
};

namespace ocs::tom {

   /**
    * @brief Remote Task
    *
    * An object of this type represents a remote task.
    * It is used in qrsh -inherit starting tasks within a tightly integrated parallel job.
    */
   struct RT {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {RT_tid, "RT_tid", FieldType::String, Flag::None},
         {RT_hostname, "RT_hostname", FieldType::Host, Flag::None},
         {RT_status, "RT_status", FieldType::Uint32, Flag::None},
         {RT_state, "RT_state", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of RT belongs to RT */
   template <>
   struct TypeOf<RT_Field> {
      using type = RT;   ///< the object type
   };

} // namespace ocs::tom
