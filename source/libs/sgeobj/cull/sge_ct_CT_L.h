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
 * This code was generated from file source/libs/sgeobj/json/CT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(CT_str) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CT_refcount) - @todo add summary
*    @todo add description
*
*    SGE_INT(CT_count) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CT_rejected) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CT_cache) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(CT_messages_added) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(CT_resource_contribution) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(CT_rc_valid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(CT_reservation_rejected) - @todo add summary
*    @todo add description
*
*/

enum {
   CT_str = CT_LOWERBOUND,
   CT_refcount,
   CT_count,
   CT_rejected,
   CT_cache,
   CT_messages_added,
   CT_resource_contribution,
   CT_rc_valid,
   CT_reservation_rejected
};

LISTDEF(CT_Type)
   SGE_STRING(CT_str, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(CT_refcount, CULL_DEFAULT)
   SGE_INT(CT_count, CULL_DEFAULT)
   SGE_ULONG(CT_rejected, CULL_DEFAULT)
   SGE_LIST(CT_cache, CCT_Type, CULL_DEFAULT)
   SGE_BOOL(CT_messages_added, CULL_DEFAULT)
   SGE_DOUBLE(CT_resource_contribution, CULL_DEFAULT)
   SGE_BOOL(CT_rc_valid, CULL_DEFAULT)
   SGE_ULONG(CT_reservation_rejected, CULL_DEFAULT)
LISTEND

NAMEDEF(CTN)
   NAME("CT_str")
   NAME("CT_refcount")
   NAME("CT_count")
   NAME("CT_rejected")
   NAME("CT_cache")
   NAME("CT_messages_added")
   NAME("CT_resource_contribution")
   NAME("CT_rc_valid")
   NAME("CT_reservation_rejected")
NAMEEND

#define CT_SIZE sizeof(CTN)/sizeof(char *)


