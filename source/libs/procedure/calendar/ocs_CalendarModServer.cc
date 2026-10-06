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

   /* The response text IS the message the old GET/MOD path printed, taken from
    * the answer list rather than formulated again - one source for the wording.
    *
    * It has to travel this way because a write's answer list does not reach the
    * client through the procedure path: sge_gdi_add_mod_generic() adds its
    * MSG_SGETEXT_MODIFIEDINLIST_SSSS as an INFO with STATUS_OK, and the client
    * receives a list without it, so answer_list_output() there prints nothing.
    * Measured on 2026-10-06: "qconf -Mcal" prints the line, "qconf -mcal"
    * through the procedure printed only an invented confirmation until this.
    * Carrying it in the response keeps qconf's output exactly what it was; the
    * transport gap itself is wider than this procedure and needs its own fix. */
   const lListElem *info = nullptr;
   for (const lListElem *aep = lFirst(*answer_list); aep != nullptr; aep = lNext(aep)) {
      if (lGetUlong(aep, AN_quality) == ANSWER_QUALITY_INFO && lGetString(aep, AN_text) != nullptr) {
         info = aep;
      }
   }
   if (info != nullptr) {
      message_ = lGetString(info, AN_text);
   }
   DRETURN(true);
}
