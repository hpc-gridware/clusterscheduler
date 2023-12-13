#ifndef SGE_PET_L_H
#define SGE_PET_L_H
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
*    SGE_STRING(PET_id) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PET_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PET_status) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PET_granted_destin_identifier_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PET_pid) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PET_osjobid) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PET_usage) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PET_scaled_usage) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PET_reported_usage) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PET_previous_usage) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PET_submission_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PET_start_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PET_end_time) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PET_cwd) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PET_path_aliases) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PET_environment) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(PET_do_contact) - @todo add summary
*    @todo add description
*
*/

enum {
   PET_id = PET_LOWERBOUND,
   PET_name,
   PET_status,
   PET_granted_destin_identifier_list,
   PET_pid,
   PET_osjobid,
   PET_usage,
   PET_scaled_usage,
   PET_reported_usage,
   PET_previous_usage,
   PET_submission_time,
   PET_start_time,
   PET_end_time,
   PET_cwd,
   PET_path_aliases,
   PET_environment,
   PET_do_contact
};

LISTDEF(PET_Type)
   SGE_STRING(PET_id, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_STRING(PET_name, CULL_SUBLIST)
   SGE_ULONG(PET_status, CULL_SUBLIST)
   SGE_LIST(PET_granted_destin_identifier_list, JG_Type, CULL_SUBLIST)
   SGE_ULONG(PET_pid, CULL_DEFAULT)
   SGE_STRING(PET_osjobid, CULL_DEFAULT)
   SGE_LIST(PET_usage, UA_Type, CULL_SUBLIST)
   SGE_LIST(PET_scaled_usage, UA_Type, CULL_SUBLIST)
   SGE_LIST(PET_reported_usage, UA_Type, CULL_SUBLIST)
   SGE_LIST(PET_previous_usage, UA_Type, CULL_DEFAULT)
   SGE_ULONG(PET_submission_time, CULL_SUBLIST)
   SGE_ULONG(PET_start_time, CULL_SUBLIST)
   SGE_ULONG(PET_end_time, CULL_SUBLIST)
   SGE_STRING(PET_cwd, CULL_SUBLIST)
   SGE_LIST(PET_path_aliases, PA_Type, CULL_DEFAULT)
   SGE_LIST(PET_environment, VA_Type, CULL_DEFAULT)
   SGE_BOOL(PET_do_contact, CULL_SUBLIST)
LISTEND

NAMEDEF(PETN)
   NAME("PET_id")
   NAME("PET_name")
   NAME("PET_status")
   NAME("PET_granted_destin_identifier_list")
   NAME("PET_pid")
   NAME("PET_osjobid")
   NAME("PET_usage")
   NAME("PET_scaled_usage")
   NAME("PET_reported_usage")
   NAME("PET_previous_usage")
   NAME("PET_submission_time")
   NAME("PET_start_time")
   NAME("PET_end_time")
   NAME("PET_cwd")
   NAME("PET_path_aliases")
   NAME("PET_environment")
   NAME("PET_do_contact")
NAMEEND

#define PET_SIZE sizeof(PETN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
