/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The event mirror: applying events to a local copy of the master lists
 *
 * One `mirror_description` per object type says what happens to an event: the
 * component's `before` callback, then the built in mirroring, then its `after`
 * callback. The `before` one may stop the event from being applied.
 *
 * The state is thread local, so several threads can each mirror into their own
 * data store - which is what @ref ocs::MirrorDataStore uses.
 *
 * @see sge_mirror.h
 */

#include <cstdlib>
#include <pthread.h>
#include <cstdio>
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_stdlib.h"

#include "tom/ocs_Types.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/ocs_object_type.h"
#include "sgeobj/sge_pack.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_ET.h"
#include "sgeobj/tom/ocs_tom_PACK.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "evc/msg_evclib.h"
#include "mir/msg_mirlib.h"
#include "mir/sge_host_mirror.h"
#include "mir/sge_queue_mirror.h"
#include "mir/sge_job_mirror.h"
#include "mir/sge_ja_task_mirror.h"
#include "mir/sge_pe_task_mirror.h"
#include "mir/sge_sharetree_mirror.h"
#include "mir/sge_sched_conf_mirror.h"
#include "mir/sge_mirror.h"

#include "sig_handlers.h"

/**
 * @brief The built in mirroring of one object type
 *
 * Unlike a component's @ref sge_mirror_callback it gets the event writable: it takes the objects the event
 * carries in `ET_new_version` over into the master lists.
 *
 * @param evc the event client the event arrived on
 * @param type the object type the event is about
 * @param action what the event asks for
 * @param event the event
 * @param clientdata always nullptr
 * @return whether the event was applied
 */
typedef sge_callback_result (*mir_update_func)(sge_evc_class_t *evc, sge_object_type type, sge_event_action action,
                                               ocs::tom::Element<ocs::tom::ET> event, void *clientdata);

/**
 * @brief What one subscribed object type is mirrored with
 *
 * Three callbacks run per event, in order: the component's own `before`, the
 * built in mirroring, then the component's `after`. The `before` one can stop
 * the event from being applied at all.
 */
typedef struct {
   sge_mirror_callback callback_before;   ///< callback before mirroring
   mir_update_func callback_default;      ///< default mirroring function
   sge_mirror_callback callback_after;    ///< callback after mirroring
   void *client_data;                     ///< client data passed to callback
   bool filtered;                         ///< subscribed with a where-filter, so the local list is a subset
} mirror_description;

static sge_mirror_error
sge_mirror_subscribe_internal(sge_evc_class_t *evc, sge_object_type type, sge_mirror_callback callback_before,
                              sge_mirror_callback callback_after, void *client_data, const ocs::tom::WhereData *where,
                              const ocs::tom::WhatData *what);

static sge_mirror_error
sge_mirror_unsubscribe_internal(sge_evc_class_t *evc, sge_object_type type);

static void
sge_mirror_free_list(sge_object_type type);

static sge_mirror_error
sge_mirror_process_event(sge_evc_class_t *evc,
                         mirror_description *mirror_base,
                         sge_object_type type,
                         sge_event_action action,
                         ocs::tom::Element<ocs::tom::ET> event);

static sge_callback_result
sge_mirror_process_shutdown(sge_evc_class_t *evc,
                            sge_object_type type,
                            sge_event_action action,
                            ocs::tom::Element<ocs::tom::ET> event,
                            void *client_data);
static sge_callback_result
sge_mirror_process_mark4registration(sge_evc_class_t *evc,
                                     sge_object_type type,
                                     sge_event_action action,
                                     ocs::tom::Element<ocs::tom::ET> event,
                                     void *client_data);

static sge_callback_result
generic_update_master_list(sge_evc_class_t *evc,
                           sge_object_type type,
                           sge_event_action action,
                           ocs::tom::Element<ocs::tom::ET> event,
                           void *client_data);

/*
 * One entry per event type, this is the basic definition.
 * Each thread will have its own table based on this one. 
 */
static const mirror_description dev_mirror_base[SGE_TYPE_ALL] = {
   /*cbb   cbd                                     cba   cd   */
   { nullptr, generic_update_master_list,             nullptr, nullptr },
   { nullptr, generic_update_master_list,             nullptr, nullptr },
   { nullptr, host_update_master_list,                nullptr, nullptr },
   { nullptr, host_update_master_list,                nullptr, nullptr },
   { nullptr, ja_task_update_master_list,             nullptr, nullptr },
   { nullptr, pe_task_update_master_list,             nullptr, nullptr },
   { nullptr, job_update_master_list,                 nullptr, nullptr },
   { nullptr, job_schedd_info_update_master_list,     nullptr, nullptr },
   { nullptr, sharetree_update_master_list,           nullptr, nullptr },
   { nullptr, generic_update_master_list,             nullptr, nullptr },
   { nullptr, generic_update_master_list,             nullptr, nullptr },
   { nullptr, cqueue_update_master_list,              nullptr, nullptr },
   { nullptr, qinstance_update_cqueue_list,           nullptr, nullptr },
   { nullptr, schedd_conf_update_master_list,         nullptr, nullptr },
   { nullptr, nullptr,                                nullptr, nullptr },
   { nullptr, sge_mirror_process_shutdown,            nullptr, nullptr },
   { nullptr, sge_mirror_process_mark4registration,   nullptr, nullptr },
   { nullptr, generic_update_master_list,             nullptr, nullptr },
   { nullptr, generic_update_master_list,             nullptr, nullptr },
   { nullptr, hgroup_update_master_list,              nullptr, nullptr }, /*hgroup*/
   { nullptr, generic_update_master_list,             nullptr, nullptr },
   { nullptr, generic_update_master_list,             nullptr, nullptr }, /*suser*/
   { nullptr, generic_update_master_list,             nullptr, nullptr }, /*rqs*/
   { nullptr, generic_update_master_list,             nullptr, nullptr }, /*advance reservation*/
   { nullptr, nullptr,                                nullptr, nullptr }, /*jobscripts*/
   { nullptr, generic_update_master_list,             nullptr, nullptr }, // sgeE_CATEGORY_LIST
   { nullptr, nullptr,                                nullptr, nullptr }, /* procedure (no mirror) */
   { nullptr, generic_update_master_list,             nullptr, nullptr }, /* role */
};

/*-------------------------*/
/* multithreading support. */
/*-------------------------*/

/**
 * @brief The mirror state of one thread
 *
 * Thread local, so several threads can each mirror into their own data store.
 */
/// CS-2859: how many failed callbacks, and how close together, are taken as proof that the mirrored
/// lists disagree with the qmaster's, and how often that may be acted upon. See
/// @ref mir_note_callback_failure().
constexpr uint32_t MIR_RESYNC_FAILURE_THRESHOLD = 10;
constexpr uint32_t MIR_RESYNC_FAILURE_WINDOW = 60;
constexpr uint32_t MIR_RESYNC_MIN_INTERVAL = 60;

typedef struct {
   bool produce_qmaster_alive_timeout; ///< test hook: produce a qmaster alive timeout when `SGE_PRODUCE_ALIVE_TIMEOUT_ERROR` is set in the environment
   mirror_description mirror_base[SGE_TYPE_ALL]; ///< subscription handlers, indexed by @ref sge_object_type
   uint32_t callback_failures;         ///< failed callbacks in the current window (CS-2859)
   uint64_t first_callback_failure;    ///< when that window opened (CS-2859)
   uint64_t last_resync_request;       ///< when a total update was last asked for (CS-2859)
   /// test hook: callbacks still to be failed on purpose when
   /// `SGE_PRODUCE_MIRROR_CALLBACK_ERROR` is set in the environment (CS-2859)
   uint32_t produce_callback_failures;
   /// test hook: last value of the qmaster_params test parameter
   /// `__TEST_MIRROR_CALLBACK_ERROR` (CS-3032)
   int test_callback_error_seen;
} mir_state_t;

static pthread_key_t   mir_state_key;
static pthread_once_t  mir_once_control = PTHREAD_ONCE_INIT;

static void mir_state_init(mir_state_t *state) {
   int i;
   /*
    * if environment varialbe SGE_PRODUCE_ALIVE_TIMEOUT_ERROR
    * is defined, the mirror event client will produce qmaster
    * alive timeout errors and reconnect to qmaster
    */
   if (getenv("SGE_PRODUCE_ALIVE_TIMEOUT_ERROR")) {
      state->produce_qmaster_alive_timeout = true;
   } else {
      state->produce_qmaster_alive_timeout = false;
   }

   /* initialize mirroring data structures - only changeable fields */
   for (i = 0; i < SGE_TYPE_ALL; i++) {
      state->mirror_base[i].callback_before  = nullptr;
      state->mirror_base[i].callback_after   = nullptr;
      state->mirror_base[i].callback_default = dev_mirror_base[i].callback_default;
      state->mirror_base[i].client_data       = nullptr;
      state->mirror_base[i].filtered          = false;
   }

   state->callback_failures = 0;
   state->first_callback_failure = 0;
   state->last_resync_request = 0;

   /*
    * if the environment variable SGE_PRODUCE_MIRROR_CALLBACK_ERROR is defined, that many
    * callbacks fail on purpose, which is how the recovery of a mirror that disagrees with the
    * qmaster is tested. A value which is not a positive number fails enough of them to reach
    * MIR_RESYNC_FAILURE_THRESHOLD, so that setting the variable is never a silent no-op.
    */
   state->produce_callback_failures = 0;
   const char *produce_callback_failures = getenv("SGE_PRODUCE_MIRROR_CALLBACK_ERROR");
   if (produce_callback_failures != nullptr) {
      state->produce_callback_failures = atoi(produce_callback_failures);
      if (state->produce_callback_failures == 0) {
         state->produce_callback_failures = MIR_RESYNC_FAILURE_THRESHOLD * 2;
      }
   }

   // CS-3032: a value which is already set when the thread starts does not fail any callbacks,
   // otherwise a qmaster started with it could not even load its initial lists
   state->test_callback_error_seen = mconf_get_test_mirror_callback_error();
}

