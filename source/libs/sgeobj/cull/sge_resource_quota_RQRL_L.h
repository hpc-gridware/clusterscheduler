#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/RQRL.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(RQRL_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(RQRL_value) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(RQRL_type) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(RQRL_dvalue) - @todo add summary
*    @todo add description
*
*    SGE_LIST(RQRL_usage) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(RQRL_dynamic) - @todo add summary
*    @todo add description
*
*/

enum {
   RQRL_name = RQRL_LOWERBOUND,
   RQRL_value,
   RQRL_type,
   RQRL_dvalue,
   RQRL_usage,
   RQRL_dynamic
};

LISTDEF(RQRL_Type)
   SGE_STRING(RQRL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_STRING(RQRL_value, CULL_SPOOL)
   SGE_ULONG(RQRL_type, CULL_SPOOL)
   SGE_DOUBLE(RQRL_dvalue, CULL_SPOOL)
   SGE_LIST(RQRL_usage, RUE_Type, CULL_DEFAULT)
   SGE_BOOL(RQRL_dynamic, CULL_DEFAULT)
LISTEND

NAMEDEF(RQRLN)
   NAME("RQRL_name")
   NAME("RQRL_value")
   NAME("RQRL_type")
   NAME("RQRL_dvalue")
   NAME("RQRL_usage")
   NAME("RQRL_dynamic")
NAMEEND

#define RQRL_SIZE sizeof(RQRLN)/sizeof(char *)


