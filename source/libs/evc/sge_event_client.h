#pragma once
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
 * @brief Public interface of the event client library
 *
 * Declares the event client handle #sge_evc_class_t, its factory and its
 * destructor. The narrative documentation of the interface as a whole - id
 * numbers, subscription, flushing, busy handling, session filtering and the
 * list of events - is on the @ref evc_interface page.
 *
 * @see @ref evc
 */

#include "uti/sge_monitor.h"

#include "sgeobj/sge_event.h"

#include "gdi/ocs_gdi_Request.h"

/** @defgroup evc Event Client Interface
 * @brief Connect to qmaster and receive object changes as events
 *
 * See @ref evc_interface for the narrative description of the interface, and
 * @ref evc_events for the list of events that can be subscribed.
 * @{
 */

/** @brief Entry points of the event master, for qmaster internal event clients
 *
 * An internal event client runs inside qmaster and reaches the event master
 * directly, without passing through the commlib and the GDI. Routing its calls
 * through this table of function pointers keeps the event client free of a
 * link time dependency on the event master, which would otherwise be circular.
 *
 * The table is filled by the component that instantiates an internal event
 * client; for external clients it stays zeroed and is never consulted.
 */
typedef struct {
   bool init;                                  ///< true once the table has been filled
   event_client_update_func_t update_func;     ///< called by the event master to hand new events to the client
   evm_add_func_t add_func;                    ///< registers the client at the event master
   evm_mod_func_t mod_func;                    ///< sends configuration changes to the event master
   evm_remove_func_t remove_func;              ///< deregisters the client at the event master
   evm_ack_func_t ack_func;                    ///< acknowledges receipt of events up to a given number
   void *update_func_arg;                      ///< opaque argument passed back to @p update_func
} local_t;


/** @brief Default interval in seconds at which qmaster delivers events */
#define DEFAULT_EVENT_DELIVERY_INTERVAL (10)

/** @brief Handle of one event client, see #sge_evc_class_str */
typedef struct sge_evc_class_str sge_evc_class_t;

/** @brief One event client, as a table of operations plus its private state
 *
 * The operations are function pointers rather than plain functions because an
 * event client comes in two flavours that behave differently but are used
 * identically by callers: an *external* client that talks to qmaster over the
 * GDI, and an *internal* client that runs inside qmaster and calls the event
 * master directly through #ec_local. #sge_evc_class_create() decides which
 * flavour to build and fills the table accordingly.
 *
 * All operations take the handle itself as their first argument @p thiz.
 *
 * Create an instance with #sge_evc_class_create() and release it with
 * #sge_evc_class_destroy(). See @ref evc_interface for what the operations
 * mean and in which order they are meant to be called.
 */
struct sge_evc_class_str {
   void *sge_evc_handle;   ///< private per client state, opaque outside the implementation

   local_t ec_local;       ///< event master entry points, used by internal clients only

   /// register at the event server, (re)connecting if necessary
   bool (*ec_register)(sge_evc_class_t *thiz, bool exit_on_qmaster_down, lList **alpp);
   /// deregister at the event server, so it stops spooling events for this client
   bool (*ec_deregister)(sge_evc_class_t *thiz);
   /// send pending configuration changes to the event server
   bool (*ec_commit)(sge_evc_class_t *thiz, lList **alpp);
   /// acknowledge all events received so far
   bool (*ec_ack)(sge_evc_class_t *thiz);
   /// true once the client has been set up, i.e. its event client object exists
   bool (*ec_is_initialized)(sge_evc_class_t *thiz);
   /// return the underlying `EV_Type` event client object, or nullptr
   lListElem* (*ec_get_event_client)(sge_evc_class_t *thiz);

   /// subscribe one event, or all of them when passed #sgeE_ALL_EVENTS
   bool (*ec_subscribe)(sge_evc_class_t *thiz, ev_event event);
   /// subscribe every event, see @ref evc_events
   bool (*ec_subscribe_all)(sge_evc_class_t *thiz);

