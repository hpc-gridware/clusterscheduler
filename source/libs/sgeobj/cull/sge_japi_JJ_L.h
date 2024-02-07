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
*    SGE_ULONG(JJ_jobid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JJ_type) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JJ_finished_tasks) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JJ_not_yet_finished_ids) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JJ_started_task_ids) - @todo add summary
*    @todo add description
*
*/

enum {
   JJ_jobid = JJ_LOWERBOUND,
   JJ_type,
   JJ_finished_tasks,
   JJ_not_yet_finished_ids,
   JJ_started_task_ids
};

LISTDEF(JJ_Type)
   SGE_ULONG(JJ_jobid, CULL_UNIQUE | CULL_HASH)
   SGE_ULONG(JJ_type, CULL_DEFAULT)
   SGE_LIST(JJ_finished_tasks, JJAT_Type, CULL_DEFAULT)
   SGE_LIST(JJ_not_yet_finished_ids, RN_Type, CULL_DEFAULT)
   SGE_LIST(JJ_started_task_ids, RN_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(JJN)
   NAME("JJ_jobid")
   NAME("JJ_type")
   NAME("JJ_finished_tasks")
   NAME("JJ_not_yet_finished_ids")
   NAME("JJ_started_task_ids")
NAMEEND

#define JJ_SIZE sizeof(JJN)/sizeof(char *)


