#ifndef SGE_UU_L_H
#define SGE_UU_L_H
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
*    SGE_STRING(UU_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(UU_oticket) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(UU_fshare) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(UU_delete_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(UU_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(UU_pending_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_LIST(UU_usage) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(UU_usage_time_stamp) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(UU_usage_seqno) - @todo add summary
*    @todo add description
*
*    SGE_LIST(UU_long_term_usage) - @todo add summary
*    @todo add description
*
*    SGE_LIST(UU_project) - @todo add summary
*    @todo add description
*
*    SGE_LIST(UU_debited_job_usage) - @todo add summary
*    @todo add description
*
*    SGE_STRING(UU_default_project) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(UU_version) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(UU_consider_with_categories) - @todo add summary
*    @todo add description
*
*/

enum {
   UU_name = UU_LOWERBOUND,
   UU_oticket,
   UU_fshare,
   UU_delete_time,
   UU_job_cnt,
   UU_pending_job_cnt,
   UU_usage,
   UU_usage_time_stamp,
   UU_usage_seqno,
   UU_long_term_usage,
   UU_project,
   UU_debited_job_usage,
   UU_default_project,
   UU_version,
   UU_consider_with_categories
};

LISTDEF(UU_Type)
   SGE_STRING(UU_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_ULONG(UU_oticket, CULL_SPOOL)
   SGE_ULONG(UU_fshare, CULL_SPOOL)
   SGE_ULONG(UU_delete_time, CULL_SPOOL)
   SGE_ULONG(UU_job_cnt, CULL_DEFAULT)
   SGE_ULONG(UU_pending_job_cnt, CULL_DEFAULT)
   SGE_LIST(UU_usage, UA_Type, CULL_SPOOL)
   SGE_ULONG(UU_usage_time_stamp, CULL_SPOOL)
   SGE_ULONG(UU_usage_seqno, CULL_DEFAULT)
   SGE_LIST(UU_long_term_usage, UA_Type, CULL_SPOOL)
   SGE_LIST(UU_project, UPP_Type, CULL_SPOOL)
   SGE_LIST(UU_debited_job_usage, UPU_Type, CULL_SPOOL)
   SGE_STRING(UU_default_project, CULL_SPOOL)
   SGE_ULONG(UU_version, CULL_DEFAULT)
   SGE_BOOL(UU_consider_with_categories, CULL_DEFAULT)
LISTEND

NAMEDEF(UUN)
   NAME("UU_name")
   NAME("UU_oticket")
   NAME("UU_fshare")
   NAME("UU_delete_time")
   NAME("UU_job_cnt")
   NAME("UU_pending_job_cnt")
   NAME("UU_usage")
   NAME("UU_usage_time_stamp")
   NAME("UU_usage_seqno")
   NAME("UU_long_term_usage")
   NAME("UU_project")
   NAME("UU_debited_job_usage")
   NAME("UU_default_project")
   NAME("UU_version")
   NAME("UU_consider_with_categories")
NAMEEND

#define UU_SIZE sizeof(UUN)/sizeof(char *)


#endif
