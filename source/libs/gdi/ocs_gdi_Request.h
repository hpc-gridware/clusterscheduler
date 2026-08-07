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

/** @file
 * @brief A GDI request: a list of tasks sent as one unit
 */

#include "cull/cull.h"

#include "gdi/ocs_gdi_Packet.h"
#include "gdi/ocs_gdi_Target.h"
#include "gdi/ocs_gdi_Mode.h"

namespace ocs::gdi {
   /**
    * @brief A GDI request: one or more tasks sent to qmaster as a unit
    *
    * Tasks are added with #request in @ref Mode::RECORD and sent together when
    * one is added in @ref Mode::SEND, so a caller pays a single round trip for
    * several operations. #get_response then picks each task's answer out by the
    * id #request returned.
    */
   class Request {
   public:
      Packet *packet;             ///< the tasks, in the form they travel in
      lList *multi_answer_list;   ///< qmaster's answers, one entry per task

      Request();   ///< Build an empty request
      ~Request();  ///< Release the packet and the answers

      /// Wait until the answer to a request sent asynchronously has arrived
      void wait();

      /**
       * @brief Add a task to this request, and send it when asked to
       *
       * @param[out] alpp receives errors detected before sending
       * @param mode @ref Mode::RECORD to only collect, @ref Mode::SEND to send now
       * @param target which object list to act on
       * @param cmd what to do
       * @param sub_cmd modifiers refining @p cmd
       * @param lp the objects to send
       * @param cp which objects to act on, from `lWhere()`
       * @param enp which fields to transfer, from `lWhat()`
       * @param do_copy true to copy the arguments instead of taking them over
       * @return the task id to pass to #get_response, or 0 on error
       */
      int request(lList **alpp, Mode mode, Target target, Command cmd,
                  SubCommand, lList **lp, lCondition *cp, lEnumeration *enp, bool do_copy);

      /**
       * @brief Take one task's answer out of a sent request
       *
       * @param[out] alpp receives the answer list of that task
       * @param cmd the command the task carried
       * @param sub_cmd the modifiers the task carried
       * @param target the target the task addressed
       * @param id the task id #request returned
       * @param[out] list receives the objects read back, if any
       * @return true when the task succeeded
       */
      bool get_response(lList **alpp, Command cmd, SubCommand, Target target, int id, lList **list) const;
   };
}
