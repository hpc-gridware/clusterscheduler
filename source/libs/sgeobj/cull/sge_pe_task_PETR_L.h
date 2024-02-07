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
*    SGE_ULONG(PETR_jobid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PETR_jataskid) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PETR_queuename) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PETR_owner) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PETR_cwd) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PETR_path_aliases) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PETR_environment) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PETR_submission_time) - @todo add summary
*    @todo add description
*
*/

enum {
   PETR_jobid = PETR_LOWERBOUND,
   PETR_jataskid,
   PETR_queuename,
   PETR_owner,
   PETR_cwd,
   PETR_path_aliases,
   PETR_environment,
   PETR_submission_time
};

LISTDEF(PETR_Type)
   SGE_ULONG(PETR_jobid, CULL_DEFAULT)
   SGE_ULONG(PETR_jataskid, CULL_DEFAULT)
   SGE_STRING(PETR_queuename, CULL_DEFAULT)
   SGE_STRING(PETR_owner, CULL_DEFAULT)
   SGE_STRING(PETR_cwd, CULL_DEFAULT)
   SGE_LIST(PETR_path_aliases, PA_Type, CULL_DEFAULT)
   SGE_LIST(PETR_environment, VA_Type, CULL_DEFAULT)
   SGE_ULONG(PETR_submission_time, CULL_DEFAULT)
LISTEND

NAMEDEF(PETRN)
   NAME("PETR_jobid")
   NAME("PETR_jataskid")
   NAME("PETR_queuename")
   NAME("PETR_owner")
   NAME("PETR_cwd")
   NAME("PETR_path_aliases")
   NAME("PETR_environment")
   NAME("PETR_submission_time")
NAMEEND

#define PETR_SIZE sizeof(PETRN)/sizeof(char *)


