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
 * This code was generated from file source/libs/sgeobj/json/RUE.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Resource Utilization
*
* Utilization of a certain resource.
* Both the current utilization can be stored, as well as
* future utilization (due to currently running jobs,
* advance reservations and resource reservations).
*
*    SGE_STRING(RUE_name) - Resource Name
*    The name of the resource (= the name of the complex variable, e.g. slots)
*
*    SGE_DOUBLE(RUE_utilized_now) - Utilized Now
*    Currently used amount.
*
*    SGE_LIST(RUE_utilized_now_resource_map_list) - Utilized Now Resource Map List
*    Currently used amount of Resource Maps
*
*    SGE_LIST(RUE_utilized) - Utilized
*    A resource diagram indicating future utilization.
*
*    SGE_DOUBLE(RUE_utilized_now_nonexclusive) - Utilized Now Non-Exclusive
*    Currently used amount of implicitly used exclusive resources.
*
*    SGE_LIST(RUE_utilized_nonexclusive) - Utilized Non-Exclusive
*    A resource diagram indicating future utilization of implicitly used exclusive resources.
*
*/

enum {
   RUE_name = RUE_LOWERBOUND,
   RUE_utilized_now,
   RUE_utilized_now_resource_map_list,
   RUE_utilized,
   RUE_utilized_now_nonexclusive,
   RUE_utilized_nonexclusive
};

LISTDEF(RUE_Type)
   SGE_STRING(RUE_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_DOUBLE(RUE_utilized_now, CULL_DEFAULT)
   SGE_LIST(RUE_utilized_now_resource_map_list, RESL_Type, CULL_DEFAULT)
   SGE_LIST(RUE_utilized, RDE_Type, CULL_DEFAULT)
   SGE_DOUBLE(RUE_utilized_now_nonexclusive, CULL_DEFAULT)
   SGE_LIST(RUE_utilized_nonexclusive, RDE_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(RUEN)
   NAME("RUE_name")
   NAME("RUE_utilized_now")
   NAME("RUE_utilized_now_resource_map_list")
   NAME("RUE_utilized")
   NAME("RUE_utilized_now_nonexclusive")
   NAME("RUE_utilized_nonexclusive")
NAMEEND

#define RUE_SIZE sizeof(RUEN)/sizeof(char *)


