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
 * @brief Reading a `qconf` read operation's objects from the master lists
 */

#include <utility>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "evm/sge_event_master.h"

#include "sgeobj/ocs_DataStore.h"

#include "basis_types.h"
#include "msg_common.h"

#include "ocs_QConfModelServer.h"

/** @brief Read the host groups from the data store (CS-2818)
 *
 * A copy, not the master list itself: the cluster queue read joins the groups'
 * member lists onto the queues it hands to the view, and that must not touch
 * what the daemon holds.
 *
 * @param answer_list unused; reading the data store cannot fail here
 * @return true
 */
bool
ocs::QConfModelServer::fetch_hgroups([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   const auto master_hgroup_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();
   hgroup_list_ = master_hgroup_list.clone("");

   DRETURN(true);
}

/**
 * @brief Read the objects out of qmaster's own lists
 *
 * The counterpart of the GDI GET the client does: same filter, same fields, but
 * a selection over the master list instead of a round trip. There is nothing to
 * fail in a recoverable way here, which is why the client's
 * answer_exit_if_not_recoverable() has no equivalent.
 *
 * The condition and the selection are taken over; their owners free them on
 * every path.
 *
 * @param answer_list for error reporting
 * @param target      the master list to read
 * @param where       which elements, an empty owner for all; taken over
 * @param what        which fields; taken over
 * @return true on success
 */
bool
ocs::QConfModelServer::fetch(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const gdi::Target target,
                             ocs::tom::RawWherePtr where, ocs::tom::RawWhatPtr what) {
   DENTER(TOP_LAYER);

   if (get_object() == QConfObj::EV) {
      /* The one read that is not a data store read: event clients are held by
       * the event master and built on demand, which is why EV_LIST carries
       * SGE_TYPE_NONE in sge_c_gdi.cc and why the GDI GET is answered from the
       * LISTENER data store. The call is MT safe and takes its own lock, so
       * whichever store this request runs under does not matter (CS-2808). */
      object_list_ = ocs::tom::RawListPtr(sge_select_event_clients("", where.get(), what.get()));
      DRETURN(true);
   }

   const sge_object_type type = qconf_master_list_type(get_object());
   if (type == SGE_TYPE_NONE) {
      /* A guard, not a reachable path: every object type the read table names
       * has a master list. It exists so that adding a singleton to the table
       * without teaching this function about it fails loudly instead of
       * reading list zero. The dispatch reports its own internal errors the
       * same way, without a catalogue entry. */
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "no master list for the object type in " SFQ, __func__);
      DRETURN(false);
   }

   // dispatched once on the read's object type: the model built the condition and the selection for it, and the
   // master list holds objects of it; object_list_ stays untyped, its type is the read's
   ocs_with_qconf_object(get_object(), [this, type, &where, &what](const auto &entry) {
      using T = typename ocs_qconf_object_entry_t<decltype(entry)>::type;
      // fetch() owns the condition and the selection, built by the model for this type: the typed handles take
      // them over unchecked, as before
      const auto typed_where = ocs::tom::Where<T>::from_typed(where.release());
      const auto typed_what = ocs::tom::What<T>::from_typed(what.release());
      const auto master_list = ocs::tom::ConstList<T>::from_typed(DataStore::master_list_raw(type));
      auto selected = master_list.select(typed_where, typed_what, "");
      // lSelect() gave no list where nothing matched, and the callers tell no list from an empty one
      if (selected && selected->empty()) {
         selected.reset();
      }
      object_list_ = ocs::tom::RawListPtr(std::move(selected));
   });
   // an object outside the dispatch table never reaches the lambda: the owners free the condition and the selection

   DRETURN(true);
}

namespace {
   /** @brief The provider qmaster registered, nullptr until it does */
   ocs::qconf_thread_pool_provider thread_pool_provider = nullptr;
}

void
ocs::qconf_set_thread_pool_provider(ocs::qconf_thread_pool_provider provider) {
   thread_pool_provider = provider;
}

/** @brief Build the thread pool list through the provider qmaster registered (CS-2820)
 *
 * Unlike every other read of this model there is no data store behind it: the
 * thread pools are the daemon's own state. An unset provider is an internal
 * error, not a user error - it means qmaster started without wiring itself up -
 * and it is reported the same way the missing master list in fetch() is, without
 * a catalogue entry.
 *
 * @param answer_list used to return error messages
 * @return true on success
 */
bool
ocs::QConfModelServer::fetch_thread_pools(ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   if (thread_pool_provider == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              "no thread pool provider registered in " SFQ, __func__);
      DRETURN(false);
   }
   // object_list_ holds whatever list the operation read, its type is known only at run time
   object_list_ = ocs::tom::RawListPtr(thread_pool_provider());

   DRETURN(true);
}

/**
 * @brief Take the complex entries from qmaster's own list
 *
 * A copy rather than the list itself, because the prepare hook works on the
 * fetched object and the model owns what it hands on; the master list belongs
 * to the daemon.
 *
 * @param answer_list unused, kept for the interface
 * @return true, always: there is nothing here that can fail
 */
bool
ocs::QConfModelServer::fetch_centries([[maybe_unused]] ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
   centry_list_ = master_centry_list.clone("");

   DRETURN(true);
}
