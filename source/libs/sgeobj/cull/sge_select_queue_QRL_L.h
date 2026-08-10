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
 * This code was generated from file source/libs/sgeobj/json/QRL.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Queue Reference
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Queue Reference
*
* A bare reference to a queue instance, for building the scheduler's temporary candidate lists.
*
*    SGE_REF(QRL_queue) - Queue Instance
*    Reference to the queue instance (`QU_Type`). Not owned by this element.
*
*/

enum {
   QRL_queue = QRL_LOWERBOUND   ///< Queue Instance
};

LISTDEF(QRL_Type)
   SGE_REF(QRL_queue, CULL_ANY_SUBTYPE, CULL_DEFAULT)
LISTEND

NAMEDEF(QRLN)
   NAME("QRL_queue")
NAMEEND

/** @brief Number of attributes of the type, i.e. the size of its name table */
#define QRL_SIZE sizeof(QRLN)/sizeof(char *)


