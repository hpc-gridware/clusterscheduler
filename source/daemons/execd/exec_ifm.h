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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "sgedefs.h"

/* Structures. */

/*
 * Process data.  An array of jd_proccount of these structures is sent
 * after each job structure, and represents the processes "owned" by a job.
 */
struct psProc_s {
	long	pd_length;		/* Length of struct (set@run-time) */
	pid_t	pd_pid;
	time_t	pd_tstamp;		/* Timestamp of last update */
	uid_t	pd_uid;			/* user ID of this proc */
	gid_t	pd_gid;			/* group ID of this proc */
	long	pd_state;		/* 0: unknown 1:active 2:complete */
					/* (unknown is *bad*) */
	double	pd_pstart;		/* Start time of the process */
	double	pd_utime;		/* total user time used */
	double	pd_stime;		/* total system time used */
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
struct psJob_s {
	int	jd_length;		   /* Length of struct (set@run-time) */
					            /* includes length of trailing procs */
	JobID_t	jd_jid;			/* Job ID */
	uid_t	jd_uid;			   /* user ID of this job */
	gid_t	jd_gid;			   /* group ID of this job */
	time_t	jd_tstamp;		/* Timestamp of last update */
	long	jd_proccount;		/* attached process count (in list) */
	long	jd_refcnt;		   /* attached process count (from OS) */
/*
 *	_c = complete procs.  _a = active procs.
 *	_c is a running total, and _a is current procs.
 */
	double	jd_utime_a;		/* total user time used */
	double	jd_stime_a;		/* total system time used */
	/* completed */
	double	jd_utime_c;		/* total user time used */
	double	jd_stime_c;		/* total system time used */

	uint64	jd_mem;			/* memory used (integral) in KB seconds */
	uint64	jd_chars;		/* characters moved in bytes */
	uint64	jd_ioops;		/* characters moved in bytes */
	double   jd_iow;        /* I/O wait time in microseconds */

	uint64	jd_vmem;		   /* virtual memory size in bytes */
	uint64	jd_rss;		   /* resident set size in bytes */
	uint64	jd_himem;		/* high-water memory size in bytes */
   uint64   jd_maxrss;     /* maximum rss in bytes */
};
