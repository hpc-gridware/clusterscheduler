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
 * This code was generated from file source/libs/sgeobj/json/CAL.json
 * DO NOT CHANGE
 */

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_STRING(CAL_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CAL_year_calendar) - @todo add summary
*    @todo add description
*
*    SGE_STRING(CAL_week_calendar) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CAL_parsed_year_calendar) - @todo add summary
*    @todo add description
*
*    SGE_LIST(CAL_parsed_week_calendar) - @todo add summary
*    @todo add description
*
*/

enum {
   CAL_name = CAL_LOWERBOUND,
   CAL_year_calendar,
   CAL_week_calendar,
   CAL_parsed_year_calendar,
   CAL_parsed_week_calendar
};

LISTDEF(CAL_Type)
   SGE_STRING(CAL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_STRING(CAL_year_calendar, CULL_SPOOL)
   SGE_STRING(CAL_week_calendar, CULL_SPOOL)
   SGE_LIST(CAL_parsed_year_calendar, CA_Type, CULL_DEFAULT)
   SGE_LIST(CAL_parsed_week_calendar, CA_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(CALN)
   NAME("CAL_name")
   NAME("CAL_year_calendar")
   NAME("CAL_week_calendar")
   NAME("CAL_parsed_year_calendar")
   NAME("CAL_parsed_week_calendar")
NAMEEND

#define CAL_SIZE sizeof(CALN)/sizeof(char *)


