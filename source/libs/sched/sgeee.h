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
 * @brief SGEEE - the ticket policies and the orders they produce
 *
 * sgeee_scheduler() is the entry point: it computes the tickets of the
 * running and pending jobs from the share tree, functional and override
 * policies, sorts the pending jobs by the result and builds the orders that
 * tell qmaster about it. The rest of this header exposes the share tree
 * primitives so that the qmaster side can recompute the node fields without
 * a `scheduler_all_data_t`.
 */

#include "sgeobj/cull/sge_boundaries.h"
#include "cull/cull.h"
#include "sge_orders.h"

#include "sge_sched_process_events.h"

int sgeee_scheduler(scheduler_all_data_t *lists,
                    lList *running_jobs,
                    lList *finished_jobs,
                    lList *pending_jobs,
                    order_t *orders);

void sgeee_resort_pending_jobs(lList **job_list); 
 
/**
 * @brief Distributes the tickets of the running jobs onto their hosts
 *
 * @param[in,out] running the running jobs
 * @param[in,out] hosts   the execution hosts, receiving the ticket sums
 *
 * @return 0 on success
 *
 * @warning Declared here, defined nowhere, and called nowhere in the source
 *          tree - see sort_host_list_by_share_load(), which was its only
 *          consumer.
 */
int calculate_host_tickets( lList **running, lList **hosts );
/**
 * @brief Sorts the hosts by the share load rather than by the load formula
 *
 * @param[in,out] host_list    the execution hosts (`EH_Type`)
 * @param[in]     complex_list the complex entries
 *
 * @return 0 on success
 *
 * @warning Declared here, defined nowhere, and called nowhere in the source
 *          tree. The host order used by the scheduler comes from
 *          sort_host_list() in `sort_hosts.cc`.
 */
int  sort_host_list_by_share_load ( lList *host_list,       /* EH_Type */
                                    lList *complex_list );  /* CX_Type */

void sge_clear_job( lListElem *job, bool is_clear_all);

void
sge_build_sgeee_orders(scheduler_all_data_t *lists, lList *running_jobs, lList *queued_jobs,
                      lList *finished_jobs, order_t *orders,
                      bool update_usage_and_configuration, int seqno, bool update_execd);

/* CS-1239 step 5: share-tree compute primitives exposed so the qmaster
 * TET share-tree publish handler can recompute master-sharetree node
 * fields without depending on a scheduler_all_data_t. The scheduler
 * itself still uses these on its mirror data store. */
int  sge_init_share_tree_nodes(lListElem *root);
void calculate_m_shares(lListElem *parent_node);
u_long update_job_ref_count(lListElem *node);
u_long update_active_job_ref_count(lListElem *node);
int  sge_calc_node_targets(lListElem *root, lListElem *node);

