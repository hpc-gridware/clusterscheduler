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

#include "sgeobj/sge_centry.h"

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
ocs::QConfModelClient::fetch(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, gdi::Target target,
                             lCondition *where, lEnumeration *what) {
   DENTER(TOP_LAYER);

   auto alp = gdi::Client::sge_gdi(target, gdi::Command::GET, gdi::SubCommand::NONE,
                                     &object_list_, where, what);
   const bool failed = answer_list_has_error(&alp);
   if (failed) {
      answer_list_append_list(answer_list, &alp);
      lFreeList(&object_list_);
   }
   alp.reset();

   DRETURN(!failed);
}

/** @brief Fetch the host groups over GDI (CS-2818)
 *
 * A request of its own rather than a task in the same multi request as the
 * queues, which is what the client did before the read moved. The two can
 * therefore disagree if a group changes in between; in the server context, the
 * one that matters, both come from the same data store snapshot.
 *
 * @param answer_list used to return error messages
 * @return true on success; false with @p answer_list filled otherwise
 */
bool
ocs::QConfModelClient::fetch_hgroups(ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   lEnumeration *what = lWhat("%T(ALL)", HGRP_Type);
   lList *hgroup_list = nullptr;
   auto alp = gdi::Client::sge_gdi(gdi::Target::HGRP_LIST, gdi::Command::GET,
                                     gdi::SubCommand::NONE, &hgroup_list, nullptr, what);
   lFreeWhat(&what);
   hgroup_list_ = ocs::tom::ListPtr<ocs::tom::HGRP>::from_typed(hgroup_list);
   const bool failed = answer_list_has_error(&alp);
   if (failed) {
      answer_list_append_list(answer_list, &alp);
      hgroup_list_.reset();
   }
   alp.reset();

   DRETURN(!failed);
}

/** @brief Ask qmaster for its thread pools over GDI (CS-2820)
 *
 * The target has no master list and no `lWhat`: qmaster answers a GET on
 * `DUMMY_LIST` by building the list from its own thread state.
 *
 * @param answer_list used to return error messages
 * @return true on success
 */
bool
ocs::QConfModelClient::fetch_thread_pools(ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   auto alp = gdi::Client::sge_gdi(gdi::Target::DUMMY_LIST, gdi::Command::GET,
                                     gdi::SubCommand::NONE, &object_list_, nullptr, nullptr);
   const bool failed = answer_list_has_error(&alp);
   if (failed) {
      answer_list_append_list(answer_list, &alp);
      lFreeList(&object_list_);
   }
   alp.reset();

   DRETURN(!failed);
}

/**
 * @brief Ask qmaster for the complex entries
 *
 * Needed only by the JSON prepare hook, which turns the string values of a
 * resource into typed numbers. The hand written code fetched them inside the
 * hook, once per call of it; here they are fetched once, by the model.
 *
 * @param answer_list for error reporting
 * @return true on success
 */
bool
ocs::QConfModelClient::fetch_centries(ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   lEnumeration *what = lWhat("%T(ALL)", CE_Type);
   lList *centry_list = nullptr;
   auto alp = gdi::Client::sge_gdi(gdi::Target::CE_LIST, gdi::Command::GET,
                                     gdi::SubCommand::NONE, &centry_list, nullptr, what);
   lFreeWhat(&what);
   centry_list_ = ocs::tom::ListPtr<ocs::tom::CE>::from_typed(centry_list);
   const bool failed = answer_list_has_error(&alp);
   if (failed) {
      answer_list_append_list(answer_list, &alp);
      centry_list_.reset();
   }
   alp.reset();

   DRETURN(!failed);
}
