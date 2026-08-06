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

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <cerrno>
#include <pthread.h>

#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"
#include "uti/msg_utilib.h"

/// maps every portable signal code to the local signal number and name
const sig_mapT sig_map[] =
        {
                {SGE_SIGHUP, SIGHUP, "HUP"},
                {SGE_SIGINT, SIGINT, "INT"},
                {SGE_SIGQUIT, SIGQUIT, "QUIT"},
                {SGE_SIGILL, SIGILL, "ILL"},
                {SGE_SIGTRAP, SIGTRAP, "TRAP"},
                {SGE_SIGABRT, SIGABRT, "ABRT"},
                {SGE_SIGIOT, SIGIOT, "IOT"},
#ifdef SIGEMT
                {SGE_SIGEMT, SIGEMT, "EMT"},
#else
                {SGE_SIGEMT, SIGUNKNOWN, "EMT"},
#endif
                {SGE_SIGFPE, SIGFPE, "FPE"},
                {SGE_SIGKILL, SIGKILL, "KILL"},
                {SGE_SIGSEGV, SIGSEGV, "SEGV"},
                {SGE_SIGPIPE, SIGPIPE, "PIPE"},
                {SGE_SIGALRM, SIGALRM, "ALRM"},
                {SGE_SIGTERM, SIGTERM, "TERM"},
                {SGE_SIGURG, SIGURG, "URG"},
                {SGE_SIGSTOP, SIGSTOP, "STOP"},
                {SGE_SIGTSTP, SIGTSTP, "TSTP"},
                {SGE_SIGCONT, SIGCONT, "CONT"},
                {SGE_SIGCHLD, SIGCHLD, "CHLD"},
                {SGE_SIGTTIN, SIGTTIN, "TTIN"},
                {SGE_SIGTTOU, SIGTTOU, "TTOU"},
                {SGE_SIGIO, SIGIO, "IO"},
                {SGE_SIGXCPU, SIGXCPU, "XCPU"},
                {SGE_SIGXFSZ, SIGXFSZ, "XFSZ"},
                {SGE_SIGVTALRM, SIGVTALRM, "VTALRM"},
                {SGE_SIGPROF, SIGPROF, "PROF"},
                {SGE_SIGWINCH, SIGWINCH, "WINCH"},
                {SGE_SIGUSR1, SIGUSR1, "USR1"},
                {SGE_SIGUSR2, SIGUSR2, "USR2"},
                {SGE_SIGBUS, SIGBUS, "BUS"},
                {SGE_MIGRATE, SIGTTOU, "MIGRATE"},
                {SIGUNKNOWN, 0, nullptr}
        };

/**
 * @brief Unmap 32bit SGE/EE signal to system signal
 *
 * Unmap the 32bit SGE/EEsignal to the system specific signal
 *
 * @param sge_sig SGE/EE signal
 *
 * @return system signal
 *
 * @note MT-NOTE: sge_unmap_signal() is MT safe
 */
int sge_unmap_signal(uint32_t sge_sig) {
   const sig_mapT *mapptr = sig_map;

   while (mapptr->sge_sig) {
      if (mapptr->sge_sig == sge_sig) {
         return mapptr->sig;
      }
      mapptr++;
   }
   return -1;
}

/**
 * @brief Map system signal to 32bit SGE/EE signal
 *
 * Map the system specific signal to the 32bit sge signal
 *
 * @param sys_sig system signal
 *
 * @return SGE/EE Signal
 *
 * @note MT-NOTE: sge_map_signal() is MT safe
 */
uint32_t sge_map_signal(int sys_sig) {
   const sig_mapT *mapptr = sig_map;

   while (mapptr->sge_sig) {
      if (mapptr->sig == sys_sig) {
         return mapptr->sge_sig;
      }
      mapptr++;
   }
   return -1;
}

/**
 * @brief Make a SGE/SGEEE signal out of a string
 *
 * Make a sge signal out of a string. 'str' can be the signal name
 * (caseinsensitive) without sig or the signal number (Take care
 * numbers are system dependent).
 *
 * @param str signal string
 *
 * @return SGE/EE signal
 *
 * @note MT-NOTE: sge_str2signal() is MT safe
 */
uint32_t sge_str2signal(const char *str) {
   const sig_mapT *mapptr = sig_map;
   uint32_t signum;

   /* look for signal names in mapping table */
   while (mapptr->sge_sig) {
      if (!strcasecmp(str, mapptr->signame)) {
         return mapptr->sge_sig;
      }
      mapptr++;
   }

   /* could not find per name -> look for signal numbers */
   if (sge_strisint(str)) {
      signum = strtol(str, nullptr, 10);
      mapptr = sig_map;
      while (mapptr->sge_sig) {
         if ((int) signum == mapptr->sig) {
            return mapptr->sge_sig;
         }
         mapptr++;
      }
   }

   return -1;
}

