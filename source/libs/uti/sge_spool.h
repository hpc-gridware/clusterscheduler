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
 * @brief Spooling directory layout and file name handling
 */

#include <fstream>

#include <cinttypes>
#include "sge_dstring.h"

#define COMMENT_CHAR '#'

/**
 * @brief Type of filename or pathname
 *
 * Type of filename or pathname if no other sge_spool_flags_t
 * and/or sge_file_path_format_t are specified
 * with sge_get_file_path():
 *
 * JOBS_SPOOL_DIR - "./jobs"
 *
 * JOB_SPOOL_DIR - "./jobs/xx/yyyy/zzzz"
 *                 zzzz is a directory
 *                 'xxyyyyzzzz' is a job id
 *
 * JOB_SPOOL_DIR_AS_FILE - "./jobs/xx/yyyy/zzzz"
 *                         zzzz is a file
 *                         'xxyyyyzzzz' is a job id
 *
 * JOB_SPOOL_FILE - "./jobs/xx/yyyy/zzzz/common"
 *
 * TASKS_SPOOL_DIR - "./jobs/xx/yyyy/zzzz/1-4096"
 *                  (example for task ids between 1 and 4096)
 *
 * TASK_SPOOL_DIR - "./jobs/xx/yyyy/zzzz/1-4096/1"
 *                   (example for task with id 1 (directory))
 *
 * TASK_SPOOL_DIR_AS_FILE - "./jobs/xx/yyyy/zzzz/1-4096/1"
 *                          (example for task with id 1 (file))
 *
 * TASK_SPOOL_FILE - "./jobs/xx/yyyy/zzzz/1-4096/1/common"
 *                   (example for task with id 1)
 *
 * PE_TASK_SPOOL_FILE - "./jobs/xx/yyyy/zzzz/1-4096/1/1"
 *                   (example for ja_task 1 pe_task 1)
 *
 * JOB_SCRIPT_DIR - "./job_scripts"
 *
 * JOB_SCRIPT_FILE - "./job_scripts/1234"
 *                   (if job id is 1234)
 *
 * JOB_ACTIVE_DIR - "./active_jobs"
 */
/** @brief Which file or directory of the spooling layout to build */
typedef enum {
   JOBS_SPOOL_DIR,                   ///< directory holding all spooled jobs
   JOB_SPOOL_DIR,                    ///< directory of one job
   JOB_SPOOL_DIR_AS_FILE,            ///< the job directory addressed as a file, for removal
   JOB_SPOOL_FILE,                   ///< spool file of one job
   TASKS_SPOOL_DIR,                  ///< directory holding the array tasks of one job
   TASK_SPOOL_DIR,                   ///< directory of one array task
   TASK_SPOOL_DIR_AS_FILE,           ///< the task directory addressed as a file, for removal
   TASK_SPOOL_FILE,                  ///< spool file of one array task
   PE_TASK_SPOOL_FILE,               ///< spool file of one parallel task
   JOB_SCRIPT_DIR,                   ///< directory holding the job scripts
   JOB_SCRIPT_FILE,                  ///< the job script of one job
   JOB_ACTIVE_DIR                    ///< active jobs directory of one job
} sge_file_path_id_t;

/**
 * @brief Context information for spooling functions
 *
 * These constants are necessary to provide spooling functions
 * with context information where they are called and what they
 * should do. It depends on these spooling functions, how these
 * constants are interpreted. The documentation of these
 * routines may give you a more detailed description than you
 * may find here.
 *
 * SPOOL_DEFAULT - as it says the standard case
 *
 * SPOOL_WITHIN_EXECD - Used for objects which are spooled
 *                      within the execd.
 *
 * SPOOL_IGNORE_TASK_INSTANCES - Dont't handle array tasks.
 *
 * SPOOL_HANDLE_PARALLEL_TASKS - Spool pe tasks individually.
 *
 * SPOOL_ONLY_JATASK - spool only the ja_task, neither job nor pe_tasks
 *
 * SPOOL_ONLY_PETASK - spool only the pe_task, neither job nor ja_task
 */
