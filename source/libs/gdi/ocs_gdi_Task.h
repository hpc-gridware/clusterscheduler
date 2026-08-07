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

#include "gdi/ocs_gdi_Command.h"
#include "gdi/ocs_gdi_Target.h"

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
      lList *data_list;        ///< the objects to write, or the ones read back
      lList *answer_list;      ///< qmaster's answer for this task
      lCondition *condition;   ///< which objects to act on, from `lWhere()`
      lEnumeration *enumeration; ///< which fields to transfer, from `lWhat()`

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
   public:
      /**
       * @brief Build a task
       *
       * @param target which object list to act on
       * @param command what to do
       * @param sub_cmd modifiers refining @p command
       * @param lp the objects to send, consumed unless @p do_copy is true
       * @param a_list receives qmaster's answer
       * @param condition which objects to act on; consumed
       * @param enumeration which fields to transfer; consumed
       * @param do_copy true to copy the arguments instead of taking them over
       */
      Task(Target target, Command command, SubCommand sub_cmd, lList **lp,
           lList **a_list, lCondition **condition, lEnumeration **enumeration, bool do_copy);
      Task();  ///< Build an empty task
      ~Task(); ///< Release the lists this task owns

      /// Log this task's command, target and data at debug level
      void debug_print();
   };
}
