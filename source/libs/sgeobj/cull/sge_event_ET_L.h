#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/ET.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Event Type
*
* An object of this type represents a single event.
* An event has a key and a value
* The key can consist of multiple integer or string fields, e.g.
* job id, array task id, pe task id
* The value represents the new version of data.
* It can be a list of objects or an individual object
* An event has a unique serial number.
*
*    SGE_ULONG64(ET_unique_id) - unique ID for an event within qmaster.
*    A unique number. IDs are always increasing for generated events. No wrap around.
*    It is used in qmaster to synchronize activities between transaction processing happening
*    in different threads. Session table contains the biggest ID of the last RW request that
*    was handled in any thread for that session.
*
*    SGE_ULONG(ET_number) - serial number
*    A unique serial number.
*    It is used to acknowledge receipt of a list of events
*    (up to a specific event represented by the serial number)
*    and also to detect inconsistencies in the event protocol, e.g. a missing event.
*
*    SGE_ULONG(ET_timestamp) - event generation time
*    Date and time (seconds since epoch) when the event was generated.
*
*    SGE_ULONG(ET_type) - event type
*    The type of the event. Values of enumeration type ev_event (libs/sgeobj/sge_event.h), e.g.
*      - sgeE_ADMIN_HOST_LIST
*      - sgeE_ADMIN_HOST_ADD
*      - sgeE_ADMIN_HOST_DEL
*      - sgeE_ADMIN_HOST_MOD
*      - sgeE_CALENDAR_LIST
*      - ...
*
*    SGE_ULONG(ET_intkey) - first integer key
*    An int key for use by a specific event type, e.g. job id in a sgeE_JOB_ADD event.
*
*    SGE_ULONG(ET_intkey2) - second integer key
*    A second int key for use by a specific event type, e.g. ja_task id in a sgeE_JATASK_MOD event.
*
*    SGE_STRING(ET_strkey) - first string key
*    A string key for use by a specific event type, e.g. the complex variable name in a sgeE_CENTRY_DEL event.
*
*    SGE_STRING(ET_strkey2) - second string key
*    A second string key for use by a specific event type, e.g.
*    in a sgeE_QINSTANCE_ADD event the first key is the cluster queue name, the second key is the host name
*
*    SGE_LIST(ET_new_version) - new version of the data
*    A list containing the new object(s). The list type depends on the event type.
*    @todo we could split this into two fields, one for lists and one for individual objects
*
*/

enum {
   ET_unique_id = ET_LOWERBOUND,
   ET_number,
   ET_timestamp,
   ET_type,
   ET_intkey,
   ET_intkey2,
   ET_strkey,
   ET_strkey2,
   ET_new_version
};

LISTDEF(ET_Type)
   SGE_ULONG64(ET_unique_id, CULL_PRIMARY_KEY)
   SGE_ULONG(ET_number, CULL_DEFAULT)
   SGE_ULONG(ET_timestamp, CULL_DEFAULT)
   SGE_ULONG(ET_type, CULL_DEFAULT)
   SGE_ULONG(ET_intkey, CULL_DEFAULT)
   SGE_ULONG(ET_intkey2, CULL_DEFAULT)
   SGE_STRING(ET_strkey, CULL_DEFAULT)
   SGE_STRING(ET_strkey2, CULL_DEFAULT)
   SGE_LIST(ET_new_version, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(ETN)
   NAME("ET_unique_id")
   NAME("ET_number")
   NAME("ET_timestamp")
   NAME("ET_type")
   NAME("ET_intkey")
   NAME("ET_intkey2")
   NAME("ET_strkey")
   NAME("ET_strkey2")
   NAME("ET_new_version")
NAMEEND

#define ET_SIZE sizeof(ETN)/sizeof(char *)


