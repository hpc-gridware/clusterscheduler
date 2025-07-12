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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
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
#include "uti/sge_time.h"

#include "gdi/ocs_gdi_ClientServerBase.h"

#include "sgeobj/ocs_Session.h"
#include "sgeobj/ocs_DataStore.h"

#include "sge_thread_ctrl.h"

#ifdef OBSERVE
#  include "cull/cull_observe.h"
#endif

#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_conf.h"

#include "gdi/ocs_gdi_Packet.h"

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
#include "sge_qmaster_process_message.h"
#include "msg_qmaster.h"

static void
sge_worker_cleanup_monitor(void *arg) {
   DENTER(TOP_LAYER);
   auto *monitor = static_cast<monitoring_t *>(arg);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

void
sge_worker_initialize() {
   const int max_initial_worker_threads = bootstrap_get_worker_thread_count();
   cl_thread_settings_t *dummy_thread_p = nullptr;

   DENTER(TOP_LAYER);

   /*
    * TODO: EB: corresponding destroy function is missing during shutdown
    */
   sge_tq_create(&GlobalRequestQueue);

   sge_init_job_number();
   sge_init_ar_id();
   DPRINTF("job/ar counter have been initialized\n");

   reporting_initialize();
   DPRINTF("accounting and reporting module has been initialized\n");

   INFO(MSG_QMASTER_THREADCOUNT_IS, max_initial_worker_threads, threadnames[WORKER_THREAD]);
   cl_thread_list_setup(&(Main_Control.worker_thread_pool), "thread pool");
   for (int i = 0; i < max_initial_worker_threads; i++) {
      cl_thread_list_create_thread(Main_Control.worker_thread_pool, &dummy_thread_p, cl_com_get_log_list(),
                                   threadnames[WORKER_THREAD], i, sge_worker_main, nullptr, nullptr, CL_TT_WORKER);
   }
   DRETURN_VOID;
}

void
sge_worker_terminate() {
   bool do_final_spooling;

   DENTER(TOP_LAYER);

   /*
    * trigger pthread_cancel for each thread so that further 
    * shutdown process will be faster
    */
   cl_thread_list_elem_t *thr;
   cl_thread_list_elem_t *thr_nxt = cl_thread_list_get_first_elem(Main_Control.worker_thread_pool);
   while ((thr = thr_nxt) != nullptr) {
      thr_nxt = cl_thread_list_get_next_elem(thr);

      cl_thread_shutdown(thr->thread_config);
   }

   sge_tq_wakeup_waiting(GlobalRequestQueue);

   /*
    * Shutdown/delete the threads and wait for termination
    */
   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.worker_thread_pool);
   while (thread != nullptr) {
      DPRINTF("gets canceled\n");
      cl_thread_list_delete_thread(Main_Control.worker_thread_pool, thread);

      thread = cl_thread_list_get_first_thread(Main_Control.worker_thread_pool);
   }
   DPRINTF("all " SFN " threads terminated\n", threadnames[WORKER_THREAD]);

   do_final_spooling = sge_qmaster_do_final_spooling();

   /* shutdown and remove JSV instances */
   jsv_list_remove_all();

   reporting_shutdown(nullptr, do_final_spooling);
   DPRINTF("accounting and reporting module has been shutdown\n");

   /*
    * final spooling is only done if the shutdown of the current instance
    * of this master process is not triggered due to the fact that
    * shadowd started another instance which is currently running ...
    *
    * ... in that case we would overwrite data which might have already
    * changed in the second instance of the master
    */
   if (do_final_spooling) {
      sge_store_job_number(nullptr, nullptr);
      sge_store_ar_id(nullptr, nullptr);
      DPRINTF("job/ar counter were made persistent\n");
      sge_userprj_spool(ocs::SessionManager::GDI_SESSION_NONE); /* spool the latest usage */
      DPRINTF("final job and user/project spooling has been triggered\n");
   }

   sge_shutdown_persistence(nullptr);
   DPRINTF("persistence module has been shutdown\n");

   INFO(MSG_THREADPOOL_XTERMINATED_S, threadnames[WORKER_THREAD]);

   DRETURN_VOID;
}

