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
 * This code was generated from file source/libs/sgeobj/json/UPU.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Job Usage
*
* This is the list type we use to hold the
* information for user/project. This objects are targets of throwing
* tickets to them and as usage accumulators. There are no real differences
* at the moment, so putting them together is convenient.
*
*    SGE_ULONG(UPU_job_number) - Job Number
*    The job number. Usage of individual array tasks and/or PE tasks is accumulated.
*
*    SGE_LIST(UPU_old_usage_list) - Old Usage List
*    UA_Type still debited usage set and used via orders by SGEEE ted_job_usageschedd by qmaster.
*
*/

enum {
   UPU_job_number = UPU_LOWERBOUND,
   UPU_old_usage_list
};

LISTDEF(UPU_Type)
   SGE_ULONG(UPU_job_number, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_LIST(UPU_old_usage_list, UA_Type, CULL_SUBLIST)
LISTEND

NAMEDEF(UPUN)
   NAME("UPU_job_number")
   NAME("UPU_old_usage_list")
NAMEEND

#define UPU_SIZE sizeof(UPUN)/sizeof(char *)


