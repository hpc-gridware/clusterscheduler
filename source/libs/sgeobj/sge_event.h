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
 * @brief The event types and the event client registration object
 *
 * @see sge_event.cc
 * @see @ref evc
 */

#include "cull/cull_list.h"
#include "uti/sge_dstring.h"

#include "gdi/ocs_gdi_Packet.h"

#include "sgeobj/cull/sge_event_EV_L.h"
#include "sgeobj/cull/sge_event_EVS_L.h"
#include "sgeobj/cull/sge_event_ET_L.h"

/// What an event master request asks for
typedef enum {
   EVR_ADD_EVC = 0, ///< register a new event client
   EVR_MOD_EVC,     ///< change an existing event client's subscription
   EVR_DEL_EVC,     ///< deregister an event client
   EVR_ADD_EVENT,   ///< add an event to be delivered
   EVR_ACK_EVENT    ///< acknowledge events the client has processed
} evm_request_t;

/**
 * @brief The id an event client registers under
 *
 * The low ids are reserved for the components qmaster knows about; everyone
 * else registers as #EV_ID_ANY and is assigned an id from #EV_ID_FIRST_DYNAMIC
 * upwards.
 *
 * @see @ref evc
 */
typedef enum {
   EV_ID_INVALID = -1,              ///< not a valid id
   EV_ID_ANY = 0,                   ///< qmaster will give the ev a unique id
   EV_ID_SCHEDD = 1,                ///< schedd registers at qmaster
   EV_ID_EVENT_MIRROR_LISTENER = 2, ///< event mirror thread of the listener registers as event client
   EV_ID_EVENT_MIRROR_READER = 3,   ///< event mirror thread of the reader registers as event client
   EV_ID_FIRST_DYNAMIC = 11         ///< first id given by qmaster for EV_ID_ANY registration
}ev_registration_id;

/*-------------------------------------------*/
/* data structurs for the local event client */
/*-------------------------------------------*/

/**
 * @brief Delivers events to a process internal event client or mirror
 *
 * A client living in the same process as the event master is handed its events
 * through this callback rather than over the network.
 *
 * @param id event client id
 * @param alpp answer list
 * @param event_list list of new events stored in the report list
 * @param arg argument passed via `sge_mirror_initialize`
 */
typedef void (*event_client_update_func_t)(
   uint32_t id,                /* event client id */
   lList **alpp,               /* answer list */
   lList *event_list,          /* list of new events stored in the report list */
   void *arg                   ///< argument passed via sge_mirror_initialize
);

/**
 * @brief Changes an existing event client, part of the event master
 *
 * @param clio the new event client structure with a set update_func
 * @param alpp an answer list
 * @param ruser calling user
 * @param rhost calling host
 * @return 0 on success
 *
 * @see `sge_mod_event_client`
 */
typedef int (*evm_mod_func_t)(
   lListElem *clio,  /* the new event client structure with a set update_func */
   lList** alpp,     /* a answer list */
   char* ruser,      /* calling user */
   char* rhost       /* calling host */
);

/**
 * @brief Registers a new event client, part of the event master
 *
 * @param packet the request the registration came in
 * @param clio the new event client
 * @param alpp the answer list
 * @param eclpp list with added event client elem
 * @param update_func the event client update_func
 * @param update_func_arg additional argument passed to update_func
 * @return 0 on success
 *
 * @see `sge_add_event_client_local`
 */
typedef int (*evm_add_func_t)(
   const ocs::gdi::Packet *packet,
   lListElem *clio,                        /* the new event client */
   lList **alpp,                           /* the answer list */
   lList **eclpp,                          /* list with added event client elem */
   event_client_update_func_t update_func, /* the event client update_func */
   void *update_func_arg                   /* additional argument passed to update_func */
);

/**
 * @brief Deregisters an event client, part of the event master
 *
 * @param aClientID the event client id to remove
 *
 * @see `sge_remove_event_client`
 */
typedef void (*evm_remove_func_t) (
   uint32_t aClientID               /* the event client id to remove */
);

/* documentation see libs/evc/sge_event_client.c */
/* #define EV_NO_FLUSH -1 */

#define EV_NOT_SUBSCRIBED false ///< the client does not want this event
#define EV_SUBSCRIBED true      ///< the client wants this event
#define EV_FLUSHED true         ///< this event triggers a delivery of its own
#define EV_NOT_FLUSHED false    ///< this event waits for the next scheduled delivery
#define EV_MAX_FLUSH 0x3f       ///< largest flush delay, in seconds
#define EV_NO_FLUSH (-1)        ///< no flush delay is configured for this event

/* documentation see libs/evc/sge_event_client.c */

/**
 * @brief When qmaster considers an event client too busy to receive more
 *
 * A slow client would otherwise accumulate an unbounded event backlog.
 */
