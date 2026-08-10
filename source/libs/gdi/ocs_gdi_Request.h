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

      int request(lList **alpp, Mode mode, Target target, Command cmd,
                  SubCommand, lList **lp, lCondition *cp, lEnumeration *enp, bool do_copy);

      bool get_response(lList **alpp, Command cmd, SubCommand, Target target, int id, lList **list) const;
   };
}
