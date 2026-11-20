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
 * This code was generated from file source/libs/sgeobj/json/SCT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Scheduler Category Reference (unused): object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of SCT, Scheduler Category Reference (unused)
 *
 * The enumeration is the type of every field number of SCT, so a parameter
 * of this type takes the field numbers of SCT only.
 */
enum SCT_Field {
   /**
    * @brief Category String
    *
    * Part of the unused SCT object; see the object description.
    */
   SCT_str = SCT_LOWERBOUND,
   /**
    * @brief Pending Jobs
    *
    * Part of the unused SCT object; see the object description.
    */
   SCT_job_pending_ref,
   /**
    * @brief Running Jobs
    *
    * Part of the unused SCT object; see the object description.
    */
   SCT_job_ref,
};

namespace ocs::tom {

   struct REF;

   /**
    * @brief Scheduler Category Reference (unused)
    *
    * A category string with the pending and running jobs that belong to it.
    * @warning No attribute of this type is read or written anywhere in either repository, and `SCT` itself is referenced nowhere. Left in place because removing a type changes the spooled object model.
    */
   struct SCT {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {SCT_str, "SCT_str", FieldType::String, Flag::Unique | Flag::Hash},
         {SCT_job_pending_ref, "SCT_job_pending_ref", FieldType::List, Flag::None},
         {SCT_job_ref, "SCT_job_ref", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of SCT belongs to SCT */
   template <>
   struct TypeOf<SCT_Field> {
      using type = SCT;   ///< the object type
   };

   /** @brief SCT_job_pending_ref holds elements of REF */
   template <>
   struct SubType<SCT_job_pending_ref> {
      using type = REF;   ///< the sub-type
   };

   /** @brief SCT_job_ref holds elements of REF */
   template <>
   struct SubType<SCT_job_ref> {
      using type = REF;   ///< the sub-type
   };

} // namespace ocs::tom
