#ifndef SGE_US_L_H
#define SGE_US_L_H
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
*    SGE_STRING(US_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_fshare) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_oticket) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(US_pending_job_cnt) - @todo add summary
*    @todo add description
*
*    SGE_LIST(US_entries) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(US_consider_with_categories) - @todo add summary
*    @todo add description
*
*/

enum {
   US_name = US_LOWERBOUND,
   US_type,
   US_fshare,
   US_oticket,
   US_job_cnt,
   US_pending_job_cnt,
   US_entries,
   US_consider_with_categories
};

LISTDEF(US_Type)
   SGE_STRING(US_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL | CULL_SUBLIST)
   SGE_ULONG(US_type, CULL_SPOOL)
   SGE_ULONG(US_fshare, CULL_SPOOL)
   SGE_ULONG(US_oticket, CULL_SPOOL)
   SGE_ULONG(US_job_cnt, CULL_DEFAULT)
   SGE_ULONG(US_pending_job_cnt, CULL_DEFAULT)
   SGE_LIST(US_entries, UE_Type, CULL_SPOOL)
   SGE_BOOL(US_consider_with_categories, CULL_DEFAULT)
LISTEND

NAMEDEF(USN)
   NAME("US_name")
   NAME("US_type")
   NAME("US_fshare")
   NAME("US_oticket")
   NAME("US_job_cnt")
   NAME("US_pending_job_cnt")
   NAME("US_entries")
   NAME("US_consider_with_categories")
NAMEEND

#define US_SIZE sizeof(USN)/sizeof(char *)


#endif
