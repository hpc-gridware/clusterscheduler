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
 * This code was generated from file source/libs/sgeobj/json/ASTR.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_HOST(ASTR_href) - @todo add summary
*    @todo add description
*
*    SGE_STRING(ASTR_value) - @todo add summary
*    @todo add description
*
*/

enum {
   ASTR_href = ASTR_LOWERBOUND,
   ASTR_value
};

LISTDEF(ASTR_Type)
   SGE_HOST(ASTR_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(ASTR_value, CULL_SUBLIST)
LISTEND

NAMEDEF(ASTRN)
   NAME("ASTR_href")
   NAME("ASTR_value")
NAMEEND

#define ASTR_SIZE sizeof(ASTRN)/sizeof(char *)


