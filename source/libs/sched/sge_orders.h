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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The orders the scheduler sends back to qmaster
 *
 * The scheduler never changes the cluster itself - it only decides, and every
 * decision leaves as an **order** to qmaster: start this job here, set these
 * tickets, suspend that queue. The orders of one scheduling run are collected
 * in an #order_t, split by kind because they are sent in that order and
 * counted separately for the profiling.
 */

#include "cull/cull.h"
#include "evc/sge_event_client.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_OR.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/**
 * @brief The orders of one scheduling run, grouped by kind
 *
 * The struct owns its order lists: a list still held when it goes out of
 * scope is freed with it. A list is created with its first order and handed
 * over (and emptied) when it is sent.
 */
typedef struct {
   ocs::tom::ListPtr<ocs::tom::OR> configOrderList;   ///< Suspend and unsuspend on threshold orders
   ocs::tom::ListPtr<ocs::tom::OR> pendingOrderList;  ///< Ticket and scheduler configuration orders
   ocs::tom::ListPtr<ocs::tom::OR> jobStartOrderList; ///< Job start, job info and remove-immediate-job orders
   uint32_t numberSendOrders;   ///< Number of orders sent so far
   uint32_t numberSendPackages; ///< Number of sends done in between
}order_t;

/** Initializer for an empty #order_t */
#define ORDER_INIT {nullptr, nullptr, nullptr, 0, 0}

void sge_add_schedd_info(ocs::tom::ListPtr<ocs::tom::OR> &or_list, int *global_mes_count, int *job_mes_count);

void sge_create_orders(ocs::tom::ListPtr<ocs::tom::OR> &or_list, uint32_t type,
                       ocs::tom::ConstElement<ocs::tom::JB> job, ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                       ocs::tom::ConstList<ocs::tom::JG> granted, bool update_execd);

#if 0
int sge_send_job_start_orders(sge_evc_class_t *evc, order_t *orders);
#endif
