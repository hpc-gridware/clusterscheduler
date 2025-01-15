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
 * This code was generated from file source/libs/sgeobj/json/FPET.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Finished PE Task
*
* We need to store information about finished pe tasks to avoid
* duplicate accounting records (see IZ 438).
* A ja task will contain a list of finished pe tasks.
* Only the task id of finished tasks will be stored.
*
*    SGE_STRING(FPET_id) - Finished PE Task Id
*    PE Task Id identifying a finished PE Task.
*
*/

enum {
   FPET_id = FPET_LOWERBOUND
};

LISTDEF(FPET_Type)
   SGE_STRING(FPET_id, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
LISTEND

NAMEDEF(FPETN)
   NAME("FPET_id")
NAMEEND

#define FPET_SIZE sizeof(FPETN)/sizeof(char *)


