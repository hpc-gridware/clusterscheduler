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
 * This code was generated from file source/libs/sgeobj/json/TE.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Time Event
*
* Time Event objects are used by the timed event thread in sge_qmaster.
* Time Event Objects store one time or recurring events which trigger predefined actions.
*
*    SGE_ULONG64(TE_when) - When
*    Time in µs since epoch when this event must be delivered.
*
*    SGE_ULONG(TE_type) - Type
*    Used to differ between different event categories from enum te_type_t, e.g.
*    TYPE_CALENDAR_EVENT, TYPE_SIGNAL_RESEND_EVENT, ...
*
*    SGE_ULONG(TE_mode) - Mode
*    one-time or recurring event (ONE_TIME_EVENT or RECURRING_EVENT).
*
*    SGE_ULONG64(TE_interval) - Interval
*    The event interval in µs, in case of recurring events.
*
*    SGE_ULONG(TE_uval0) - UVal0
*    1st ulong key, e.g. the job number for a job specific event like a TYPE_JOB_RESEND_EVENT.
*
*    SGE_ULONG(TE_uval1) - UVal1
*    2nd ulong key, e.g. the array task number for a job/task specific event like a TYPE_JOB_RESEND_EVENT.
*
*    SGE_STRING(TE_sval) - String Key
*    String key, e.g. for a queue instance specific event.
*
*    SGE_ULONG(TE_seqno) - Sequence Number
*    Every event is assigned an unique sequence number.
*
*/

enum {
   TE_when = TE_LOWERBOUND,
   TE_type,
   TE_mode,
   TE_interval,
   TE_uval0,
   TE_uval1,
   TE_sval,
   TE_seqno
};

LISTDEF(TE_Type)
   SGE_ULONG64(TE_when, CULL_DEFAULT)
   SGE_ULONG(TE_type, CULL_DEFAULT)
   SGE_ULONG(TE_mode, CULL_DEFAULT)
   SGE_ULONG64(TE_interval, CULL_DEFAULT)
   SGE_ULONG(TE_uval0, CULL_DEFAULT)
   SGE_ULONG(TE_uval1, CULL_DEFAULT)
   SGE_STRING(TE_sval, CULL_DEFAULT)
   SGE_ULONG(TE_seqno, CULL_DEFAULT)
LISTEND

NAMEDEF(TEN)
   NAME("TE_when")
   NAME("TE_type")
   NAME("TE_mode")
   NAME("TE_interval")
   NAME("TE_uval0")
   NAME("TE_uval1")
   NAME("TE_sval")
   NAME("TE_seqno")
NAMEEND

#define TE_SIZE sizeof(TEN)/sizeof(char *)


