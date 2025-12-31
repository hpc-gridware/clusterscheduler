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
 *  Portions of this software are Copyright (c) 2024-2025 HPC-Gridware GmbH
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
#include "uti/sge_time.h"

#include "comm/cl_commlib.h"

#include "sgeobj/ocs_Session.h"
#include "sgeobj/sge_conf.h"

#include "msg_qmaster.h"
#include "basis_types.h"
#include "sge_qmaster_process_message.h"
#include "evm/sge_event_master.h"
#include "sge_qmaster_timed_event.h"
#include "setup_qmaster.h"
#include "sge_thread_main.h"
#include "sge_thread_listener.h"
#include "sge_thread_utility.h"
#include "ocs_ReportingFileWriter.h"

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

   INFO(MSG_QMASTER_THREADCOUNT_IS, max_initial_listener_threads, threadnames[LISTENER_THREAD]);
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
   sge_add_event(0, sgeE_QMASTER_GOES_DOWN, 0, 0, nullptr, nullptr, nullptr, nullptr, ocs::SessionManager::GDI_SESSION_NONE);
   DPRINTF("triggered shutdown event for event clients to be delivered by event master module\n");

   /*
    * trigger pthread_cancel for each thread so that further
    * shutdown process will be faster
    */
#if 0
   cl_com_handle_t *handle = cl_com_get_handle(component_get_component_name(), 0);
#endif
   cl_thread_list_elem_t *thr;
   cl_thread_list_elem_t *thr_nxt = cl_thread_list_get_first_elem(Main_Control.listener_thread_pool);
   while ((thr = thr_nxt) != nullptr) {
      thr_nxt = cl_thread_list_get_next_elem(thr);

      cl_thread_shutdown(thr->thread_config);
   }

#if 0
         // signal (broadcast) the commlib handle app_condition variable
         // this will make it leave waiting for new messages in sge_qmaster_process_message->sge_gdi_get_any_request()
         // @todo CS-982 that's the theory, but it doesn't work
         //       => should work now with the fix of CS-1735
         // as it is now shutting down the listener threads takes some time (> 1 second)
         cl_thread_trigger_thread_condition(handle->app_condition, 1);
#endif

   /*
    * delete all threads and wait for termination
    */
   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.listener_thread_pool);
   while (thread != nullptr) {
      DPRINTF(SFN " gets canceled\n", thread->thread_name);
      cl_thread_list_delete_thread(Main_Control.listener_thread_pool, thread);
      thread = cl_thread_list_get_first_thread(Main_Control.listener_thread_pool);
   }
   DPRINTF("all " SFN " threads exited\n", threadnames[LISTENER_THREAD]);
   INFO(MSG_THREADPOOL_XTERMINATED_S, threadnames[LISTENER_THREAD]);
   DRETURN_VOID;
}

[[noreturn]] void *
sge_listener_main(void *arg) {
   auto *thread_config = (cl_thread_settings_t *) arg;
   monitoring_t monitor;
   u_long64 next_prof_output = 0;

   DENTER(TOP_LAYER);

   // set thread name and id used by logging an others
   const char *thread_name = thread_config->thread_name;
   int thread_id = thread_config->thread_id;
   component_set_thread_name(thread_name);
   component_set_thread_id(thread_id);
   DPRINTF(SFN "(%d) started\n", thread_name, thread_id);

   // init monitoring
   cl_thread_func_startup(thread_config);
   sge_monitor_init(&monitor, thread_config->thread_name, LIS_EXT, MT_WARNING, MT_ERROR,
                    ocs::ReportingFileWriter::create_monitoring_records);
   sge_qmaster_thread_init(QMASTER, LISTENER_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "Listener Thread");
   conf_update_thread_profiling("Listener Thread");

   // this thread will use the LISTENER data store
   ocs::DataStore::select_active_ds(ocs::DataStore::Id::LISTENER);

   DPRINTF("entering main loop\n");
   while (true) {
      if (!sge_thread_has_shutdown_started()) {
         thread_start_stop_profiling();

         sge_qmaster_process_message(&monitor);

         thread_output_profiling("listener thread profiling summary:\n", &next_prof_output);

         sge_monitor_output(&monitor);
      }

      /* pthread cancellation point */
      do {
         int execute = 0;
         pthread_cleanup_push((void (*)(void *)) sge_listener_cleanup_monitor, (void *) &monitor);
         cl_thread_func_testcancel(thread_config);
         pthread_cleanup_pop(execute);

         sge_thread_usleep_during_shutdown();
      } while (sge_thread_has_shutdown_started());
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before the call of cl_thread_func_testcancel()
}