static void mir_state_destroy(void *state) {
   sge_free(&state);
}

static void mir_mt_init() {
   pthread_key_create(&mir_state_key, &mir_state_destroy);
}

static bool mir_get_produce_qmaster_alive_timeout() {
   GET_SPECIFIC(mir_state_t, mir_state, mir_state_init, mir_state_key);
   return mir_state->produce_qmaster_alive_timeout;
}

static mirror_description *mir_get_mirror_base() {
   GET_SPECIFIC(mir_state_t, mir_state, mir_state_init, mir_state_key);
   return mir_state->mirror_base;
}

/**
 * @brief Fail callbacks on purpose when the test parameter __TEST_MIRROR_CALLBACK_ERROR changes.
 *
 * The qmaster_params test parameter __TEST_MIRROR_CALLBACK_ERROR=<n> is the runtime counterpart
 * of the SGE_PRODUCE_MIRROR_CALLBACK_ERROR environment variable for the mirror threads inside
 * qmaster. Whenever its value changes, the calling thread fails its next n callbacks, see
 * mir_produce_callback_failure(). It is used to test that a qmaster internal mirror whose lists
 * disagree with the qmaster's registers again (CS-3032).
 *
 * Outside of qmaster the parameter is never set, and the function does nothing.
 */
static void mir_arm_test_callback_failures() {
   DENTER(TOP_LAYER);
   GET_SPECIFIC(mir_state_t, mir_state, mir_state_init, mir_state_key);

   int test_callback_error = mconf_get_test_mirror_callback_error();
   if (test_callback_error != mir_state->test_callback_error_seen) {
      mir_state->test_callback_error_seen = test_callback_error;
      if (test_callback_error > 0) {
         DPRINTF("__TEST_MIRROR_CALLBACK_ERROR: failing the next %d callbacks\n",
                 test_callback_error);
         mir_state->produce_callback_failures = test_callback_error;
      }
   }
   DRETURN_VOID;
}

static bool mir_produce_callback_failure() {
   GET_SPECIFIC(mir_state_t, mir_state, mir_state_init, mir_state_key);
   if (mir_state->produce_callback_failures == 0) {
      return false;
   }
   mir_state->produce_callback_failures--;
   return true;
}

/**
 * @brief note a failed callback and ask for a total update once they prove the mirror is broken
 *
 * A failed callback is the client's own assertion that its mirrored lists disagree with the
 * qmaster's: a MOD or a DEL event can only be applied to an object the client already holds, so
 * a callback which cannot find that object proves the two views have drifted apart. A single
 * failure can be benign - an object removed while an event about it was in flight - so it is the
 * repetition which is taken as evidence: @ref MIR_RESYNC_FAILURE_THRESHOLD failures within
 * @ref MIR_RESYNC_FAILURE_WINDOW seconds. Failures further apart than that say nothing about the
 * state the mirror is in now and open a new window instead of adding to the old one.
 *
 * Successful callbacks in between are deliberately not counted and do not clear the window. An
 * empty mirror still applies every ADD and every LIST event without complaint, so a run of
 * failures is broken up by successes while the client is quite unable to work - which is the
 * state this exists to get out of.
 *
 * Nothing else repairs it. sge_resync_schedd() is the only function which forces a full resend,
 * it is reachable only from the GDI order path, and a scheduler whose mirror holds no exec hosts
 * dispatches nothing and therefore sends no orders - so the single trigger sits behind the very
 * outage it would cure, and the only cure left is restarting the thread (CS-2859). Asking for a
 * new registration reaches the same end down the path which already serves the acknowledge
 * timeout: the client registers again and sge_add_event_client() answers with a total update of
 * every subscribed list.
 *
 * The flag is acted upon by the client's own loop and not here. The scheduler thread asks
 * ec_need_new_registration() before it processes an event list, and its wait for events has a
 * timeout, so the registration happens on its next pass. The qmaster mirror threads register again
 * at the end of the same pass instead (CS-3032): they are busy until they release that state with
 * ec_commit(), which is refused while a registration is pending, so they would never receive
 * another event to start a next pass with.
 *
 * A total update is expensive, so one is asked for at most every @ref MIR_RESYNC_MIN_INTERVAL
 * seconds. That bound is what matters when the registration does not help: without it a client
 * whose mirror stays broken would ask again as fast as events arrive.
 *
 * @param evc the event client whose callback failed
 */
static void
mir_note_callback_failure(sge_evc_class_t *evc) {
   DENTER(TOP_LAYER);
   GET_SPECIFIC(mir_state_t, mir_state, mir_state_init, mir_state_key);

   const uint64_t now = sge_get_gmt64();

   if (mir_state->callback_failures == 0 ||
       now - mir_state->first_callback_failure > sge_gmt32_to_gmt64(MIR_RESYNC_FAILURE_WINDOW)) {
      mir_state->first_callback_failure = now;
      mir_state->callback_failures = 0;
   }

   if (++mir_state->callback_failures < MIR_RESYNC_FAILURE_THRESHOLD) {
      DRETURN_VOID;
   }

   // A registration which is already pending delivers the total update anyway
   if (evc->ec_need_new_registration(evc)) {
      mir_state->callback_failures = 0;
      DRETURN_VOID;
   }

   if (mir_state->last_resync_request != 0 &&
       now - mir_state->last_resync_request < sge_gmt32_to_gmt64(MIR_RESYNC_MIN_INTERVAL)) {
      DPRINTF("mirror still broken, but a total update was asked for less than " sge_u32
              " seconds ago\n", MIR_RESYNC_MIN_INTERVAL);
      DRETURN_VOID;
   }

   ERROR(MSG_MIRROR_RESYNC_UUU, static_cast<uint32_t>(evc->ec_get_id(evc)),
         mir_state->callback_failures, MIR_RESYNC_FAILURE_WINDOW);
   evc->ec_mark4registration(evc);

   mir_state->last_resync_request = now;
   mir_state->callback_failures = 0;
   DRETURN_VOID;
}

/**
 * @brief Is the local copy of this object type only a subset?
 *
 * A component may subscribe with a `where` filter, in which case its list holds
 * only the matching elements. Anything that iterates the list has to know that.
 *
 * @param type the object type to ask about
 * @return true when the type was subscribed with a filter
 */
bool sge_mirror_type_is_partial(sge_object_type type) {
   if (type < 0 || type >= SGE_TYPE_ALL) {
      return false;
   }
   return mir_get_mirror_base()[type].filtered;
}

/*----------------------------*/
/* End multithreading support */
/*----------------------------*/

/**
 * @brief Initialize a process local event mirror interface
 *
 * Initializes internal data structures and registers with qmaster
 * using the event client mechanisms.
 * Events covering shutdown requests and qmaster shutdown notification
 * are subscribed.
 *
 * @param evc the event client to mirror into this process
 * @param update_func a function which knows how to handle new events; it stores
 *                    them rather than sending them out
 * @param mod_func changes an existing event client registration
 * @param add_func registers a new event client
 * @param remove_func deregisters an event client
 * @param ack_func acknowledges delivered events
 * @param update_func_arg additional argument passed to `update_func`
 *
 * @return SGE_EM_OK or an error code
 *
 * @see #sge_mirror_shutdown
 */
sge_mirror_error
sge_mirror_initialize(sge_evc_class_t *evc, event_client_update_func_t update_func,
                      evm_mod_func_t mod_func, evm_add_func_t add_func, evm_remove_func_t remove_func,
                      evm_ack_func_t ack_func, void *update_func_arg) {
   DENTER(TOP_LAYER);

   evc->ec_local.update_func = update_func;
   evc->ec_local.mod_func = mod_func;
   evc->ec_local.add_func = add_func;
   evc->ec_local.remove_func = remove_func;
   evc->ec_local.ack_func = ack_func;
   evc->ec_local.init = true;
   evc->ec_local.update_func_arg = update_func_arg;

   pthread_once(&mir_once_control, mir_mt_init);

   /* subscribe some events with default handling */
   sge_mirror_subscribe(evc, SGE_TYPE_SHUTDOWN, nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_MARK_4_REGISTRATION, nullptr, nullptr, nullptr, nullptr, nullptr);

   /* register with qmaster */
   evc->ec_commit(evc, nullptr);

   DRETURN(SGE_EM_OK);
}

/**
 * @brief Shutdown mirroring
 *
 * @param evc the event client to shut down
 *
 * Shuts down the event mirroring mechanism:
 * Unsubscribes all events, deletes contents of the corresponding
 * object lists and deregisteres from qmaster.
 *
 * @return SGE_EM_OK or error code
 *
 * @see #sge_mirror_initialize
 */
