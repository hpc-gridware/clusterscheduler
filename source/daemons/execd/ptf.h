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

#include <sys/types.h>

#include "cull/cull.h"

#define PTF_COMPENSATION_FACTOR 2.0

typedef pid_t osjobid_t;
typedef unsigned int u_osjobid_t;
#define OSJOBID_FMT pid_t_fmt

typedef gid_t addgrpid_t;
#define ADDGRPID_FMT gid_t_fmt  

/* job states */

#define JL_JOB_ACTIVE	0x00
#define JL_JOB_COMPLETE	0x01
#define JL_JOB_DELETED	0x02

/*-----------------------------------------------------

    PTF constants

*/

#define PTF_SCHEDULE_TIME 2

#define PTF_USAGE_DECAY_FACTOR 1.0

/* #define PTF_MIN_JOB_USAGE 0.001 */
#define PTF_MIN_JOB_USAGE 1.0

#define PTF_NICE_BASED

#define PTF_DIFF_DECAY_CONSTANT 0.8

#ifdef PTF_NICE_BASED
#  if defined(SOLARIS)
#    define ENFORCE_PRI_RANGE     1
#    define PTF_MIN_PRIORITY      20
#    define PTF_MAX_PRIORITY     -10
#    define PTF_OS_MIN_PRIORITY   20
#    define PTF_OS_MAX_PRIORITY  -20
#  elif defined(LINUX)
#    define ENFORCE_PRI_RANGE     1
#    define PTF_MIN_PRIORITY      20
#    define PTF_MAX_PRIORITY      0
#    define PTF_OS_MIN_PRIORITY   20
#    define PTF_OS_MAX_PRIORITY  -20
#  elif defined(DARWIN)
#    define ENFORCE_PRI_RANGE     1
#    define PTF_MIN_PRIORITY      20
#    define PTF_MAX_PRIORITY     -10
#    define PTF_OS_MIN_PRIORITY   20
#    define PTF_OS_MAX_PRIORITY  -20
#  elif defined(FREEBSD) || defined(NETBSD)
#    define ENFORCE_PRI_RANGE     1
#    define PTF_MIN_PRIORITY      20
#    define PTF_MAX_PRIORITY     -10
#    define PTF_OS_MIN_PRIORITY   20
#    define PTF_OS_MAX_PRIORITY  -20
#  endif
#  define PTF_BACKGROUND_JOB_PROPORTION 0.015
#  define PTF_BACKGROUND_JOB_PRIORITY NDPLOMAX
#endif

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
void ptf_unregister_registered_job(u_long32 job_id, u_long32 ja_task_id );

void ptf_reinit_queue_priority(u_long32 job_id, u_long32 ja_task_idr, 
                               const char *pe_task_id_str, int priority);

int ptf_job_started(osjobid_t os_jobid, const char *task_id_str,
                    const lListElem *job, u_long32 jataskid, const char *systemd_scope);

lList *ptf_build_usage_list(const char *name);
int ptf_get_usage(lList **jobs);

lList *ptf_get_job_usage(u_long job_id, u_long ja_task_id, const char *task_id);

int ptf_process_job_ticket_list(lList *jobs);

int ptf_job_complete(u_long32 job_id, u_long32 ja_task_id, const char *pe_task_id, lList **usage);

void ptf_update_job_usage();

int ptf_adjust_job_priorities();

const char *ptf_errstr(int ptf_error_code);

void ptf_show_registered_jobs();

/*-----------------------------------------------------

    PTF errors
*/

/* #define PTF_Exxxx 1 */

#define PTF_ERROR_NONE                  0
#define PTF_ERROR_JOB_NOT_FOUND         1
#define PTF_ERROR_INVALID_ARGUMENT      2
