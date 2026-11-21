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
 * This code was generated from file source/libs/sgeobj/json/PETR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief PE Task Request: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of PETR, PE Task Request
 *
 * The enumeration is the type of every field number of PETR, so a parameter
 * of this type takes the field numbers of PETR only.
 */
enum PETR_Field {
   /**
    * @brief Job ID
    *
    * Job id of the job to start the task in.
    */
   PETR_jobid = PETR_LOWERBOUND,
   /**
    * @brief Array Task ID
    *
    * Array task id of the ja_task to start the pe task in.
    */
   PETR_jataskid,
   /**
    * @brief Queue Name
    *
    * Optionally: Name of the queue to start the job in.
    */
   PETR_queuename,
   /**
    * @brief Owner
    *
    * Owner of the pe task.
    */
   PETR_owner,
   /**
    * @brief Current Working Directory
    *
    * Current working directory for the execution of the task.
    */
   PETR_cwd,
   /**
    * @brief Path Aliases
    *
    * Optional: Path alias configuration.
    */
   PETR_path_aliases,
   /**
    * @brief Environment
    *
    * Environment variables to set / to overwrite the job environment with (from qrsh -inherit -v).
    */
   PETR_environment,
   /**
    * @brief Submission Time
    *
    * Time when qrsh -inherit was started in microseconds since epoch.
    */
   PETR_submission_time,
   /**
    * @brief TLS Credentials
    *
    * Used with the TLS encryption to transport the credentials
    * from qrsh to the sge_execd, which will store it in the pe_task's active_jobs directory.
    */
   PETR_cred,
};

namespace ocs::tom {

   struct PA;
   struct VA;

   /**
    * @brief PE Task Request
    *
    * Objects of PETR are used to request the start of a task in a
    * tightly integrated parallel job.
    */
   struct PETR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PETR_jobid, "PETR_jobid", FieldType::Uint32, Flag::None},
         {PETR_jataskid, "PETR_jataskid", FieldType::Uint32, Flag::None},
         {PETR_queuename, "PETR_queuename", FieldType::String, Flag::None},
         {PETR_owner, "PETR_owner", FieldType::String, Flag::None},
         {PETR_cwd, "PETR_cwd", FieldType::String, Flag::None},
         {PETR_path_aliases, "PETR_path_aliases", FieldType::List, Flag::None},
         {PETR_environment, "PETR_environment", FieldType::List, Flag::None},
         {PETR_submission_time, "PETR_submission_time", FieldType::Uint64, Flag::None},
         {PETR_cred, "PETR_cred", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PETR belongs to PETR */
   template <>
   struct TypeOf<PETR_Field> {
      using type = PETR;   ///< the object type
   };

   /** @brief PETR_path_aliases holds elements of PA */
   template <>
   struct SubType<PETR_path_aliases> {
      using type = PA;   ///< the sub-type
   };

   /** @brief PETR_environment holds elements of VA */
   template <>
   struct SubType<PETR_environment> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
