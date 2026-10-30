/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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
 * @brief The qmaster half of the qconf action procedures, see the header
 */

#include <map>
#include <string>

#include "sgeobj/sge_answer.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "../msg_qconf.h"
#include "ocs_QConfActionServer.h"

namespace {
   /* Empty until qmaster registers its actions. A client linking this library
    * registers none and must never reach the model that uses them. */
   std::map<std::string, ocs::qconf_action_provider> &
   qconf_action_providers() {
      static std::map<std::string, ocs::qconf_action_provider> providers;
      return providers;
   }
}

/** @brief Register the function of one action, see the header
 * @param action the action
 * @param provider the function that carries it out
 */
void
ocs::set_qconf_action_provider(const char *action, const qconf_action_provider provider) {
   DENTER(TOP_LAYER);
   qconf_action_providers()[action] = provider;
   DRETURN_VOID;
}

/** @brief Carry out the action the parameters name, see the header
 *
 * The dispatch table has already matched the action to an entry and checked the
 * permission for the object list it changes; what is left is finding the
 * function. A missing one means the action is in the table but qmaster did not
 * register it, which is refused rather than ignored.
 *
 * @param answer_list receives what the action has to say
 * @param parameter the action and its arguments
 * @return true when the action did not fail
 */
bool
ocs::QConfActionModelServer::make_snapshot(lList **answer_list, QConfActionParameter &parameter) {
   DENTER(TOP_LAYER);

   const std::string &action = parameter.get_sub_procedure_name();
   const auto found = qconf_action_providers().find(action);
   if (found == qconf_action_providers().end() || found->second == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_QCONF_NOACTIONPROVIDER_S,
                              action.c_str());
      DRETURN(false);
   }

   DRETURN(found->second(packet_, task_, monitor_, parameter, answer_list));
}
