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
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations of the event master
 *
 * @see sge_event_master.cc
 */

#include <cinttypes>
#include <cstddef>

#include "sgeobj/sge_event.h"
#include "sgeobj/sge_daemonize.h"
#include "sgeobj/tom/ocs_tom_EV.h"
#include "sgeobj/tom/ocs_tom_REP.h"

#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "uti/sge_monitor.h"
#include "sgeobj/sge_answer.h"

/*
 * EVENT_MASTER_MIN_FREE_DESCRIPTORS
 * Event master assumes that every event client requires one file descriptor
 * for communication (in commlib).
 * This define is the number of file descriptors not to be used by
 * event master, but for use by the program containing event master
 * (sge_qmaster).
 */
/**
 * @brief File descriptors the event master leaves to the rest of qmaster
 *
 * Every event client costs a commlib connection, i.e. a file descriptor. The
 * limit on dynamic clients is derived from the process descriptor limit minus
 * this reserve, so the daemon around the event master still has descriptors to
 * work with.
 */
#define EVENT_MASTER_MIN_FREE_DESCRIPTORS 25

/**
 * @brief One thread's open event transaction
 *
 * A request handler may produce several events that only make sense together.
 * Between `sge_event_master_transaction_begin` and its commit the events are
 * collected here instead of being handed to the delivery thread, so a client
 * never sees half of a change. Thread local, keyed by
 * #event_master_control_t::transaction_key.
 */
typedef struct {
   bool     is_transaction;         ///< identifies, if a transaction is open, or not
   ocs::tom::ListData    *transaction_requests;  ///< all event add requests collected while the transaction is open
} event_master_transaction_t;
 
/**
 * @brief All state the event master runs on
 *
 * One global instance, #Event_Master_Control. Two mutexes guard it and they are
 * not interchangeable: `mutex` is taken by the public entry points, `cond_mutex`
 * by the internal ones together with `cond_var`. The request list has a third,
 * `request_mutex`, so a producer can queue a request without contending with
 * the delivery thread.
 */
typedef struct {
   pthread_mutex_t  mutex;                 ///< mutual exclusion; only use in public functions
   pthread_cond_t   cond_var;              ///< used for waiting
   pthread_mutex_t  cond_mutex;            ///< mutual exclusion; only use in internal functions
   bool             delivery_signaled;     ///< an event delivery has been signaled; protected by `cond_mutex`

   uint32_t         max_event_clients;     ///< max number of custom event clients, the scheduler not counted; protected by `mutex`

   bool             is_prepare_shutdown;   ///< set when qmaster is going down; no new event clients are accepted then; protected by `mutex`
   ocs::tom::ListData*           clients;               ///< list of event master clients
   ocs::tom::ListData*           client_ids;            ///< range list holding free event client ids
   ocs::tom::ListData*           requests;              ///< event master requests (add/mod/del evc, add/ack event)
   pthread_mutex_t  request_mutex;         ///< protects access to the request list

   pthread_key_t     transaction_key;      ///< key to access thread local transaction storage
} event_master_control_t;

/// The one event master instance; see @ref event_master_control_t
extern event_master_control_t Event_Master_Control;
void sge_cleanup_event_master_control(void *arg);
void sge_event_master_flush_requests(bool force = false);

void sge_event_master_process_requests(monitoring_t *monitor);
void sge_event_master_send_events(ocs::tom::Element<ocs::tom::REP> report, ocs::tom::List<ocs::tom::REP> report_list,
                                  monitoring_t *monitor);
void sge_event_master_wait_next();

int sge_add_event_client(const ocs::gdi::Packet *packet, ocs::tom::ElementData *ev,
                         ocs::tom::ListPtr<ocs::tom::AN> *alpp, ocs::tom::ListData **eclpp,
                         event_client_update_func_t update_func, void *update_func_arg);

int sge_mod_event_client(ocs::tom::ElementData *clio, ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost);
bool sge_has_event_client(uint32_t aClientID);
void sge_remove_event_client(uint32_t aClientID);
ocs::tom::ListData* sge_select_event_clients(const char *list_name, const ocs::tom::WhereData *where,
                                             const ocs::tom::WhatData *what);
