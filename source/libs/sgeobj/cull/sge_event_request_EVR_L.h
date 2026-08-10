#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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

/*
 * This code was generated from file source/libs/sgeobj/json/EVR.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Event Master Request
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Event Master Request
*
* One request queued for the event master thread.
* Every change to the event master - a client registering, changing its subscription, leaving, or an event being added - is posted as one of these rather than applied directly, so the callers never block on the event master's lock.
*
*    SGE_ULONG(EVR_operation) - Operation
*    Which request this is: `EVR_ADD_EVC`, `EVR_MOD_EVC`, `EVR_DEL_EVC` or `EVR_ADD_EVENT`. See `sge_event.h`.
*
*    SGE_ULONG64(EVR_timestamp) - Timestamp
*    When the request was posted, so the event master can order and age requests.
*
*    SGE_ULONG(EVR_event_client_id) - Event Client Id
*    Which event client the request is about.
*
*    SGE_ULONG(EVR_event_number) - Event Number
*    The client's event serial the request refers to.
*
*    SGE_STRING(EVR_session) - Session
*    The session the request belongs to, so a client sees its own changes.
*
*    SGE_OBJECT(EVR_event_client) - Event Client
*    The event client object (`EV_Type`), for the register and modify operations.
*
*    SGE_LIST(EVR_event_list) - Events
*    The events to deliver (`ET_Type`), for `EVR_ADD_EVENT`.
*
*/

enum {
   EVR_operation = EVR_LOWERBOUND,   ///< Operation
   EVR_timestamp,   ///< Timestamp
   EVR_event_client_id,   ///< Event Client Id
   EVR_event_number,   ///< Event Number
   EVR_session,   ///< Session
   EVR_event_client,   ///< Event Client
   EVR_event_list   ///< Events
};

LISTDEF(EVR_Type)
   SGE_ULONG(EVR_operation, CULL_DEFAULT)
   SGE_ULONG64(EVR_timestamp, CULL_DEFAULT)
   SGE_ULONG(EVR_event_client_id, CULL_DEFAULT)
   SGE_ULONG(EVR_event_number, CULL_DEFAULT)
   SGE_STRING(EVR_session, CULL_DEFAULT)
   SGE_OBJECT(EVR_event_client, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_LIST(EVR_event_list, ET_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(EVRN)
   NAME("EVR_operation")
   NAME("EVR_timestamp")
   NAME("EVR_event_client_id")
   NAME("EVR_event_number")
   NAME("EVR_session")
   NAME("EVR_event_client")
   NAME("EVR_event_list")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define EVR_SIZE sizeof(EVRN)/sizeof(char *)


