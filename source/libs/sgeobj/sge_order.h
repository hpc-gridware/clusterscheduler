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

/** @file
 * @brief Declarations of the order object the scheduler sends to qmaster
 *
 * @see sge_order.cc
 */

#include "sgeobj/cull/sge_order_OR_L.h"
#include "sgeobj/cull/sge_order_OQ_L.h"
#include "sgeobj/cull/sge_order_RTIC_L.h"

/**
 * @brief Valid values for `OR_type`, i.e. what the scheduler asks qmaster to do
 *
 * The scheduler does not change the object model itself. It produces a list of
 * orders, and qmaster executes them; this says what one order is.
 *
 * @note CS-1239 removed `ORT_remove_job`, `ORT_update_project_usage`,
 *       `ORT_update_user_usage` and `ORT_share_tree`. Finished job booking and
 *       burial moved to the worker thread (`sge_book_finished_job_usage`), and
 *       the master share tree recomputation plus the `sgeE_NEW_SHARETREE`
 *       emission moved to the qmaster timed event handler. The scheduler emits
 *       none of them any more and `sge_follow.cc` has no handlers for them.
 */
enum {
   ORT_start_job = 1,            ///< start the job on the queues named in the order
   ORT_tickets,                  ///< update a running job's ticket values
   ORT_ptickets,                 ///< update a pending job's ticket values
   ORT_remove_immediate_job,     ///< an immediate job could not be scheduled and has to go
   ORT_sched_conf,               ///< the scheduler configuration changed
   ORT_suspend_on_threshold,     ///< suspend the job because a threshold was exceeded
   ORT_unsuspend_on_threshold,   ///< resume a job suspended by a threshold
   ORT_job_schedd_info,          ///< deliver the reason messages for pending jobs
   /**
    * @brief Reset priority information
    *
    * The `ja_task_number` field has a special meaning with this order: 0
    * resets only pending jobs, anything else resets pending and running ones.
    */
   ORT_clear_pri_info
};

