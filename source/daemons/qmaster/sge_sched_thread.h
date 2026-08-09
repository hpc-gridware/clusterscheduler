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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The scheduler thread inside qmaster, and the lists it schedules from
 *
 * The scheduler does not read the master lists directly. It is an event client
 * like any other: the mirror keeps it a private copy of everything it needs,
 * and a scheduling run works entirely from that snapshot. That is what lets
 * the run take as long as it takes without holding a lock on the live data.
 */

#include "evc/sge_event_client.h"
#include "mir/sge_mirror.h"
#include "sge_sched_prepare_data.h"

/** @brief The scheduler's private snapshot of the cluster
 *
 * Maintained by the mirror from the event stream, so a scheduling run never
 * touches the live master lists.
 */
typedef struct {
   lList *host_list;        ///< Execution hosts (`EH_Type`)
   lList *queue_list;       ///< Queue instances that can take jobs (`QU_Type`)
   lList *dis_queue_list;   ///< `QU_Type` - contains the queues only for the reservation
   lList *all_queue_list;   ///< Every queue instance, including the unusable ones (`QU_Type`)
   lList *job_list;         ///< Jobs (`JB_Type`)
   const lList *centry_list;      ///< Complex entries (`CE_Type`)
   lList *acl_list;         ///< Access control lists (`US_Type`)
   lList *pe_list;          ///< Parallel environments (`PE_Type`)
   lList *user_list;        ///< Users (`UP_Type`)
   lList *dept_list;        ///< Departments (`US_Type`)
   lList *project_list;     ///< Projects (`UP_Type`)
   lList *share_tree;       ///< The share tree (`STN_Type`)
   const lList *ckpt_list;        ///< Checkpointing environments (`CK_Type`)
   lList *running_per_user; ///< How many jobs each user is running (`JC_Type`)
   const lList *hgrp_list;        ///< Host groups (`HGRP_Type`)
   lList *rqs_list;         ///< Resource quota sets (`RQS_Type`)
   lList *ar_list;          ///< Advance reservations (`AR_Type`)
   lList *category_list;    ///< Scheduling categories, so jobs that look alike are decided once (`CT_Type`)
   bool monitor_next_run;   ///< Whether the next run writes a scheduling decision profile
} scheduler_all_data_t;

/** @brief How the rest of qmaster wakes the scheduler up */
typedef struct {
   pthread_mutex_t mutex;      ///< used for mutual exclusion
   pthread_cond_t cond_var;   ///< used for waiting
   bool exit;       ///< true -> exit event delivery
   bool triggered;  ///< new events added, a scheduling run is triggered
   lList *new_events; ///< the storage for new events
   bool new_global_conf;   ///< The global configuration changed and has to be re-read
} scheduler_control_t;

extern scheduler_control_t Scheduler_Control;   ///< The scheduler thread's wake-up state

void
st_set_flag_new_global_conf(bool new_value);

bool
st_get_flag_new_global_conf();

/** @brief Subscribe the scheduler to the events it needs
 *
 * The subscription decides what the mirror keeps, so it has to cover exactly
 * the lists in #scheduler_all_data_t and no more.
 *
 * @param evc the event client
 * @param where_what the filters and field selections to subscribe with
 * @return 0 on success
 */
int
subscribe_scheduler(sge_evc_class_t *evc, sge_where_what_t *where_what);

void
scheduler_method(sge_evc_class_t *evc, lList **answer_list, scheduler_all_data_t *lists);
