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
 * @brief Signal names, numbers and signal handling helpers
 */

#ifndef __BASIS_TYPES_H

#   include "basis_types.h"

#endif

#include <cinttypes>
#include <csignal>

/** @name Portable signal numbers
 *
 * Signal numbers differ between operating systems, so a signal that travels
 * between hosts is carried as one of these instead. #sge_map_signal and
 * #sge_unmap_signal translate between a portable code and the local number.
 * @{
 */
#define SGE_SIGHUP                         901   ///< portable code for `SIGHUP`
#define SGE_SIGINT                         902   ///< portable code for `SIGINT`
#define SGE_SIGQUIT                        903   ///< portable code for `SIGQUIT`
#define SGE_SIGILL                         904   ///< portable code for `SIGILL`
#define SGE_SIGTRAP                        905   ///< portable code for `SIGTRAP`
#define SGE_SIGABRT                        906   ///< portable code for `SIGABRT`
#define SGE_SIGIOT                         907   ///< portable code for `SIGIOT`
#define SGE_SIGEMT                         908   ///< portable code for `SIGEMT`
#define SGE_SIGFPE                         909   ///< portable code for `SIGFPE`
#define SGE_SIGKILL                        910   ///< portable code for `SIGKILL`
#define SGE_SIGBUS                         911   ///< portable code for `SIGBUS`
#define SGE_SIGSEGV                        912   ///< portable code for `SIGSEGV`
#define SGE_SIGPIPE                        914   ///< portable code for `SIGPIPE`
#define SGE_SIGALRM                        915   ///< portable code for `SIGALRM`
#define SGE_SIGTERM                        916   ///< portable code for `SIGTERM`
#define SGE_SIGURG                         917   ///< portable code for `SIGURG`
#define SGE_SIGSTOP                        918   ///< portable code for `SIGSTOP`
#define SGE_SIGTSTP                        919   ///< portable code for `SIGTSTP`
#define SGE_SIGCONT                        920   ///< portable code for `SIGCONT`
#define SGE_SIGCHLD                        921   ///< portable code for `SIGCHLD`
#define SGE_SIGTTIN                        922   ///< portable code for `SIGTTIN`
#define SGE_SIGTTOU                        923   ///< portable code for `SIGTTOU`
#define SGE_SIGIO                          924   ///< portable code for `SIGIO`
#define SGE_SIGXCPU                        925   ///< portable code for `SIGXCPU`
#define SGE_SIGXFSZ                        926   ///< portable code for `SIGXFSZ`
#define SGE_SIGVTALRM                      927   ///< portable code for `SIGVTALRM`
#define SGE_SIGPROF                        928   ///< portable code for `SIGPROF`
#define SGE_SIGWINCH                       929   ///< portable code for `SIGWINCH`
#define SGE_SIGUSR1                        931   ///< portable code for `SIGUSR1`
#define SGE_SIGUSR2                        932   ///< portable code for `SIGUSR2`
#define SGE_MIGRATE            933   ///< checkpoint and migrate the job, no POSIX equivalent
/** @} */

/// stands in for a signal the local system does not have
#define SIGUNKNOWN                         0

/** @def SIGIGNORE
 * @brief ignore signal @p x, spelled the way this platform requires
 *
 * @note `sigignore()` is a legacy SVID call that glibc deprecates since 2.32,
 *       and `-Werror` turns that deprecation into a build error. `LINUX` is
 *       defined for every `lx-`/`ulx-`/`xlx-` architecture, so a new one is
 *       covered without another edit here (CS-2667).
 */
#if defined(FREEBSD) || defined(LINUX)
#  define SIGIGNORE(x) signal(x,SIG_IGN)
#else
#  define SIGIGNORE(x) sigignore(x)
#endif

/** @brief One row of the portable-to-local signal mapping table */
struct sig_mapT {
   uint32_t sge_sig;      ///< the portable code, one of the `SGE_SIG*` values
   int sig;               ///< the local signal number, #SIGUNKNOWN when absent here
   const char *signame;   ///< the signal name as a user would write it
};

int sge_unmap_signal(uint32_t sge_sig);

uint32_t sge_map_signal(int sys_sig);

uint32_t sge_str2signal(const char *str);

const char *sge_sig2str(uint32_t sge_sig);

const char *sge_sys_sig2str(uint32_t sig);

uint32_t sge_sys_str2signal(const char *str);

/** @brief Reports an error while signal handling is being set up
 * @param s the error text
 */
typedef void (*err_func_t)(char *s);

void sge_set_def_sig_mask(sigset_t *, err_func_t);

void sge_unblock_all_signals();

int sge_thread_block_all_signals(sigset_t *oldsigmask);
