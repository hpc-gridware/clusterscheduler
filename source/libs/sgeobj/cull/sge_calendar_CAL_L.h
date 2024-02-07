#pragma once
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

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