/**
 * @brief Make a SGE/SGEEE signal out of a string
 *
 * Make a SGE/SGEEE signal out of a string
 *
 * @param str signal name
 *
 * @return SGE/EE signal
 *
 * @note MT-NOTE: sge_sys_str2signal() is MT safe
 */
uint32_t sge_sys_str2signal(const char *str) {
   const sig_mapT *mapptr = sig_map;
   uint32_t signum;

   /* look for signal names in mapping table */
   while (mapptr->sge_sig != SIGUNKNOWN) {
      if (strcasecmp(str, mapptr->signame) == 0) {
         return mapptr->sig;
      }
      mapptr++;
   }

   /* could not find per name -> look for signal numbers */
   if (sge_strisint(str)) {
      signum = SGE_STRTOU_LONG32(str);
      return signum;
   }

   return SIGUNKNOWN;
}

/**
 * @brief Make a string out of a SGE/EE signal
 *
 * Make a string out of a SGE/EE signal
 *
 * @param sge_sig SGE/EE signal
 *
 * @return signal string
 *
 * @note MT-NOTE: sge_sig2str() is MT safe
 */
const char *sge_sig2str(uint32_t sge_sig) {
   const sig_mapT *mapptr;

   /* look for signal names in mapping table */
   for (mapptr = sig_map; mapptr->sge_sig; mapptr++) {
      if (sge_sig == mapptr->sge_sig) {
         return mapptr->signame;
      }
   }

   return MSG_PROC_UNKNOWNSIGNAL;
}

/**
 * @brief Make a string out of a system signal
 *
 * Make a string out of a system signal
 *
 * @param sys_sig system signal
 *
 * @return signal string
 *
 * @note MT-NOTE: sge_sys_sig2str() is MT safe
 */
const char *sge_sys_sig2str(uint32_t sys_sig) {
   const sig_mapT *mapptr;

   /* look for signal names in mapping table */
   for (mapptr = sig_map; mapptr->sge_sig; mapptr++) {
      if ((int) sys_sig == mapptr->sig) {
         return mapptr->signame;
      }
   }

   return MSG_PROC_UNKNOWNSIGNAL;
}

/**
 * @brief Set signal mask to default
 *
 * Set signal mask to default for all signals except given signal
 *
 * @param sig_num signals which should be ignored (use sigemptyset and sigaddset to set signals, if nullptr, no signals are ignored)
 * @param err_func callback function to report errors
 *
 * @note MT-NOTE: sge_set_def_sig_mask() is MT safe
 */
void sge_set_def_sig_mask(sigset_t *sig_num, err_func_t err_func) {
   int i = 1;
   struct sigaction sig_vec;

   while (i < NSIG) {
      /*
       * never set default handler for 
       * SIGKILL and SIGSTOP
       */
      if ((i == SIGKILL) || (i == SIGSTOP)) {
         i++;
         continue;
      }

      /*
       * never set default handler for signals set
       * in sig_num if not nullptr
       */
      if (sig_num != nullptr && sigismember(sig_num, i)) {
         i++;
         continue;
      }

      errno = 0;
      sigemptyset(&sig_vec.sa_mask);
      sig_vec.sa_flags = 0;
      sig_vec.sa_handler = 0;
      sig_vec.sa_handler = SIG_DFL;
      if (sigaction(i, &sig_vec, nullptr)) {
         if (err_func) {
            char err_str[256];
            snprintf(err_str, 256, MSG_PROC_SIGACTIONFAILED_IS, i, strerror(errno));
            err_func(err_str);
         }
      }
      i++;
   }
}

/**
 * @brief Allow for all signals
 *
 * Allow for all signals.
 *
 * @note MT-NOTE: sge_unblock_all_signals() is MT safe
 */
void sge_unblock_all_signals() {
   sigset_t sigmask;
   /* unblock all signals */
   /* without this we depend on shell to unblock the signals */
   /* result is that SIGXCPU was not delivered with several shells */
   sigemptyset(&sigmask);
   sigprocmask(SIG_SETMASK, &sigmask, nullptr);
}

/**
 * @brief Blocks all signals the OS knows for the calling thread
 *
 * Blocks all signals the OS knows for the calling thread.
 *
 * @param oldsigmask the sigmask of this thread that was set before this function was called.
 *
 * @return 0 if ok, errno if pthread_sigmask failed, 1000 if oldsigmask == nullptr.
 *
 * @note MT-NOTE: sge_thread_block_signals() is MT safe
 */
int sge_thread_block_all_signals(sigset_t *oldsigmask) {
   sigset_t new_mask;
   int ret = 1000;

   if (oldsigmask != nullptr) {
      sigfillset(&new_mask);
      ret = pthread_sigmask(SIG_BLOCK, &new_mask, oldsigmask);
   }
   return ret;
}
