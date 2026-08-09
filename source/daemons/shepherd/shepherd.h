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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The shepherd: one process per job, from fork to exit status
 *
 * The execution daemon does not run jobs itself. For each one it forks a
 * shepherd, which sets the limits, drops to the job owner, starts the job and
 * stays alive until it ends - so that a daemon restart cannot orphan a running
 * job, and so that anything the job does badly happens in a process that is
 * not the daemon.
 */

/** @brief What the shepherd needs to know to checkpoint the job it supervises */
typedef struct {
   int type;       ///< The checkpointing mechanism, as a `CKPT_*` bitmask
   int pid;        ///< The process to checkpoint
   int interval;   ///< Seconds between two automatic checkpoints; 0 for none
} ckpt_info_t;

/** @brief The descriptors connecting an interactive job to its client
 *
 * Either a pty or three pipes, depending on whether the job asked for a
 * terminal.
 */
typedef struct {
   int pty_master;     ///< Master side of the pty, -1 when pipes are used
   int pipe_in;        ///< The job's standard input
   int pipe_out;       ///< The job's standard output
   int pipe_err;       ///< The job's standard error
   int pipe_to_child;  ///< Control channel to the child, for window size changes and the like
} ijs_fds_t;

int
wait_my_child(int pid, const char *childname, int timeout, ckpt_info_t *p_ckpt_info,
              struct rusage *rusage, int fd_pty_master, int fd_std_err);

bool
shepherd_signal_job(pid_t pid, int sig);
