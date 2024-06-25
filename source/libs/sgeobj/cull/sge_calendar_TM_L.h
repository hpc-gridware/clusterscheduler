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
 * This code was generated from file source/libs/sgeobj/json/TM.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(TM_mday) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_mon) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_year) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_sec) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_min) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_hour) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_wday) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_yday) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(TM_isdst) - @todo add summary
*    @todo add description
*
*/

enum {
   TM_mday = TM_LOWERBOUND,
   TM_mon,
   TM_year,
   TM_sec,
   TM_min,
   TM_hour,
   TM_wday,
   TM_yday,
   TM_isdst
};

LISTDEF(TM_Type)
   SGE_ULONG(TM_mday, CULL_DEFAULT)
   SGE_ULONG(TM_mon, CULL_DEFAULT)
   SGE_ULONG(TM_year, CULL_DEFAULT)
   SGE_ULONG(TM_sec, CULL_DEFAULT)
   SGE_ULONG(TM_min, CULL_DEFAULT)
   SGE_ULONG(TM_hour, CULL_DEFAULT)
   SGE_ULONG(TM_wday, CULL_DEFAULT)
   SGE_ULONG(TM_yday, CULL_DEFAULT)
   SGE_ULONG(TM_isdst, CULL_DEFAULT)
LISTEND

NAMEDEF(TMN)
   NAME("TM_mday")
   NAME("TM_mon")
   NAME("TM_year")
   NAME("TM_sec")
   NAME("TM_min")
   NAME("TM_hour")
   NAME("TM_wday")
   NAME("TM_yday")
   NAME("TM_isdst")
NAMEEND

#define TM_SIZE sizeof(TMN)/sizeof(char *)


