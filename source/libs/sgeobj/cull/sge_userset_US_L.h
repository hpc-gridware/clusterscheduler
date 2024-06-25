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
 * This code was generated from file source/libs/sgeobj/json/US.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(US_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_fshare) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_oticket) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_pending_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_LIST(US_entries) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(US_consider_with_categories) - @todo add summary
*    @todo add description
*
*/

enum {
   US_name = US_LOWERBOUND,
   US_type,
   US_fshare,
   US_oticket,
   US_job_cnt,
   US_pending_job_cnt,
   US_entries,
   US_consider_with_categories
};

LISTDEF(US_Type)
   SGE_STRING(US_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_ULONG(US_type, CULL_SPOOL)
   SGE_ULONG(US_fshare, CULL_SPOOL)
   SGE_ULONG(US_oticket, CULL_SPOOL)
   SGE_ULONG(US_job_cnt, CULL_DEFAULT)
   SGE_ULONG(US_pending_job_cnt, CULL_DEFAULT)
   SGE_LIST(US_entries, UE_Type, CULL_SPOOL)
   SGE_BOOL(US_consider_with_categories, CULL_DEFAULT)
LISTEND

NAMEDEF(USN)
   NAME("US_name")
   NAME("US_type")
   NAME("US_fshare")
   NAME("US_oticket")
   NAME("US_job_cnt")
   NAME("US_pending_job_cnt")
   NAME("US_entries")
   NAME("US_consider_with_categories")
NAMEEND

#define US_SIZE sizeof(USN)/sizeof(char *)