sge_mirror_error sge_mirror_shutdown(sge_evc_class_t *evc) {
   DENTER(TOP_LAYER);

   if (evc && evc->ec_is_initialized(evc)) {
      sge_mirror_unsubscribe(evc, SGE_TYPE_ALL);
      evc->ec_deregister(evc);
#if 0
    488 (352 direct, 136 indirect) bytes in 1 blocks are definitely lost in loss record 239 of 274
    at 0x4C360A5 malloc (vg_replace_malloc.c:380)
    at 0x6CBC97 sge_malloc(unsigned long) (sge_stdlib.cc:72)
    at 0x59D17F sge_evc_class_create(ev_registration_id, ocs::tom::ListData**, char const*) (sge_event_client.cc:667)
    at 0x5A5979 ocs::gdi::Client::sge_gdi2_evc_setup(sge_evc_class_str**, ev_registration_id, ocs::tom::ListData**,
                                                     char const*) (sge_event_client.cc:3066)
    at 0x47F575 ocs::MirrorDataStore::main(void*) (ocs_MirrorDataStore.cc:253)
    at 0x4860CB ocs::event_mirror_main(void*) (ocs_thread_mirror.cc:33)
    at 0x4E4E179 start_thread
    at 0x60DDDC2 clone

#endif
      sge_evc_class_destroy(&evc);
   }


   DRETURN(SGE_EM_OK);
}

/**
 * @brief Subscribe certain event types
 *
 * Subscribe a certain event type.
 * Callback functions can be specified, that can be executed before the
 * mirroring action and/or after the mirroring action.
 * The corresponding data structures are initialized,
 * the events associated with the event type are subscribed with the
 * event client interface.
 *
 * @param evc event client to work with
 * @param type event type to subscribe or SGE_TYPE_ALL
 * @param callback_before callback to be executed before mirroring
 * @param callback_after callback to be executed after mirroring
 * @param client_data client data to be passed to the callback functions
 * @param where filter deciding which elements are mirrored at all
 * @param what field selection limiting what is mirrored of each element
 *
 * @return SGE_EM_OK or an error code
 *
 * @see #sge_mirror_unsubscribe
 */
sge_mirror_error sge_mirror_subscribe(sge_evc_class_t *evc, sge_object_type type, sge_mirror_callback callback_before,
                                      sge_mirror_callback callback_after, void *client_data,
                                      const ocs::tom::WhereData *where, const ocs::tom::WhatData *what) {
   DENTER(TOP_LAYER);

   sge_mirror_error ret = SGE_EM_OK;

   if (type > SGE_TYPE_ALL) {
      ERROR(MSG_MIRROR_INVALID_OBJECT_TYPE_SI, __func__, type);
      DRETURN(SGE_EM_BAD_ARG);
   }

   if (type == SGE_TYPE_ALL) {
      int i;
      
      for (i = (int)SGE_TYPE_FIRST; i < (int)SGE_TYPE_ALL; i++) {
         sge_mirror_subscribe_internal(evc, (sge_object_type) i, callback_before, callback_after, client_data,
                                       nullptr, nullptr);
      }
   } else {
      ret = sge_mirror_subscribe_internal(evc, type, callback_before, callback_after, client_data, where, what);
   }

   DRETURN(ret);
}

