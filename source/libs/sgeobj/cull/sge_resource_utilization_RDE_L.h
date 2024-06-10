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
 * This code was generated from file source/libs/sgeobj/json/RDE.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource Diagram Entry
*
* A resource diagram lists future resource utilization (it starts at the current time).
* An object of this type is related to the usage of a specific resource,
* whose name is defined in the parent object (RUE_Type).
* The list of resource diaram entries represents a diagram showing
* resource progression over time.
* 
* E.g. the following resource diagram
*    N ^
*      |             +-------+
*      |   +-----+   |  J3   |
*      |   |     +---+-------+--+
*      |   | J1  |      J2      |
*      +---+-----+--------------+-----> t
*      0   4     10  14      22 25
* 
* is respresented by the the following table
*     t | N
*    ---+---
*     0 | 0
*     4 | 3
*    10 | 2
*    14 | 4
*    22 | 2
*    25 | 2
* 
* 
* 
*
*    SGE_ULONG64(RDE_time) - Time
*    Time stamp (microseconds since epoch).
*
*    SGE_DOUBLE(RDE_amount) - Amount
*    Amount of the resource which is used.
*
*    SGE_LIST(RDE_resource_map_list) - Resource Map List
*    Amount of Resource Maps which are used.
*
*/

enum {
   RDE_time = RDE_LOWERBOUND,
   RDE_amount,
   RDE_resource_map_list
};

LISTDEF(RDE_Type)
   SGE_ULONG64(RDE_time, CULL_DEFAULT)
   SGE_DOUBLE(RDE_amount, CULL_DEFAULT)
   SGE_LIST(RDE_resource_map_list, RESL_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(RDEN)
   NAME("RDE_time")
   NAME("RDE_amount")
   NAME("RDE_resource_map_list")
NAMEEND

#define RDE_SIZE sizeof(RDEN)/sizeof(char *)


