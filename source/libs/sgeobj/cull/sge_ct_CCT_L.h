#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/CCT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(CCT_pe_name) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CCT_ignore_queues) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CCT_ignore_hosts) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CCT_job_messages) - @todo add summary
*    @todo add description
*
*    SGE_REF(CCT_pe_job_slots) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CCT_pe_job_slot_count) - @todo add summary
*    @todo add description
*
*/

enum {
   CCT_pe_name = CCT_LOWERBOUND,
   CCT_ignore_queues,
   CCT_ignore_hosts,
   CCT_job_messages,
   CCT_pe_job_slots,
   CCT_pe_job_slot_count
};

LISTDEF(CCT_Type)
   SGE_STRING(CCT_pe_name, CULL_DEFAULT)
   SGE_LIST(CCT_ignore_queues, CTI_Type, CULL_DEFAULT)
   SGE_LIST(CCT_ignore_hosts, CTI_Type, CULL_DEFAULT)
   SGE_LIST(CCT_job_messages, MES_Type, CULL_DEFAULT)
   SGE_REF(CCT_pe_job_slots, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(CCT_pe_job_slot_count, CULL_DEFAULT)
LISTEND

NAMEDEF(CCTN)
   NAME("CCT_pe_name")
   NAME("CCT_ignore_queues")
   NAME("CCT_ignore_hosts")
   NAME("CCT_job_messages")
   NAME("CCT_pe_job_slots")
   NAME("CCT_pe_job_slot_count")
NAMEEND

#define CCT_SIZE sizeof(CCTN)/sizeof(char *)


