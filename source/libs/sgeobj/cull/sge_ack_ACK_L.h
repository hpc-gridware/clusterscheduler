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
 * @brief Acknowledgement
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Acknowledgement
*
* A short acknowledgement message, identifying what is being acknowledged rather than carrying data.
*
*    SGE_ULONG(ACK_type) - Acknowledgement Type
*    What is being acknowledged, e.g. a job report or a signal. Dispatched on by the receiver.
*
*    SGE_ULONG(ACK_id) - First Id
*    Primary key of the acknowledged thing, usually a job id.
*
*    SGE_ULONG(ACK_id2) - Second Id
*    Secondary key, usually an array task id.
*
*    SGE_STRING(ACK_str) - Name
*    String key, used where the acknowledged thing is named rather than numbered.
*
*/

enum {
   ACK_type = ACK_LOWERBOUND,   ///< Acknowledgement Type
   ACK_id,   ///< First Id
   ACK_id2,   ///< Second Id
   ACK_str   ///< Name
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


