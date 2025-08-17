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
 *   Copyright: 2003 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <pthread.h>

#include "sge_mtutil.h"
#include "sge_rmon_macros.h"
#include "sge_time.h"

#include "sge_thread_ctrl.h"

#define THREAD_CONTROL_MUTEX "thread_control_mutex"

#define THREAD_LAYER BASIS_LAYER

thread_control_t Thread_Control = {
        PTHREAD_MUTEX_INITIALIZER,
        PTHREAD_COND_INITIALIZER,
        false
};

/****** uti/thread_ctrl/sge_thread_has_shutdown_started() **********************
*  NAME
*     sge_thread_has_shutdown_started() -- shutdown in progress?
*
*  SYNOPSIS
*     bool sge_thread_has_shutdown_started()
*
*  FUNCTION
*     Service function which can be used to check if the executing
*     component is already shutting down.
*
*  INPUTS
*     void - NONE
*
*  RESULT
*     bool - is in progress?
*        true  - yes
*        false - no
*
*  NOTES
*     MT-NOTE: sge_thread_has_shutdown_started() is MT safe
*******************************************************************************/
bool
sge_thread_has_shutdown_started() {
   DENTER(THREAD_LAYER);
   sge_mutex_lock(THREAD_CONTROL_MUTEX, __func__, __LINE__, &Thread_Control.mutex);
   bool res = Thread_Control.shutdown_started;
   sge_mutex_unlock(THREAD_CONTROL_MUTEX, __func__, __LINE__, &Thread_Control.mutex);
   DRETURN(res);
}

/****** uti/thread_ctrl/sge_thread_notify_all_waiting() ************************
*  NAME
*     sge_thread_notify_all_waiting() -- notify waiting thread
*
*  SYNOPSIS
*     void sge_thread_notify_all_waiting()
*
*  FUNCTION
*     After the main thread has initialized all needed components and
*     threads it waits for a certain condition to be signaled
*     (sge_thread_wait_for_signal).
*     This signal will start the shutdown process of the master.
*     This function triggers this signal.
*
*  INPUTS
*     void - NONE
*
*  RESULT
*     void - NONE
*
*  NOTES
*     MT-NOTE: sge_thread_notify_all_waiting() is MT safe
*
*  SEE ALSO
*     uti/thread_ctrl/sge_thread_wait_for_signal()
*******************************************************************************/
void
sge_thread_notify_all_waiting() {
   DENTER(THREAD_LAYER);

   sge_mutex_lock(THREAD_CONTROL_MUTEX, __func__, __LINE__, &Thread_Control.mutex);

   Thread_Control.shutdown_started = true;
   pthread_cond_broadcast(&Thread_Control.cond_var);

   sge_mutex_unlock(THREAD_CONTROL_MUTEX, __func__, __LINE__, &Thread_Control.mutex);

   DRETURN_VOID;
}

/****** uti/thread_ctrl/sge_thread_wait_for_signal() ***************************
*  NAME
*     sge_thread_wait_for_signal() -- block current thread till shutdown
*
*  SYNOPSIS
*     void sge_thread_wait_for_signal()
*
*  FUNCTION
*     A call of this function will block the executing thread until the
*     shutdown of the process is triggered via sge_thread_notify_all_waiting()
*
*  INPUTS
*     void - NONE
*
*  RESULT
*     void - NONE
*
*  NOTES
*     MT-NOTE: sge_thread_wait_for_signal() is not MT safe
*
*  SEE ALSO
*     uti/thread_ctrl/sge_thread_notify_all_waiting()
*******************************************************************************/
void
sge_thread_wait_for_signal() {
   DENTER(THREAD_LAYER);
   sge_mutex_lock(THREAD_CONTROL_MUTEX, __func__, __LINE__, &Thread_Control.mutex);
   while (!Thread_Control.shutdown_started) {
      pthread_cond_wait(&Thread_Control.cond_var, &Thread_Control.mutex);
   }
   sge_mutex_unlock(THREAD_CONTROL_MUTEX, __func__, __LINE__, &Thread_Control.mutex);
   DRETURN_VOID;
}

/****** uti/thread_ctrl/sge_thread_usleep_during_shutdown() ********************
*  @brief Sleep for a short duration during shutdown.
*
*  This function is used to introduce a short sleep period while
*  a thread is waiting to be cancelled during sge_qmaster shutdown.
*  The thread will no longer perform any useful work, to a void busy
*  waiting it does micro sleeps for 50 milliseconds.
*  Example:
*     // pthread cancellation point
*     do {
*        pthread_cleanup_push((void (*)(void *)) sge_timer_cleanup_monitor, (void *) p_monitor);
*        cl_thread_func_testcancel(thread_config);
*        pthread_cleanup_pop(execute);
*
*        sge_thread_usleep_during_shutdown();
*     } while (sge_thread_has_shutdown_started());
*******************************************************************************/
void
sge_thread_usleep_during_shutdown() {
   DENTER(THREAD_LAYER);
   if (sge_thread_has_shutdown_started()) {
      DPRINTF("waiting for termination\n");
      sge_usleep(50000);
   }
   DRETURN_VOID;
}