static sge_mirror_error
sge_mirror_subscribe_internal(sge_evc_class_t *evc, sge_object_type type, sge_mirror_callback callback_before,
                              sge_mirror_callback callback_after, void *client_data, const ocs::tom::WhereData *where,
                              const ocs::tom::WhatData *what) {
   sge_mirror_error ret = SGE_EM_OK;
   const auto what_el = ocs_what_to_elem(what);
   const auto where_el = ocs_where_to_elem(where);

   /* type already has been checked before */
   switch (type) {
      case SGE_TYPE_CALENDAR:
         evc->ec_subscribe(evc, sgeE_CALENDAR_LIST);
         evc->ec_subscribe(evc, sgeE_CALENDAR_ADD);
         evc->ec_subscribe(evc, sgeE_CALENDAR_DEL);
         evc->ec_subscribe(evc, sgeE_CALENDAR_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_CALENDAR_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CALENDAR_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CALENDAR_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CALENDAR_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_CKPT:
         evc->ec_subscribe(evc, sgeE_CKPT_LIST);
         evc->ec_subscribe(evc, sgeE_CKPT_ADD);
         evc->ec_subscribe(evc, sgeE_CKPT_DEL);
         evc->ec_subscribe(evc, sgeE_CKPT_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_CKPT_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CKPT_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CKPT_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CKPT_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_CENTRY:
         evc->ec_subscribe(evc, sgeE_CENTRY_LIST);
         evc->ec_subscribe(evc, sgeE_CENTRY_ADD);
         evc->ec_subscribe(evc, sgeE_CENTRY_DEL);
         evc->ec_subscribe(evc, sgeE_CENTRY_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_CENTRY_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CENTRY_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CENTRY_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CENTRY_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_CONFIG:
         evc->ec_subscribe(evc, sgeE_CONFIG_LIST);
         evc->ec_subscribe(evc, sgeE_CONFIG_ADD);
         evc->ec_subscribe(evc, sgeE_CONFIG_DEL);
         evc->ec_subscribe(evc, sgeE_CONFIG_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_CONFIG_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CONFIG_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CONFIG_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CONFIG_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_EXECHOST:
         evc->ec_subscribe(evc, sgeE_EXECHOST_LIST);
         evc->ec_subscribe(evc, sgeE_EXECHOST_ADD);
         evc->ec_subscribe(evc, sgeE_EXECHOST_DEL);
         evc->ec_subscribe(evc, sgeE_EXECHOST_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_EXECHOST_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_EXECHOST_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_EXECHOST_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_EXECHOST_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_JATASK:
         evc->ec_subscribe(evc, sgeE_JATASK_ADD);
         evc->ec_subscribe(evc, sgeE_JATASK_DEL);
         evc->ec_subscribe(evc, sgeE_JATASK_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_JATASK_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_JATASK_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_JATASK_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_PETASK:
         evc->ec_subscribe(evc, sgeE_PETASK_ADD);
         evc->ec_subscribe(evc, sgeE_PETASK_DEL);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_PETASK_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_PETASK_DEL, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_JOB:
         evc->ec_subscribe(evc, sgeE_JOB_LIST);
         evc->ec_subscribe(evc, sgeE_JOB_ADD);
         evc->ec_subscribe(evc, sgeE_JOB_DEL);
         evc->ec_subscribe(evc, sgeE_JOB_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_JOB_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_JOB_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_JOB_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_JOB_MOD, *what_el, *where_el);
         }
         /* TODO: SG: what and where for usage */
         evc->ec_subscribe(evc, sgeE_JOB_USAGE);
         evc->ec_subscribe(evc, sgeE_JOB_FINAL_USAGE);
         evc->ec_subscribe(evc, sgeE_JOB_FINISH);
         break;
      case SGE_TYPE_JOB_SCHEDD_INFO:
         evc->ec_subscribe(evc, sgeE_JOB_SCHEDD_INFO_LIST);
         evc->ec_subscribe(evc, sgeE_JOB_SCHEDD_INFO_ADD);
         evc->ec_subscribe(evc, sgeE_JOB_SCHEDD_INFO_DEL);
         evc->ec_subscribe(evc, sgeE_JOB_SCHEDD_INFO_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_JOB_SCHEDD_INFO_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_JOB_SCHEDD_INFO_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_JOB_SCHEDD_INFO_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_JOB_SCHEDD_INFO_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_SHARETREE:
         evc->ec_subscribe(evc, sgeE_NEW_SHARETREE);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_NEW_SHARETREE, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_PE:
         evc->ec_subscribe(evc, sgeE_PE_LIST);
         evc->ec_subscribe(evc, sgeE_PE_ADD);
         evc->ec_subscribe(evc, sgeE_PE_DEL);
         evc->ec_subscribe(evc, sgeE_PE_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_PE_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_PE_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_PE_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_PE_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_PROJECT:
         evc->ec_subscribe(evc, sgeE_PROJECT_LIST);
         evc->ec_subscribe(evc, sgeE_PROJECT_ADD);
         evc->ec_subscribe(evc, sgeE_PROJECT_DEL);
         evc->ec_subscribe(evc, sgeE_PROJECT_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_PROJECT_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_PROJECT_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_PROJECT_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_PROJECT_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_CQUEUE:
         evc->ec_subscribe(evc, sgeE_CQUEUE_LIST);
         evc->ec_subscribe(evc, sgeE_CQUEUE_ADD);
         evc->ec_subscribe(evc, sgeE_CQUEUE_DEL);
         evc->ec_subscribe(evc, sgeE_CQUEUE_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_CQUEUE_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CQUEUE_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CQUEUE_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CQUEUE_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_QINSTANCE:
         evc->ec_subscribe(evc, sgeE_QINSTANCE_ADD);
         evc->ec_subscribe(evc, sgeE_QINSTANCE_DEL);
         evc->ec_subscribe(evc, sgeE_QINSTANCE_MOD);
         evc->ec_subscribe(evc, sgeE_QINSTANCE_SOS);
         evc->ec_subscribe(evc, sgeE_QINSTANCE_USOS);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_QINSTANCE_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_QINSTANCE_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_QINSTANCE_MOD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_QINSTANCE_SOS, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_QINSTANCE_USOS, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_SCHEDD_CONF:
         evc->ec_subscribe(evc, sgeE_SCHED_CONF);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_SCHED_CONF, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_SCHEDD_MONITOR:
         evc->ec_subscribe(evc, sgeE_SCHEDDMONITOR);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_SCHEDDMONITOR, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_SHUTDOWN:
         evc->ec_subscribe_flush(evc, sgeE_SHUTDOWN, 0);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_SHUTDOWN, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_MARK_4_REGISTRATION:
         evc->ec_subscribe_flush(evc, sgeE_QMASTER_GOES_DOWN, 0);
         evc->ec_subscribe_flush(evc, sgeE_ACK_TIMEOUT, 0);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_QMASTER_GOES_DOWN, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_ACK_TIMEOUT, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_USER:
         evc->ec_subscribe(evc, sgeE_USER_LIST);
         evc->ec_subscribe(evc, sgeE_USER_ADD);
         evc->ec_subscribe(evc, sgeE_USER_DEL);
         evc->ec_subscribe(evc, sgeE_USER_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_USER_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_USER_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_USER_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_USER_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_USERSET:
         evc->ec_subscribe(evc, sgeE_USERSET_LIST);
         evc->ec_subscribe(evc, sgeE_USERSET_ADD);
         evc->ec_subscribe(evc, sgeE_USERSET_DEL);
         evc->ec_subscribe(evc, sgeE_USERSET_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_USERSET_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_USERSET_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_USERSET_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_USERSET_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_HGROUP:
         evc->ec_subscribe(evc, sgeE_HGROUP_LIST);
         evc->ec_subscribe(evc, sgeE_HGROUP_ADD);
         evc->ec_subscribe(evc, sgeE_HGROUP_DEL);
         evc->ec_subscribe(evc, sgeE_HGROUP_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_HGROUP_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_HGROUP_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_HGROUP_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_HGROUP_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_RQS:
         evc->ec_subscribe(evc, sgeE_RQS_LIST);
         evc->ec_subscribe(evc, sgeE_RQS_ADD);
         evc->ec_subscribe(evc, sgeE_RQS_DEL);
         evc->ec_subscribe(evc, sgeE_RQS_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_RQS_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_RQS_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_RQS_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_RQS_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_SUSER:
         ret = SGE_EM_NOT_INITIALIZED;
         break;
      case SGE_TYPE_AR:
         evc->ec_subscribe(evc, sgeE_AR_LIST);
         evc->ec_subscribe(evc, sgeE_AR_ADD);
         evc->ec_subscribe(evc, sgeE_AR_DEL);
         evc->ec_subscribe(evc, sgeE_AR_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_AR_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_AR_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_AR_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_AR_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_JOBSCRIPT:
         ret = SGE_EM_NOT_INITIALIZED;
         break;
      case SGE_TYPE_CATEGORY:
         evc->ec_subscribe(evc, sgeE_CATEGORY_LIST);
         evc->ec_subscribe(evc, sgeE_CATEGORY_ADD);
         evc->ec_subscribe(evc, sgeE_CATEGORY_DEL);
         evc->ec_subscribe(evc, sgeE_CATEGORY_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_CATEGORY_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CATEGORY_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CATEGORY_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_CATEGORY_MOD, *what_el, *where_el);
         }
         break;
      case SGE_TYPE_PROCEDURE:
         ret = SGE_EM_NOT_INITIALIZED;
         break;
      case SGE_TYPE_RL:
         evc->ec_subscribe(evc, sgeE_RL_LIST);
         evc->ec_subscribe(evc, sgeE_RL_ADD);
         evc->ec_subscribe(evc, sgeE_RL_DEL);
         evc->ec_subscribe(evc, sgeE_RL_MOD);
         if (what_el && where_el) {
            evc->ec_mod_subscription_where(evc, sgeE_RL_LIST, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_RL_ADD, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_RL_DEL, *what_el, *where_el);
            evc->ec_mod_subscription_where(evc, sgeE_RL_MOD, *what_el, *where_el);
         }
         break;
      default:
         ret = SGE_EM_BAD_ARG;
         break;
   }

   if (ret == SGE_EM_OK) { /* install callback function */
      mirror_description *mirror_base = mir_get_mirror_base();

      mirror_base[type].callback_before = callback_before;
      mirror_base[type].callback_after  = callback_after;
      mirror_base[type].client_data      = client_data;
      mirror_base[type].filtered         = (where != nullptr);
   }

   return ret;
}

/**
 * @brief Unsubscribe event types
 *
 * Unsubscribes a certain event type or all if SGE_TYPE_ALL is given as type.
 * Unsubscribes the corresponding events in the underlying event client
 * interface and frees data stored in the corresponding mirrored list(s).
 *
 * @param evc the event client to work with
 * @param type the event type to unsubscribe or SGE_TYPE_ALL
 *
 * @return SGE_EM_OK or an error code
 *
 * @see #sge_mirror_subscribe
 */
sge_mirror_error sge_mirror_unsubscribe(sge_evc_class_t *evc, sge_object_type type) {
   DENTER(TOP_LAYER);

   sge_mirror_error ret = SGE_EM_OK;
   if (type > SGE_TYPE_ALL) {
      ERROR(MSG_MIRROR_INVALID_OBJECT_TYPE_SI, __func__, type);
      DRETURN(SGE_EM_BAD_ARG);
   }

   if (type == SGE_TYPE_ALL) {
      int i;

      for (i = (int)SGE_TYPE_FIRST; i < (int)SGE_TYPE_ALL; i++) {
         if (i != (int)SGE_TYPE_SHUTDOWN && i != (int)SGE_TYPE_MARK_4_REGISTRATION) {
            sge_mirror_unsubscribe_internal(evc, (sge_object_type) i);
         }
      }
   } else {
      ret = sge_mirror_unsubscribe_internal(evc, type);
   }

   DRETURN(ret);
}

static sge_mirror_error
sge_mirror_unsubscribe_internal(sge_evc_class_t *evc, sge_object_type type) {
   DENTER(TOP_LAYER);

   mirror_description *mirror_base = mir_get_mirror_base();
 
   /* type has been checked in calling function - clear callback information */
   mirror_base[type].callback_before  = nullptr;
   mirror_base[type].callback_after   = nullptr;
   mirror_base[type].client_data       = nullptr;
   mirror_base[type].filtered          = false;

   switch (type) {
      case SGE_TYPE_CALENDAR:
         evc->ec_unsubscribe(evc, sgeE_CALENDAR_LIST);
         evc->ec_unsubscribe(evc, sgeE_CALENDAR_ADD);
         evc->ec_unsubscribe(evc, sgeE_CALENDAR_DEL);
         evc->ec_unsubscribe(evc, sgeE_CALENDAR_MOD);
         break;
      case SGE_TYPE_CKPT:
         evc->ec_unsubscribe(evc, sgeE_CKPT_LIST);
         evc->ec_unsubscribe(evc, sgeE_CKPT_ADD);
         evc->ec_unsubscribe(evc, sgeE_CKPT_DEL);
         evc->ec_unsubscribe(evc, sgeE_CKPT_MOD);
         break;
      case SGE_TYPE_CENTRY:
         evc->ec_unsubscribe(evc, sgeE_CENTRY_LIST);
         evc->ec_unsubscribe(evc, sgeE_CENTRY_ADD);
         evc->ec_unsubscribe(evc, sgeE_CENTRY_DEL);
         evc->ec_unsubscribe(evc, sgeE_CENTRY_MOD);
         break;
      case SGE_TYPE_CONFIG:
         evc->ec_unsubscribe(evc, sgeE_CONFIG_LIST);
         evc->ec_unsubscribe(evc, sgeE_CONFIG_ADD);
         evc->ec_unsubscribe(evc, sgeE_CONFIG_DEL);
         evc->ec_unsubscribe(evc, sgeE_CONFIG_MOD);
         break;
      case SGE_TYPE_EXECHOST:
         evc->ec_unsubscribe(evc, sgeE_EXECHOST_LIST);
         evc->ec_unsubscribe(evc, sgeE_EXECHOST_ADD);
         evc->ec_unsubscribe(evc, sgeE_EXECHOST_DEL);
         evc->ec_unsubscribe(evc, sgeE_EXECHOST_MOD);
         break;
      case SGE_TYPE_JATASK:
         evc->ec_unsubscribe(evc, sgeE_JATASK_ADD);
         evc->ec_unsubscribe(evc, sgeE_JATASK_DEL);
         evc->ec_unsubscribe(evc, sgeE_JATASK_MOD);
         break;
      case SGE_TYPE_PETASK:
         evc->ec_unsubscribe(evc, sgeE_PETASK_ADD);
         evc->ec_unsubscribe(evc, sgeE_PETASK_DEL);
         break;
      case SGE_TYPE_JOB:
         evc->ec_unsubscribe(evc, sgeE_JOB_LIST);
         evc->ec_unsubscribe(evc, sgeE_JOB_ADD);
         evc->ec_unsubscribe(evc, sgeE_JOB_DEL);
         evc->ec_unsubscribe(evc, sgeE_JOB_MOD);
         evc->ec_unsubscribe(evc, sgeE_JOB_USAGE);
         evc->ec_unsubscribe(evc, sgeE_JOB_FINAL_USAGE);
         evc->ec_unsubscribe(evc, sgeE_JOB_FINISH);
         break;
      case SGE_TYPE_JOB_SCHEDD_INFO:
         evc->ec_unsubscribe(evc, sgeE_JOB_SCHEDD_INFO_LIST);
         evc->ec_unsubscribe(evc, sgeE_JOB_SCHEDD_INFO_ADD);
         evc->ec_unsubscribe(evc, sgeE_JOB_SCHEDD_INFO_DEL);
         evc->ec_unsubscribe(evc, sgeE_JOB_SCHEDD_INFO_MOD);
         break;
      case SGE_TYPE_SHARETREE:
         evc->ec_unsubscribe(evc, sgeE_NEW_SHARETREE);
         break;
      case SGE_TYPE_PE:
         evc->ec_unsubscribe(evc, sgeE_PE_LIST);
         evc->ec_unsubscribe(evc, sgeE_PE_ADD);
         evc->ec_unsubscribe(evc, sgeE_PE_DEL);
         evc->ec_unsubscribe(evc, sgeE_PE_MOD);
         break;
      case SGE_TYPE_PROJECT:
         evc->ec_unsubscribe(evc, sgeE_PROJECT_LIST);
         evc->ec_unsubscribe(evc, sgeE_PROJECT_ADD);
         evc->ec_unsubscribe(evc, sgeE_PROJECT_DEL);
         evc->ec_unsubscribe(evc, sgeE_PROJECT_MOD);
         break;
      case SGE_TYPE_CQUEUE:
         evc->ec_unsubscribe(evc, sgeE_CQUEUE_LIST);
         evc->ec_unsubscribe(evc, sgeE_CQUEUE_ADD);
         evc->ec_unsubscribe(evc, sgeE_CQUEUE_DEL);
         evc->ec_unsubscribe(evc, sgeE_CQUEUE_MOD);
         break;
      case SGE_TYPE_QINSTANCE:
         evc->ec_unsubscribe(evc, sgeE_QINSTANCE_ADD);
         evc->ec_unsubscribe(evc, sgeE_QINSTANCE_DEL);
         evc->ec_unsubscribe(evc, sgeE_QINSTANCE_MOD);
         evc->ec_unsubscribe(evc, sgeE_QINSTANCE_SOS);
         evc->ec_unsubscribe(evc, sgeE_QINSTANCE_USOS);
         break;
      case SGE_TYPE_SCHEDD_CONF:
         evc->ec_unsubscribe(evc, sgeE_SCHED_CONF);
         break;
      case SGE_TYPE_SCHEDD_MONITOR:
         evc->ec_unsubscribe(evc, sgeE_SCHEDDMONITOR);
         break;
      case SGE_TYPE_SHUTDOWN:
         ERROR(SFNMAX, MSG_EVENT_HAVETOHANDLEEVENTS);
         break;
      case SGE_TYPE_MARK_4_REGISTRATION:
         ERROR(SFNMAX, MSG_EVENT_HAVETOHANDLEEVENTS);
         break;
      case SGE_TYPE_USER:
         evc->ec_unsubscribe(evc, sgeE_USER_LIST);
         evc->ec_unsubscribe(evc, sgeE_USER_ADD);
         evc->ec_unsubscribe(evc, sgeE_USER_DEL);
         evc->ec_unsubscribe(evc, sgeE_USER_MOD);
         break;
      case SGE_TYPE_USERSET:
         evc->ec_unsubscribe(evc, sgeE_USERSET_LIST);
         evc->ec_unsubscribe(evc, sgeE_USERSET_ADD);
         evc->ec_unsubscribe(evc, sgeE_USERSET_DEL);
         evc->ec_unsubscribe(evc, sgeE_USERSET_MOD);
         break;
      case SGE_TYPE_HGROUP:
         evc->ec_unsubscribe(evc, sgeE_HGROUP_LIST);
         evc->ec_unsubscribe(evc, sgeE_HGROUP_ADD);
         evc->ec_unsubscribe(evc, sgeE_HGROUP_DEL);
         evc->ec_unsubscribe(evc, sgeE_HGROUP_MOD);
         break;
      case SGE_TYPE_RQS:
         evc->ec_unsubscribe(evc, sgeE_RQS_LIST);
         evc->ec_unsubscribe(evc, sgeE_RQS_ADD);
         evc->ec_unsubscribe(evc, sgeE_RQS_DEL);
         evc->ec_unsubscribe(evc, sgeE_RQS_MOD);
         break;
      case SGE_TYPE_SUSER:
         DRETURN(SGE_EM_NOT_INITIALIZED);
      case SGE_TYPE_AR:
         evc->ec_unsubscribe(evc, sgeE_AR_LIST);
         evc->ec_unsubscribe(evc, sgeE_AR_ADD);
         evc->ec_unsubscribe(evc, sgeE_AR_DEL);
         evc->ec_unsubscribe(evc, sgeE_AR_MOD);
         break;
      case SGE_TYPE_JOBSCRIPT:
         DRETURN(SGE_EM_NOT_INITIALIZED);
      case SGE_TYPE_CATEGORY:
         evc->ec_unsubscribe(evc, sgeE_CATEGORY_LIST);
         evc->ec_unsubscribe(evc, sgeE_CATEGORY_ADD);
         evc->ec_unsubscribe(evc, sgeE_CATEGORY_DEL);
         evc->ec_unsubscribe(evc, sgeE_CATEGORY_MOD);
         break;
      case SGE_TYPE_PROCEDURE:
         DRETURN(SGE_EM_NOT_INITIALIZED);
      case SGE_TYPE_RL:
         evc->ec_unsubscribe(evc, sgeE_RL_LIST);
         evc->ec_unsubscribe(evc, sgeE_RL_ADD);
         evc->ec_unsubscribe(evc, sgeE_RL_DEL);
         evc->ec_unsubscribe(evc, sgeE_RL_MOD);
         break;
     default:
         ERROR("received invalid event group %d", type);
         DRETURN(SGE_EM_BAD_ARG);
   }

   /* clear data */
   sge_mirror_free_list(type);

   DRETURN(SGE_EM_OK);
}

/**
 * @brief Retrieve and process events
 *
 * Retrieves new events from qmaster.
 * If new events have arrived from qmaster, they are processed,
 * that means, for each event
 * - if installed, a "before mirroring" callback is called
 * - the event is mirrored into the corresponding master list
 * - if installed, a "after mirroring" callback is called
 * If retrieving new events from qmaster fails over a time period
 * of 10 times the configured event delivery interval (see event
 * client interface, function ec_get_edtime), a timeout warning
 * is generated and a new registration of the event client is
 * prepared.
 *
 * @param evc the event client to fetch events for
 *
 * @return SGE_EM_OK or an error code
 *
 * @see `ec_get_edtime()`, `ec_set_edtime()`
 */
sge_mirror_error sge_mirror_process_events(sge_evc_class_t *evc) {
   DENTER(TOP_LAYER);

   // out-parameter of the event client, which hands the events over raw
   ocs::tom::ListData *event_list_raw = nullptr;
   sge_mirror_error ret = SGE_EM_OK;
   static int test_debug = 0;

   /// ec2_get()
   if (evc && evc->ec_get(evc, &event_list_raw, false)) {
      // the event client delivers ET events
      const auto event_list = ocs::tom::ListPtr<ocs::tom::ET>::from_typed(event_list_raw);
      if (event_list) {
         ret = sge_mirror_process_event_list(evc, *event_list);
      }
   } else {
      WARNING(SFNMAX, MSG_MIRROR_QMASTERALIVETIMEOUTEXPIRED);
      evc->ec_mark4registration(evc);
      ret = SGE_EM_TIMEOUT;
   }

   if (mir_get_produce_qmaster_alive_timeout()) {
      test_debug++;
      if (test_debug > 3) {
         test_debug = 0;
         WARNING(SFNMAX, MSG_MIRROR_QMASTERALIVETIMEOUTEXPIRED);
         evc->ec_mark4registration(evc);
         ret = SGE_EM_TIMEOUT;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Map errorcode to error message
 *
 * Returns a string describing a given error number.
 * This function can be used to output error messages
 * if a function of the event mirror interface fails.
 *
 * @param num error number
 *
 * @return corresponding error message
 */
const char *sge_mirror_strerror(sge_mirror_error num) {
   switch (num) {
      case SGE_EM_OK:
         return MSG_MIRROR_OK;
      case SGE_EM_NOT_INITIALIZED:
         return MSG_MIRROR_NOTINITIALIZED;
      case SGE_EM_BAD_ARG:
         return MSG_MIRROR_BADARG;
      case SGE_EM_TIMEOUT:
         return MSG_MIRROR_TIMEOUT;
      case SGE_EM_DUPLICATE_KEY:
         return MSG_MIRROR_DUPLICATEKEY;
      case SGE_EM_KEY_NOT_FOUND:
         return MSG_MIRROR_KEYNOTFOUND;
      case SGE_EM_CALLBACK_FAILED:
         return MSG_MIRROR_CALLBACKFAILED;
      case SGE_EM_PROCESS_ERRORS:
         return MSG_MIRROR_PROCESSERRORS;
      default:
         return "";
   }
}

static void sge_mirror_free_list(sge_object_type type) {
   ocs::DataStore::free_master_list(type);
}

/**
 * @brief Apply a list of events to the mirrored lists, one event after the other
 *
 * Calls sge_mirror_process_event() for every event, in list order. A failing
 * event makes the result an error but does not stop the processing of the
 * following ones.
 *
 * @param evc the event client the events arrived on
 * @param event_list the events to apply (ET), may be an empty handle
 *
 * @return SGE_EM_OK, or SGE_EM_PROCESS_ERRORS if at least one event failed
 */
sge_mirror_error
sge_mirror_process_event_list(sge_evc_class_t *evc, const ocs::tom::List<ocs::tom::ET> event_list) {
   DENTER(TOP_LAYER);
   sge_mirror_error function_ret;
   bool no_more_events=false;
   int num_events = 0;
   mirror_description *mirror_base = mir_get_mirror_base();

   PROF_START_MEASUREMENT(SGE_PROF_MIRROR);

   function_ret = SGE_EM_OK;

   mir_arm_test_callback_failures();

   for (const auto event : event_list) {
      sge_mirror_error ret = SGE_EM_OK;
      if (no_more_events) {
         break;
      }
      num_events++;

      switch (event.get<ET_type>()) {

         case sgeE_CALENDAR_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CALENDAR, SGE_EMA_LIST, event);
            break;
         case sgeE_CALENDAR_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CALENDAR, SGE_EMA_ADD, event);
            break;
         case sgeE_CALENDAR_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CALENDAR, SGE_EMA_DEL, event);
            break;
         case sgeE_CALENDAR_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CALENDAR, SGE_EMA_MOD, event);
            break;

         case sgeE_CKPT_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CKPT, SGE_EMA_LIST, event);
            break;
         case sgeE_CKPT_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CKPT, SGE_EMA_ADD, event);
            break;
         case sgeE_CKPT_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CKPT, SGE_EMA_DEL, event);
            break;
         case sgeE_CKPT_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CKPT, SGE_EMA_MOD, event);
            break;

         case sgeE_CENTRY_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CENTRY, SGE_EMA_LIST, event);
            break;
         case sgeE_CENTRY_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CENTRY, SGE_EMA_ADD, event);
            break;
         case sgeE_CENTRY_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CENTRY, SGE_EMA_DEL, event);
            break;
         case sgeE_CENTRY_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CENTRY, SGE_EMA_MOD, event);
            break;

         case sgeE_CONFIG_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CONFIG, SGE_EMA_LIST, event);
            break;
         case sgeE_CONFIG_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CONFIG, SGE_EMA_ADD, event);
            break;
         case sgeE_CONFIG_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CONFIG, SGE_EMA_DEL, event);
            break;
         case sgeE_CONFIG_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CONFIG, SGE_EMA_MOD, event);
            break;

         case sgeE_EXECHOST_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_EXECHOST, SGE_EMA_LIST, event);
            break;
         case sgeE_EXECHOST_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_EXECHOST, SGE_EMA_ADD, event);
            break;
         case sgeE_EXECHOST_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_EXECHOST, SGE_EMA_DEL, event);
            break;
         case sgeE_EXECHOST_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_EXECHOST, SGE_EMA_MOD, event);
            break;

         case sgeE_JATASK_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JATASK, SGE_EMA_ADD, event);
            break;
         case sgeE_JATASK_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JATASK, SGE_EMA_DEL, event);
            break;
         case sgeE_JATASK_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JATASK, SGE_EMA_MOD, event);
            break;

         case sgeE_PETASK_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PETASK, SGE_EMA_ADD, event);
            break;
         case sgeE_PETASK_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PETASK, SGE_EMA_DEL, event);
            break;

         case sgeE_JOB_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JOB, SGE_EMA_LIST, event);
            break;
         case sgeE_JOB_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JOB, SGE_EMA_ADD, event);
            break;
         case sgeE_JOB_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JOB, SGE_EMA_DEL, event);
            break;
         case sgeE_JOB_MOD:
         case sgeE_JOB_USAGE:
         case sgeE_JOB_FINAL_USAGE:
         case sgeE_JOB_FINISH:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JOB, SGE_EMA_MOD, event);
            break;
         case sgeE_JOB_SCHEDD_INFO_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JOB_SCHEDD_INFO, SGE_EMA_LIST, event);
            break;
         case sgeE_JOB_SCHEDD_INFO_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JOB_SCHEDD_INFO, SGE_EMA_ADD, event);
            break;
         case sgeE_JOB_SCHEDD_INFO_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JOB_SCHEDD_INFO, SGE_EMA_DEL, event);
            break;
         case sgeE_JOB_SCHEDD_INFO_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_JOB_SCHEDD_INFO, SGE_EMA_MOD, event);
            break;

         case sgeE_NEW_SHARETREE:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_SHARETREE, SGE_EMA_LIST, event);
            break;

         case sgeE_PE_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PE, SGE_EMA_LIST, event);
            break;
         case sgeE_PE_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PE, SGE_EMA_ADD, event);
            break;
         case sgeE_PE_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PE, SGE_EMA_DEL, event);
            break;
         case sgeE_PE_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PE, SGE_EMA_MOD, event);
            break;

         case sgeE_PROJECT_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PROJECT, SGE_EMA_LIST, event);
            break;
         case sgeE_PROJECT_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PROJECT, SGE_EMA_ADD, event);
            break;
         case sgeE_PROJECT_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PROJECT, SGE_EMA_DEL, event);
            break;
         case sgeE_PROJECT_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_PROJECT, SGE_EMA_MOD, event);
            break;

         case sgeE_QMASTER_GOES_DOWN:
         case sgeE_ACK_TIMEOUT:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_MARK_4_REGISTRATION, SGE_EMA_TRIGGER, event);
            break;
         
         case sgeE_CQUEUE_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CQUEUE, SGE_EMA_LIST, event);
            break;
         case sgeE_CQUEUE_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CQUEUE, SGE_EMA_ADD, event);
            break;
         case sgeE_CQUEUE_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CQUEUE, SGE_EMA_DEL, event);
            break;
         case sgeE_CQUEUE_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CQUEUE, SGE_EMA_MOD, event);
            break;
         
         case sgeE_QINSTANCE_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_QINSTANCE, SGE_EMA_ADD, event);
            break;
         case sgeE_QINSTANCE_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_QINSTANCE, SGE_EMA_DEL, event);
            break;
         case sgeE_QINSTANCE_MOD:
         case sgeE_QINSTANCE_SOS:
         case sgeE_QINSTANCE_USOS:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_QINSTANCE, SGE_EMA_MOD, event);
            break;

         case sgeE_SCHED_CONF:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_SCHEDD_CONF, SGE_EMA_MOD, event);
            break;

         case sgeE_SCHEDDMONITOR:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_SCHEDD_MONITOR, SGE_EMA_TRIGGER, event);
            break;

         case sgeE_SHUTDOWN:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_SHUTDOWN, SGE_EMA_TRIGGER, event);
            no_more_events = true;
            break;


         case sgeE_USER_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_USER, SGE_EMA_LIST, event);
            break;
         case sgeE_USER_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_USER, SGE_EMA_ADD, event);
            break;
         case sgeE_USER_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_USER, SGE_EMA_DEL, event);
            break;
         case sgeE_USER_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_USER, SGE_EMA_MOD, event);
            break;

         case sgeE_USERSET_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_USERSET, SGE_EMA_LIST, event);
            break;
         case sgeE_USERSET_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_USERSET, SGE_EMA_ADD, event);
            break;
         case sgeE_USERSET_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_USERSET, SGE_EMA_DEL, event);
            break;
         case sgeE_USERSET_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_USERSET, SGE_EMA_MOD, event);
            break;

         case sgeE_RQS_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_RQS, SGE_EMA_LIST, event);
            break;
         case sgeE_RQS_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_RQS, SGE_EMA_ADD, event);
            break;
         case sgeE_RQS_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_RQS, SGE_EMA_DEL, event);
            break;
         case sgeE_RQS_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_RQS, SGE_EMA_MOD, event);
            break;
   
         case sgeE_HGROUP_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_HGROUP, SGE_EMA_LIST, event);
            break;
         case sgeE_HGROUP_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_HGROUP, SGE_EMA_ADD, event);
            break;
         case sgeE_HGROUP_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_HGROUP, SGE_EMA_DEL, event);
            break;
         case sgeE_HGROUP_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_HGROUP, SGE_EMA_MOD, event);
            break;

         case sgeE_AR_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_AR, SGE_EMA_LIST, event);
            break;
         case sgeE_AR_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_AR, SGE_EMA_ADD, event);
            break;
         case sgeE_AR_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_AR, SGE_EMA_DEL, event);
            break;
         case sgeE_AR_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_AR, SGE_EMA_MOD, event);
            break;

         case sgeE_CATEGORY_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CATEGORY, SGE_EMA_LIST, event);
            break;
         case sgeE_CATEGORY_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CATEGORY, SGE_EMA_ADD, event);
            break;
         case sgeE_CATEGORY_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CATEGORY, SGE_EMA_DEL, event);
            break;
         case sgeE_CATEGORY_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_CATEGORY, SGE_EMA_MOD, event);
            break;

         case sgeE_RL_LIST:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_RL, SGE_EMA_LIST, event);
            break;
         case sgeE_RL_ADD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_RL, SGE_EMA_ADD, event);
            break;
         case sgeE_RL_DEL:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_RL, SGE_EMA_DEL, event);
            break;
         case sgeE_RL_MOD:
            ret = sge_mirror_process_event(evc, mirror_base, SGE_TYPE_RL, SGE_EMA_MOD, event);
            break;

         default:
            break;
      }

      /* 
       * if processing of an event failed, return an error to the caller of
       * this function, but continue processing.
       */
      if (ret != SGE_EM_OK) {
         function_ret = SGE_EM_PROCESS_ERRORS;

         if (ret == SGE_EM_CALLBACK_FAILED) {
            // CS-2859: enough of these close together mean the events this client receives can
            // no longer repair its lists, and only a total update can
            mir_note_callback_failure(evc);
         }
      }
   }

   if (prof_is_active(SGE_PROF_MIRROR)) {
      prof_stop_measurement(SGE_PROF_MIRROR, nullptr);
      PROFILING("PROF: sge_mirror processed %d events in %.3f s",
                 num_events, prof_get_measurement_wallclock(SGE_PROF_MIRROR,
                 false, nullptr));
   }

   DRETURN(function_ret);
}

