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
 * This code was generated from file source/libs/sgeobj/json/JJAT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(JJAT_task_id) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JJAT_stat) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JJAT_rusage) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JJAT_failed_text) - @todo add summary
*    @todo add description
*
*/

enum {
   JJAT_task_id = JJAT_LOWERBOUND,
   JJAT_stat,
   JJAT_rusage,
   JJAT_failed_text
};

LISTDEF(JJAT_Type)
   SGE_ULONG(JJAT_task_id, CULL_DEFAULT)
   SGE_ULONG(JJAT_stat, CULL_DEFAULT)
   SGE_LIST(JJAT_rusage, UA_Type, CULL_DEFAULT)
   SGE_STRING(JJAT_failed_text, CULL_DEFAULT)
LISTEND

NAMEDEF(JJATN)
   NAME("JJAT_task_id")
   NAME("JJAT_stat")
   NAME("JJAT_rusage")
   NAME("JJAT_failed_text")
NAMEEND

#define JJAT_SIZE sizeof(JJATN)/sizeof(char *)