[[noreturn]] void *
sge_worker_main(void *arg) {
   auto *thread_config = (cl_thread_settings_t *) arg;
   monitoring_t monitor;
   monitoring_t *p_monitor = &monitor;
   u_long64 next_prof_output = 0;

   DENTER(TOP_LAYER);

   DPRINTF("started\n");

   // set thread name and id used by logging an others
   const char *thread_name = thread_config->thread_name;
   int thread_id = thread_config->thread_id;
   component_set_thread_name(thread_name);
   component_set_thread_id(thread_id);
   DPRINTF(SFN "(%d) started\n", thread_name, thread_id);

   // init monitoring
   cl_thread_func_startup(thread_config);
   sge_monitor_init(p_monitor, thread_config->thread_name, GDI_EXT, WT_WARNING, WT_ERROR,
                    ocs::ReportingFileWriter::create_monitoring_records);
   sge_qmaster_thread_init(QMASTER, WORKER_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "Worker Thread");
   conf_update_thread_profiling("Worker Thread");

   while (true) {
      ocs::gdi::Packet *packet = nullptr;

      /*
       * Wait for packets. As long as packets are available cancellation
       * of this thread is ignored. The shutdown procedure in the main 
       * thread takes care that packet producers will be terminated 
       * before all worker threads so that this won't be a problem.
       */
      MONITOR_IDLE_TIME(sge_tq_wait_for_task(GlobalRequestQueue, 1, SGE_TQ_GDI_PACKET, (void **) &packet),
                        p_monitor, mconf_get_monitoring_options());

      MONITOR_SET_QLEN(p_monitor, sge_tq_get_task_count(GlobalRequestQueue));

      // handle the packet only if it is not nullptr and the shutdown has not started
      if (packet != nullptr) {
         bool is_only_read_request = true;

         DPRINTF("Request should be handled by thread type %d\n", packet->ds_type);

         thread_start_stop_profiling();

         /*
          * prepare buffer for sending an answer 
          */
         if (!packet->is_intern_request && packet->request_type == PACKET_GDI_REQUEST) {
            init_packbuffer(&(packet->pb), 0);
         }

         MONITOR_MESSAGES(p_monitor);

         if (packet->request_type == PACKET_GDI_REQUEST) {
            /*
             * test if a write lock is necessary
             */
            for (const auto *task : packet->tasks) {
               u_long32 command = task->command;

               if (command != ocs::gdi::Command::SGE_GDI_GET) {
                  is_only_read_request = false;
                  break;
               }
            }
         } else {
            // PACKET_REPORT_REQUEST or PACKET_ACK_REQUEST
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
         INFO("======================");
#endif

         // handle the request (GDI/Report/Ack ...
         if (packet->request_type == PACKET_GDI_REQUEST) {
            lList *tmp_answer_list = nullptr;
            if (!packet->is_intern_request) {
               packet->pack_header(&tmp_answer_list, &packet->pb);
            }

            for (size_t i = 0; i < packet->tasks.size(); ++i) {
               bool has_next = (i < packet->tasks.size() - 1);

               ocs::gdi::Task *task = packet->tasks[i];
               sge_c_gdi_process_in_worker(packet, task, &(task->answer_list), p_monitor, has_next);
            }
         } else if (packet->request_type == PACKET_REPORT_REQUEST) {
            sge_c_report(packet, packet->tasks[0], packet->host, packet->commproc, packet->commproc_id, packet->tasks[0]->data_list, p_monitor);
         } else if (packet->request_type == PACKET_ACK_REQUEST) {
            sge_c_ack(packet, packet->tasks[0], p_monitor);
         } else {
            DPRINTF("unknown request type %d\n", packet->request_type);
         }

#ifdef OBSERVE
         dstring observ = DSTRING_INIT;
         lObserveGetInfoString(&observ);
         {
            struct saved_vars_s *context = nullptr;
            const char *line = sge_strtok_r(sge_dstring_get_string(&observ), "\n", &context);
            while (line) {
               INFO("%s", line);
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
            SGE_UNLOCK(LOCK_GLOBAL, LOCK_READ);
         } else {
            SGE_UNLOCK(LOCK_GLOBAL, LOCK_WRITE);
         }

         if (packet->request_type == PACKET_GDI_REQUEST) {
            /*
             * Send the answer to the client
             */
            if (!packet->is_intern_request) {
               MONITOR_MESSAGES_OUT(p_monitor);
               ocs::gdi::ClientServerBase::sge_gdi_send_any_request(0, nullptr, packet->host, packet->commproc, packet->commproc_id,
                                                                &(packet->pb), ocs::gdi::ClientServerBase::TAG_GDI_REQUEST, packet->response_id, nullptr);
               clear_packbuffer(&(packet->pb));
               delete packet;
               /*
                * Code only for TS: 
                *
                * Following if-block will only be executed in testsuite if the qmaster
                * parameter __TEST_SLEEP_AFTER_REQUEST is defined. This will block the
                * worker thread if it handled a request. Only this makes sure that
                * other worker threads can handle incoming requests. Otherwise,
                * it might be possible that one worker threads handles all requests
                * on fast qmaster hosts if testsuite is not fast enough to generate
                * gdi requests.
                */
               if (mconf_get_enable_test_sleep_after_request()) {
                  sleep(5);
               }
            } else {
               packet->broadcast_that_handled();
               /* this is an internal request, packet will get destroyed later,
                * where the caller waits for the answer
                * make sure it is no longer accessed here
                */
               packet = nullptr;
            }
         } else {
            delete packet;
         }

         thread_output_profiling("worker thread profiling summary:\n", &next_prof_output);

         sge_monitor_output(p_monitor);
      }

      // pthread cancellation point
      int execute = 0;
      pthread_cleanup_push(sge_worker_cleanup_monitor, static_cast<void *>(p_monitor));
      cl_thread_func_testcancel(thread_config);
      pthread_cleanup_pop(execute); // cleanup monitor

      // we still process requests also during shutdown until the thread is explicitly cancelled
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before and after the call of cl_thread_func_testcancel()
}

