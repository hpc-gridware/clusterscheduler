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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The `SSTATE_*` codes a shepherd reports back as the reason a job failed
 */

#include <sys/types.h>

/*
** when we dont know more than that
*/
#define SSTATE_FAILURE_BEFORE_JOB   1   ///< assumedly before job

/* these error conditions are recovered by the execd
 * they are shepherd errors too and the numbers must be distinct from
 * SSTATE_*
 */
#define ESSTATE_NO_SHEPHERD         2   ///< on executing shepherd (not used at the moment)
#define ESSTATE_NO_CONFIG           3   ///< before writing config
#define ESSTATE_NO_PID              4   ///< before writing pid
/* these states are returned by the shepherd and written to the 
 * exit_status file
 * jobs with failure <= SSTATE_BEFORE_JOB are rescheduled
 * but see src/job_exit.c for actual implementation
 */
#define SSTATE_READ_CONFIG          5   ///< on reading config file
#define SSTATE_PROCSET_NOTSET       6   ///< setting processor set
#define SSTATE_BEFORE_PROLOG        7   ///< before prolog
#define SSTATE_PROLOG_FAILED        8   ///< in prolog
#define SSTATE_BEFORE_PESTART       9   ///< before pestart
#define SSTATE_PESTART_FAILED      10   ///< in pestart
#define SSTATE_BEFORE_JOB          11   ///< before job
#define SSTATE_BEFORE_PESTOP       12   ///< before pestop
#define SSTATE_PESTOP_FAILED       13   ///< in pestop
#define SSTATE_BEFORE_EPILOG       14   ///< before epilog
#define SSTATE_EPILOG_FAILED       15   ///< in epilog
#define SSTATE_PROCSET_NOTFREED    16   ///< releasing processor set

#define ESSTATE_DIED_THRU_SIGNAL   17   ///< through signal
#define ESSTATE_SHEPHERD_EXIT      18   ///< shepherd returned error
#define ESSTATE_NO_EXITSTATUS      19   ///< before writing exit_status
#define ESSTATE_UNEXP_ERRORFILE    20   ///< found unexpected error file
#define ESSTATE_UNKNOWN_JOB        21   ///< in recognizing job

/*
 * these error conditions can be met
 * by the qmaster
 */
#define ESSTATE_EXECD_LOST_RUNNING 22   ///< removed manually

/* this is an error that occurs only in a SGE execd */
#define ESSTATE_PTF_CANT_GET_PIDS  23   ///< the PTF could not read the job's process ids

#define SSTATE_MIGRATE             24   ///< migrating
#define SSTATE_AGAIN               25   ///< rescheduling

#define SSTATE_OPEN_OUTPUT         26   ///< opening input/output file
#define SSTATE_NO_SHELL            27   ///< searching requested shell
#define SSTATE_NO_CWD              28   ///< changing into working directory
#define SSTATE_AFS_PROBLEM         29   ///< setting up AFS credentials
#define SSTATE_APPERROR            30   ///< rescheduling on application error
#define SSTATE_UNUSED1             31   ///< reserved, never reported
#define SSTATE_UNUSED2             32   ///< reserved, never reported
#define SSTATE_UNUSED3             33   ///< reserved, never reported
#define SSTATE_UNUSED4             34   ///< reserved, never reported
#define SSTATE_UNUSED5             35   ///< reserved, never reported
#define SSTATE_CHECK_DAEMON_CONFIG 36   ///< checking configured daemons
#define SSTATE_QMASTER_ENFORCED_LIMIT 37   ///< qmaster enforced h_rt limit
#define SSTATE_ADD_GRP_SET_ERROR   38   ///< setting the additional group id used to track the job's processes

#define MAX_SSTATE SSTATE_ADD_GRP_SET_ERROR   ///< the highest defined `SSTATE_*` below the "after job" range

#define SSTATE_FAILURE_AFTER_JOB  100   ///< assumedly after job

/*
 * we differentiate between several general failure states
 * the queue, all queues on that host or all queues in all
 * might have to be halted
 */
#define GFSTATE_NO_HALT           0   ///< the failure halts nothing
#define GFSTATE_QUEUE             1   ///< the failure halts the queue instance the job ran in
#define GFSTATE_HOST              2   ///< the failure halts every queue instance on that host
#define GFSTATE_SYSTEM            3   ///< the failure halts every queue instance in the cluster
#define GFSTATE_JOB               4   ///< the failure puts the job in error state, leaving the queues alone

const char *get_sstate_description(int sstate);

/** @brief How far the shepherd has got, as an `SSTATE_*` value
 *
 * Starts at `SSTATE_BEFORE_PROLOG` and is advanced as the shepherd moves
 * through prolog, pe_start, job, pe_stop and epilog. If the shepherd dies, this
 * is the value written to the `error` file, so it is what names the step that
 * failed.
 */
extern int shepherd_state;
/** @brief The pid of the co-shepherd, or a negative value when there is none */
extern pid_t coshepherd_pid;
