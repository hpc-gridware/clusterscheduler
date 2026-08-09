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
 * @brief Turning a job's resource requests into process limits
 *
 * The shepherd applies `setrlimit()` for each requested resource just before
 * it exec's the job, so the kernel enforces the limits rather than the
 * daemons having to police them.
 *
 * Two things complicate that. A limit configured for a *job* has to be
 * multiplied by the slots the job holds on this host before it becomes a
 * per-process limit; and a limit small enough to stop the job would also stop
 * the shepherd, so every limit is raised to at least the `LIMIT_*_MIN` values
 * below.
 */

/** @name What a limit applies to on this platform
 *
 * Whether the kernel counts a resource per process or across the whole job
 * differs between platforms, and decides whether the configured value has to
 * be multiplied by the slot count.
 * @{
 */
#define RES_PROC     1                 ///< The kernel enforces this limit per process
#define RES_JOB      2                 ///< The kernel enforces it across the job
#define RES_BOTH     (RES_PROC|RES_JOB) ///< Both, so either reading is defensible
/** @} */

/** @name Floors below which a limit would break the shepherd itself
 *
 * The shepherd runs inside the job's limits, so a job that asks for almost no
 * memory or almost no CPU would kill the process that is supposed to start and
 * supervise it. Each limit is raised to at least these values.
 * @{
 */
#define LIMIT_VMEM_MIN (10l*1024l*1024l)   ///< Virtual memory, 10 MB
#define LIMIT_STACK_MIN (1l*1024l*1024l)   ///< Stack, 1 MB
#define LIMIT_CPU_MIN (2l)                 ///< CPU seconds
#define LIMIT_FSIZE_MIN (15l*1024l)        ///< File size, 15 KB - enough for the trace files
#define LIMIT_DESCR_MIN (100)              ///< Open file descriptors
#define LIMIT_DESCR_MAX (65535)            ///< Upper bound on descriptors, whatever was asked for
#define LIMIT_PROC_MIN  (20)               ///< Processes
#define LIMIT_MEMLOCK_MIN (4*1024)         ///< Locked memory
#define LIMIT_LOCKS_MIN (2)                ///< File locks
/** @} */

/** @brief One row of the table mapping a kernel limit to its name and scope */
struct resource_table_entry {
   uint32_t resource;         ///< The `RLIMIT_*` constant
   const char *resource_name; ///< Its name, for the trace file
   int resource_type[2];      ///< `RES_*` per platform: [0] NEC SX, [1] every other architecture
};

void setrlimits(bool trace_limits);
