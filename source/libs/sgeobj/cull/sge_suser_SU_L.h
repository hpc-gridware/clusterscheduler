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
 * This code was generated from file source/libs/sgeobj/json/SU.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(SU_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SU_jobs) - @todo add summary
*    @todo add description
*
*/

enum {
   SU_name = SU_LOWERBOUND,
   SU_jobs
};

LISTDEF(SU_Type)
   SGE_STRING(SU_name, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(SU_jobs, CULL_DEFAULT)
LISTEND

NAMEDEF(SUN)
   NAME("SU_name")
   NAME("SU_jobs")
NAMEEND

#define SU_SIZE sizeof(SUN)/sizeof(char *)


