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
 * This code was generated from file source/libs/sgeobj/json/FES.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Feature Set
*
* Used to store the information if specific optional product features are active.
* @note None of the possible features is currently part of a Cluster Scheduler build.
*
*    SGE_ULONG(FES_id) - Id
*    Featureset Id, see enum feature_id_t, one of
*    FEATURE_NO_SECURITY
*    FEATURE_AFS_SECURITY
*    FEATURE_DCE_SECURITY
*    FEATURE_KERBEROS_SECURITY
*    FEATURE_CSP_SECURITY
*
*    SGE_ULONG(FES_active) - Active
*    Is the feature active (1) or not (0).
*    @todo should better be a boolean.
*
*/

enum {
   FES_id = FES_LOWERBOUND,
   FES_active
};

LISTDEF(FES_Type)
   SGE_ULONG(FES_id, CULL_DEFAULT)
   SGE_ULONG(FES_active, CULL_DEFAULT)
LISTEND

NAMEDEF(FESN)
   NAME("FES_id")
   NAME("FES_active")
NAMEEND

#define FES_SIZE sizeof(FESN)/sizeof(char *)


