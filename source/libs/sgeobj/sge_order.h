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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sgeobj/cull/sge_order_OR_L.h"
#include "sgeobj/cull/sge_order_OQ_L.h"
#include "sgeobj/cull/sge_order_RTIC_L.h"

/* 
 * valid values for OR_type 
 */
enum {
   /* CS-1239: ORT_remove_job, ORT_update_project_usage, ORT_update_user_usage
    * and ORT_share_tree are gone - finished-job booking + bury moved to the
    * worker thread (sge_book_finished_job_usage), and master share-tree
    * recomputation + sgeE_NEW_SHARETREE emission moved to the qmaster TET
    * sharetree publish handler. The scheduler no longer emits any of these
    * and no handlers exist in sge_follow.cc. */
   ORT_start_job = 1,
   ORT_tickets,
   ORT_ptickets,
   ORT_remove_immediate_job,
   ORT_sched_conf,
   ORT_suspend_on_threshold,
   ORT_unsuspend_on_threshold,
   ORT_job_schedd_info,
   ORT_clear_pri_info                       /*the ja_task_number field has a special meaning with the order: */
                                            /* == 0 : only pending jobs are set to 0*/
                                            /* != 0 : pending and running jobs are set to 0 */
};

/* struct containing the cull pos for fields in the ticket order */
typedef struct {
   int JAT_status_pos;
   int JAT_tix_pos;

   int JAT_oticket_pos;
   int JAT_fticket_pos;
   int JAT_sticket_pos;
   int JAT_share_pos;
   int JAT_prio_pos;
   int JAT_ntix_pos;
   //int JAT_granted_resources_list_pos;
} ja_task_pos_t;


/* struct containing the cull pos for fields in the ticket order */
typedef struct {
   int JB_version_pos;
   int JB_nurg_pos;
   int JB_urg_pos;
   int JB_rrcontr_pos;
   int JB_dlcontr_pos;
   int JB_wtcontr_pos;
} job_pos_t;


/* struct containing the cull field position of the job target structures
   and the reduced order elements */
typedef struct {
   ja_task_pos_t ja_task;
   ja_task_pos_t order_ja_task;   
   job_pos_t   job;
   job_pos_t   order_job;
} order_pos_t;

void 
sge_free_cull_order_pos(order_pos_t **cull_order_pos);

void 
sge_create_cull_order_pos(order_pos_t **cull_order_pos, const lListElem *jep, const lListElem *jatp,
                          const lListElem *joker, const lListElem *joker_task); 
