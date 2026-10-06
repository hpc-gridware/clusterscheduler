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
 * @brief The qmaster half of the calendar modify procedure, see the header
 */

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_CalendarModServer.h"

namespace {
   /* Null until qmaster installs the writing code. A client linking this
    * library never installs one, and must never reach the model that uses it:
    * the procedure runs inside qmaster only. */
   ocs::calendar_mod_provider calendar_mod_provider_fn = nullptr;
}

/** @brief Install the writing code, called once by qmaster at startup
 * @param provider the function that performs the modification
 */
void
ocs::set_calendar_mod_provider(const calendar_mod_provider provider) {
   DENTER(TOP_LAYER);
   calendar_mod_provider_fn = provider;
   DRETURN_VOID;
}

/** @brief Apply the modification
 *
 * Everything that can be rejected without touching the cluster is rejected
 * here; the rest is the provider's, which runs the object's own modifier,
 * spools and emits the event. Returning false skips the rendering, the same
 * contract a read has when its snapshot fails.
 *
 * @param answer_list receives errors and the confirmation
 * @param parameter what to change
 * @return true when the calendar was modified
 */
bool
ocs::CalendarModModelServer::make_snapshot(lList **answer_list, CalendarModParameter &parameter) {
   DENTER(TOP_LAYER);

   if (calendar_mod_provider_fn == nullptr) {
      // Only reachable if this ran outside qmaster, which must not happen: the
      // dispatch table is the daemon's and no client sends this procedure to
      // itself.
      answer_list_add(answer_list, "no calendar modify provider installed",
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if (parameter.get_object_text().empty()) {
      // Refused rather than parsed: an empty text has no object in it, and
      // letting it through would take the global write lock to find that out.
      answer_list_add(answer_list, "no calendar text given", STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   if (!calendar_mod_provider_fn(packet_, task_, monitor_, parameter, answer_list)) {
      // the provider wrote the reason
      DRETURN(false);
   }

   /* No response text. What a write has to say is already in the answer list -
    * sge_gdi_add_mod_generic() puts MSG_SGETEXT_MODIFIEDINLIST_SSSS there - and
    * that list reaches the client (measured 2026-10-06). A procedure that
    * invented its own confirmation would say the same thing in different words
    * and in a different stream, which is exactly what qconf's callers and the
    * testsuite match against.
    *
    * A reading procedure's response is its rendered output; a writing one has
    * no output, only a verdict. */

   DRETURN(true);
}
