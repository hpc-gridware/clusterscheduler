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
 * This code was generated from file source/libs/sgeobj/json/PRO.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Process Element: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of PRO, Process Element
 *
 * The enumeration is the type of every field number of PRO, so a parameter
 * of this type takes the field numbers of PRO only.
 */
enum PRO_Field {
   /**
    * @brief Pid
    *
    * The process id.
    */
   PRO_pid = PRO_LOWERBOUND,
   /**
    * @brief User Time
    *
    * Number of jiffies that this process has been scheduled in user mode.
    */
   PRO_utime,
   /**
    * @brief System Time
    *
    * Number of jiffies that this process has been scheduled in kernel mode.
    */
   PRO_stime,
   /**
    * @brief Virtual Memory
    *
    * Virtual memory size in bytes.
    */
   PRO_vsize,
   /**
    * @brief Resident Set Size
    *
    * Resident Set Size (physical memory) in bytes.
    */
   PRO_rss,
   /**
    * @brief Groups
    *
    * GR_Type list with all groups associated with this process.
    */
   PRO_groups,
   /**
    * @brief Related to Cluster Scheduler Job
    *
    * Flag if this process belongs to a GE job.
    */
   PRO_rel,
   /**
    * @brief Running
    *
    * Flag if this process is still running.
    */
   PRO_run,
   /**
    * @brief IO
    *
    * IO characters for the running process.
    */
   PRO_io,
   /**
    * @brief IOOPS
    *
    * IO operations for the running process.
    */
   PRO_ioops,
   /**
    * @brief IOW
    *
    * IO wait time in clock ticks for the running process.
    */
   PRO_iow,
};

namespace ocs::tom {

   struct GR;

   /**
    * @brief Process Element
    *
    * Used in sge_execd to keep track of all processes on the machine.
    */
   struct PRO {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {PRO_pid, "PRO_pid", FieldType::Uint32, Flag::PrimaryKey | Flag::Unique | Flag::Hash},
         {PRO_utime, "PRO_utime", FieldType::Uint32, Flag::None},
         {PRO_stime, "PRO_stime", FieldType::Uint32, Flag::None},
         {PRO_vsize, "PRO_vsize", FieldType::Uint64, Flag::None},
         {PRO_rss, "PRO_rss", FieldType::Uint64, Flag::None},
         {PRO_groups, "PRO_groups", FieldType::List, Flag::None},
         {PRO_rel, "PRO_rel", FieldType::Bool, Flag::None},
         {PRO_run, "PRO_run", FieldType::Bool, Flag::None},
         {PRO_io, "PRO_io", FieldType::Uint32, Flag::None},
         {PRO_ioops, "PRO_ioops", FieldType::Uint32, Flag::None},
         {PRO_iow, "PRO_iow", FieldType::Uint64, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of PRO belongs to PRO */
   template <>
   struct TypeOf<PRO_Field> {
      using type = PRO;   ///< the object type
   };

   /** @brief PRO_groups holds elements of GR */
   template <>
   struct SubType<PRO_groups> {
      using type = GR;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of PRO for the list library: a synonym for ocs::tom::PRO::descriptor */
inline constexpr const lDescr *PRO_Type = ocs::tom::PRO::descriptor.e;