/** @brief Options changing how a spooling path is built or traversed */
typedef enum {
   SPOOL_DEFAULT = 0x0000,           ///< no special handling
   SPOOL_WITHIN_EXECD = 0x0002,      ///< paths are built for execd rather than qmaster
   SPOOL_IGNORE_TASK_INSTANCES = 0x0004, ///< do not descend into the array task instances
   SPOOL_HANDLE_PARALLEL_TASKS = 0x0008, ///< include the parallel tasks of a job
   SPOOL_ONLY_JATASK = 0x0010,       ///< restrict the operation to the array task
   SPOOL_ONLY_PETASK = 0x0020        ///< restrict the operation to the parallel task
} sge_spool_flags_t;

/**
 * @brief Format of filename and pathname
 *
 * These constants are used with sge_get_file_path() to retrieve
 * file and pathnames for objects which should be spooled onto
 * a filesystem.
 *
 * FORMAT_DEFAULT - as it says the default format
 *
 * FORMAT_DOT_FILENAME - insert a '.' in front of the filename
 *                       (e.g. '/path/path/.filename)
 *
 * FORMAT_FIRST_PART   - first part of pathname (e.g /path)
 *
 * FORMAT_SECOND_PART  - (e.g /path/part2)
 *
 * FORMAT_THIRD_PART   - (e.g /path/part2/part3)
 */
typedef enum {
   FORMAT_DEFAULT = 0x0000,       ///< the full path, e.g. `/path/part2/part3`
   FORMAT_DOT_FILENAME = 0x0001,  ///< hide the file by prefixing its name with a dot
   FORMAT_FIRST_PART = 0x0002,    ///< only the leading directory, e.g. `/path`
   FORMAT_SECOND_PART = 0x0004,   ///< up to the second component, e.g. `/path/part2`
   FORMAT_THIRD_PART = 0x0008     ///< up to the third component, e.g. `/path/part2/part3`
} sge_file_path_format_t;

/** @brief How progress is shown while a long running operation waits */
typedef enum {
   STATUS_ROTATING_BAR,   ///< a spinning bar
   STATUS_DOTS            ///< one dot per step
} washing_machine_t;

/** @brief One entry of the bootstrap file */
typedef struct {
   const char *name;      ///< key as it appears in the bootstrap file
   bool is_required;      ///< startup fails when the key is missing
} bootstrap_entry_t;

uint32_t sge_get_ja_tasks_per_directory();

uint32_t sge_get_ja_tasks_per_file();

char *sge_get_file_path(char *buffer, size_t buffer_size, sge_file_path_id_t,
                        sge_file_path_format_t format_flags,
                        sge_spool_flags_t spool_flags,
                        uint32_t ulong_val1, uint32_t ulong_val2,
                        const char *string_val1);

int sge_spoolmsg_write(FILE *file, char comment_char, const char *version);
int sge_spoolmsg_write(std::ofstream &stream, char comment_char, const char *version);

void sge_spoolmsg_append(dstring *ds, char comment_char, const char *version);

char *sge_get_confval(const char *conf_val, const char *file);

int sge_get_confval_array(const char *fname,
                          int n,
                          int nmissing,
                          bootstrap_entry_t name[],
                          char value[][4097],
                          dstring *error_dstring
);

pid_t sge_readpid(const char *fname);

void sge_write_pid(const char *pid_log_file);

void sge_status_set_type(washing_machine_t type);

void sge_status_next_turn();

void sge_status_end_turn();

void sge_silent_set(int i);

int sge_silent_get();

int sge_get_management_entry(const char *fname, int n, int nmissing, bootstrap_entry_t name[],
                             char value[][SGE_PATH_MAX], dstring *error_dstring);

/* get path to active_jobs directory (just for execd and shepherd) */
const char *sge_get_active_job_file_path(dstring *buffer, uint32_t job_id,
                                         uint32_t ja_task_id, const char *pe_task_id, const char *filename);
