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
 * This code was generated from file source/libs/sgeobj/json/RTIC.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_HOST(RTIC_host) - @todo add summary
*    @todo add description
*
*    SGE_LIST(RTIC_tickets) - @todo add summary
*    @todo add description
*
*/

enum {
   RTIC_host = RTIC_LOWERBOUND,
   RTIC_tickets
};

LISTDEF(RTIC_Type)
   SGE_HOST(RTIC_host, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH)
   SGE_LIST(RTIC_tickets, OR_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(RTICN)
   NAME("RTIC_host")
   NAME("RTIC_tickets")
NAMEEND

#define RTIC_SIZE sizeof(RTICN)/sizeof(char *)


