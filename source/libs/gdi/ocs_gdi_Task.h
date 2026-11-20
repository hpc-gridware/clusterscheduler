#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025-2026 HPC-Gridware GmbH
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

/** @file
 * @brief One operation inside a GDI request
 */

#include <string>

#include <cinttypes>

#include "cull/cull.h"
#include "tom/ocs_Raw.h"

#include "gdi/ocs_gdi_Command.h"
#include "gdi/ocs_gdi_Target.h"
#include "sgeobj/sge_answer.h"

namespace ocs::gdi {
   /**
    * @brief One operation inside a GDI request
    *
    * A request carries a list of tasks, each naming what to do (@ref Command),
    * to which object list (@ref Target) and with which data. Sending several
    * tasks in one request is what makes a "multi GDI" — the round trip to
    * qmaster is paid once.
    */
   class Task {
   public:
      Command command;         ///< what to do
      SubCommand sub_command;  ///< modifiers refining #command

      Target target;           ///< which object list to act on

      /**
       * @brief The objects to write, or the ones read back
       *
       * Stays a raw pointer: the type is given at run time by #target, and it
       * is not always owned - for a GET of an external client qmaster lets it
       * point to the master list until pack_task() selects from it (see
       * #do_select_pack_simultaneous). Besides, many qmaster files and the
       * `lList **` out-parameters of the object functions (sge_gdi_add_job(),
       * sge_add_event_client(), ...) use it directly. Owned data is handed in and
       * out with reset_data_list() and take_data_list().
       */
      lList *data_list;
      ocs::tom::ListPtr<ocs::tom::AN> answer_list; ///< qmaster's answer for this task
      ocs::tom::RawWherePtr condition;   ///< which objects to act on, from `lWhere()`; empty for all
      ocs::tom::RawWhatPtr enumeration;  ///< which fields to transfer, from `lWhat()`; empty for all

      /**
       * @brief May the selection pack straight into the send buffer?
       *
       * Set by qmaster for @ref Command::GET requests from an *external*
       * client — one that is not a thread using the GDI internally. Only then
       * can `lSelectHashPack()` be given the packbuffer directly, so the
       * selected objects are packed as they are found instead of being copied
       * into an intermediate list first.
       */
      bool do_select_pack_simultaneous;

      /**
       * @brief Does `@exec_hosts` still have to follow this request?
       *
       * CS-2753. The reserved host group mirrors the execution host list, and
       * something in this task changed that list. Set by the writers
       * (`sge_del_host()`, `host_success()`), consumed once by
       * `sge_c_gdi_process_in_worker()` after the request is done.
       *
       * A flag rather than a call at the end of each element loop, because the
       * execution host list is not only written through EH_LIST requests:
       * `host_list_add_missing_href()` creates hosts from inside `hgroup_mod()`,
       * i.e. under an HGRP_LIST request, and an execd reporting in for the first
       * time creates one through `sge_execd_startedup()`. A flag cannot be
       * forgotten by whoever adds the next such path.
       */
      bool exec_hostgroup_out_of_sync;
   public:
      Task(Target target, Command command, SubCommand sub_cmd, ocs::tom::RawListPtr data,
           ocs::tom::ListPtr<ocs::tom::AN> answers, ocs::tom::RawWherePtr condition,
           ocs::tom::RawWhatPtr enumeration);
      Task();  ///< Build an empty task
      ~Task(); ///< Release the lists this task owns

      Task(const Task &) = delete;
      Task &operator=(const Task &) = delete;

      void reset_data_list(ocs::tom::RawListPtr data = {});

      [[nodiscard]] ocs::tom::RawListPtr take_data_list();

      /// Log this task's command, target and data at debug level
      void debug_print();
   };
}
