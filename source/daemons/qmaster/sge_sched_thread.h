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
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JC.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"
#include "sgeobj/sge_answer.h"

/** @brief The scheduler's private snapshot of the cluster
 *
 * Maintained by the mirror from the event stream, so a scheduling run never
 * touches the live master lists. The copies are owned by the snapshot and
 * freed with it; the read-only lists are borrowed from the mirror. Move-only,
 * as its owning members are.
 */
struct scheduler_all_data_t {
   ocs::tom::ListPtr<ocs::tom::EH> host_list;          ///< Execution hosts
   ocs::tom::ListPtr<ocs::tom::QU> queue_list;         ///< Queue instances that can take jobs, reduced
   ocs::tom::ListPtr<ocs::tom::QU> dis_queue_list;     ///< Queue instances only for the reservation, reduced
   ocs::tom::ListPtr<ocs::tom::QU> all_queue_list;     ///< Every queue instance, with full name and state only
   ocs::tom::ListPtr<ocs::tom::JB> job_list;           ///< Jobs
   ocs::tom::List<ocs::tom::CE> centry_list;           ///< Complex entries, the mirror's list; takes default requests
   ocs::tom::ListPtr<ocs::tom::US> acl_list;           ///< Access control lists
   ocs::tom::ListPtr<ocs::tom::PE> pe_list;            ///< Parallel environments
   ocs::tom::ListPtr<ocs::tom::UU> user_list;          ///< Users
   ocs::tom::ListPtr<ocs::tom::US> dept_list;          ///< Departments
   ocs::tom::ListPtr<ocs::tom::PR> project_list;       ///< Projects
   ocs::tom::ListPtr<ocs::tom::STN> share_tree;        ///< The share tree
   ocs::tom::ConstList<ocs::tom::CK> ckpt_list;        ///< Checkpointing environments, the mirror's list
   ocs::tom::ListPtr<ocs::tom::JC> running_per_user;   ///< How many jobs each user is running
   ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list;      ///< Host groups, the mirror's list
   ocs::tom::ListPtr<ocs::tom::RQS> rqs_list;          ///< Resource quota sets
   ocs::tom::ListPtr<ocs::tom::AR> ar_list;            ///< Advance reservations
   ocs::tom::ListPtr<ocs::tom::CT> category_list;      ///< Scheduling categories, so jobs that look alike are
                                                       ///< decided once
   bool monitor_next_run = false;                      ///< Whether the next run writes a scheduling decision profile
};

/** @brief How the rest of qmaster wakes the scheduler up */
typedef struct {
   pthread_mutex_t mutex;      ///< used for mutual exclusion
   pthread_cond_t cond_var;   ///< used for waiting
   bool exit;       ///< true -> exit event delivery
   bool triggered;  ///< new events added, a scheduling run is triggered
   ocs::tom::ListData *new_events; ///< the storage for new events
   bool new_global_conf;   ///< The global configuration changed and has to be re-read
} scheduler_control_t;

extern scheduler_control_t Scheduler_Control;   ///< The scheduler thread's wake-up state

void
st_set_flag_new_global_conf(bool new_value);

bool
st_get_flag_new_global_conf();

int
subscribe_scheduler(sge_evc_class_t *evc, sge_where_what_t *where_what);

void
scheduler_method(sge_evc_class_t *evc, ocs::tom::ListPtr<ocs::tom::AN> *answer_list, scheduler_all_data_t *lists);
