#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/ACK.json
 * DO NOT CHANGE
 */

/** @file
 * @brief @todo add summary
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(ACK_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(ACK_id) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(ACK_id2) - @todo add summary
*    @todo add description
*
*    SGE_STRING(ACK_str) - @todo add summary
*    @todo add description
*
*/

enum {
   ACK_type = ACK_LOWERBOUND,   ///< @todo add summary
   ACK_id,   ///< @todo add summary
   ACK_id2,   ///< @todo add summary
   ACK_str   ///< @todo add summary
};

LISTDEF(ACK_Type)
   SGE_ULONG(ACK_type, CULL_DEFAULT)
   SGE_ULONG(ACK_id, CULL_DEFAULT)
   SGE_ULONG(ACK_id2, CULL_DEFAULT)
   SGE_STRING(ACK_str, CULL_DEFAULT)
LISTEND

NAMEDEF(ACKN)
   NAME("ACK_type")
   NAME("ACK_id")
   NAME("ACK_id2")
   NAME("ACK_str")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define ACK_SIZE sizeof(ACKN)/sizeof(char *)


