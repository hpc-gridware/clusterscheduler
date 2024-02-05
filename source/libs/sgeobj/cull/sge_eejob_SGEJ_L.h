#ifndef SGE_SGEJ_L_H
#define SGE_SGEJ_L_H
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
*    SGE_DOUBLE(SGEJ_priority) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SGEJ_job_number) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SGEJ_job_name) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SGEJ_owner) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SGEJ_state) - @todo add summary
*    @todo add description
*
*    SGE_STRING(SGEJ_master_queue) - @todo add summary
*    @todo add description
*
*    SGE_REF(SGEJ_job_reference) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(SGEJ_submission_time) - @todo add summary
*    @todo add description
*
*/

enum {
   SGEJ_priority = SGEJ_LOWERBOUND,
   SGEJ_job_number,
   SGEJ_job_name,
   SGEJ_owner,
   SGEJ_state,
   SGEJ_master_queue,
   SGEJ_job_reference,
   SGEJ_submission_time
};

LISTDEF(SGEJ_Type)
   SGE_DOUBLE(SGEJ_priority, CULL_DEFAULT)
   SGE_ULONG(SGEJ_job_number, CULL_DEFAULT)
   SGE_STRING(SGEJ_job_name, CULL_DEFAULT)
   SGE_STRING(SGEJ_owner, CULL_DEFAULT)
   SGE_ULONG(SGEJ_state, CULL_DEFAULT)
   SGE_STRING(SGEJ_master_queue, CULL_DEFAULT)
   SGE_REF(SGEJ_job_reference, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(SGEJ_submission_time, CULL_DEFAULT)
LISTEND

NAMEDEF(SGEJN)
   NAME("SGEJ_priority")
   NAME("SGEJ_job_number")
   NAME("SGEJ_job_name")
   NAME("SGEJ_owner")
   NAME("SGEJ_state")
   NAME("SGEJ_master_queue")
   NAME("SGEJ_job_reference")
   NAME("SGEJ_submission_time")
NAMEEND

#define SGEJ_SIZE sizeof(SGEJN)/sizeof(char *)


#endif
