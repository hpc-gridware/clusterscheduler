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
 * This code was generated from file source/libs/sgeobj/json/RQRF.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_BOOL(RQRF_expand) - @todo add summary
*    @todo add description
*
*    SGE_LIST(RQRF_scope) - @todo add summary
*    @todo add description
*
*    SGE_LIST(RQRF_xscope) - @todo add summary
*    @todo add description
*
*/

enum {
   RQRF_expand = RQRF_LOWERBOUND,
   RQRF_scope,
   RQRF_xscope
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

#define RQRF_SIZE sizeof(RQRFN)/sizeof(char *)


