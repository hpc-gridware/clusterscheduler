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
 * @brief The scheduler run log
 *
 * The scheduler explains its decisions in two places, and both go through
 * schedd_log(): the file `<cell>/common/schedd_runlog`, written when
 * monitoring was switched on with `qconf -tsm`, and an answer list, used by
 * `qalter -w v` to tell a user why a job would not be scheduled. Which of the
 * two a call reaches is decided by its arguments, so most callers pass both
 * through from the top of the scheduling run.
 */

#include "sgeobj/sge_daemonize.h"

/** Name of the scheduler run log file, below `<cell root>/common` */
#define SCHED_LOG_NAME "schedd_runlog"

/**
 * @brief Returns a string representation of a job id for the run log
 *
 * @param[in] jobid the job id, or 0 when no single job is meant
 *
 * @return `"Job <id>"`, or `"Job"` for id 0
 *
 * @warning The result points into a static buffer and is overwritten by the
 *          next call.
 */
const char *job_descr(uint32_t jobid);

/**
 * @brief Writes one line to the scheduler run log
 *
 * The line goes to the answer list if one was passed, and to the run log file
 * if `monitor_next_run` is set - both can happen in the same call, and
 * neither is an error.
 *
 * @param[in]     logstr           the line to log
 * @param[in,out] monitor_alpp     answer list for `qalter -w v`, or nullptr
 * @param[in]     monitor_next_run whether to append to the run log file
 *
 * @return 0 on success, -1 if the run log file could not be written
 */
int schedd_log(const char *logstr, lList **monitor_alpp, bool monitor_next_run);

/**
 * @brief Logs a list of items behind a common prefix, wrapping the lines
 *
 * Used for enumerations such as job ids. The list is printed in chunks, so
 * that no single log line becomes unreadably long.
 *
 * @param[in,out] monitor_alpp     answer list for `qalter -w v`, or nullptr
 * @param[in]     monitor_next_run whether to append to the run log file
 * @param[in]     logstr           prefix put in front of every line
 * @param[in]     lp               the list of items to print
 * @param[in]     nm               the field of an element to print
 *
 * @return always 0
 */
int schedd_log_list(lList **monitor_alpp, bool monitor_next_run, const char *logstr, lList *lp, int nm);

/**
 * @brief Sets whether the next scheduling run writes the run log
 *
 * @param[in] set true to switch monitoring on for the next run
 *
 * @warning Declared but never defined, and nothing in the source tree calls
 *          it. The flag is passed through the call chain as a parameter
 *          instead. See also schedd_is_monitor_next_run().
 */
void schedd_set_monitor_next_run(bool set);
/**
 * @brief Returns whether the next scheduling run writes the run log
 *
 * @return true if monitoring is on for the next run
 *
 * @warning Declared but never defined, and nothing in the source tree calls
 *          it. See schedd_set_monitor_next_run().
 */
bool schedd_is_monitor_next_run();

/**
 * @brief Determines the path of the scheduler run log
 *
 * Composes `<cell root>/common/` #SCHED_LOG_NAME once; a second call does
 * nothing, so the path stays stable for the lifetime of the process.
 */
void schedd_set_schedd_log_file();
