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
 * This code was generated from file source/libs/sgeobj/json/JC.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief User Job Count
*
* Job count per user or user group.
* this list is used by scheduler thread to keep the number
* of running jobs per user/group efficiently
*
*    SGE_STRING(JC_name) - User Name
*    User or user group name
*
*    SGE_ULONG(JC_jobs) - Number of Jobs
*    Number of running jobs of this user / this user group.
*
*/

enum {
   JC_name = JC_LOWERBOUND,
   JC_jobs
};

LISTDEF(JC_Type)
   SGE_STRING(JC_name, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(JC_jobs, CULL_DEFAULT)
LISTEND

NAMEDEF(JCN)
   NAME("JC_name")
   NAME("JC_jobs")
NAMEEND

#define JC_SIZE sizeof(JCN)/sizeof(char *)


