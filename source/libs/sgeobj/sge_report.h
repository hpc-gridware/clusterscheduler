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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Declarations of the report object and the job wait status layout
 *
 * @see sge_report.cc
 */

#include "cull/cull.h"

#include "sgeobj/cull/sge_report_REP_L.h"
#include "sgeobj/cull/sge_report_JR_L.h"
#include "sgeobj/cull/sge_report_LIC_L.h"
#include "sgeobj/cull/sge_report_LR_L.h"

/**
 * @brief Valid values for REP_type
 *
 * An execution host sends everything it has to say in one report object; the
 * type says what `REP_list` holds, so the receiver knows which element type to
 * expect.
 */

#define NUM_REP_REPORT_LOAD    1                                         ///< load values that changed; `REP_list` is `LR_Type`

#define NUM_REP_REPORT_EVENTS  2                                         ///< events an event client acknowledges; `REP_list` is `ET_Type`

#define NUM_REP_REPORT_CONF    3                                         ///< the host's configuration; `REP_list` is `CONF_Type`

#define NUM_REP_REPORT_PROCESSORS 4                                      ///< the host's processor count; `REP_list` is `LIC_Type`

#define NUM_REP_REPORT_JOB     5                                         ///< the state of the jobs running here; `REP_list` is `JR_Type`

#define NUM_REP_FULL_REPORT_LOAD  6                                      ///< every load value, not only the changed ones; `REP_list` is `LR_Type`

/**
 * @brief Layout of JR_wait_status
 *
 * How a job ended, packed into one value: the flags say which of the other
 * fields are meaningful, and the exit status and signal number sit in their
 * own bit ranges. The layout exists because the shepherd's `wait()` status is
 * not portable across platforms, so it is normalised before it is reported.
 *
 * Read and write it with the `SGE_GET_*` and `SGE_SET_*` macros rather than by
 * masking by hand.
 */
#define SGE_WEXITED_BIT      0x00000001                                  ///< the job exited normally
#define SGE_WSIGNALED_BIT    0x00000002                                  ///< the job was terminated by a signal
#define SGE_WCOREDUMP_BIT    0x00000004                                  ///< the job dumped core
#define SGE_NEVERRAN_BIT     0x00000008                                  ///< the job never started at all
#define SGE_EXIT_STATUS_BITS 0x00000FF0                                  ///< the exit status; POSIX gives it only 8 bits
#define SGE_SIGNAL_BITS      0x0FFFF000                                  ///< the signal number; SGE numbers are high, so 16 bits

#define SGE_GET_WEXITED(status)   ((status)&SGE_WEXITED_BIT)             ///< did the job exit normally?
#define SGE_GET_WSIGNALED(status) ((status)&SGE_WSIGNALED_BIT)           ///< was the job terminated by a signal?
#define SGE_GET_WCOREDUMP(status)   ((status)&SGE_WCOREDUMP_BIT)         ///< did the job dump core?
#define SGE_GET_NEVERRAN(status)    ((status)&SGE_NEVERRAN_BIT)          ///< did the job never start?
#define SGE_GET_WEXITSTATUS(status) (((status)&SGE_EXIT_STATUS_BITS)>>4) ///< the job's exit status
#define SGE_GET_WSIGNAL(status)     (((status)&SGE_SIGNAL_BITS)>>12)     ///< the signal that terminated the job


/// Set or clear #SGE_WEXITED_BIT
#define SGE_SET_WEXITED(status, flag) \
   ((status) & ~SGE_WEXITED_BIT)   | ((flag)?SGE_WEXITED_BIT:0)
/// Set or clear #SGE_WSIGNALED_BIT
#define SGE_SET_WSIGNALED(status, flag) \
   ((status) & ~SGE_WSIGNALED_BIT) | ((flag)?SGE_WSIGNALED_BIT:0)
/// Set or clear #SGE_WCOREDUMP_BIT
#define SGE_SET_WCOREDUMP(status, flag) \
   ((status) & ~SGE_WCOREDUMP_BIT) | ((flag)?SGE_WCOREDUMP_BIT:0)
/// Set or clear #SGE_NEVERRAN_BIT
#define SGE_SET_NEVERRAN(status, flag) \
   ((status) & ~SGE_NEVERRAN_BIT)  | ((flag)?SGE_NEVERRAN_BIT:0)
/// Store the exit status in its bit range
#define SGE_SET_WEXITSTATUS(status, exit_status) \
   ((status) & ~SGE_EXIT_STATUS_BITS)  |(((exit_status)<<4) & SGE_EXIT_STATUS_BITS)
/// Store the signal number in its bit range
#define SGE_SET_WSIGNAL(status, signal) \
   ((status) & ~SGE_SIGNAL_BITS)       |(((signal)<<12) & SGE_SIGNAL_BITS)


/**
 * @brief Print usage information contained in a job report
 *
 * @param jr JR_Type element
 * @param fp file stream, or nullptr to print as debug messages
 *
 * @warning The definition in `sge_report.cc` sits inside an `#if 0`, so this
 *          does not link. There are no callers.
 */
void job_report_print_usage(const lListElem *jr, FILE *fp);
void job_report_init_from_job(lListElem *jr, const lListElem *jep, 
                              const lListElem *jatep, const lListElem *petep);
void job_report_init_from_job_with_usage(lListElem *job_report,
                                         const lListElem *job,
                                         lListElem *ja_task,
                                         lListElem *pe_task,
                                         uint64_t time_stamp);
