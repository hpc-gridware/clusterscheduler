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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Event Subscription
*
* An object of the EventSubscription type specifies if a certain event is subscribed
* and additional information for flushing and filtering
* see also the documentation in source/libs/evc/sge_event_client.cc
*
*    SGE_ULONG(EVS_id) - Event Id
*    The Id of the specific event from enumeration type ev_event (libs/sgeobj/sge_event.h), e.g.
*      - sgeE_ADMIN_HOST_LIST
*      - sgeE_ADMIN_HOST_ADD
*      - sgeE_ADMIN_HOST_DEL
*      - sgeE_ADMIN_HOST_MOD
*      - sgeE_CALENDAR_LIST
*      - ...
*
*    SGE_BOOL(EVS_flush) - Flush this Event
*    Specifies if flushing of the event is enabled. This means that flushing of events is triggered
*    if this event will be delivered to a specific event client.
*    Flushing of event data means that the event are sent to the client earlier than forseen
*    by the event client's event delivery interval.
*
*    SGE_ULONG(EVS_interval) - Flushing Interval
*    Flushing interval in seconds.
*    Events will be sent to the event client not later than current time + interval.
*
*    SGE_OBJECT(EVS_what) - Attribute Filter
*    Enumeration defining which attributes of an object will be sent to the event client (reduced objects).
*    We can for example configure: We are only interested in job id, job name and job owner.
*
*    SGE_OBJECT(EVS_where) - Object Filter
*    Condition filtering objects to be sent.
*    We can for example configure: Send only events for jobs of user xyz.
*
*/

enum {
   EVS_id = EVS_LOWERBOUND,
   EVS_flush,
   EVS_interval,
   EVS_what,
   EVS_where
};

LISTDEF(EVS_Type)
   SGE_ULONG(EVS_id, CULL_DEFAULT)
   SGE_BOOL(EVS_flush, CULL_DEFAULT)
   SGE_ULONG(EVS_interval, CULL_DEFAULT)
   SGE_OBJECT(EVS_what, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_OBJECT(EVS_where, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(EVSN)
   NAME("EVS_id")
   NAME("EVS_flush")
   NAME("EVS_interval")
   NAME("EVS_what")
   NAME("EVS_where")
NAMEEND

#define EVS_SIZE sizeof(EVSN)/sizeof(char *)


