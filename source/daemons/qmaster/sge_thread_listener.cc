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
 *  Copyright: 2003 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <pthread.h>
#include <cstring>

#include "uti/sge_bootstrap.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_thread_ctrl.h"

#include "comm/cl_commlib.h"

#include "sgeobj/sge_conf.h"

#include "msg_qmaster.h"
#include "basis_types.h"
#include "sge_qmaster_process_message.h"
#include "evm/sge_event_master.h"
#include "sge_qmaster_timed_event.h"
#include "setup_qmaster.h"
#include "sge_thread_main.h"
#include "sge_thread_listener.h"

static void
sge_listener_cleanup_monitor(monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

void
sge_listener_initialize() {
   const int max_initial_listener_threads = bootstrap_get_listener_thread_count();
   cl_thread_settings_t *dummy_thread_p = nullptr;

   DENTER(TOP_LAYER);

   INFO((SGE_EVENT, MSG_QMASTER_THREADCOUNT_US, sge_u32c(max_initial_listener_threads), threadnames[LISTENER_THREAD]));
   cl_thread_list_setup(&(Main_Control.listener_thread_pool), "thread pool");
   for (int i = 0; i < max_initial_listener_threads; i++) {
      cl_thread_list_create_thread(Main_Control.listener_thread_pool, &dummy_thread_p,
                                   cl_com_get_log_list(), threadnames[LISTENER_THREAD], i,
                                   sge_listener_main, nullptr, nullptr, CL_TT_LISTENER);
   }
   DRETURN_VOID;
}

void
sge_listener_terminate() {
   DENTER(TOP_LAYER);

   /*
    * Currently the event master (EDE) does need a working gdi thread to 
    * successfully deliver events. For this reason we need to add the
    * 'sgeE_QMASTER_GOES_DOWN' event *before* the listener threads 
    * are terminated.
    */
   sge_add_event(0, sgeE_QMASTER_GOES_DOWN, 0, 0, nullptr, nullptr, nullptr, nullptr);
   DPRINTF(("triggered shutdown event for event master module\n"));

   /*
    * trigger pthread_cancel for each thread so that further 
    * shutdown process will be faster
    */
   {
      cl_thread_list_elem_t *thr;

      cl_thread_list_elem_t *thr_nxt = cl_thread_list_get_first_elem(Main_Control.listener_thread_pool);
      while ((thr = thr_nxt) != nullptr) {
         thr_nxt = cl_thread_list_get_next_elem(thr);

         cl_thread_shutdown(thr->thread_config);
      }
   }

   /*
    * delete all threads and wait for termination
    */
   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.listener_thread_pool);
   while (thread != nullptr) {
      DPRINTF((SFN" gets canceled\n", thread->thread_name));
      cl_thread_list_delete_thread(Main_Control.listener_thread_pool, thread);
      thread = cl_thread_list_get_first_thread(Main_Control.listener_thread_pool);
   }
   DPRINTF(("all "SFN" threads exited\n", threadnames[LISTENER_THREAD]));
   DRETURN_VOID;
}

[[noreturn]] void *
sge_listener_main(void *arg) {
   auto *thread_config = (cl_thread_settings_t *) arg;
   monitoring_t monitor;
   time_t next_prof_output = 0;

   DENTER(TOP_LAYER);

   DPRINTF(("started\n"));
   cl_thread_func_startup(thread_config);
   sge_monitor_init(&monitor, thread_config->thread_name, LIS_EXT, MT_WARNING, MT_ERROR);
   sge_qmaster_thread_init(QMASTER, LISTENER_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "Listener Thread");
   conf_update_thread_profiling("Listener Thread");

   DPRINTF(("entering main loop\n"));
   while (true) {
      int execute = 0;

      if (sge_thread_has_shutdown_started() == false) {
         thread_start_stop_profiling();

         sge_qmaster_process_message(&monitor);

         thread_output_profiling("listener thread profiling summary:\n",
                                 &next_prof_output);

         sge_monitor_output(&monitor);
      }

      /* pthread cancellation point */
      do {
         pthread_cleanup_push((void (*)(void *)) sge_listener_cleanup_monitor,
                              (void *) &monitor);
            cl_thread_func_testcancel(thread_config);
         pthread_cleanup_pop(execute);
         if (sge_thread_has_shutdown_started()) {
            DPRINTF(("waiting for termination\n"));
            sleep(1);
         }
      } while (sge_thread_has_shutdown_started());
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before the call of cl_thread_func_testcancel()
}

