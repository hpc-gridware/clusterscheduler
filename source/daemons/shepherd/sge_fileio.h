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
 * @brief The files the shepherd leaves in the job's active_jobs directory
 *
 * The execution daemon and the shepherd share no memory and no connection, so
 * everything the daemon needs to know afterwards - the pid, the usage, the
 * exit status, whether the shepherd got as far as exiting cleanly - is written
 * into the job's directory and read back from there.
 *
 * That is also what makes a restarted daemon able to pick up jobs it did not
 * start: the state is on disk, not in the daemon.
 */
bool 
shepherd_write_pid_file(pid_t pid, dstring *errmsg);

bool
shepherd_read_qrsh_pid_file(const char *filename, pid_t *qrsh_pid,
                            int *replace_qrsh_pid);

bool
shepherd_write_usage_file(uint32_t wait_status, int exit_status,
                          int child_signal, uint64_t start_time,
                          uint64_t end_time, struct rusage *rusage);

bool
shepherd_write_job_pid_file(const char *job_pid);

bool
shepherd_write_shepherd_about_to_exit_file();

bool 
shepherd_read_exit_status_file(int *return_code);

void
create_checkpointed_file(int ckpt_is_in_arena);

int 
checkpointed_file_exists();

bool
shepherd_write_sig_info_file(const char *filename, const char *task_id,
                             uint32_t exit_status);

bool
shepherd_read_qrsh_file(const char *filename, pid_t *qrsh_pid);

bool
shepherd_write_processor_set_number_file(int proc_set);

bool
shepherd_read_processor_set_number_file(int *proc_set);
