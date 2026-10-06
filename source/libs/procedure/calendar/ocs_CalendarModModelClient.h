#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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
 * @brief The client half of the calendar modify procedure
 *
 * Nothing but the two addressing answers; the work happens in qmaster. Kept
 * apart from ocs_CalendarModServer.h so a client translation unit needs
 * neither the server model nor the monitoring structure it uses.
 */

#include "gdi/ocs_gdi_Command.h"
#include "gdi/ocs_gdi_Target.h"

#include "../ocs_ProcedureModel.h"
#include "ocs_CalendarModParameter.h"

namespace ocs {

   /** @brief The client half: sends the modification to qmaster
    *
    * Only the two addressing answers differ from a reading model, and they are
    * what the whole of CS-2822 turns on: the command puts the request through
    * the modify branch of the permission checks instead of the read branch, and
    * the target says which object list those checks ask about. qmaster refuses
    * the request if either disagrees with what the procedure's dispatch entry
    * declares, so this is not a free choice.
    *
    * @ingroup libprocedure
    */
   class CalendarModModelClient : public ProcedureModel {
   public:
      /// @return MOD_PROCEDURE, because this procedure changes state
      [[nodiscard]] gdi::Command get_command() const override {
         return gdi::Command::MOD_PROCEDURE;
      }

      /// @return the calendar list, the object the procedure writes
      [[nodiscard]] gdi::Target get_target() const override {
         return gdi::Target::CAL_LIST;
      }
   };

}