/**
 * @brief The key an event names its object by, for objects keyed by the field K
 *
 * A number key travels in `ET_intkey`, a string or a host name in `ET_strkey`.
 *
 * @tparam K the key field of the object type
 * @param event the event (ET)
 * @return the key
 */
template <auto K>
static ocs::tom::KeyArgOf<ocs::tom::FieldTraits<K>::type>
ocs_mirror_event_key(const ocs::tom::ConstElement<ocs::tom::ET> event) {
   static_assert(ocs::tom::FieldTraits<K>::type != ocs::tom::FieldType::Uint64, "no event carries a 64 bit key");
   if constexpr (ocs::tom::FieldTraits<K>::type == ocs::tom::FieldType::Uint32) {
      return event.get<ET_intkey>();
   } else {
      return event.get<ET_strkey>();
   }
}

/**
 * @brief Whether the object an event's root type is keyed by is missing from a partial mirror of that root
 *
 * @tparam R the root type, an object type with a key
 * @param event the event (ET), naming the root object by its key
 * @return true if the root list is filtered and does not hold the root object; false otherwise
 */
template <sge_object_type R>
static bool
mir_event_root_missing(const ocs::tom::ConstElement<ocs::tom::ET> event) {
   using E = ocs_object_type_t<R>;
   static_assert(E::has_key, "a root type is found by its key");

   /* Only a deliberately partial (filtered) root list can legitimately lack it. */
   if (!sge_mirror_type_is_partial(R)) {
      return false;
   }

   const auto rlist = ocs::DataStore::master_list<R>();
   if (!rlist) {
      return true; /* nothing of the root type mirrored -> event targets an object we lack */
   }

   return !rlist.template find_first<E::key>(ocs_mirror_event_key<E::key>(event));
}

