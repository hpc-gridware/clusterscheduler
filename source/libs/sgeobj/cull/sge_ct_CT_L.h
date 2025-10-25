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
 * This code was generated from file source/libs/sgeobj/json/CT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Category
*
* An object of this type describes a category of jobs.
*
*    SGE_ULONG(CT_id) - Category ID
*    Unique ID of a category
*
*    SGE_STRING(CT_str) - Category String
*    String holding all elements of a category (requests, user, project, ...).
*
*    SGE_ULONG(CT_refcount) - Reference Count
*    Number of jobs referencing this category.
*
*    SGE_BOOL(CT_rejected) - Rejected
*    Has this category been rejected as it can not be dispatched now?
*
*    SGE_LIST(CT_cache) - Cache
*    Stores all info, which cannot run this job category.
*
*    SGE_BOOL(CT_messages_added) - Messages Added
*    If true, the scheduler info messages have been added for this category.
*
*    SGE_DOUBLE(CT_resource_contribution) - Resource Contribution
*    Resource request dependent contribution on urgency.
*    This value is common for all jobs of a category.
*
*    SGE_BOOL(CT_rc_valid) - Resource Contribution valid
*    Indicates whether the cached CT_resource_contribution is valid.
*
*    SGE_BOOL(CT_reservation_rejected) - Reservation Rejected
*    Has this category been rejected as it can not be reserved?
*
*/

enum {
   CT_id = CT_LOWERBOUND,
   CT_str,
   CT_refcount,
   CT_rejected,
   CT_cache,
   CT_messages_added,
   CT_resource_contribution,
   CT_rc_valid,
   CT_reservation_rejected
};

LISTDEF(CT_Type)
   SGE_ULONG(CT_id, CULL_UNIQUE | CULL_HASH)
   SGE_STRING(CT_str, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(CT_refcount, CULL_DEFAULT)
   SGE_BOOL(CT_rejected, CULL_DEFAULT)
   SGE_LIST(CT_cache, CCT_Type, CULL_DEFAULT)
   SGE_BOOL(CT_messages_added, CULL_DEFAULT)
   SGE_DOUBLE(CT_resource_contribution, CULL_DEFAULT)
   SGE_BOOL(CT_rc_valid, CULL_DEFAULT)
   SGE_BOOL(CT_reservation_rejected, CULL_DEFAULT)
LISTEND

NAMEDEF(CTN)
   NAME("CT_id")
   NAME("CT_str")
   NAME("CT_refcount")
   NAME("CT_rejected")
   NAME("CT_cache")
   NAME("CT_messages_added")
   NAME("CT_resource_contribution")
   NAME("CT_rc_valid")
   NAME("CT_reservation_rejected")
NAMEEND

#define CT_SIZE sizeof(CTN)/sizeof(char *)


