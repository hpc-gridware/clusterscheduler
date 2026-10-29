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
 * @brief The qmaster half of the qconf purge procedure, see the header
 */

#include "sgeobj/sge_answer.h"
#include "uti/sge_rmon_macros.h"

#include "msg_qconf.h"
#include "ocs_QConfPurgeServer.h"

namespace {
   /* Null until qmaster installs the purging code. A client linking this
    * library never installs one and must never reach the model that uses it. */
   ocs::qconf_purge_provider qconf_purge_provider_fn = nullptr;
}

/** @brief Install the purging code, called once by qmaster at startup
 * @param provider the function that purges a queue
 */
void
ocs::set_qconf_purge_provider(const qconf_purge_provider provider) {
   DENTER(TOP_LAYER);
   qconf_purge_provider_fn = provider;
   DRETURN_VOID;
}

/** @brief Purge, see the header
 *
 * What can be refused without touching the cluster is refused here: a request
 * that names no queue, no attribute or no host. The client never sends one, so
 * these guard the procedure against a caller that is not qconf.
 *
 * @param answer_list receives the outcome
 * @param parameter what to purge from which queue
 * @return true when the purge did not fail
 */
bool
ocs::QConfPurgeModelServer::make_snapshot(lList **answer_list, QConfPurgeParameter &parameter) {
   DENTER(TOP_LAYER);

   if (qconf_purge_provider_fn == nullptr) {
      answer_list_add(answer_list, MSG_QCONF_NOPURGEPROVIDER, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if (parameter.get_queue().empty() || parameter.get_attributes().empty() ||
       parameter.get_domain().empty()) {
      answer_list_add(answer_list, MSG_QCONF_PURGEINCOMPLETE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   DRETURN(qconf_purge_provider_fn(packet_, task_, monitor_, parameter, answer_list));
}
