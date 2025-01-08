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
 * This code was generated from file source/libs/sgeobj/json/KTGT.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief kerberos Client TGT
*
* This is the list type we use to hold the client TGT list for kerberos TGT forwarding.
* @todo: Probably completly outdated, remove or fix
*
*    SGE_ULONG(KTGT_id) - Id
*    
*
*    SGE_STRING(KTGT_tgt) - TGT
*    
*
*/

enum {
   KTGT_id = KTGT_LOWERBOUND,
   KTGT_tgt
};

LISTDEF(KTGT_Type)
   SGE_ULONG(KTGT_id, CULL_DEFAULT)
   SGE_STRING(KTGT_tgt, CULL_DEFAULT)
LISTEND

NAMEDEF(KTGTN)
   NAME("KTGT_id")
   NAME("KTGT_tgt")
NAMEEND

#define KTGT_SIZE sizeof(KTGTN)/sizeof(char *)


