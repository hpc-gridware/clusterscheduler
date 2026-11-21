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
 * This code was generated from file source/libs/sgeobj/json/QAJ.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Accounting Summary: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of QAJ, Accounting Summary
 *
 * The enumeration is the type of every field number of QAJ, so a parameter
 * of this type takes the field numbers of QAJ only.
 */
enum QAJ_Field {
   /**
    * @brief Host Name
    *
    * Host where a job ran.
    */
   QAJ_host = QAJ_LOWERBOUND,
   /**
    * @brief Cluster Queue Name
    *
    * Cluster queue in which a job ran.
    */
   QAJ_queue,
   /**
    * @brief User Group
    *
    * Unix user group of the job owner.
    */
   QAJ_group,
   /**
    * @brief Owner
    *
    * Unix user name of the job owner.
    */
   QAJ_owner,
   /**
    * @brief Project
    *
    * Project the job was running for.
    */
   QAJ_project,
   /**
    * @brief Department
    *
    * The job owner's department.
    */
   QAJ_department,
   /**
    * @brief Rusage Wallclock
    *
    * Wallclock time from rusage (end time - start_time).
    */
   QAJ_ru_wallclock,
   /**
    * @brief Rusage User Time
    *
    * User CPU time from rusage.
    */
   QAJ_ru_utime,
   /**
    * @brief Rusage System Time
    *
    * System CPU time from rusage.
    */
   QAJ_ru_stime,
   /**
    * @brief Rusage Maximum RSS
    *
    * Maximum resident set size of the job's processes from rusage.
    */
   QAJ_ru_maxrss,
   /**
    * @brief Rusage Block Input
    *
    * Number of block input operations from rusage.
    */
   QAJ_ru_inblock,
   /**
    * @brief Granted Parallel Environment
    *
    * In case of a parallel job: The PE the job was running in.
    */
   QAJ_granted_pe,
   /**
    * @brief Slots
    *
    * The number of slots the job occupied.
    */
   QAJ_slots,
   /**
    * @brief Cpu Usage
    *
    * The cpu usage of the job reported by sge_execd in seconds.
    */
   QAJ_cpu,
   /**
    * @brief Integral Memory Usage
    *
    * The integral memory usage of the job reported by sge_execd in Gbytes cpu seconds.
    */
   QAJ_mem,
   /**
    * @brief IO Usage
    *
    * The IO usage of the job reported by sge_execd (the amount of data transferred in input and output operations.
    */
   QAJ_io,
   /**
    * @brief IO Wait Time
    *
    * The amount of time the job waited for IO operations to happen as reported by sge_execd.
    */
   QAJ_iow,
   /**
    * @brief Maximum Virtual Memory
    *
    * The maximum virtual memory usage of the job reported by sge_execd.
    */
   QAJ_maxvmem,
   /**
    * @brief AR Id
    *
    * The id of an advance reservation the job might have run in.
    */
   QAJ_arid,
};

namespace ocs::tom {

   /**
    * @brief Accounting Summary
    *
    * Used in qsub for summing up usage values (?).
    * It contains some of the fields which can appear in the qacct -j job_id output.
    * See also the sge_accounting(5) man page.
    */
   struct QAJ {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {QAJ_host, "QAJ_host", FieldType::Host, Flag::None},
         {QAJ_queue, "QAJ_queue", FieldType::String, Flag::None},
         {QAJ_group, "QAJ_group", FieldType::String, Flag::None},
         {QAJ_owner, "QAJ_owner", FieldType::String, Flag::None},
         {QAJ_project, "QAJ_project", FieldType::String, Flag::None},
         {QAJ_department, "QAJ_department", FieldType::String, Flag::None},
         {QAJ_ru_wallclock, "QAJ_ru_wallclock", FieldType::Double, Flag::None},
         {QAJ_ru_utime, "QAJ_ru_utime", FieldType::Double, Flag::None},
         {QAJ_ru_stime, "QAJ_ru_stime", FieldType::Double, Flag::None},
         {QAJ_ru_maxrss, "QAJ_ru_maxrss", FieldType::Double, Flag::None},
         {QAJ_ru_inblock, "QAJ_ru_inblock", FieldType::Double, Flag::None},
         {QAJ_granted_pe, "QAJ_granted_pe", FieldType::String, Flag::None},
         {QAJ_slots, "QAJ_slots", FieldType::Uint32, Flag::None},
         {QAJ_cpu, "QAJ_cpu", FieldType::Double, Flag::None},
         {QAJ_mem, "QAJ_mem", FieldType::Double, Flag::None},
         {QAJ_io, "QAJ_io", FieldType::Double, Flag::None},
         {QAJ_iow, "QAJ_iow", FieldType::Double, Flag::None},
         {QAJ_maxvmem, "QAJ_maxvmem", FieldType::Double, Flag::None},
         {QAJ_arid, "QAJ_arid", FieldType::Uint32, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of QAJ belongs to QAJ */
   template <>
   struct TypeOf<QAJ_Field> {
      using type = QAJ;   ///< the object type
   };

} // namespace ocs::tom
