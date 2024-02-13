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
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#ifdef OBSERVE
#  include "cull/cull_observe.h"
#endif

#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_conf.h"

#include "gdi/sge_gdi_packet.h"
#include "gdi/sge_gdi_packet_internal.h"
#include "gdi/sge_gdi2.h"

#include "comm/cl_commlib.h"

#include "basis_types.h"
#include "setup_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "sge_reporting_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_job_qmaster.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_c_report.h"
#include "sge_thread_main.h"
#include "sge_thread_worker.h"
#include "msg_qmaster.h"

static void
sge_worker_cleanup_monitor(monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

void
sge_worker_initialize() {
   const u_long32 max_initial_worker_threads = bootstrap_get_worker_thread_count();
   cl_thread_settings_t *dummy_thread_p = nullptr;
   u_long32 i;

   DENTER(TOP_LAYER);

   /*
    * TODO: EB: corresponding destroy function is missing during shutdown
    */
   sge_tq_create(&Master_Task_Queue);

   sge_init_job_number();
   sge_init_ar_id();
   DPRINTF(("job/ar counter have been initialized\n"));

   reporting_initialize(nullptr);
   DPRINTF(("accounting and reporting modlue has been initialized\n"));

   INFO((SGE_EVENT, MSG_QMASTER_THREADCOUNT_US, sge_u32c(max_initial_worker_threads), threadnames[WORKER_THREAD]));
   cl_thread_list_setup(&(Main_Control.worker_thread_pool), "thread pool");
   for (i = 0; i < max_initial_worker_threads; i++) {
      dstring thread_name = DSTRING_INIT;

      sge_dstring_sprintf(&thread_name, "%s%03d", threadnames[WORKER_THREAD], i);
      cl_thread_list_create_thread(Main_Control.worker_thread_pool, &dummy_thread_p, cl_com_get_log_list(),
                                   sge_dstring_get_string(&thread_name), i, sge_worker_main, nullptr, nullptr, CL_TT_WORKER);
      sge_dstring_free(&thread_name);
   }
   DRETURN_VOID;
}

void
sge_worker_terminate() {
   bool do_final_spooling;

   DENTER(TOP_LAYER);

   sge_tq_wakeup_waiting(Master_Task_Queue);

   /*
    * trigger pthread_cancel for each thread so that further 
    * shutdown process will be faster
    */
   {
      cl_thread_list_elem_t *thr = nullptr;
      cl_thread_list_elem_t *thr_nxt = nullptr;

      thr_nxt = cl_thread_list_get_first_elem(Main_Control.worker_thread_pool);
      while ((thr = thr_nxt) != nullptr) {
         thr_nxt = cl_thread_list_get_next_elem(thr);

         cl_thread_shutdown(thr->thread_config);
      }
   }

   /*
    * Shutdown/delete the threads and wait for termination
    */
   {
      cl_thread_settings_t *thread = nullptr;

      thread = cl_thread_list_get_first_thread(Main_Control.worker_thread_pool);
      while (thread != nullptr) {
         DPRINTF(("gets canceled\n"));
         cl_thread_list_delete_thread(Main_Control.worker_thread_pool, thread);

         thread = cl_thread_list_get_first_thread(Main_Control.worker_thread_pool);
      }
      DPRINTF(("all "SFN" threads terminated\n", threadnames[WORKER_THREAD]));
   }

   do_final_spooling = sge_qmaster_do_final_spooling();

   /* shutdown and remove JSV instances */
   jsv_list_remove_all();

   reporting_shutdown(nullptr, do_final_spooling);
   DPRINTF(("accounting and reporting module has been shutdown\n"));

   /*
    * final spooling is only done if the shutdown of the current instance
    * of this master process is not triggered due to the fact that
    * shadowd started another instance which is currently running ...
    *
    * ... in that case we would overwrite data which might have already
    * changed in the second instance of the master
    */
   if (do_final_spooling == true) {
      sge_store_job_number(nullptr, nullptr);
      sge_store_ar_id(nullptr, nullptr);
      DPRINTF(("job/ar counter were made persistant\n"));
      sge_job_spool();     /* store qmaster jobs to database */
      sge_userprj_spool(); /* spool the latest usage */
      DPRINTF(("final job and user/project spooling has been triggered\n"));
   }

   sge_shutdown_persistence(nullptr);
   DPRINTF(("persistance module has been shutdown\n"));

   DRETURN_VOID;
}

void *
sge_worker_main(void *arg) {
   bool do_endlessly = true;
   cl_thread_settings_t *thread_config = (cl_thread_settings_t *) arg;
   monitoring_t monitor;
   monitoring_t *p_monitor = &monitor;
   time_t next_prof_output = 0;

   DENTER(TOP_LAYER);

   DPRINTF(("started"));
   cl_thread_func_startup(thread_config);
   sge_monitor_init(p_monitor, thread_config->thread_name, GDI_EXT, MT_WARNING, MT_ERROR);
   sge_qmaster_thread_init(QMASTER, WORKER_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "Worker Thread");
   conf_update_thread_profiling("Worker Thread");

   while (do_endlessly) {
      sge_gdi_packet_class_t *packet = nullptr;

      /*
       * Wait for packets. As long as packets are available cancellation
       * of this thread is ignored. The shutdown procedure in the main 
       * thread takes care that packet producers will be terminated 
       * before all worker threads so that this won't be a problem.
       */
      MONITOR_IDLE_TIME(sge_tq_wait_for_task(Master_Task_Queue, 1, SGE_TQ_GDI_PACKET, (void **) &packet),
                        p_monitor, mconf_get_monitor_time(), mconf_is_monitor_message());

      MONITOR_SET_QLEN(p_monitor, sge_tq_get_task_count(Master_Task_Queue));

      if (packet != nullptr) {
         sge_gdi_task_class_t *task = packet->first_task;
         bool is_only_read_request = true;

         thread_start_stop_profiling();

         /*
          * prepare buffer for sending an answer 
          */
         if (packet->is_intern_request == false && packet->is_gdi_request == true) {
            init_packbuffer(&(packet->pb), 0, 0);
         }

         MONITOR_MESSAGES(p_monitor);

         if (packet->is_gdi_request == true) {
            /*
             * test if a write lock is necessary
             */
            task = packet->first_task;
            while (task != nullptr) {
               u_long32 command = SGE_GDI_GET_OPERATION(task->command);

               if (command != SGE_GDI_GET) {
                  is_only_read_request = false;
                  break;
               }
               task = task->next;
            }
         } else {
            is_only_read_request = false;
         }

         /*
          * acquire the correct lock
          */
         if (is_only_read_request) {
            MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_READ), p_monitor);
         } else {
            MONITOR_WAIT_TIME(SGE_LOCK(LOCK_GLOBAL, LOCK_WRITE), p_monitor);
         }

#ifdef OBSERVE
         lObserveStart();
         INFO((SGE_EVENT, "======================"));
#endif

         if (packet->is_gdi_request == true) {
            /*
             * do the GDI request
             */
            task = packet->first_task;
            while (task != nullptr) {
               sge_c_gdi(packet, task, &(task->answer_list), p_monitor);

               task = task->next;
            }
         } else {
            task = packet->first_task;
            sge_c_report(packet->host, packet->commproc, packet->commproc_id,
                         task->data_list, p_monitor);
         }

#ifdef OBSERVE
         dstring observ = DSTRING_INIT;
         lObserveGetInfoString(&observ);
         {
            struct saved_vars_s *context = nullptr;
            const char *line = sge_strtok_r(sge_dstring_get_string(&observ), "\n", &context);
            while (line) {
               INFO((SGE_EVENT, "%s", line));
               line = sge_strtok_r(nullptr, "\n", &context);
            }
            sge_free_saved_vars(context);
         }
         sge_dstring_free(&observ);
         lObserveEnd();
#endif

         /*
          * do unlock
          */
         if (is_only_read_request) {
            SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ)
         } else {
            SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE)
         }

         if (packet->is_gdi_request == true) {
            /*
             * Send the answer to the client
             */
            if (packet->is_intern_request == false) {
               MONITOR_MESSAGES_OUT(p_monitor);
               sge_gdi2_send_any_request(0, nullptr, packet->host, packet->commproc, packet->commproc_id,
                                         &(packet->pb), TAG_GDI_REQUEST, packet->response_id, nullptr);
               clear_packbuffer(&(packet->pb));
               sge_gdi_packet_free(&packet);
               /*
                * Code only for TS: 
                *
                * Following if-block will only be executed in testsuite if the qmaster
                * parameter __TEST_SLEEP_AFTER_REQUEST is defined. This will block the
                * worker thread if it handled a request. Only this makes sure that
                * other worker threads can handle incloming requests. Otherwise
                * it might be possible that one worker threads handles all requests
                * on fast qmaster hosts if testsuite is not fast enough to generate
                * gdi requests.
                */
               if (mconf_get_enable_test_sleep_after_request() == true) {
                  sleep(5);
               }
            } else {
               sge_gdi_packet_broadcast_that_handled(packet);
               /* this is an internal request, packet will get destroyed later,
                * where the caller waits for the answer
                * make sure it is no longer accessed here
                */
               packet = nullptr;
            }
         } else {
            sge_gdi_packet_free(&packet);
         }

         thread_output_profiling("worker thread profiling summary:\n", &next_prof_output);

         sge_monitor_output(p_monitor);
      } else {
         int execute = 0;

         /* 
          * pthread cancellation point
          */
         pthread_cleanup_push((void (*)(void *)) sge_worker_cleanup_monitor, (void *) p_monitor);
            cl_thread_func_testcancel(thread_config);
         pthread_cleanup_pop(execute);
      }
   }

   /*
    * Don't add cleanup code here. It will never be executed. Instead register
    * a cleanup function with pthread_cleanup_push()/pthread_cleanup_pop() before 
    * and after the call of cl_thread_func_testcancel()
    */

   DRETURN(nullptr);
}

