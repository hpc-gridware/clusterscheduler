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
 * This code was generated from file source/libs/sgeobj/json/LDR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_LIST(LDR_queue_ref_list) - @todo add summary
*    @todo add description
*
*    SGE_STRING(LDR_limit) - @todo add summary
*    @todo add description
*
*    SGE_REF(LDR_global) - @todo add summary
*    @todo add description
*
*    SGE_REF(LDR_host) - @todo add summary
*    @todo add description
*
*    SGE_REF(LDR_queue) - @todo add summary
*    @todo add description
*
*/

enum {
   LDR_queue_ref_list = LDR_LOWERBOUND,
   LDR_limit,
   LDR_global,
   LDR_host,
   LDR_queue
};

LISTDEF(LDR_Type)
   SGE_LIST(LDR_queue_ref_list, QR_Type, CULL_DEFAULT)
   SGE_STRING(LDR_limit, CULL_DEFAULT)
   SGE_REF(LDR_global, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(LDR_host, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_REF(LDR_queue, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(LDRN)
   NAME("LDR_queue_ref_list")
   NAME("LDR_limit")
   NAME("LDR_global")
   NAME("LDR_host")
   NAME("LDR_queue")
NAMEEND

#define LDR_SIZE sizeof(LDRN)/sizeof(char *)