/**
 * @brief Whether an event belongs to a hierarchy whose root a partial mirror deliberately did not replicate
 *
 * On a partial (where-filtered) mirror, an event may reference an object
 * hierarchy whose filterable root we deliberately did not replicate - e.g. a
 * ja-task / pe-task / usage / finish event for another user's job, or a queue
 * instance of a cqueue outside our filter. Such events are expected, not errors.
 *
 * Deciding this once - before any callback runs - lets us skip the whole event
 * (before, default and after) silently and symmetrically, rather than having
 * each default handler log "can't find parent" and each before-callback hit the
 * same dead end. A full (unfiltered) mirror never skips, so its behaviour and its
 * genuine "missing object" diagnostics are unchanged.
 *
 * The per-type hierarchy below maps each child/sub-object type to the filterable
 * root whose presence it requires; add a case here to cover a new relationship.
 * The root is looked up by the key field of its type (#ocs_object_types).
 *
 * @param type the object type of the event
 * @param action the action of the event
 * @param event the event (ET)
 * @return true if the event is to be skipped
 */
static bool
mir_event_root_unmirrored(const sge_object_type type, const sge_event_action action,
                          const ocs::tom::ConstElement<ocs::tom::ET> event) {
   switch (type) {
      case SGE_TYPE_JOB:
      case SGE_TYPE_JATASK:
      case SGE_TYPE_PETASK:
         /* Creating or (re-)listing the root object itself must always be processed. */
         if (type == SGE_TYPE_JOB && (action == SGE_EMA_ADD || action == SGE_EMA_LIST)) {
            return false;
         }
         return mir_event_root_missing<SGE_TYPE_JOB>(event);
      case SGE_TYPE_CQUEUE:
      case SGE_TYPE_QINSTANCE:
         if (type == SGE_TYPE_CQUEUE && (action == SGE_EMA_ADD || action == SGE_EMA_LIST)) {
            return false;
         }
         return mir_event_root_missing<SGE_TYPE_CQUEUE>(event);
      default:
         return false; /* no filterable root -> never skip */
   }
}

