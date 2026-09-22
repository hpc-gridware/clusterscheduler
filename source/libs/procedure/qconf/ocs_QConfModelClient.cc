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
 * @brief Fetching a `qconf` read operation's objects over GDI
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_QConfModelClient.h"

/**
 * @brief Ask qmaster for the objects
 *
 * The GDI GET the hand written engine did, unchanged. Unlike the client engine
 * it does not call answer_exit_if_not_recoverable(): the model reports and the
 * caller decides, which is the rule for everything that can also run in the
 * daemon.
 *
 * @param answer_list for error reporting
 * @param target      the master list to read
 * @param where       which elements, or nullptr for all
 * @param what        which fields
 * @return true on success
 */
bool
ocs::QConfModelClient::fetch(lList **answer_list, gdi::Target target,
                             lCondition *where, lEnumeration *what) {
   DENTER(TOP_LAYER);

   lList *alp = gdi::Client::sge_gdi(target, gdi::Command::GET, gdi::SubCommand::NONE,
                                     &object_list_, where, what);
   const bool failed = answer_list_has_error(&alp);
   if (failed) {
      answer_list_append_list(answer_list, &alp);
      lFreeList(&object_list_);
   }
   lFreeList(&alp);

   DRETURN(!failed);
}
