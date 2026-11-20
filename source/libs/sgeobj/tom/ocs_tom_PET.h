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
 * This code was generated from file source/libs/sgeobj/json/PET.json
 * DO NOT CHANGE
 */

/** @file
 * @brief PE Task: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of PET, PE Task
 *
 * The enumeration is the type of every field number of PET, so a parameter
 * of this type takes the field numbers of PET only.
 */
enum PET_Field {
   /**
    * @brief PE Task Id
    *
    * The pe task id. It is unique per job.
    */
   PET_id = PET_LOWERBOUND,
   /**
    * @brief Task Name
    *
    * The name of this parallel environment task, used in the job's output file names and in accounting. Defaults to `petask`.
    */
   PET_name,
   /**
    * @brief Status
    *
    * Status of the pe job, see defines in libs/gdi/sge_jobL.h.
    */
   PET_status,
   /**
    * @brief Granted Destination Identifier List
    *
    * Contains one entry specifying the queue the pe task runs in.
    */
   PET_granted_destin_identifier_list,
   /**
    * @brief Process Id
    *
    * Pid of a running pe task (process group id)
    */
   PET_pid,
   /**
    * @brief OS JobId
    *
    * OS jobid / additional group id of a running pe task.
    */
   PET_osjobid,
   /**
    * @brief Systemd Scope
    *
    * In case a pe task is started in a systemd scope, this is the
    * scope unit name.
    * Only used in sge_execd, it is set during the startup phase of the
    * pe task.
    */
   PET_systemd_scope,
   /**
    * @brief Usage Collection Mode
    *
    * Usage collection mode of a pe task.
    * One of 0 (FALSE), 1 (PDC), 2 (HYBRID), 3 (TRUE).
    */
   PET_usage_collection,
   /**
    * @brief Usage
    *
    * Usage list of a running/just finished pe task.
    */
   PET_usage,
   /**
    * @brief Scaled Usage
    *
    * Scaled usage list of a running/just finished pe task.
    */
   PET_scaled_usage,
   /**
    * @brief Reported Usage
    *
    * Used in reporting of long running jobs:
    * An intermediate usage record is written around midnight every day,
    * the usage already reported so far is stored in the PET_reported_usage list
    */
   PET_reported_usage,
   /**
    * @brief Previous Usage
    *
    * In case a petask is re-run (@todo is this ever done?) the usage of the previous run.
    */
   PET_previous_usage,
   /**
    * @brief Submission Time
    *
    * Submission time of a task (when qrsh -inherit was called) in microseconds since epoch
    */
   PET_submission_time,
   /**
    * @brief Start Time
    *
    * Start time of the petask in microseconds since epoch
    */
   PET_start_time,
   /**
    * @brief End Time
    *
    * End time of the petask in microseconds since epoch
    */
   PET_end_time,
   /**
    * @brief Current Working Directory
    *
    * Path to the petask's current working directory.
    * If not set, the cwd from the ja task is inherited.
    */
   PET_cwd,
   /**
    * @brief Path Aliases
    *
    * Path alias list for the petask.
    */
   PET_path_aliases,
   /**
    * @brief Environment
    *
    * Environment variables to be set when executing the job (qrsh inherit -v).
    * They will overwrite inherited variables from the ja task.
    */
   PET_environment,
   /**
    * @brief Do Contact
    *
    * Used in limit enforcement to remember whether to contact the host where a task is running.
    * Is set to false when the host is down/unknown.
    */
   PET_do_contact,
   /**
    * @brief TLS Credentials
    *
    * Used with the TLS encryption to transport the credentials
    * from qrsh to the sge_execd, which will store it in the pe_task's active_jobs directory.
    */
   PET_cred,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   PET_joker,
};

namespace ocs::tom {

   struct JG;
   struct PA;
   struct UA;
   struct VA;

   /**
    * @brief PE Task
    *
    * A PET object describes one task of a tightly integrated parallel job.
    */
   struct PET {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PET_id, "PET_id", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {PET_name, "PET_name", FieldType::String, Flag::SpoolSublist},
         {PET_status, "PET_status", FieldType::Uint32, Flag::SpoolSublist},
         {PET_granted_destin_identifier_list, "PET_granted_destin_identifier_list", FieldType::List, Flag::SpoolSublist},
         {PET_pid, "PET_pid", FieldType::Uint32, Flag::None},
         {PET_osjobid, "PET_osjobid", FieldType::String, Flag::None},
         {PET_systemd_scope, "PET_systemd_scope", FieldType::String, Flag::None},
         {PET_usage_collection, "PET_usage_collection", FieldType::Uint32, Flag::None},
         {PET_usage, "PET_usage", FieldType::List, Flag::SpoolSublist},
         {PET_scaled_usage, "PET_scaled_usage", FieldType::List, Flag::SpoolSublist},
         {PET_reported_usage, "PET_reported_usage", FieldType::List, Flag::SpoolSublist},
         {PET_previous_usage, "PET_previous_usage", FieldType::List, Flag::None},
         {PET_submission_time, "PET_submission_time", FieldType::Uint64, Flag::SpoolSublist},
         {PET_start_time, "PET_start_time", FieldType::Uint64, Flag::SpoolSublist},
         {PET_end_time, "PET_end_time", FieldType::Uint64, Flag::SpoolSublist},
         {PET_cwd, "PET_cwd", FieldType::String, Flag::SpoolSublist},
         {PET_path_aliases, "PET_path_aliases", FieldType::List, Flag::None},
         {PET_environment, "PET_environment", FieldType::List, Flag::None},
         {PET_do_contact, "PET_do_contact", FieldType::Bool, Flag::SpoolSublist},
         {PET_cred, "PET_cred", FieldType::String, Flag::None},
         {PET_joker, "PET_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PET belongs to PET */
   template <>
   struct TypeOf<PET_Field> {
      using type = PET;   ///< the object type
   };

   /** @brief PET_granted_destin_identifier_list holds elements of JG */
   template <>
   struct SubType<PET_granted_destin_identifier_list> {
      using type = JG;   ///< the sub-type
   };

   /** @brief PET_usage holds elements of UA */
   template <>
   struct SubType<PET_usage> {
      using type = UA;   ///< the sub-type
   };

   /** @brief PET_scaled_usage holds elements of UA */
   template <>
   struct SubType<PET_scaled_usage> {
      using type = UA;   ///< the sub-type
   };

   /** @brief PET_reported_usage holds elements of UA */
   template <>
   struct SubType<PET_reported_usage> {
      using type = UA;   ///< the sub-type
   };

   /** @brief PET_previous_usage holds elements of UA */
   template <>
   struct SubType<PET_previous_usage> {
      using type = UA;   ///< the sub-type
   };

   /** @brief PET_path_aliases holds elements of PA */
   template <>
   struct SubType<PET_path_aliases> {
      using type = PA;   ///< the sub-type
   };

   /** @brief PET_environment holds elements of VA */
   template <>
   struct SubType<PET_environment> {
      using type = VA;   ///< the sub-type
   };

   /** @brief PET_joker holds elements of VA */
   template <>
   struct SubType<PET_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
