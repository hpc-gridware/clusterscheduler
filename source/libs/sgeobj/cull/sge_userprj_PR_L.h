#ifndef SGE_PR_L_H
#define SGE_PR_L_H
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
*    SGE_STRING(PR_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PR_oticket) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PR_fshare) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PR_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PR_pending_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PR_usage) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PR_usage_time_stamp) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PR_usage_seqno) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PR_long_term_usage) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PR_project) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PR_acl) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PR_xacl) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PR_debited_job_usage) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PR_version) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(PR_consider_with_categories) - @todo add summary
*    @todo add description
*
*/

enum {
   PR_name = PR_LOWERBOUND,
   PR_oticket,
   PR_fshare,
   PR_job_cnt,
   PR_pending_job_cnt,
   PR_usage,
   PR_usage_time_stamp,
   PR_usage_seqno,
   PR_long_term_usage,
   PR_project,
   PR_acl,
   PR_xacl,
   PR_debited_job_usage,
   PR_version,
   PR_consider_with_categories
};

LISTDEF(PR_Type)
   SGE_STRING(PR_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_ULONG(PR_oticket, CULL_SPOOL)
   SGE_ULONG(PR_fshare, CULL_SPOOL)
   SGE_ULONG(PR_job_cnt, CULL_DEFAULT)
   SGE_ULONG(PR_pending_job_cnt, CULL_DEFAULT)
   SGE_LIST(PR_usage, UA_Type, CULL_SPOOL)
   SGE_ULONG(PR_usage_time_stamp, CULL_SPOOL)
   SGE_ULONG(PR_usage_seqno, CULL_DEFAULT)
   SGE_LIST(PR_long_term_usage, UA_Type, CULL_SPOOL)
   SGE_LIST(PR_project, UPP_Type, CULL_SPOOL)
   SGE_LIST(PR_acl, US_Type, CULL_SPOOL_PROJECT)
   SGE_LIST(PR_xacl, US_Type, CULL_SPOOL_PROJECT)
   SGE_LIST(PR_debited_job_usage, UPU_Type, CULL_SPOOL)
   SGE_ULONG(PR_version, CULL_DEFAULT)
   SGE_BOOL(PR_consider_with_categories, CULL_DEFAULT)
LISTEND

NAMEDEF(PRN)
   NAME("PR_name")
   NAME("PR_oticket")
   NAME("PR_fshare")
   NAME("PR_job_cnt")
   NAME("PR_pending_job_cnt")
   NAME("PR_usage")
   NAME("PR_usage_time_stamp")
   NAME("PR_usage_seqno")
   NAME("PR_long_term_usage")
   NAME("PR_project")
   NAME("PR_acl")
   NAME("PR_xacl")
   NAME("PR_debited_job_usage")
   NAME("PR_version")
   NAME("PR_consider_with_categories")
NAMEEND

#define PR_SIZE sizeof(PRN)/sizeof(char *)


#endif
