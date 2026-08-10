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
 * This code was generated from file source/libs/sgeobj/json/RQRF.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Resource Quota Rule Filter
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource Quota Rule Filter
*
* One of a rule's five filters: the set of names it matches, and whether the limit is shared or per name.
*
*    SGE_BOOL(RQRF_expand) - Expand
*    False means one limit shared by everything the filter matches; true means a separate limit for each matched name. This is what distinguishes "100 slots between all users" from "100 slots each".
*
*    SGE_LIST(RQRF_scope) - Scope
*    The names the filter matches (`ST_Type`). A single `*` entry means everything.
*
*    SGE_LIST(RQRF_xscope) - Excluded Scope
*    Names explicitly excluded, applied after RQRF_scope.
*
*/

enum {
   RQRF_expand = RQRF_LOWERBOUND,   ///< Expand
   RQRF_scope,   ///< Scope
   RQRF_xscope   ///< Excluded Scope
};

LISTDEF(RQRF_Type)
   SGE_BOOL(RQRF_expand, CULL_SPOOL)
   SGE_LIST(RQRF_scope, ST_Type, CULL_SPOOL)
   SGE_LIST(RQRF_xscope, ST_Type, CULL_SPOOL)
LISTEND

NAMEDEF(RQRFN)
   NAME("RQRF_expand")
   NAME("RQRF_scope")
   NAME("RQRF_xscope")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define RQRF_SIZE sizeof(RQRFN)/sizeof(char *)