   /// unsubscribe one event; the three mandatory events cannot be unsubscribed
   bool (*ec_unsubscribe)(sge_evc_class_t *thiz, ev_event event);
   /// unsubscribe every event except the three mandatory ones
   bool (*ec_unsubscribe_all)(sge_evc_class_t *thiz);

   /// flush interval of one event, or `EV_NO_FLUSH` when flushing is off
   int (*ec_get_flush)(sge_evc_class_t *thiz, ev_event event);
   /// configure flushing of one already subscribed event
   bool (*ec_set_flush)(sge_evc_class_t *thiz, ev_event event, bool flush, int interval);
   /// switch flushing off for one event
   bool (*ec_unset_flush)(sge_evc_class_t *thiz, ev_event event);

   /// subscribe one event and configure its flushing in a single call
   bool (*ec_subscribe_flush)(sge_evc_class_t *thiz, ev_event event, int flush);

   /// attach a what/where filter so qmaster reduces the data sent, see @ref evc_list_filtering
   bool (*ec_mod_subscription_where)(sge_evc_class_t *thiz, ev_event event, const lListElem *what, const lListElem *where);

   /// set the event delivery interval in seconds
   bool (*ec_set_edtime)(sge_evc_class_t *thiz, uint32_t intval);
   /// current event delivery interval in seconds
   uint32_t (*ec_get_edtime)(sge_evc_class_t *thiz);

   /// choose how the busy state is set and cleared, see @ref evc_busy_state
   bool (*ec_set_busy_handling)(sge_evc_class_t *thiz, ev_busy_handling handling);
   /// currently configured busy handling policy
   ev_busy_handling (*ec_get_busy_handling)(sge_evc_class_t *thiz);

   /// mark the client busy or idle; effective after the next commit
   bool (*ec_set_busy)(sge_evc_class_t *thiz, int busy);
   /// locally known busy state of this client
   bool (*ec_get_busy)(sge_evc_class_t *thiz);

   /// set the session key used to filter events, see @ref evc_session_filtering
   bool (*ec_set_session)(sge_evc_class_t *thiz, const char *session);
   /// session key currently used for filtering, or nullptr
   const char *(*ec_get_session)(sge_evc_class_t *thiz);

   /// event client id assigned by qmaster, or #EV_ID_INVALID
   ev_registration_id (*ec_get_id)(sge_evc_class_t *thiz);

   /// commit configuration changes as the final part of a GDI multi request
   bool (*ec_commit_multi)(sge_evc_class_t *thiz, lList **malp, ocs::gdi::Request *state);

   /// fetch newly arrived events, registering and committing first if needed
   bool (*ec_get)(sge_evc_class_t *thiz, lList **event_list, bool exit_on_qmaster_down);

   /// force a new registration, e.g. after the connection to qmaster broke
   void (*ec_mark4registration)(sge_evc_class_t *thiz);
   /// true while the client still has to (re)register
   bool (*ec_need_new_registration)(sge_evc_class_t *thiz);

   /// hand newly produced events to an internal client and wake it up
   int (*ec_signal)(sge_evc_class_t *thiz, lList **alpp, lList *event_list);
   /// release the busy state after the client finished processing a batch
   void (*ec_wait)(sge_evc_class_t *thiz);

   /// true if new events are queued for an internal client
   bool (*ec_evco_triggered)(sge_evc_class_t *thiz);
   /// true once an internal client has been asked to shut down
   bool (*ec_evco_exit)(sge_evc_class_t *thiz);

   /// dump the current settings, for debugging
   void (*dprintf)(sge_evc_class_t *thiz);

   /// scheduler monitoring requested via `qconf -tsm`, inherited from old releases
   bool monitor_next_run;
};

/** @} */

sge_evc_class_t *
sge_evc_class_create(ev_registration_id reg_id, lList **alpp, const char *name);

void
sge_evc_class_destroy(sge_evc_class_t **pst);

bool
sge_gdi2_evc_setup(sge_evc_class_t **evc_ref, ev_registration_id reg_id, lList **alpp, const char * name);
