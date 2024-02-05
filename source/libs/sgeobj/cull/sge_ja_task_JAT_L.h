#ifndef SGE_JAT_L_H
#define SGE_JAT_L_H
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
*    SGE_ULONG(JAT_task_number) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_status) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_start_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_end_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_hold) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JAT_granted_pe) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_job_restarted) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JAT_granted_destin_identifier_list) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JAT_master_queue) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_state) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_pvm_ckpt_pid) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_pending_signal) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_pending_signal_delivery_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_pid) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JAT_osjobid) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JAT_usage_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JAT_scaled_usage_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JAT_reported_usage_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_fshare) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JAT_tix) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JAT_oticket) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JAT_fticket) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JAT_sticket) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JAT_share) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_suitable) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JAT_task_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JAT_finished_task_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JAT_previous_usage_list) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(JAT_pe_object) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_next_pe_task_id) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_stop_initiate_time) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JAT_prio) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JAT_ntix) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JAT_wallclock_limit) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JAT_message_list) - @todo add summary
*    @todo add description
*
*/

enum {
   JAT_task_number = JAT_LOWERBOUND,
   JAT_status,
   JAT_start_time,
   JAT_end_time,
   JAT_hold,
   JAT_granted_pe,
   JAT_job_restarted,
   JAT_granted_destin_identifier_list,
   JAT_master_queue,
   JAT_state,
   JAT_pvm_ckpt_pid,
   JAT_pending_signal,
   JAT_pending_signal_delivery_time,
   JAT_pid,
   JAT_osjobid,
   JAT_usage_list,
   JAT_scaled_usage_list,
   JAT_reported_usage_list,
   JAT_fshare,
   JAT_tix,
   JAT_oticket,
   JAT_fticket,
   JAT_sticket,
   JAT_share,
   JAT_suitable,
   JAT_task_list,
   JAT_finished_task_list,
   JAT_previous_usage_list,
   JAT_pe_object,
   JAT_next_pe_task_id,
   JAT_stop_initiate_time,
   JAT_prio,
   JAT_ntix,
   JAT_wallclock_limit,
   JAT_message_list
};

LISTDEF(JAT_Type)
   SGE_ULONG(JAT_task_number, CULL_PRIMARY_KEY | CULL_UNIQUE | CULL_HASH | CULL_SUBLIST)
   SGE_ULONG(JAT_status, CULL_SUBLIST)
   SGE_ULONG(JAT_start_time, CULL_SUBLIST)
   SGE_ULONG(JAT_end_time, CULL_SUBLIST)
   SGE_ULONG(JAT_hold, CULL_SUBLIST)
   SGE_STRING(JAT_granted_pe, CULL_SUBLIST)
   SGE_ULONG(JAT_job_restarted, CULL_SUBLIST)
   SGE_LIST(JAT_granted_destin_identifier_list, JG_Type, CULL_SUBLIST)
   SGE_STRING(JAT_master_queue, CULL_SUBLIST)
   SGE_ULONG(JAT_state, CULL_SUBLIST)
   SGE_ULONG(JAT_pvm_ckpt_pid, CULL_SUBLIST)
   SGE_ULONG(JAT_pending_signal, CULL_SUBLIST)
   SGE_ULONG(JAT_pending_signal_delivery_time, CULL_SUBLIST)
   SGE_ULONG(JAT_pid, CULL_SUBLIST)
   SGE_STRING(JAT_osjobid, CULL_SUBLIST)
   SGE_LIST(JAT_usage_list, UA_Type, CULL_SUBLIST)
   SGE_LIST(JAT_scaled_usage_list, UA_Type, CULL_SUBLIST)
   SGE_LIST(JAT_reported_usage_list, UA_Type, CULL_SUBLIST)
   SGE_ULONG(JAT_fshare, CULL_SUBLIST)
   SGE_DOUBLE(JAT_tix, CULL_SUBLIST)
   SGE_DOUBLE(JAT_oticket, CULL_SUBLIST)
   SGE_DOUBLE(JAT_fticket, CULL_SUBLIST)
   SGE_DOUBLE(JAT_sticket, CULL_SUBLIST)
   SGE_DOUBLE(JAT_share, CULL_SUBLIST)
   SGE_ULONG(JAT_suitable, CULL_DEFAULT)
   SGE_LIST(JAT_task_list, PET_Type, CULL_SUBLIST)
   SGE_LIST(JAT_finished_task_list, FPET_Type, CULL_SUBLIST)
   SGE_LIST(JAT_previous_usage_list, UA_Type, CULL_DEFAULT)
   SGE_OBJECT(JAT_pe_object, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(JAT_next_pe_task_id, CULL_DEFAULT)
   SGE_ULONG(JAT_stop_initiate_time, CULL_SUBLIST)
   SGE_DOUBLE(JAT_prio, CULL_DEFAULT)
   SGE_DOUBLE(JAT_ntix, CULL_DEFAULT)
   SGE_ULONG(JAT_wallclock_limit, CULL_SPOOL)
   SGE_LIST(JAT_message_list, QIM_Type, CULL_SPOOL)
LISTEND

NAMEDEF(JATN)
   NAME("JAT_task_number")
   NAME("JAT_status")
   NAME("JAT_start_time")
   NAME("JAT_end_time")
   NAME("JAT_hold")
   NAME("JAT_granted_pe")
   NAME("JAT_job_restarted")
   NAME("JAT_granted_destin_identifier_list")
   NAME("JAT_master_queue")
   NAME("JAT_state")
   NAME("JAT_pvm_ckpt_pid")
   NAME("JAT_pending_signal")
   NAME("JAT_pending_signal_delivery_time")
   NAME("JAT_pid")
   NAME("JAT_osjobid")
   NAME("JAT_usage_list")
   NAME("JAT_scaled_usage_list")
   NAME("JAT_reported_usage_list")
   NAME("JAT_fshare")
   NAME("JAT_tix")
   NAME("JAT_oticket")
   NAME("JAT_fticket")
   NAME("JAT_sticket")
   NAME("JAT_share")
   NAME("JAT_suitable")
   NAME("JAT_task_list")
   NAME("JAT_finished_task_list")
   NAME("JAT_previous_usage_list")
   NAME("JAT_pe_object")
   NAME("JAT_next_pe_task_id")
   NAME("JAT_stop_initiate_time")
   NAME("JAT_prio")
   NAME("JAT_ntix")
   NAME("JAT_wallclock_limit")
   NAME("JAT_message_list")
NAMEEND

#define JAT_SIZE sizeof(JATN)/sizeof(char *)


#endif
