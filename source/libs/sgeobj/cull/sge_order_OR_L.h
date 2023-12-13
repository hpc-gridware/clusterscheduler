#ifndef SGE_OR_L_H
#define SGE_OR_L_H
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

#ifdef __cplusplus
extern "C" {
#endif

/**
* @brief @todo add summary
*
* @todo add description
*
*    SGE_ULONG(OR_type) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(OR_job_number) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(OR_ja_task_number) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(OR_job_version) - @todo add summary
*    @todo add description
*
*    SGE_LIST(OR_queuelist) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(OR_ticket) - @todo add summary
*    @todo add description
*
*    SGE_LIST(OR_joker) - @todo add summary
*    @todo add description
*
*    SGE_STRING(OR_pe) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(OR_ntix) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(OR_prio) - @todo add summary
*    @todo add description
*
*/

enum {
   OR_type = OR_LOWERBOUND,
   OR_job_number,
   OR_ja_task_number,
   OR_job_version,
   OR_queuelist,
   OR_ticket,
   OR_joker,
   OR_pe,
   OR_ntix,
   OR_prio
};

LISTDEF(OR_Type)
   SGE_ULONG(OR_type, CULL_DEFAULT)
   SGE_ULONG(OR_job_number, CULL_DEFAULT)
   SGE_ULONG(OR_ja_task_number, CULL_DEFAULT)
   SGE_ULONG(OR_job_version, CULL_DEFAULT)
   SGE_LIST(OR_queuelist, OQ_Type, CULL_DEFAULT)
   SGE_DOUBLE(OR_ticket, CULL_DEFAULT)
   SGE_LIST(OR_joker, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_STRING(OR_pe, CULL_DEFAULT)
   SGE_DOUBLE(OR_ntix, CULL_DEFAULT)
   SGE_DOUBLE(OR_prio, CULL_DEFAULT)
LISTEND

NAMEDEF(ORN)
   NAME("OR_type")
   NAME("OR_job_number")
   NAME("OR_ja_task_number")
   NAME("OR_job_version")
   NAME("OR_queuelist")
   NAME("OR_ticket")
   NAME("OR_joker")
   NAME("OR_pe")
   NAME("OR_ntix")
   NAME("OR_prio")
NAMEEND

#define OR_SIZE sizeof(ORN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