int sge_shutdown_event_client(const ocs::gdi::Packet *packet, uint32_t aClientID,
                              ocs::tom::ListPtr<ocs::tom::AN> *alpp);
int sge_shutdown_dynamic_event_clients(const ocs::gdi::Packet *packet, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
                                       monitoring_t *monitor);

/*
 * Adding events. The object an event carries is a copy of the one passed, in a list of the object's type
 * (ET_new_version). The functions are templates on that type; they are instantiated in sge_event_master.cc for
 * every object type of ocs_object_types and for the job report (JR) and the usage (UA), the types events carry.
 */

template <ocs::tom::ObjectType T>
bool sge_add_event_for_client(uint32_t event_client_id, uint64_t timestamp, ev_event type, uint32_t intkey,
                              uint32_t intkey2, const char *strkey, const char *strkey2, const char *session,
                              ocs::tom::Element<T> element, uint64_t gdi_session);

bool sge_add_event_for_client(uint32_t event_client_id, uint64_t timestamp, ev_event type, uint32_t intkey,
                              uint32_t intkey2, const char *strkey, const char *strkey2, const char *session,
                              std::nullptr_t element, uint64_t gdi_session);

/**
 * @brief Add an object as event for all event clients
 *
 * Adds an object to the list of events to deliver. Called, if an event
 * occurs to that object, e.g. it was added to Cluster Scheduler, modified or
 * deleted. Internally, a list with a copy of the object is created and
 * queued as the event's payload.
 *
 * @tparam T the object type, deduced
 * @param timestamp event creation time, 0 -> use current time
 * @param type the event id
 * @param intkey additional data
 * @param intkey2 additional data
 * @param strkey additional data
 * @param strkey2 additional data
 * @param session events session key
 * @param element the object to deliver as event, or an empty handle for an event without an object
 * @param gdi_session the session the change belongs to, for read-after-write
 * @return true when the event was queued
 *
 * @note MT-NOTE: sge_add_event() is MT safe.
 */
template <ocs::tom::ObjectType T>
inline bool sge_add_event(const uint64_t timestamp, const ev_event type, const uint32_t intkey,
                          const uint32_t intkey2, const char *strkey, const char *strkey2, const char *session,
                          const ocs::tom::Element<T> element, const uint64_t gdi_session) {
   return sge_add_event_for_client<T>(EV_ID_ANY, timestamp, type, intkey, intkey2, strkey, strkey2, session,
                                      element, gdi_session);
}

/**
 * @brief Add an event without an object for all event clients
 *
 * @param timestamp event creation time, 0 -> use current time
 * @param type the event id
 * @param intkey additional data
 * @param intkey2 additional data
 * @param strkey additional data
 * @param strkey2 additional data
 * @param session events session key
 * @param element nullptr: the event carries no object
 * @param gdi_session the session the change belongs to, for read-after-write
 * @return true when the event was queued
 *
 * @note MT-NOTE: sge_add_event() is MT safe.
 */
inline bool sge_add_event(const uint64_t timestamp, const ev_event type, const uint32_t intkey,
                          const uint32_t intkey2, const char *strkey, const char *strkey2, const char *session,
                          const std::nullptr_t element, const uint64_t gdi_session) {
   return sge_add_event_for_client(EV_ID_ANY, timestamp, type, intkey, intkey2, strkey, strkey2, session, element,
                                   gdi_session);
}

template <ocs::tom::ObjectType T>
bool sge_add_list_event(uint64_t timestamp, ev_event type, uint32_t intkey, uint32_t intkey2, const char *strkey,
                        const char *strkey2, const char *session, ocs::tom::List<T> list, uint64_t gdi_session);

bool sge_handle_event_ack(uint32_t event_client_id, uint32_t event_number);
void sge_deliver_events_immediately(uint32_t aClientID);

int sge_resync_schedd(monitoring_t *monitor, uint64_t gdi_session);

uint32_t sge_set_max_dynamic_event_clients(uint32_t max);
uint32_t sge_get_max_dynamic_event_clients();
uint32_t sge_get_num_event_clients();

void sge_event_master_init();
bool sge_commit(uint64_t gdi_session);
void sge_set_commit_required();
