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
 * @brief SERF - the schedule entry recording facility
 *
 * The scheduler does not only decide what runs now, it also builds a
 * **schedule**: for every resource it knows when a job will occupy how much of
 * it, which is what advance reservations and resource reservation need. SERF
 * is the hook that lets a module record that schedule as it is built.
 *
 * A recorder registers two callbacks with serf_init() - one called once per
 * scheduling interval, one per entry - and is then fed every resource
 * debitation the scheduler makes. One job produces several entries: one per
 * resource it takes, plus one with level `P` for a parallel environment.
 */


/**
 * @name Reasons a utilization appears in the schedule
 *
 * Passed as the `type` of serf_record_entry().
 * @{
 */
#define SCHEDULING_RECORD_ENTRY_TYPE_RUNNING    "RUNNING"     ///< The job was already running before this scheduling run
#define SCHEDULING_RECORD_ENTRY_TYPE_SUSPENDED  "SUSPENDED"   ///< The job was suspended before this scheduling run
#define SCHEDULING_RECORD_ENTRY_TYPE_PREEMPTING "MIGRATING"   ///< The job is being preempted
#define SCHEDULING_RECORD_ENTRY_TYPE_STARTING   "STARTING"    ///< The job will be started
#define SCHEDULING_RECORD_ENTRY_TYPE_RESERVING  "RESERVING"   ///< The job reserves the resource for later
/** @} */

/**
 * @brief Callback that receives one entry of the schedule
 *
 * Registered with serf_init(). It is called once per resource debitation, so
 * a job that takes several resources produces several calls. `level_char` is
 * `Q` for a queue, `H` for a host, `G` for global and `P` for a parallel
 * environment, and `object_name` names the object at that level.
 */
typedef void (*record_schedule_entry_func_t)(uint32_t job_id, uint32_t ja_taskid,
      const char *state, uint64_t start_time, uint64_t end_time, char level_char,
      const char *object_name, const char *name, double utilization);
/**
 * @brief Callback announcing that a new schedule is being built
 *
 * Registered with serf_init() and called once at the start of a scheduling
 * interval, before the first entry of that interval.
 */
typedef void (*new_schedule_func_t)();

void serf_init(record_schedule_entry_func_t, new_schedule_func_t);
void serf_record_entry(uint32_t job_id, uint32_t ja_taskid,
                       const char *state, uint64_t start_time, uint64_t end_time, uint32_t level,
                       const char *object_name, const char *name, double utilization);
void serf_new_interval();
void serf_exit();
