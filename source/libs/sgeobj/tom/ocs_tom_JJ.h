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
 * This code was generated from file source/libs/sgeobj/json/JJ.json
 * DO NOT CHANGE
 */

/** @file
 * @brief JAPI Job: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of JJ, JAPI Job
 *
 * The enumeration is the type of every field number of JJ, so a parameter
 * of this type takes the field numbers of JJ only.
 */
enum JJ_Field {
   /**
    * @brief Job Id
    *
    * JAPI job id.
    */
   JJ_jobid = JJ_LOWERBOUND,
   /**
    * @brief Type
    *
    * Job type - analoguous to JB_type.
    */
   JJ_type,
   /**
    * @brief Finished Tasks
    *
    * List of finished job tasks.
    */
   JJ_finished_tasks,
   /**
    * @brief Not Yet Finished Ids
    *
    * Ids of not yet finished tasks.
    */
   JJ_not_yet_finished_ids,
   /**
    * @brief Started Task Ids
    *
    * Ids of started tasks.
    */
   JJ_started_task_ids,
};

namespace ocs::tom {

   struct JJAT;
   struct RN;

   /**
    * @brief JAPI Job
    *
    * Element holding per job information about JAPI session.
    */
   struct JJ {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JJ_jobid, "JJ_jobid", FieldType::Uint32, Flag::Unique | Flag::Hash},
         {JJ_type, "JJ_type", FieldType::Uint32, Flag::None},
         {JJ_finished_tasks, "JJ_finished_tasks", FieldType::List, Flag::None},
         {JJ_not_yet_finished_ids, "JJ_not_yet_finished_ids", FieldType::List, Flag::None},
         {JJ_started_task_ids, "JJ_started_task_ids", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JJ belongs to JJ */
   template <>
   struct TypeOf<JJ_Field> {
      using type = JJ;   ///< the object type
   };

   /** @brief JJ_finished_tasks holds elements of JJAT */
   template <>
   struct SubType<JJ_finished_tasks> {
      using type = JJAT;   ///< the sub-type
   };

   /** @brief JJ_not_yet_finished_ids holds elements of RN */
   template <>
   struct SubType<JJ_not_yet_finished_ids> {
      using type = RN;   ///< the sub-type
   };

   /** @brief JJ_started_task_ids holds elements of RN */
   template <>
   struct SubType<JJ_started_task_ids> {
      using type = RN;   ///< the sub-type
   };

} // namespace ocs::tom
