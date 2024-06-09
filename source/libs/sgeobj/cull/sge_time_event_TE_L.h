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
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(TE_when) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_mode) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_interval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_uval0) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_uval1) - @todo add summary
*    @todo add description
*
*    SGE_STRING(TE_sval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TE_seqno) - @todo add summary
*    @todo add description
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
   SGE_ULONG(TE_when, CULL_DEFAULT)
   SGE_ULONG(TE_type, CULL_DEFAULT)
   SGE_ULONG(TE_mode, CULL_DEFAULT)
   SGE_ULONG(TE_interval, CULL_DEFAULT)
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


