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
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The wire format between the process data collector and its callers
 *
 * A job's figures and the figures of its processes are handed over as one
 * block: a `psJob_s`, immediately followed by `jd_proccount` `psProc_s`
 * structures. Both carry their own length as the first field, set at run time,
 * so a reader can walk the block without knowing which version wrote it.
 */

#include "sgedefs.h"

/* Structures. */

/** @brief One process owned by a job
 *
 * An array of psJob_s::jd_proccount of these follows each job structure.
 */
struct psProc_s {
	long	pd_length;		///< Length of struct, set at run time
	pid_t	pd_pid;			///< The process id
	time_t	pd_tstamp;		///< Timestamp of last update
	uid_t	pd_uid;			///< user ID of this proc
	gid_t	pd_gid;			///< group ID of this proc
	long	pd_state;		///< 0: unknown 1:active 2:complete - unknown is *bad*
	double	pd_pstart;		///< Start time of the process
	double	pd_utime;		///< total user time used
	double	pd_stime;		///< total system time used
};
/*
 * Job data.  This structure contains the cumulative job data for the
 * jd_jid job.  An array of psProc_s structures follows immediately after
 * this in the data stream.  jd_proccount tells how many psProc_s structures
 * follow.  They represent the processes "owned" by a job.
 *
 * Note that some of the data is derived from the completed process/session
 * data, and can vary for the procs.  For instance the acid of some procs can be
 * different from others, and the acid in the job record is what is reported
 * by the OS on job completion, or derived from the first proc seen if not
 * available from the OS.
 */
/** @brief The cumulative data for one job, followed by its processes
 *
 * The `_a` fields cover the processes still running and the `_c` fields are a
 * running total over the ones that have finished; a process that exits is
 * folded into `_c` before it disappears from `/proc`. Reading only `_a` would
 * therefore lose the usage of every job step that has already ended.
 *
 * Some fields are derived from completed process data and can differ between
 * processes - the accounting id, for instance, is what the operating system
 * reported at job completion, or the first one seen if the OS does not supply
 * it.
 */
struct psJob_s {
	int	jd_length;		   ///< Length of struct, set at run time, including the trailing procs
	JobID_t	jd_jid;			///< Job ID
	uid_t	jd_uid;			   ///< user ID of this job
	gid_t	jd_gid;			   ///< group ID of this job
	time_t	jd_tstamp;		///< Timestamp of last update
	long	jd_proccount;		///< attached process count (in list)
	long	jd_refcnt;		   ///< attached process count (from OS)

	double	jd_utime_a;		///< user time used by the processes still running
	double	jd_stime_a;		///< system time used by the processes still running
	/* completed */
	double	jd_utime_c;		///< user time used by the processes that have finished
	double	jd_stime_c;		///< system time used by the processes that have finished

	uint64	jd_mem;			///< memory used (integral) in KB seconds
	uint64	jd_chars;		///< characters moved in bytes
	uint64	jd_ioops;		///< number of I/O operations
	double   jd_iow;        ///< I/O wait time in microseconds

	uint64	jd_vmem;		   ///< virtual memory size in bytes
	uint64	jd_rss;		   ///< resident set size in bytes
	uint64	jd_himem;		///< high-water memory size in bytes
   uint64   jd_maxrss;     ///< maximum rss in bytes

	uint64   jd_pss;			///< proportional set size in bytes
	uint64   jd_maxpss;		///< maximum pss in bytes
	uint64   jd_pmem;			///< private memory in bytes
	uint64   jd_smem;			///< shared memory in bytes
};
