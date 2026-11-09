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
 * This code was generated from file source/libs/sgeobj/json/JO.json
 * DO NOT CHANGE
 */

/** @file
 * @brief OS Job: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of JO, OS Job
 *
 * The enumeration is the type of every field number of JO, so a parameter
 * of this type takes the field numbers of JO only.
 */
enum JO_Field {
   /**
    * @brief OS Job Id
    *
    * OS job id (lower 32 bits)
    */
   JO_OS_job_ID = JO_LOWERBOUND,
   /**
    * @brief OS Job Id 2
    *
    * OS job id (upper 32 bits). @todo Replace the two id fields by one uint64_t?
    */
   JO_OS_job_ID2,
   /**
    * @brief Systemd Scope
    *
    * When the job is running in a systemd scope, this field contains the name of the scope.
    */
   JO_systemd_scope,
   /**
    * @brief Usage Collection Mode
    *
    * Usage collection mode used for the os job.
    * One of 0 (FALSE), 1 (PDC), 2 (HYBRID), 3 (TRUE).
    */
   JO_usage_collection,
   /**
    * @brief Array Task Id
    *
    * In case of an array job: Task number of an array task.
    */
   JO_ja_task_ID,
   /**
    * @brief Task Id String
    *
    * In case of a tightly integrated parallel job: Pe Task Id String.
    */
   JO_task_id_str,
   /**
    * @brief State
    *
    * Job state (JL_JOB_* values).
    */
   JO_state,
   /**
    * @brief Usage List
    *
    * PTF interval usage values.
    */
   JO_usage_list,
   /**
    * @brief Pid List
    *
    * List of process Ids belonging to this job/task.
    */
   JO_pid_list,
};

namespace ocs::tom {

   struct JP;
   struct UA;

   /**
    * @brief OS Job
    *
    * This is the list type we use to hold the list of
    * OS jobs being tracked for each PTF job. There
    * will normally only be one OS job per PTF job,
    * except in the case of tightly integrated parallel jobs or jobs with multiple tasks.
    */
   struct JO {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JO_OS_job_ID, "JO_OS_job_ID", FieldType::Uint32, Flag::None},
         {JO_OS_job_ID2, "JO_OS_job_ID2", FieldType::Uint32, Flag::None},
         {JO_systemd_scope, "JO_systemd_scope", FieldType::String, Flag::None},
         {JO_usage_collection, "JO_usage_collection", FieldType::Uint32, Flag::None},
         {JO_ja_task_ID, "JO_ja_task_ID", FieldType::Uint32, Flag::None},
         {JO_task_id_str, "JO_task_id_str", FieldType::String, Flag::None},
         {JO_state, "JO_state", FieldType::Uint32, Flag::None},
         {JO_usage_list, "JO_usage_list", FieldType::List, Flag::None},
         {JO_pid_list, "JO_pid_list", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JO belongs to JO */
   template <>
   struct TypeOf<JO_Field> {
      using type = JO;   ///< the object type
   };

   /** @brief JO_usage_list holds elements of UA */
   template <>
   struct SubType<JO_usage_list> {
      using type = UA;   ///< the sub-type
   };

   /** @brief JO_pid_list holds elements of JP */
   template <>
   struct SubType<JO_pid_list> {
      using type = JP;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of JO for the list library: a synonym for ocs::tom::JO::descriptor */
inline constexpr const lDescr *JO_Type = ocs::tom::JO::descriptor.e;