typedef enum {
   EV_BUSY_NO_HANDLING = 0,  ///< never treat the client as busy
   EV_BUSY_UNTIL_ACK,        ///< busy from delivery until the client acknowledges
   EV_BUSY_UNTIL_RELEASED    ///< busy until the client explicitly says it is ready again
} ev_busy_handling;

/// Where an event client is in its life cycle
typedef enum {
   EV_subscribing = 0, ///< registering and stating what it wants
   EV_connected,       ///< receiving events
   EV_closing,         ///< shutting down, no longer subscribing
   EV_terminated       ///< gone; qmaster may drop its state
} ev_state_handling;

/**
 * @brief Every kind of event qmaster can deliver to an event client
 *
 * Most objects contribute four values: a `_LIST` sent once at registration to
 * seed the client's copy, and `_ADD` / `_DEL` / `_MOD` for each later change.
 *
 * @warning The value is an index. These arrays in
 *          `libs/evm/sge_event_master.cc` are indexed by this enum and have to
 *          be adapted whenever a value is added or removed:
 *          `block_events`, `total_update_events`, `EVENT_LIST`, `FIELD_LIST`,
 *          `SOURCE_LIST`.
 *
 * @see @ref evc
 */
typedef enum {
   sgeE_ALL_EVENTS,                  ///< not an event; subscribes to all of them


   sgeE_CALENDAR_LIST,               ///< send calendar list at registration
   sgeE_CALENDAR_ADD,                ///< event add calendar
   sgeE_CALENDAR_DEL,                ///< event delete calendar
   sgeE_CALENDAR_MOD,                ///< event modify calendar

   sgeE_CKPT_LIST,                   ///< send ckpt list at registration
   sgeE_CKPT_ADD,                    ///< event add ckpt
   sgeE_CKPT_DEL,                    ///< event delete ckpt
   sgeE_CKPT_MOD,                    ///< event modify ckpt

   sgeE_CENTRY_LIST,                 ///< send complex list at registration
   sgeE_CENTRY_ADD,                  ///< event add complex
   sgeE_CENTRY_DEL,                  ///< event delete complex
   sgeE_CENTRY_MOD,                  ///< event modify complex

   sgeE_CONFIG_LIST,                 ///< send config list at registration
   sgeE_CONFIG_ADD,                  ///< event add config
   sgeE_CONFIG_DEL,                  ///< event delete config
   sgeE_CONFIG_MOD,                  ///< event modify config

   sgeE_EXECHOST_LIST,               ///< send exec host list at registration
   sgeE_EXECHOST_ADD,                ///< event add exec host
   sgeE_EXECHOST_DEL,                ///< event delete exec host
   sgeE_EXECHOST_MOD,                ///< event modify exec host

   sgeE_JATASK_ADD,                  ///< event add array job task
   sgeE_JATASK_DEL,                  ///< event delete array job task
   sgeE_JATASK_MOD,                  ///< event modify array job task

   sgeE_PETASK_ADD,                  ///< event add a new pe task
   sgeE_PETASK_MOD,                  ///< event add a new pe task
   sgeE_PETASK_DEL,                  ///< event delete a pe task

   sgeE_JOB_LIST,                    ///< send job list at registration
   sgeE_JOB_ADD,                     ///< event job add (new job)
   sgeE_JOB_DEL,                     ///< event job delete
   sgeE_JOB_MOD,                     ///< event job modify
   sgeE_JOB_USAGE,                   ///< event job online usage
   sgeE_JOB_FINAL_USAGE,             ///< event job final usage report after job end
   sgeE_JOB_FINISH,                  ///< job finally finished or aborted (user view)

   sgeE_JOB_SCHEDD_INFO_LIST,        ///< send job schedd info list at registration
   sgeE_JOB_SCHEDD_INFO_ADD,         ///< event jobs schedd info added
   sgeE_JOB_SCHEDD_INFO_DEL,         ///< event jobs schedd info deleted
   sgeE_JOB_SCHEDD_INFO_MOD,         ///< event jobs schedd info modified

   sgeE_NEW_SHARETREE,               ///< replace possibly existing share tree

   sgeE_PE_LIST,                     ///< send pe list at registration
   sgeE_PE_ADD,                      ///< event pe add
   sgeE_PE_DEL,                      ///< event pe delete
   sgeE_PE_MOD,                      ///< event pe modify

   sgeE_PROJECT_LIST,                ///< send project list at registration
   sgeE_PROJECT_ADD,                 ///< event project add
   sgeE_PROJECT_DEL,                 ///< event project delete
   sgeE_PROJECT_MOD,                 ///< event project modify

   sgeE_QMASTER_GOES_DOWN,           ///< qmaster notifies all event clients, before it exits

   sgeE_CQUEUE_LIST,                 ///< send cluster queue list at registration
   sgeE_CQUEUE_ADD,                  ///< event cluster queue add
   sgeE_CQUEUE_DEL,                  ///< event cluster queue delete
   sgeE_CQUEUE_MOD,                  ///< event cluster queue modify

   sgeE_QINSTANCE_ADD,               ///< event queue instance add
   sgeE_QINSTANCE_DEL,               ///< event queue instance delete
   sgeE_QINSTANCE_MOD,               ///< event queue instance mod
   sgeE_QINSTANCE_SOS,               ///< event queue instance sos
   sgeE_QINSTANCE_USOS,              ///< event queue instance usos

   sgeE_SCHED_CONF,                  ///< replace existing (sge) scheduler configuration

   sgeE_SCHEDDMONITOR,               ///< trigger scheduling run

   sgeE_SHUTDOWN,                    ///< request shutdown of an event client


   sgeE_USER_LIST,                   ///< send user list at registration
   sgeE_USER_ADD,                    ///< event user add
   sgeE_USER_DEL,                    ///< event user delete
   sgeE_USER_MOD,                    ///< event user modify

   sgeE_USERSET_LIST,                ///< send userset list at registration
   sgeE_USERSET_ADD,                 ///< event userset add
   sgeE_USERSET_DEL,                 ///< event userset delete
   sgeE_USERSET_MOD,                 ///< event userset modify

   sgeE_HGROUP_LIST,                 ///< send host group list at registration
   sgeE_HGROUP_ADD,                  ///< event add host group
   sgeE_HGROUP_DEL,                  ///< event delete host group
   sgeE_HGROUP_MOD,                  ///< event modify host group

   sgeE_RQS_LIST,                    ///< send resource quota set list at registration
   sgeE_RQS_ADD,                     ///< event add resource quota set
   sgeE_RQS_DEL,                     ///< event delete resource quota set
   sgeE_RQS_MOD,                     ///< event modify resource quota set

   sgeE_AR_LIST,                     ///< send advance reservation list at registration
   sgeE_AR_ADD,                      ///< event add advance reservation
   sgeE_AR_DEL,                      ///< event delete advance reservation
   sgeE_AR_MOD,                      ///< event modify advance reservation

   sgeE_ACK_TIMEOUT,                 ///< an event client did not acknowledge in time

   sgeE_CATEGORY_LIST,               ///< events for job categories
   sgeE_CATEGORY_ADD,                ///< event add job category
   sgeE_CATEGORY_DEL,                ///< event delete job category
   sgeE_CATEGORY_MOD,                ///< event modify job category

   sgeE_RL_LIST,                    ///< send role list at registration
   sgeE_RL_ADD,                     ///< event role add
   sgeE_RL_DEL,                     ///< event role delete
   sgeE_RL_MOD,                     ///< event role modify

   sgeE_EVENTSIZE                    ///< not an event; the number of values above
} ev_event;

