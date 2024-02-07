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
*    SGE_ULONG(JO_OS_job_ID) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JO_OS_job_ID2) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JO_ja_task_ID) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JO_task_id_str) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JO_state) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JO_usage_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JO_pid_list) - @todo add summary
*    @todo add description
*
*/

enum {
   JO_OS_job_ID = JO_LOWERBOUND,
   JO_OS_job_ID2,
   JO_ja_task_ID,
   JO_task_id_str,
   JO_state,
   JO_usage_list,
   JO_pid_list
};

LISTDEF(JO_Type)
   SGE_ULONG(JO_OS_job_ID, CULL_DEFAULT)
   SGE_ULONG(JO_OS_job_ID2, CULL_DEFAULT)
   SGE_ULONG(JO_ja_task_ID, CULL_DEFAULT)
   SGE_STRING(JO_task_id_str, CULL_DEFAULT)
   SGE_ULONG(JO_state, CULL_DEFAULT)
   SGE_LIST(JO_usage_list, UA_Type, CULL_DEFAULT)
   SGE_LIST(JO_pid_list, JP_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(JON)
   NAME("JO_OS_job_ID")
   NAME("JO_OS_job_ID2")
   NAME("JO_ja_task_ID")
   NAME("JO_task_id_str")
   NAME("JO_state")
   NAME("JO_usage_list")
   NAME("JO_pid_list")
NAMEEND

#define JO_SIZE sizeof(JON)/sizeof(char *)


