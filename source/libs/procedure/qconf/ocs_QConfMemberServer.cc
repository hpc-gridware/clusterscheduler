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
 * @brief The qmaster half of the access list membership procedure, see the header
 */

#include "sgeobj/sge_answer.h"
#include "uti/sge_rmon_macros.h"

#include "msg_qconf.h"
#include "ocs_QConfMemberServer.h"

namespace {
   /* Null until qmaster installs the membership code. A client linking this
    * library never installs one and must never reach the model that uses it. */
   ocs::qconf_member_provider qconf_member_provider_fn = nullptr;
}

/** @brief Install the membership code, called once by qmaster at startup
 * @param provider the function that changes the entries of access lists
 */
void
ocs::set_qconf_member_provider(const qconf_member_provider provider) {
   DENTER(TOP_LAYER);
   qconf_member_provider_fn = provider;
   DRETURN_VOID;
}

/** @brief Change the entries, see the header
 *
 * A request that names no user or no list is refused here. The client never
 * sends one, so this guards the procedure against a caller that is not qconf.
 *
 * @param answer_list receives one answer per user and list
 * @param parameter which users to add to or remove from which lists
 * @return true when every user could be added or removed
 */
bool
ocs::QConfMemberModelServer::make_snapshot(lList **answer_list, QConfMemberParameter &parameter) {
   DENTER(TOP_LAYER);

   if (qconf_member_provider_fn == nullptr) {
      answer_list_add(answer_list, MSG_QCONF_NOMEMBERPROVIDER, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if (parameter.get_users().empty() || parameter.get_lists().empty()) {
      answer_list_add(answer_list, MSG_QCONF_MEMBERINCOMPLETE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   DRETURN(qconf_member_provider_fn(packet_, task_, monitor_, parameter, answer_list));
}
