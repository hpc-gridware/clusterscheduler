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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief A small queue for the signals the shepherd receives
 *
 * @see signal_queue.h for why the signals are queued rather than acted on.
 */

#include <cstdio>

#include <cctype>

#include "uti/sge_signal.h"

#include "signal_queue.h"
#include "err_trace.h"

/** @brief How many signals the ring buffer holds before it starts refusing them */
#define SGE_MAXSIG 128

/* ring buffer to queue signals */
static int sig_queue[SGE_MAXSIG];
static int n_sigs = 0;
static int next_sig = 0;
static int free_sig = 0; 


/** @brief Step one place round the ring buffer
 * @param i the current index
 */
#define NEXT_INDEX(i) (((i+1)>SGE_MAXSIG-1)?0:(i+1))

#ifdef DEBUG
/** @brief Write the queue's contents to the trace file
 *
 * Debug builds only.
 */
void report_signal_queue()
{
   char str[256];
   int n, i;
 
   if (n_sigs==0) {
      shepherd_trace("no signals in queue");
      return;
   }

   i=next_sig;

   for (n=n_sigs; n; n--) {
      sprintf(str, "%d. %d", i, sig_queue[i]); 
      shepherd_trace(str);
      i = NEXT_INDEX(i);
   }

   return;
}
#endif

/** @brief Turn a signal name from the configuration into a signal number
 * @param override_signal the name, e.g. `SIGKILL`
 * @return the signal number, or -1 when the name is not known
 */
int shepherd_sys_str2signal(char *override_signal)
{
   if (!isdigit(override_signal[0]))
      override_signal = &override_signal[3];
   return sge_sys_str2signal(override_signal);
}

/** @brief Store an additional signal in the queue
 *
 * Called from a signal handler, so it does nothing but append.
 *
 * @param signal signal number
 * @return 0 on success, -1 when the buffer is full
 */
int add_signal(int signal)
{
   int ret = -1;

   if (n_sigs != SGE_MAXSIG) {
      char err_str[256];
      ret = 0;

      n_sigs++;
      sig_queue[free_sig] = signal;
      free_sig = NEXT_INDEX(free_sig);

      snprintf(err_str, sizeof(err_str), "queued signal %s", sge_sys_sig2str(signal));
      shepherd_trace(err_str);
   } 
   return ret;
}  

/** @brief Take the next signal out of the queue
 * @return the signal number, or -1 when there are no more signals
 */
int get_signal()
{
   int signal = -1;

   if (n_sigs != 0) {
      n_sigs--;
      signal = sig_queue[next_sig];
      next_sig = NEXT_INDEX(next_sig);
   }

   return signal;
}

/** @brief Is this signal already waiting in the queue?
 * @param sig signal number
 * @return 1 when found, 0 when not
 */
int pending_sig(int sig) 
{
   int ret = 0;
   int n, i;

   for (n = n_sigs, i = next_sig; n; n--, i = NEXT_INDEX(i)) {
      if (sig_queue[i] == sig) {
         ret = 1;
         break;
      }
   }
   return ret;
}

/** @brief How many signals are waiting
 * @return the number of queued signals
 */
int get_n_sigs()
{
   return n_sigs; 
}

/** @brief Discard every queued signal */
void clear_queued_signals()
{
   n_sigs = next_sig = free_sig = 0; 
}
