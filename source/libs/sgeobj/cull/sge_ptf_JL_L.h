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
*    SGE_ULONG(JL_job_ID) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JL_OS_job_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JL_state) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JL_tickets) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_share) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_ticket_share) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_timeslice) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_usage) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_old_usage_value) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_adjusted_usage) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_last_usage) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_old_usage) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_proportion) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_adjusted_proportion) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_adjusted_current_proportion) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_actual_proportion) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_diff_proportion) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_last_proportion) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JL_curr_pri) - @todo add summary
*    @todo add description
*
*    SGE_LONG(JL_pri) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JL_procfd) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JL_interactive) - @todo add summary
*    @todo add description
*
*/

enum {
   JL_job_ID = JL_LOWERBOUND,
   JL_OS_job_list,
   JL_state,
   JL_tickets,
   JL_share,
   JL_ticket_share,
   JL_timeslice,
   JL_usage,
   JL_old_usage_value,
   JL_adjusted_usage,
   JL_last_usage,
   JL_old_usage,
   JL_proportion,
   JL_adjusted_proportion,
   JL_adjusted_current_proportion,
   JL_actual_proportion,
   JL_diff_proportion,
   JL_last_proportion,
   JL_curr_pri,
   JL_pri,
   JL_procfd,
   JL_interactive
};

LISTDEF(JL_Type)
   SGE_ULONG(JL_job_ID, CULL_DEFAULT)
   SGE_LIST(JL_OS_job_list, JO_Type, CULL_DEFAULT)
   SGE_ULONG(JL_state, CULL_DEFAULT)
   SGE_ULONG(JL_tickets, CULL_DEFAULT)
   SGE_DOUBLE(JL_share, CULL_DEFAULT)
   SGE_DOUBLE(JL_ticket_share, CULL_DEFAULT)
   SGE_DOUBLE(JL_timeslice, CULL_DEFAULT)
   SGE_DOUBLE(JL_usage, CULL_DEFAULT)
   SGE_DOUBLE(JL_old_usage_value, CULL_DEFAULT)
   SGE_DOUBLE(JL_adjusted_usage, CULL_DEFAULT)
   SGE_DOUBLE(JL_last_usage, CULL_DEFAULT)
   SGE_DOUBLE(JL_old_usage, CULL_DEFAULT)
   SGE_DOUBLE(JL_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_adjusted_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_adjusted_current_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_actual_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_diff_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_last_proportion, CULL_DEFAULT)
   SGE_DOUBLE(JL_curr_pri, CULL_DEFAULT)
   SGE_LONG(JL_pri, CULL_DEFAULT)
   SGE_ULONG(JL_procfd, CULL_DEFAULT)
   SGE_ULONG(JL_interactive, CULL_DEFAULT)
LISTEND

NAMEDEF(JLN)
   NAME("JL_job_ID")
   NAME("JL_OS_job_list")
   NAME("JL_state")
   NAME("JL_tickets")
   NAME("JL_share")
   NAME("JL_ticket_share")
   NAME("JL_timeslice")
   NAME("JL_usage")
   NAME("JL_old_usage_value")
   NAME("JL_adjusted_usage")
   NAME("JL_last_usage")
   NAME("JL_old_usage")
   NAME("JL_proportion")
   NAME("JL_adjusted_proportion")
   NAME("JL_adjusted_current_proportion")
   NAME("JL_actual_proportion")
   NAME("JL_diff_proportion")
   NAME("JL_last_proportion")
   NAME("JL_curr_pri")
   NAME("JL_pri")
   NAME("JL_procfd")
   NAME("JL_interactive")
NAMEEND

#define JL_SIZE sizeof(JLN)/sizeof(char *)


