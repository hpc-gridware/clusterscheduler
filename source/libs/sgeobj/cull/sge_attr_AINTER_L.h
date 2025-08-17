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
 * This code was generated from file source/libs/sgeobj/json/AINTER.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Attribute Interval
*
* Used for interval attributes in the cluster queues.
* E.g. used for the queue suspend_interval and the notify interval.
*
*    SGE_HOST(AINTER_href) - Host Reference
*    Name of a host or a host group.
*
*    SGE_STRING(AINTER_value) - Value
*    The interval value (represented as string).
*
*/

enum {
   AINTER_href = AINTER_LOWERBOUND,
   AINTER_value
};

LISTDEF(AINTER_Type)
   SGE_HOST(AINTER_href, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(AINTER_value, CULL_SUBLIST)
LISTEND

NAMEDEF(AINTERN)
   NAME("AINTER_href")
   NAME("AINTER_value")
NAMEEND

#define AINTER_SIZE sizeof(AINTERN)/sizeof(char *)


