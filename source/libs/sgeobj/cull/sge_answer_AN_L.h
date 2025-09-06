#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * This code was generated from file source/libs/sgeobj/json/AN.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief answer element
*
* element holding information for an answer to a request
*
*    SGE_ULONG(AN_status) - answer status
*    status of an answer: a high level error code, e.g.
*     - STATUS_OK
*     - STATUS_ESEMANTIC
*     - STATUS_EEXIST
*     - STATUS_EUNKNOWN
*     - ...
*
*    SGE_STRING(AN_text) - answer text
*    printable answer text, e.g. an error message
*
*    SGE_ULONG(AN_quality) - answer quality
*    answer quality (level, severity):
*     - ANSWER_QUALITY_CRITICAL
*     - ANSWER_QUALITY_ERROR
*     - ANSWER_QUALITY_WARNING
*     - ANSWER_QUALITY_INFO
*
*/

enum {
   AN_status = AN_LOWERBOUND,
   AN_text,
   AN_quality
};

LISTDEF(AN_Type)
   SGE_ULONG(AN_status, CULL_DEFAULT)
   SGE_STRING(AN_text, CULL_DEFAULT)
   SGE_ULONG(AN_quality, CULL_DEFAULT)
LISTEND

NAMEDEF(ANN)
   NAME("AN_status")
   NAME("AN_text")
   NAME("AN_quality")
NAMEEND

#define AN_SIZE sizeof(ANN)/sizeof(char *)


