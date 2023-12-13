#ifndef SGE_PE_L_H
#define SGE_PE_L_H
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
*    SGE_STRING(PE_name) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(PE_slots) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PE_user_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PE_xuser_list) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PE_start_proc_args) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PE_stop_proc_args) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PE_allocation_rule) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(PE_control_slaves) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(PE_job_is_first_task) - @todo add summary
*    @todo add description
*
*    SGE_LIST(PE_resource_utilization) - @todo add summary
*    @todo add description
*
*    SGE_STRING(PE_urgency_slots) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(PE_accounting_summary) - @todo add summary
*    @todo add description
*
*/

enum {
   PE_name = PE_LOWERBOUND,
   PE_slots,
   PE_user_list,
   PE_xuser_list,
   PE_start_proc_args,
   PE_stop_proc_args,
   PE_allocation_rule,
   PE_control_slaves,
   PE_job_is_first_task,
   PE_resource_utilization,
   PE_urgency_slots,
   PE_accounting_summary
};

LISTDEF(PE_Type)
   SGE_STRING(PE_name, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SPOOL)
   SGE_ULONG(PE_slots, CULL_SPOOL)
   SGE_LIST(PE_user_list, US_Type, CULL_SPOOL)
   SGE_LIST(PE_xuser_list, US_Type, CULL_SPOOL)
   SGE_STRING(PE_start_proc_args, CULL_SPOOL)
   SGE_STRING(PE_stop_proc_args, CULL_SPOOL)
   SGE_STRING(PE_allocation_rule, CULL_SPOOL)
   SGE_BOOL(PE_control_slaves, CULL_SPOOL)
   SGE_BOOL(PE_job_is_first_task, CULL_SPOOL)
   SGE_LIST(PE_resource_utilization, RUE_Type, CULL_DEFAULT)
   SGE_STRING(PE_urgency_slots, CULL_SPOOL)
   SGE_BOOL(PE_accounting_summary, CULL_SPOOL)
LISTEND

NAMEDEF(PEN)
   NAME("PE_name")
   NAME("PE_slots")
   NAME("PE_user_list")
   NAME("PE_xuser_list")
   NAME("PE_start_proc_args")
   NAME("PE_stop_proc_args")
   NAME("PE_allocation_rule")
   NAME("PE_control_slaves")
   NAME("PE_job_is_first_task")
   NAME("PE_resource_utilization")
   NAME("PE_urgency_slots")
   NAME("PE_accounting_summary")
NAMEEND

#define PE_SIZE sizeof(PEN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
