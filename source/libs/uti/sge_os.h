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

#include <sys/time.h>
#include <unistd.h>

#include "sge_getloadavg.h"
#include "sge_loadmem.h"
#include "sge_nprocs.h"
#include "sge_nice.h"

#if defined(LINUX) || defined(FREEBSD) || defined(NETBSD) || defined(DARWIN)
#  define PSCMD "/bin/ps -axc"
#elif defined(SOLARIS)
#  define PSCMD "/bin/ps -eo pid,fname"
#else
#  define PSCMD "/bin/ps -e"
#endif

/*
 * typedef for sge_daemonize_prepare() and sge_daemonize_finalize() 
 * max. supported number = 999 
 */
typedef enum uti_daemonize_state_type {
   SGE_DEAMONIZE_OK = 0,
   SGE_DAEMONIZE_DEAD_CHILD = 100,
   SGE_DAEMONIZE_TIMEOUT = 101
} uti_daemonize_state_t;


int sge_get_pids(pid_t *, int, const char *, const char *);

int sge_contains_pid(pid_t, pid_t *, int);

int sge_checkprog(pid_t, const char *, const char *);

void sge_close_all_fds(int *keep_open, unsigned long nr_of_fds);

int sge_get_max_fd();

int sge_dup_fd_above_stderr(int *fd);

int sge_occupy_first_three();
