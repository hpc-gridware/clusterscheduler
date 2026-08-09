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
 *  Copyright: 2003 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2024-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief qmaster's threads, and the state the main thread keeps about them
 *
 * qmaster is not one loop but a set of thread pools, each with a job it does
 * and nothing else: workers answer requests that change something, readers
 * answer read-only ones, listeners accept connections, the timer fires the
 * deferred work, the event master ships events, the scheduler schedules, and
 * the mirror threads keep the read-only data stores current.
 *
 * Splitting reads from writes is the point of the reader pool: a `qstat`
 * storm then costs nothing that a `qsub` has to wait for.
 */

#include <pthread.h>
#include <vector>

#include "sgeobj/sge_daemonize.h"

#include "gdi/ocs_gdi_Packet.h"

#include "ocs_MirrorDataStore.h"

/** @brief The thread pools qmaster runs, and how it means to exit */
typedef struct {
   /** @brief exit state: 100 = another master took over */
   int exit_state;

   /** @brief Worker threads: handling incoming GDI requests (RW and RO) */
   cl_raw_list_t *worker_thread_pool;

   /** @brief Reader threads: handling incoming GDI requests (RO) */
   cl_raw_list_t *reader_thread_pool;

   /** @brief Message threads: accepting and answering certain commlib requests */
   cl_raw_list_t *listener_thread_pool;

   /** @brief Signal thread */
   cl_raw_list_t *signal_thread_pool;

   /** @brief Timed event thread */
   cl_raw_list_t *timer_thread_pool;

   /** @brief Event master thread */
   cl_raw_list_t *event_master_thread_pool;

   /** @brief Scheduler thread */
   cl_raw_list_t *scheduler_thread_pool;

   /** @brief JVM thread */
   cl_raw_list_t *jvm_thread_pool;

   /** @brief intern GDI test thread */
   cl_raw_list_t *test_thread_pool;

   /** @brief Event mirror thread */
   std::vector<ocs::MirrorDataStore *>mirror_thread_pool;
} main_control_t;

extern main_control_t Main_Control;   ///< The running threads and the exit state

int
sge_qmaster_shutdown_via_signal_thread(int i);

int
sge_qmaster_get_exit_state();

void
sge_qmaster_set_exit_state(int new_state);

bool
sge_qmaster_do_final_spooling();
