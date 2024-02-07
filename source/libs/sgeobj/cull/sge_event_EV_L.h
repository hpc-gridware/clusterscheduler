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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Event Client
*
* An object of the event client type represents one event client.
* For more information about the event client interface see the documentation in
* source/libs/evc/sge_event_client.cc
*
*    SGE_ULONG(EV_id) - event client id
*    Unique id requested by client or given by qmaster
*
*    SGE_STRING(EV_name) - event client name
*    name of event client (non unique)
*
*    SGE_HOST(EV_host) - event client address: host name
*    Commlib address: The host name where the event client is running
*
*    SGE_STRING(EV_commproc) - event client address: commproc
*    Commlib address: The process name, e.g. qsub for the qsub -sync y event client
*
*    SGE_ULONG(EV_commid) - event client address: commid
*    Commlib address: A unique id assigned by the sge_qmaster commlib
*
*    SGE_ULONG(EV_uid) - user id
*    The user id of the user who started the event client.
*
*    SGE_ULONG(EV_d_time) - event delivery interval
*    The time interval in seconds in which an event package is delivered to the client.
*
*    SGE_ULONG(EV_flush_delay) - flush delay
*    @todo is it actually used? Used for throttling of the event flushing mechanism (?)
*
*    SGE_LIST(EV_subscribed) - subscribed events
*    a list of subscribed events
*
*    SGE_BOOL(EV_changed) - event client changed?
*    true, if any configuration information of the event client has been changed.
*    Requires then updating the event client information in the event master
*
*    SGE_ULONG(EV_busy_handling) - busy handling
*    Defines how the event master shall deal with busy event clients.
*
*    SGE_STRING(EV_session) - session key
*    Session key used tfor filtering subscribed events, used with job submission via the DRMAA interface
*
*    SGE_ULONG(EV_last_heard_from) - last heard from
*    Timestamp (seconds since epoch) of the last communication between event client and event master.
*
*    SGE_ULONG(EV_last_send_time) - last send time
*    Timestamp (seconds since epoch) when the last event package was sent to the event client.
*
*    SGE_ULONG(EV_next_send_time) - next send time
*    Timestamp (seconds since epoch) when the next event package shall be sent to the event client.
*
*    SGE_ULONG(EV_next_number) - next event serial number
*    Serial number of the next event which will be sent to the event client.
*
*    SGE_ULONG(EV_busy) - busy
*    true if the event client is considered busy, else false.
*    no events will be sent to a busy client
*
*    SGE_LIST(EV_events) - events to be sent
*    List of events which will next be delivered to the event client.
*
*    SGE_REF(EV_sub_array) - subscription array
*    Subscription information used in event master only.
*
*    SGE_ULONG(EV_state) - event client state
*    State of the event client, e.g. connected, closing, terminated.
*
*    SGE_REF(EV_update_function) - update function
*    Pointer to an update function used for updating internal event clients (threads in sge_qmaster).
*
*/

enum {
   EV_id = EV_LOWERBOUND,
   EV_name,
   EV_host,
   EV_commproc,
   EV_commid,
   EV_uid,
   EV_d_time,
   EV_flush_delay,
   EV_subscribed,
   EV_changed,
   EV_busy_handling,
   EV_session,
   EV_last_heard_from,
   EV_last_send_time,
   EV_next_send_time,
   EV_next_number,
   EV_busy,
   EV_events,
   EV_sub_array,
   EV_state,
   EV_update_function
};

LISTDEF(EV_Type)
   SGE_ULONG(EV_id, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH)
   SGE_STRING(EV_name, CULL_DEFAULT)
   SGE_HOST(EV_host, CULL_DEFAULT)
   SGE_STRING(EV_commproc, CULL_DEFAULT)
   SGE_ULONG(EV_commid, CULL_DEFAULT)
   SGE_ULONG(EV_uid, CULL_DEFAULT)
   SGE_ULONG(EV_d_time, CULL_DEFAULT)
   SGE_ULONG(EV_flush_delay, CULL_DEFAULT)
   SGE_LIST(EV_subscribed, EVS_Type, CULL_DEFAULT)
   SGE_BOOL(EV_changed, CULL_DEFAULT)
   SGE_ULONG(EV_busy_handling, CULL_DEFAULT)
   SGE_STRING(EV_session, CULL_DEFAULT)
   SGE_ULONG(EV_last_heard_from, CULL_DEFAULT)
   SGE_ULONG(EV_last_send_time, CULL_DEFAULT)
   SGE_ULONG(EV_next_send_time, CULL_DEFAULT)
   SGE_ULONG(EV_next_number, CULL_DEFAULT)
   SGE_ULONG(EV_busy, CULL_DEFAULT)
   SGE_LIST(EV_events, ET_Type, CULL_DEFAULT)
   SGE_REF(EV_sub_array, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(EV_state, CULL_DEFAULT)
   SGE_REF(EV_update_function, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(EVN)
   NAME("EV_id")
   NAME("EV_name")
   NAME("EV_host")
   NAME("EV_commproc")
   NAME("EV_commid")
   NAME("EV_uid")
   NAME("EV_d_time")
   NAME("EV_flush_delay")
   NAME("EV_subscribed")
   NAME("EV_changed")
   NAME("EV_busy_handling")
   NAME("EV_session")
   NAME("EV_last_heard_from")
   NAME("EV_last_send_time")
   NAME("EV_next_send_time")
   NAME("EV_next_number")
   NAME("EV_busy")
   NAME("EV_events")
   NAME("EV_sub_array")
   NAME("EV_state")
   NAME("EV_update_function")
NAMEEND

#define EV_SIZE sizeof(EVN)/sizeof(char *)


