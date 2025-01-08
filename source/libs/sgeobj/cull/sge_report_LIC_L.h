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
 * This code was generated from file source/libs/sgeobj/json/LIC.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief License Report
*
* Definition for a license report.
* There is no license management or license enforcement in Cluster Scheduler.
*
*    SGE_ULONG(LIC_processors) - Processors
*    Number of processors (nprocs = number of cores) on the execution host.
*
*    SGE_STRING(LIC_arch) - Architecture
*    The execution host's architecture, e.g. lx-amd64.
*
*/

enum {
   LIC_processors = LIC_LOWERBOUND,
   LIC_arch
};

LISTDEF(LIC_Type)
   SGE_ULONG(LIC_processors, CULL_DEFAULT)
   SGE_STRING(LIC_arch, CULL_DEFAULT)
LISTEND

NAMEDEF(LICN)
   NAME("LIC_processors")
   NAME("LIC_arch")
NAMEEND

#define LIC_SIZE sizeof(LICN)/sizeof(char *)