/**
 * @brief Apply one event to the mirrored lists
 *
 * Skips events of a filtered-out hierarchy root and calls the before, default
 * and after callbacks of the event's object type.
 *
 * @param evc the event client the event arrived on
 * @param mirror_base the callback table of the mirror
 * @param type the object type the event concerns
 * @param action the action of the event
 * @param event the event (ET)
 * @return SGE_EM_OK, or SGE_EM_CALLBACK_FAILED when a callback failed
 */
static sge_mirror_error
sge_mirror_process_event(sge_evc_class_t *evc, mirror_description *mirror_base,
                         const sge_object_type type, const sge_event_action action,
                         const ocs::tom::Element<ocs::tom::ET> event) {
   DENTER(TOP_LAYER);

   sge_callback_result ret;
   char buffer[1024];
   dstring buffer_wrapper;

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));

   // CS-2859: SGE_PRODUCE_MIRROR_CALLBACK_ERROR fails callbacks on purpose, to test that a
   // client whose lists no longer agree with the qmaster's asks for a total update. The event is
   // not mirrored either, so the lists really do fall behind, as they do in the real case.
   if (mir_produce_callback_failure()) {
      ERROR(MSG_MIRROR_CALLBACKFAILED_S, event_text(event, &buffer_wrapper));
      DRETURN(SGE_EM_CALLBACK_FAILED);
   }

/* DEBUG */
   DPRINTF("%s\n", event_text(event, &buffer_wrapper));

   /* Skip events whose filtered-out hierarchy root we never mirrored, before any
    * callback runs - keeps a partial mirror silent and symmetric (no before/
    * default/after fires, no dirty key recorded). */
   if (mir_event_root_unmirrored(type, action, event)) {
      DPRINTF("ignoring %s: filtered-out parent not mirrored\n", event_text(event, &buffer_wrapper));
      DRETURN(SGE_EM_OK);
   }

   if (mirror_base[type].callback_before != nullptr) {
      ret = mirror_base[type].callback_before(evc, type, action, event, mirror_base[type].client_data);
      if (ret == SGE_EMA_FAILURE) {
         ERROR(MSG_MIRROR_CALLBACKFAILED_S, event_text(event, &buffer_wrapper));
         DRETURN(SGE_EM_CALLBACK_FAILED);
      } else if (ret == SGE_EMA_IGNORE) {
         DRETURN(SGE_EM_OK);
      }
   }

   if (mirror_base[type].callback_default != nullptr) {
      ret = mirror_base[type].callback_default(evc, type, action, event, nullptr);
      if (ret == SGE_EMA_FAILURE) {
         ERROR(MSG_MIRROR_CALLBACKFAILED_S, event_text(event, &buffer_wrapper));
         DRETURN(SGE_EM_CALLBACK_FAILED);
      } else if (ret == SGE_EMA_IGNORE) {
         DRETURN(SGE_EM_OK);
      }
   }

   if (mirror_base[type].callback_after != nullptr) {
      ret = mirror_base[type].callback_after(evc, type, action, event, mirror_base[type].client_data);
      if (ret == SGE_EMA_FAILURE) {
         ERROR(MSG_MIRROR_CALLBACKFAILED_S, event_text(event, &buffer_wrapper));
         DRETURN(SGE_EM_CALLBACK_FAILED);
      }
   }

   DRETURN(SGE_EM_OK);
}

/**
 * @brief The built in handling of a shutdown event: shut the mirror and the process down
 *
 * @param evc the event client the event arrived on
 * @param type the object type of the event, unused
 * @param action the action of the event, unused
 * @param event the event, unused
 * @param client_data client data, unused
 * @return always SGE_EMA_OK
 */
static sge_callback_result
sge_mirror_process_shutdown(sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                            [[maybe_unused]] const sge_event_action action,
                            [[maybe_unused]] const ocs::tom::Element<ocs::tom::ET> event,
                            [[maybe_unused]] void *client_data) {
   DENTER(TOP_LAYER);

   DPRINTF("shutting down sge mirror\n");
   sge_mirror_shutdown(evc);

   shut_me_down = 1;
   DRETURN(SGE_EMA_OK);
}

/**
 * @brief The built in handling of an acknowledge timeout or a qmaster shutdown: register again
 *
 * @param evc the event client the event arrived on
 * @param type the object type of the event, unused
 * @param action the action of the event, unused
 * @param event the event, unused
 * @param client_data client data, unused
 * @return always SGE_EMA_OK
 */
