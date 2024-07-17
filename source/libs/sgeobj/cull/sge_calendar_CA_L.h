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
 * This code was generated from file source/libs/sgeobj/json/CA.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief Parsed Calendar
*
* This data structure is used for storing a parsed calendar.
*
*    SGE_LIST(CA_yday_range_list) - YearDay Range List
*    TMR_Type with begin/end using TM_mday/TM_mon/TM_year.
*
*    SGE_LIST(CA_wday_range_list) - WeekDay Range List
*    TMR_Type with begin/end using TM_wday.
*
*    SGE_LIST(CA_daytime_range_list) - DayTime Range List
*    TMR_Type with begin/end using TM_sec/TM_min/TM_hour.
*
*    SGE_ULONG(CA_state) - State
*    Queue state in the given time range, e.g. QI_DO_NOTHING, QI_DO_DISABLE, QI_DO_SUSPEND, ...
*
*/

enum {
   CA_yday_range_list = CA_LOWERBOUND,
   CA_wday_range_list,
   CA_daytime_range_list,
   CA_state
};

LISTDEF(CA_Type)
   SGE_LIST(CA_yday_range_list, TMR_Type, CULL_DEFAULT)
   SGE_LIST(CA_wday_range_list, TMR_Type, CULL_DEFAULT)
   SGE_LIST(CA_daytime_range_list, TMR_Type, CULL_DEFAULT)
   SGE_ULONG(CA_state, CULL_DEFAULT)
LISTEND

NAMEDEF(CAN)
   NAME("CA_yday_range_list")
   NAME("CA_wday_range_list")
   NAME("CA_daytime_range_list")
   NAME("CA_state")
NAMEEND

#define CA_SIZE sizeof(CAN)/sizeof(char *)