/**
 * @brief Acknowledges delivered events, part of the event master
 *
 * The first parameter is the event client id, the second the number of the
 * last event the client processed.
 *
 * @return true when the acknowledgement was accepted
 *
 * @see `sge_handle_event_ack`
 */
typedef bool (*evm_ack_func_t)(
   uint32_t,         /* the event client id */
   uint32_t          /* the last event to ack */
);

/// Is this one of the events that carries a whole list rather than one change?
#define IS_TOTAL_UPDATE_EVENT(x) \
  (((x)==sgeE_CALENDAR_LIST) || \
  ((x)==sgeE_CKPT_LIST) || \
  ((x)==sgeE_CENTRY_LIST) || \
  ((x)==sgeE_CONFIG_LIST) || \
  ((x)==sgeE_EXECHOST_LIST) || \
  ((x)==sgeE_CATEGORY_LIST) || \
  ((x)==sgeE_JOB_LIST) || \
  ((x)==sgeE_JOB_SCHEDD_INFO_LIST) || \
  ((x)==sgeE_PE_LIST) || \
  ((x)==sgeE_PROJECT_LIST) || \
  ((x)==sgeE_CQUEUE_LIST) || \
  ((x)==sgeE_USER_LIST) || \
  ((x)==sgeE_USERSET_LIST) || \
  ((x)==sgeE_HGROUP_LIST) || \
  ((x)==sgeE_RL_LIST) || \
  ((x)==sgeE_SHUTDOWN) || \
  ((x)==sgeE_QMASTER_GOES_DOWN) || \
  ((x)==sgeE_ACK_TIMEOUT))

const char *event_text(const lListElem *event, dstring *buffer);

bool event_client_verify(const lListElem *event_client, lList **answer_list, bool add);