static sge_callback_result
sge_mirror_process_mark4registration(sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                                     [[maybe_unused]] const sge_event_action action,
                                     [[maybe_unused]] const ocs::tom::Element<ocs::tom::ET> event,
                                     [[maybe_unused]] void *client_data) {
   DENTER(TOP_LAYER);

   DPRINTF("mark4registration - this happens for ACK TIMEOUT or QMASTER GOES DOWN event\n");

   evc->ec_mark4registration(evc);

   DRETURN(SGE_EMA_OK);
}

/**
 * @brief Apply an event to a master list whose elements are found by the key field K
 *
 * Looks the element up by the key the event names - by string, by host name or by number, as the key field
 * is - and hands it to sge_mirror_update_master_list(). A number key of 0 names no element and is not looked
 * up. The list may hold a reduced descriptor; a new list takes the descriptor of the event's elements.
 *
 * @tparam K the key field of the object type
 * @tparam V the #sge_object_type value of the master list
 * @param action action to perform on list
 * @param event the event (ET); its object list is taken over into the master list
 * @return SGE_EM_OK or an error code
 *
 * @see #sge_mirror_update_master_list
 */
template <auto K, sge_object_type V>
static sge_mirror_error
ocs_mirror_update_master_list_by_key(const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event) {
   DENTER(TOP_LAYER);

   const auto key = ocs_mirror_event_key<K>(event);
   const auto list = ocs::DataStore::master_list_rw<V>();
   sge_mirror_error ret;

   if constexpr (ocs::tom::FieldTraits<K>::type == ocs::tom::FieldType::Uint32) {
      ocs::tom::Element<typename ocs::tom::FieldTraits<K>::Type> ep;
      if (key > 0) {
         ep = list.template find_first<K>(key);
      }
      DSTRING_STATIC(dstr, 32);
      ret = ocs_mirror_update_data_store_list<V>(ep, sge_dstring_sprintf(&dstr, sge_u32, key), action, event);
   } else if constexpr (ocs::tom::FieldTraits<K>::type == ocs::tom::FieldType::Host) {
      ret = ocs_mirror_update_data_store_list<V>(list.template find_first<K>(ocs::tom::Host(key)), key, action,
                                                 event);
   } else {
      ret = ocs_mirror_update_data_store_list<V>(list.template find_first<K>(key), key, action, event);
   }

   DRETURN(ret);
}

/**
 * @brief Apply an event to the master list of its object type, finding the element by the type's key field
 *
 * Dispatches on the object type once (ocs_with_object_type()); the lookup and the update are then compiled
 * per object type with its key field. Serves every type that is mirrored by key alone: the generic callback
 * (strings and numbers, e.g. advance reservations and categories), the host lists and the host groups.
 *
 * @param type the object type of the event
 * @param action action to perform on the list
 * @param event the event (ET); its object list is taken over into the master list
 * @return SGE_EM_OK or an error code; SGE_EM_BAD_ARG for a type without an object or without a key
 */
sge_mirror_error
ocs_mirror_update_master_list_by_type(const sge_object_type type, const sge_event_action action,
                                      const ocs::tom::Element<ocs::tom::ET> event) {
   DENTER(TOP_LAYER);

   const auto ret = ocs_with_object_type(type, [&](const auto &entry) {
      using E = ocs_object_type_entry_t<decltype(entry)>;
      if constexpr (E::has_key) {
         return ocs_mirror_update_master_list_by_key<E::key, E::value>(action, event);
      } else {
         return SGE_EM_BAD_ARG;
      }
   });

   DRETURN(ret.value_or(SGE_EM_BAD_ARG));
}

/**
 * @brief The default mirroring of an object type that is found by its key field alone
 *
 * @param evc the event client the event arrived on
 * @param type event type
 * @param action action to perform
 * @param event the event (ET); its object list is taken over into the master list
 * @param client_data client data
 * @return SGE_EMA_OK, or SGE_EMA_FAILURE if the update or the validation of the scheduler configuration failed
 *
 * @see #ocs_mirror_update_master_list_by_type
 */
static sge_callback_result
generic_update_master_list([[maybe_unused]] sge_evc_class_t *evc, const sge_object_type type,
                           const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event,
                           [[maybe_unused]] void *client_data) {
   DENTER(TOP_LAYER);

   if (ocs_mirror_update_master_list_by_type(type, action, event) != SGE_EM_OK) {
      DRETURN(SGE_EMA_FAILURE);
   }

   if (type == SGE_TYPE_SCHEDD_CONF && !sconf_validate_config_(nullptr)) {
      DRETURN(SGE_EMA_FAILURE);
   }

   DRETURN(SGE_EMA_OK);
}

/**
 * @brief Update a master list
 *
 * Updates a given master list according to the given event information.
 * The following actions are performed (depending on parameter action):
 * - SGE_EMA_LIST: an existing mirrored list is completely replaced
 * - SGE_EMA_ADD:  a new element is added to the mirrored list
 * - SGE_EMA_MOD:  a given element is modified
 * - SGE_EMA_DEL:  a given element is deleted
 *
 * A list created for an ADD takes the descriptor of the event's objects, which is a reduced one for a
 * subscription with a field selection.
 *
 * @tparam T the object type of the list and of the event's objects
 * @param list mirrored list to manipulate, taken out of the slot of a master list of the data store or of a
 *        list field of the parent (ocs_mirror_update_data_store_list(), ocs_mirror_update_sub_list()); created
 *        for an ADD if it is empty, replaced for a LIST
 * @param ep element to manipulate or an empty handle
 * @param key key of an element to manipulate
 * @param action action to perform
 * @param event the event (ET); on LIST its list is exchanged with the master list, on ADD and MOD its
 *        first object is moved into the master list
 *
 * @return SGE_EM_OK, or an error code
 *
 * @see #ocs_mirror_update_master_list_by_type
 */
template <ocs::tom::ObjectType T>
sge_mirror_error
sge_mirror_update_master_list(ocs::tom::ListPtr<T> &list, const ocs::tom::Element<T> ep, const char *key,
                              const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event) {
   DENTER(TOP_LAYER);

   switch (action) {
      case SGE_EMA_LIST:
         /* switch to new list; the event takes the old one */
         list = ocs_mirror_exchange_event_objects<T>(event, std::move(list));
         break;

      case SGE_EMA_ADD: {
         /* check for duplicate */
         if (ep) {
            ERROR("duplicate list element " SFQ "\n", (key != nullptr) ?key:"nullptr");
            DRETURN(SGE_EM_DUPLICATE_KEY);
         }

         const auto objects = ocs_mirror_event_objects<T>(event);

         /* if neccessary, create list; without objects the event has no descriptor and no list is created */
         if (!list && objects) {
            list = ocs::tom::ListPtr<T>::create("", objects.descr());
         }

         /* insert element */
         if (const auto first = objects.front()) {
            list->push_back(objects.extract(first));
         }
         break;
      }

      case SGE_EMA_DEL:
         /* check for existence */
         if (!ep) {
            ERROR("element " SFQ " does not exist\n", (key != nullptr) ?key:"nullptr");
            DRETURN(SGE_EM_KEY_NOT_FOUND);
         }

         /* remove element */
         list->erase(ep);
         break;

      case SGE_EMA_MOD: {
         /* check for existence */
         if (!ep) {
            ERROR("element " SFQ " does not exist\n", (key != nullptr) ?key:"nullptr");
            DRETURN(SGE_EM_KEY_NOT_FOUND);
         }
         const auto master_list = list.get();
         master_list.erase(ep);
         const auto objects = ocs_mirror_event_objects<T>(event);
         if (const auto first = objects.front()) {
            master_list.push_back(objects.extract(first));
         }
         break;
      }
      default:
         DRETURN(SGE_EM_BAD_ARG);
   }

   DRETURN(SGE_EM_OK);
}

/*
 * The mirror callbacks outside this file update the jobs, array tasks, parallel tasks, cluster queues and queue
 * instances with sge_mirror_update_master_list(); the object types updated by key alone instantiate it above.
 */
template sge_mirror_error
sge_mirror_update_master_list<ocs::tom::JB>(ocs::tom::ListPtr<ocs::tom::JB> &, ocs::tom::Element<ocs::tom::JB>,
                              const char *, sge_event_action, ocs::tom::Element<ocs::tom::ET>);
template sge_mirror_error
sge_mirror_update_master_list<ocs::tom::JAT>(ocs::tom::ListPtr<ocs::tom::JAT> &, ocs::tom::Element<ocs::tom::JAT>,
                              const char *, sge_event_action, ocs::tom::Element<ocs::tom::ET>);
template sge_mirror_error
sge_mirror_update_master_list<ocs::tom::PET>(ocs::tom::ListPtr<ocs::tom::PET> &, ocs::tom::Element<ocs::tom::PET>,
                              const char *, sge_event_action, ocs::tom::Element<ocs::tom::ET>);
template sge_mirror_error
sge_mirror_update_master_list<ocs::tom::CQ>(ocs::tom::ListPtr<ocs::tom::CQ> &, ocs::tom::Element<ocs::tom::CQ>,
                              const char *, sge_event_action, ocs::tom::Element<ocs::tom::ET>);
template sge_mirror_error
sge_mirror_update_master_list<ocs::tom::QU>(ocs::tom::ListPtr<ocs::tom::QU> &, ocs::tom::Element<ocs::tom::QU>,
                              const char *, sge_event_action, ocs::tom::Element<ocs::tom::ET>);
