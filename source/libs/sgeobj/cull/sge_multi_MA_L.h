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
 * This code was generated from file source/libs/sgeobj/json/MA.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Multi Gdi Id
*
* Multiple requests can be sent by a client to sge_qmaster in a single message.
* Every such request gets a unique number.
* A list of MA_Type is used to return the answers to a multi GDI request.
*
*    SGE_ULONG(MA_id) - Id
*    Id of the individual request to associate request with result.
*
*    SGE_LIST(MA_objects) - Objects
*    A list of the requested type containing the result of the request.
*
*    SGE_LIST(MA_answers) - Answers
*    A list of answers. This can be error, warning or info messages from processing the request.
*
*/

enum {
   MA_id = MA_LOWERBOUND,
   MA_objects,
   MA_answers
};

LISTDEF(MA_Type)
   SGE_ULONG(MA_id, CULL_DEFAULT)
   SGE_LIST(MA_objects, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_LIST(MA_answers, AN_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(MAN)
   NAME("MA_id")
   NAME("MA_objects")
   NAME("MA_answers")
NAMEEND

#define MA_SIZE sizeof(MAN)/sizeof(char *)


