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
 * @brief The priority translation facility: turning tickets into nice values
 *
 * The scheduler expresses entitlement as *tickets*, which are relative and
 * cluster-wide. The operating system understands only a nice value, which is
 * absolute and local. The PTF is what converts one into the other, on each
 * execution host, every #PTF_SCHEDULE_TIME seconds.
 *
 * It does that by comparing what each job has actually consumed against what
 * its tickets entitled it to, and nudging the nice value of the jobs that are
 * ahead or behind. Because it is a feedback loop rather than a calculation,
 * the decay and compensation constants below matter: they decide how fast it
 * reacts and how much it overshoots.
 */

#include <sys/types.h>

#include "cull/cull.h"

#include "sgeobj/sge_conf.h"

/** @brief How hard a job that fell behind is compensated
 *
 * A job that used less than its share is allowed up to this multiple of its
 * entitlement while it catches up.
 */
#define PTF_COMPENSATION_FACTOR 2.0

typedef pid_t osjobid_t;          ///< The operating system's own job identifier
typedef unsigned int u_osjobid_t; ///< #osjobid_t as an unsigned value
/** @brief printf conversion for an #osjobid_t */
#define OSJOBID_FMT pid_t_fmt

typedef gid_t addgrpid_t;         ///< The additional group id a job's processes carry
/** @brief printf conversion for an #addgrpid_t */
#define ADDGRPID_FMT gid_t_fmt    

/* job states */

/** @name State of a job in the PTF's own list
 * @{
 */
#define JL_JOB_ACTIVE	0x00   ///< Still running, still being adjusted
#define JL_JOB_COMPLETE	0x01   ///< Finished; kept until its usage has been collected
#define JL_JOB_DELETED	0x02   ///< Gone; the entry may be reused
/** @} */

/*-----------------------------------------------------

    PTF constants

*/

/** @brief Seconds between two rounds of priority adjustment */
#define PTF_SCHEDULE_TIME 2

/** @brief How fast past usage is forgotten
 *
 * 1.0 means it is not forgotten at all, so entitlement is measured over the
 * whole life of the job.
 */
#define PTF_USAGE_DECAY_FACTOR 1.0

/* #define PTF_MIN_JOB_USAGE 0.001 */
/** @brief Usage assumed for a job that has not consumed anything measurable yet
 *
 * Without a floor the share calculation would divide by nearly zero and give a
 * brand new job an unbounded priority.
 */
#define PTF_MIN_JOB_USAGE 1.0

/** @brief Adjust priorities through the nice value
 *
 * The only mechanism still built. `PTF_NDPRI_BASED` and `PTF_SLICE_BASED`
 * below were for platforms this no longer runs on.
 */
#define PTF_NICE_BASED

/** @brief How much of the previous round's correction is carried forward
 *
 * Damps the feedback loop; without it the nice value would oscillate.
 */
#define PTF_DIFF_DECAY_CONSTANT 0.8

#ifdef PTF_NICE_BASED
#  if defined(SOLARIS)
#    define ENFORCE_PRI_RANGE     1
#    define PTF_MIN_PRIORITY      20
#    define PTF_MAX_PRIORITY     -10
#    define PTF_OS_MIN_PRIORITY   20l
#    define PTF_OS_MAX_PRIORITY  -20l
#  elif defined(LINUX)
#    define ENFORCE_PRI_RANGE     1   ///< Clamp to the range below rather than letting the OS refuse
#    define PTF_MIN_PRIORITY      20  ///< Least favourable priority the PTF will hand out
#    define PTF_MAX_PRIORITY      0   ///< Most favourable; 0 rather than -20, so jobs never outrank the daemons
#    define PTF_OS_MIN_PRIORITY   20l ///< Least favourable nice value the platform accepts
#    define PTF_OS_MAX_PRIORITY  -20l ///< Most favourable nice value the platform accepts
#  elif defined(DARWIN)
#    define ENFORCE_PRI_RANGE     1
#    define PTF_MIN_PRIORITY      20
#    define PTF_MAX_PRIORITY     -10
#    define PTF_OS_MIN_PRIORITY   20l
#    define PTF_OS_MAX_PRIORITY  -20l
#  elif defined(FREEBSD) || defined(NETBSD)
#    define ENFORCE_PRI_RANGE     1
#    define PTF_MIN_PRIORITY      20
#    define PTF_MAX_PRIORITY     -10
#    define PTF_OS_MIN_PRIORITY   20l
#    define PTF_OS_MAX_PRIORITY  -20l
#  endif
/** @brief Share below which a job is treated as a background job */
#  define PTF_BACKGROUND_JOB_PROPORTION 0.015
/** @brief The priority such a background job is given */
#  define PTF_BACKGROUND_JOB_PRIORITY NDPLOMAX
#endif

/** @brief Convert a PTF priority into the platform's own scale
 *
 * The identity on every platform still supported; the indirection is left in
 * place for the ones where the two scales differed.
 *
 * @param priority the PTF priority
 */
#define PTF_PRIORITY_TO_NATIVE_PRIORITY(priority) (priority)

#ifdef PTF_NDPRI_BASED
#define PTF_MIN_PRIORITY NDPNORMMIN
/* #define PTF_MAX_PRIORITY NDPNORMMAX */
#define PTF_MAX_PRIORITY (NDPNORMMIN-10)
/*
#define PTF_MIN_PRIORITY NDPLOMIN
#define PTF_MAX_PRIORITY NDPLOMAX
*/
#endif


#ifdef PTF_SLICE_BASED
#define PTF_MIN_PRIORITY 1
#define PTF_MAX_PRIORITY 1000
#define PTF_TIME_TO_SLICE_UP 500   /* milliseconds */
#define PTF_MIN_SLICE        0.1    /* milliseconds */
#endif


/*-----------------------------------------------------

   PTF library

*/

int ptf_init();

void ptf_start(); 

void ptf_stop(); 

int ptf_is_running(); 

void ptf_unregister_registered_jobs();
void ptf_unregister_registered_job(uint32_t job_id, uint32_t ja_task_id );

void ptf_reinit_queue_priority(uint32_t job_id, uint32_t ja_task_idr,
                               const char *pe_task_id_str, int priority);

int ptf_job_started(osjobid_t os_jobid, const char *task_id_str,
                    const lListElem *job, uint32_t jataskid, const char *systemd_scope, usage_collection_t usage_collection);

lList *ptf_build_usage_list(const char *name, usage_collection_t usage_collection);
int ptf_get_usage(lList **jobs);

lList *ptf_get_job_usage(u_long job_id, u_long ja_task_id, const char *task_id);

int ptf_process_job_ticket_list(lList *jobs);

int ptf_job_complete(uint32_t job_id, uint32_t ja_task_id, const char *pe_task_id, lList **usage);

void ptf_update_job_usage();

int ptf_adjust_job_priorities();

const char *ptf_errstr(int ptf_error_code);

void ptf_show_registered_jobs();

/*-----------------------------------------------------

    PTF errors
*/

/* #define PTF_Exxxx 1 */

/** @name What a PTF call went wrong with
 * @{
 */
#define PTF_ERROR_NONE                  0   ///< Success
#define PTF_ERROR_JOB_NOT_FOUND         1   ///< No such job is registered
#define PTF_ERROR_INVALID_ARGUMENT      2   ///< A parameter did not make sense
/** @} */
