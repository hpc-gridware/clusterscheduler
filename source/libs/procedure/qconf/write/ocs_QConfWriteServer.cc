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
 * @brief The qmaster half of the qconf write procedure, see the header
 */

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "../msg_qconf.h"
#include "ocs_QConfWriteServer.h"

namespace {
   /* Null until qmaster installs the writing code. A client linking this
    * library never installs one and must never reach the model that uses it:
    * the procedure runs inside qmaster only. */
   ocs::qconf_write_provider qconf_write_provider_fn = nullptr;
}

/** @brief Install the writing code, called once by qmaster at startup
 * @param provider the function that applies a set of objects
 */
void
ocs::set_qconf_write_provider(const qconf_write_provider provider) {
   DENTER(TOP_LAYER);
   qconf_write_provider_fn = provider;
   DRETURN_VOID;
}

/** @brief Apply the set
 *
 * Everything that can be refused without touching the cluster is refused here;
 * the rest belongs to the provider, which parses every item, decides add
 * against modify, and drives the object's own modifier, spooler and event
 * hooks.
 *
 * @param answer_list receives one entry per item
 * @param parameter the items and the batch policy
 * @return true when the batch was applied as the policy demands
 */
bool
ocs::QConfWriteModelServer::make_snapshot(lList **answer_list, QConfWriteParameter &parameter) {
   DENTER(TOP_LAYER);

   if (qconf_write_provider_fn == nullptr) {
      // Only reachable if this ran outside qmaster, which must not happen: the
      // dispatch table is the daemon's and no client sends this to itself.
      answer_list_add(answer_list, MSG_QCONF_NOWRITEPROVIDER, STATUS_EUNKNOWN,
                      ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if (!obj_known_) {
      // Likewise: the dispatch entry names the object type, so reaching this
      // means the handler did not pass it on. Refused rather than guessed - a
      // write against the wrong object type is the one mistake the whole
      // command/target binding of CS-2822 exists to prevent.
      answer_list_add(answer_list, MSG_QCONF_NOWRITEOBJECT, STATUS_EUNKNOWN,
                      ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if (parameter.get_item_count() == 0) {
      // An empty set has nothing to apply, and letting it through would take
      // the global write lock to find that out.
      answer_list_add(answer_list, MSG_QCONF_EMPTYOBJECTSET, STATUS_EUNKNOWN,
                      ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   DRETURN(qconf_write_provider_fn(packet_, task_, monitor_, obj_, parameter, answer_list, &result_));
}
