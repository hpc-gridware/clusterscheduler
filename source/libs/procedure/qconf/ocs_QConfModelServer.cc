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

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "evm/sge_event_master.h"

#include "sgeobj/ocs_DataStore.h"

#include "basis_types.h"
#include "msg_common.h"

#include "ocs_QConfModelServer.h"

/** @brief Which master list each object type lives in
 *
 * The qmaster-internal `gdi_object_t` table knows this too, but it sits in
 * `daemons/qmaster/sge_c_gdi.cc` and is about the write path - its rows carry
 * the modify, spool and success hooks. A library cannot reach it, so the read
 * path states the one thing it needs here. Kept in the order of #QConfObj so
 * that the lookup is a plain index.
 */
static const sge_object_type qconf_master_list[] = {
   SGE_TYPE_CALENDAR,    /* CAL  */
   SGE_TYPE_CKPT,        /* CK   */
   SGE_TYPE_CENTRY,      /* CE   */
   SGE_TYPE_EXECHOST,    /* EH   */
   SGE_TYPE_HGROUP,      /* HGRP */
   SGE_TYPE_PE,          /* PE   */
   SGE_TYPE_PROJECT,     /* PR   */
   SGE_TYPE_CQUEUE,      /* CQ   */
   SGE_TYPE_RL,          /* RL   */
   SGE_TYPE_RQS,         /* RQS  */
   SGE_TYPE_USERSET,     /* US   */
   SGE_TYPE_USER,        /* UU   */
   SGE_TYPE_SHARETREE,   /* STN  */
   SGE_TYPE_SCHEDD_CONF, /* SC   */
   SGE_TYPE_CONFIG,      /* CONF */
   SGE_TYPE_NONE,        /* EV - event clients live in the event master */
};

/* the second array on the same index - see the note at QConfObj (CS-2808) */
static_assert(sizeof(qconf_master_list) / sizeof(qconf_master_list[0]) ==
              static_cast<size_t>(QConfObj::EV) + 1,
              "qconf_master_list needs exactly one row per QConfObj value");

/**
 * @brief Read the objects out of qmaster's own lists
 *
 * The counterpart of the GDI GET the client does: same filter, same fields, but
 * `lSelect()` over the master list instead of a round trip. There is nothing to
 * fail in a recoverable way here, which is why the client's
 * answer_exit_if_not_recoverable() has no equivalent.
 *
 * @param answer_list for error reporting
 * @param target      the master list to read
 * @param where       which elements, or nullptr for all
 * @param what        which fields
 * @return true on success
 */
bool
ocs::QConfModelServer::fetch(lList **answer_list, gdi::Target target,
                             lCondition *where, lEnumeration *what) {
   DENTER(TOP_LAYER);

   if (get_object() == QConfObj::EV) {
      /* The one read that is not a data store read: event clients are held by
       * the event master and built on demand, which is why EV_LIST carries
       * SGE_TYPE_NONE in sge_c_gdi.cc and why the GDI GET is answered from the
       * LISTENER data store. The call is MT safe and takes its own lock, so
       * whichever store this request runs under does not matter (CS-2808). */
      object_list_ = sge_select_event_clients("", where, what);
      DRETURN(true);
   }

   const size_t index = static_cast<size_t>(get_object());
   const sge_object_type type = qconf_master_list[index];
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

   const lList *master_list = *DataStore::get_master_list(type);
   object_list_ = lSelect("", master_list, where, what);

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
ocs::QConfModelServer::fetch_centries([[maybe_unused]] lList **answer_list) {
   DENTER(TOP_LAYER);

   const lList *master_centry_list = *DataStore::get_master_list(SGE_TYPE_CENTRY);
   centry_list_ = lCopyList("", master_centry_list);

   DRETURN(true);
}
