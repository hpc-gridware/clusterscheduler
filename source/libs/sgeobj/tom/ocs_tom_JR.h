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
 * This code was generated from file source/libs/sgeobj/json/JR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Report: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of JR, Job Report
 *
 * The enumeration is the type of every field number of JR, so a parameter
 * of this type takes the field numbers of JR only.
 */
enum JR_Field {
   /**
    * @brief Job Number
    *
    * The job number / job id of the corresponding job.
    */
   JR_job_number = JR_LOWERBOUND,
   /**
    * @brief Array Task Number
    *
    * The array task number / id of the corresponding array task.
    */
   JR_ja_task_number,
   /**
    * @brief Queue Name
    *
    * Queue this job is running in / tried to run in.
    */
   JR_queue_name,
   /**
    * @brief State
    *
    * Either JRUNNING or JEXITING, JRUNNING is sent as ack for jobdelivery and in the load_report_interval.
    */
   JR_state,
   /**
    * @brief Failed
    *
    * Failure code, e.g. EESTATE_NO_CONFIG, EESTATE_NO_PID, ...
    */
   JR_failed,
   /**
    * @brief General Failure
    *
    * General failure code, e.g. GFSTATE_NO_HALT, GFSTATE_QUEUE, ...
    */
   JR_general_failure,
   /**
    * @brief Error String
    *
    * Error description for failed jobs.
    */
   JR_err_str,
   /**
    * @brief Usage
    *
    * List of resource usage by the job/task, e.g. wallclock, cpu, mem, ...
    */
   JR_usage,
   /**
    * @brief Job Pid
    *
    * Process Id of the job script / the toplevel process of the job.
    */
   JR_job_pid,
   /**
    * @brief Checkpoint Arena
    *
    * If there is a checkpoint in the arena.
    */
   JR_ckpt_arena,
   /**
    * @brief Pe Task Id
    *
    * For tasks of tightly integrated parallel jobs: The tasks unique id.
    * It consists of the host name and a sequential number, e.g. myhostname.5
    */
   JR_pe_task_id_str,
   /**
    * @brief OS Job Id
    *
    * String containing osjobid for ckpt jobs.
    */
   JR_osjobid,
   /**
    * @brief Wait Status
    *
    * japi_wait() 'status' information.
    */
   JR_wait_status,
   /**
    * @brief Flush
    *
    * Used in sge_execd: Whether to send the job report immediately / as early as possible.
    */
   JR_flush,
   /**
    * @brief No Send
    *
    * Used in sge_execd: Do not send the job report, e.g. for pe tasks when accounting_summary is configured.
    */
   JR_no_send,
   /**
    * @brief Delay Report
    *
    * Used in sge_execd: Delay sending of the job report. Used for qsub -sync/DRMAA jobs after a qmaster failover.
    */
   JR_delay_report,
   /**
    * @brief Deleted By
    *
    * Who deleted the job. Set by sge_execd to execd@<hostname> when the job was killed for exceeding a resource limit.
    */
   JR_deleted_by,
};

namespace ocs::tom {

   struct UA;

   /**
    * @brief Job Report
    *
    * Definition of a job report sent from sge_execd to sge_qmaster.
    * Job reports are sent for every running array task and every task of a tightly integrated parallel job.
    */
   struct JR {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JR_job_number, "JR_job_number", FieldType::Uint32, Flag::Hash},
         {JR_ja_task_number, "JR_ja_task_number", FieldType::Uint32, Flag::None},
         {JR_queue_name, "JR_queue_name", FieldType::String, Flag::None},
         {JR_state, "JR_state", FieldType::Uint32, Flag::None},
         {JR_failed, "JR_failed", FieldType::Uint32, Flag::None},
         {JR_general_failure, "JR_general_failure", FieldType::Uint32, Flag::None},
         {JR_err_str, "JR_err_str", FieldType::String, Flag::None},
         {JR_usage, "JR_usage", FieldType::List, Flag::None},
         {JR_job_pid, "JR_job_pid", FieldType::Uint32, Flag::None},
         {JR_ckpt_arena, "JR_ckpt_arena", FieldType::Uint32, Flag::None},
         {JR_pe_task_id_str, "JR_pe_task_id_str", FieldType::String, Flag::None},
         {JR_osjobid, "JR_osjobid", FieldType::String, Flag::None},
         {JR_wait_status, "JR_wait_status", FieldType::Uint32, Flag::None},
         {JR_flush, "JR_flush", FieldType::Bool, Flag::None},
         {JR_no_send, "JR_no_send", FieldType::Bool, Flag::None},
         {JR_delay_report, "JR_delay_report", FieldType::Bool, Flag::None},
         {JR_deleted_by, "JR_deleted_by", FieldType::String, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JR belongs to JR */
   template <>
   struct TypeOf<JR_Field> {
      using type = JR;   ///< the object type
   };

   /** @brief JR_usage holds elements of UA */
   template <>
   struct SubType<JR_usage> {
      using type = UA;   ///< the sub-type
   };

} // namespace ocs::tom
