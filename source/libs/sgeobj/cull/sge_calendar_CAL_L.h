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
* @brief Calendar
*
* This data structure represents the SGE calendar object.
*
*    SGE_STRING(CAL_name) - Name
*    The calendar name.
*    See man page sge_calendar_conf.5 for detailed information about the attributes.
*
*    SGE_STRING(CAL_year_calendar) - Year Calendar
*    A year calendar as string, e.g. 12.03.2004=12-11=off.
*
*    SGE_STRING(CAL_week_calendar) - Week Calendar
*    A week calendar as string, e.g. mon-fri=6-20=suspended.
*
*    SGE_LIST(CAL_parsed_year_calendar) - Parsed Year Calendar
*    The year calendar parsed to internal data structures.
*
*    SGE_LIST(CAL_parsed_week_calendar) - Parsed Week Calendar
*    The week calendar parsed to internal data structures.
*
*    SGE_LIST(CAL_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*/

enum {
   CAL_name = CAL_LOWERBOUND,
   CAL_year_calendar,
   CAL_week_calendar,
   CAL_parsed_year_calendar,
   CAL_parsed_week_calendar,
   CAL_joker
};

LISTDEF(CAL_Type)
   SGE_STRING(CAL_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_STRING(CAL_year_calendar, CULL_SPOOL)
   SGE_STRING(CAL_week_calendar, CULL_SPOOL)
   SGE_LIST(CAL_parsed_year_calendar, CA_Type, CULL_DEFAULT)
   SGE_LIST(CAL_parsed_week_calendar, CA_Type, CULL_DEFAULT)
   SGE_LIST(CAL_joker, VA_Type, CULL_SPOOL)
LISTEND

NAMEDEF(CALN)
   NAME("CAL_name")
   NAME("CAL_year_calendar")
   NAME("CAL_week_calendar")
   NAME("CAL_parsed_year_calendar")
   NAME("CAL_parsed_week_calendar")
   NAME("CAL_joker")
NAMEEND

#define CAL_SIZE sizeof(CALN)/sizeof(char *)


