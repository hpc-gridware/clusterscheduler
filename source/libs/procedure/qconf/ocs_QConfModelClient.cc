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
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Select.h"

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
 * The condition and the selection are taken over and wrapped at once into owning
 * handles of the read's type, which free them on every path.
 *
 * @param answer_list for error reporting
 * @param target      the master list to read
 * @param where       which elements, or nullptr for all; taken over
 * @param what        which fields; taken over
 * @return true on success
 */
bool
ocs::QConfModelClient::fetch(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, gdi::Target target,
                             lCondition *where, lEnumeration *what) {
   DENTER(TOP_LAYER);

   // dispatched once on the read's object type: every caller builds the condition and the selection for it and
   // reads the target of its row; object_list_ stays untyped, its type is the read's
   auto alp = ocs_with_qconf_object(get_object(), [this, target, &where, &what](const auto &entry) {
      using T = typename ocs_qconf_object_entry_t<decltype(entry)>::type;
      // fetch() owns the condition and the selection, built by the model for this type: the handles free them
      const auto typed_where = ocs::tom::Where<T>::from_typed(std::exchange(where, nullptr));
      const auto typed_what = ocs::tom::What<T>::from_typed(std::exchange(what, nullptr));
      // a list held before goes out as the request data, as before, and is freed when the answer replaces it
      auto data = ocs::tom::ListPtr<T>::from_typed(std::exchange(object_list_, nullptr));
      auto answers = gdi::Client::sge_gdi(target, gdi::Command::GET, gdi::SubCommand::NONE, data, typed_where,
                                          typed_what);
      object_list_ = data.release();
      return answers;
   });
   // only for an object outside the dispatch table, which never reaches the lambda: nothing else frees them
   lFreeWhere(&where);
   lFreeWhat(&what);
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

   ocs::tom::ListPtr<ocs::tom::HGRP> hgroup_list;
   auto alp = gdi::Client::sge_gdi(gdi::Target::HGRP_LIST, gdi::Command::GET, gdi::SubCommand::NONE, hgroup_list,
                                   ocs::tom::Where<ocs::tom::HGRP>(), ocs::tom::what_all<ocs::tom::HGRP>());
   hgroup_list_ = std::move(hgroup_list);
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

   // qmaster answers with the names of its threads; object_list_ stays untyped, its type is the read's
   auto data = ocs::tom::ListPtr<ocs::tom::ST>::from_typed(std::exchange(object_list_, nullptr));
   auto alp = gdi::Client::sge_gdi(gdi::Target::DUMMY_LIST, gdi::Command::GET, gdi::SubCommand::NONE, data);
   object_list_ = data.release();
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

   ocs::tom::ListPtr<ocs::tom::CE> centry_list;
   auto alp = gdi::Client::sge_gdi(gdi::Target::CE_LIST, gdi::Command::GET, gdi::SubCommand::NONE, centry_list,
                                   ocs::tom::Where<ocs::tom::CE>(), ocs::tom::what_all<ocs::tom::CE>());
   centry_list_ = std::move(centry_list);
   const bool failed = answer_list_has_error(&alp);
   if (failed) {
      answer_list_append_list(answer_list, &alp);
      centry_list_.reset();
   }
   alp.reset();

   DRETURN(!failed);
}
