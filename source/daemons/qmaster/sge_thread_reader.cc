/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <pthread.h>
#include <cstring>

#ifdef WITH_PYTHON_IN_QMASTER
#  include <pybind11/embed.h>
#endif

#include "uti/sge_bootstrap.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_os.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_DataStore.h"

#include "sge_thread_ctrl.h"

#ifdef OBSERVE
#  include "cull/cull_observe.h"
#endif

#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_conf.h"

#include "gdi/sge_gdi_packet.h"
#include "gdi/sge_gdi_packet_internal.h"
#include "gdi/sge_gdi.h"

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
#include "sge_thread_reader.h"
#include "sge_qmaster_process_message.h"
#include "msg_qmaster.h"

static void
sge_reader_cleanup_monitor(void *arg) {
   DENTER(TOP_LAYER);
   auto *monitor = static_cast<monitoring_t *>(arg);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

void
sge_reader_initialize() {
   const int max_initial_reader_threads = bootstrap_get_reader_thread_count();
   cl_thread_settings_t *dummy_thread_p = nullptr;

   DENTER(TOP_LAYER);

   INFO(MSG_QMASTER_THREADCOUNT_US, sge_u32c(max_initial_reader_threads), threadnames[READER_THREAD]);
   cl_thread_list_setup(&(Main_Control.reader_thread_pool), "thread pool");
   for (int i = 0; i < max_initial_reader_threads; i++) {
      cl_thread_list_create_thread(Main_Control.reader_thread_pool, &dummy_thread_p, cl_com_get_log_list(),
                                   threadnames[READER_THREAD], i, sge_reader_main, nullptr, nullptr, CL_TT_READER);
   }
   DRETURN_VOID;
}

void
sge_reader_terminate() {
   DENTER(TOP_LAYER);

   /*
    * trigger pthread_cancel for each thread so that further 
    * shutdown process will be faster
    */
   cl_thread_list_elem_t *thr;

   cl_thread_list_elem_t *thr_nxt = cl_thread_list_get_first_elem(Main_Control.reader_thread_pool);
   while ((thr = thr_nxt) != nullptr) {
      thr_nxt = cl_thread_list_get_next_elem(thr);

      cl_thread_shutdown(thr->thread_config);
   }

   sge_tq_wakeup_waiting(ReaderRequestQueue);
   // no need to handle ReaderWaitingRequestQueue because we have no threads waiting there

   /*
    * Shutdown/delete the threads and wait for termination
    */
   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.reader_thread_pool);
   while (thread != nullptr) {
      DPRINTF("gets canceled\n");
      cl_thread_list_delete_thread(Main_Control.reader_thread_pool, thread);

      thread = cl_thread_list_get_first_thread(Main_Control.reader_thread_pool);
   }
   DPRINTF("all " SFN " threads terminated\n", threadnames[READER_THREAD]);
   INFO(MSG_THREADPOOL_XTERMINATED_S, threadnames[READER_THREAD]);

   DRETURN_VOID;
}

[[noreturn]] void *
sge_reader_main(void *arg) {
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

   // this thread will use the READER data store
   ocs::DataStore::select_active_ds(ocs::DataStore::Id::READER);

   // init monitoring
   cl_thread_func_startup(thread_config);
   sge_monitor_init(p_monitor, thread_config->thread_name, GDI_EXT, RT_WARNING, RT_ERROR,
                    ocs::ReportingFileWriter::create_monitoring_records);
   sge_qmaster_thread_init(QMASTER, READER_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "Reader Thread");
   conf_update_thread_profiling("Reader Thread");

   while (true) {
      sge_gdi_packet_class_t *packet = nullptr;

      /*
       * Wait for packets. As long as packets are available cancellation
       * of this thread is ignored. The shutdown procedure in the main 
       * thread takes care that packet producers will be terminated 
       * before all reader threads so that this won't be a problem.
       */
      MONITOR_IDLE_TIME(sge_tq_wait_for_task(ReaderRequestQueue, 1, SGE_TQ_GDI_PACKET, (void **) &packet),
                        p_monitor, mconf_get_monitoring_options());

      MONITOR_SET_QLEN(p_monitor, sge_tq_get_task_count(GlobalRequestQueue));
      MONITOR_SET_RQLEN(p_monitor, sge_tq_get_task_count(ReaderRequestQueue));
      MONITOR_SET_WRQLEN(p_monitor, sge_tq_get_task_count(ReaderWaitingRequestQueue));

      // handle the packet only if it is not nullptr and the shutdown has not started
      if (packet != nullptr && !sge_thread_has_shutdown_started()) {
         sge_gdi_task_class_t *task;
         bool is_only_read_request = true;

         DPRINTF("Request should be handled by thread type %d\n", packet->ds_type);

         thread_start_stop_profiling();

         /*
          * prepare buffer for sending an answer 
          */
         if (!packet->is_intern_request && packet->request_type == PACKET_GDI_REQUEST) {
            init_packbuffer(&(packet->pb), 0, 0);
         }

         MONITOR_MESSAGES(p_monitor);

         if (packet->request_type == PACKET_GDI_REQUEST) {
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
            // PACKET_REPORT_REQUEST or PACKET_ACK_REQUEST
            is_only_read_request = false;
         }

#if defined (ENABLE_DEBUG_CHECKS)
         if (!is_only_read_request) {
            CRITICAL("reader thread tries to execute write request");
            abort();
         }
#endif

         /*
          * acquire the correct lock
          */
         if (is_only_read_request) {
            MONITOR_WAIT_TIME(SGE_LOCK(LOCK_READER, LOCK_READ), p_monitor);
         } else {
            MONITOR_WAIT_TIME(SGE_LOCK(LOCK_READER, LOCK_WRITE), p_monitor);
         }

#ifdef WITH_PYTHON_IN_QMASTER
         // We need to open a new scope here so that at the end the GIL is released
         {
            pybind11::gil_scoped_acquire acquire;
            // Beginning from here it is safe to utilize the python code

#if 0
            pybind11::exec(R"(
                kwargs = dict(name="World", number=42)
                message = "Hello, {name}! The answer is {number}".format(**kwargs)
                print(message)
                version = gcs_bridge.GcsVersion.get_string()
                print("GCS version: " + version)
            )");
#endif

            pybind11::eval_file("/home/ebablick/test.py");

#endif

#ifdef OBSERVE
            lObserveStart();
            INFO("======================");
#endif

            // handle the request (GDI/Report/Ack ...
            if (packet->request_type == PACKET_GDI_REQUEST) {
               //sge_usleep(1000000);

               task = packet->first_task;
               while (task != nullptr) {
                  sge_c_gdi_process_in_worker(packet, task, &(task->answer_list), p_monitor);

                  task = task->next;
               }
            } else if (packet->request_type == PACKET_REPORT_REQUEST) {
               task = packet->first_task;
               sge_c_report(packet, task, packet->host, packet->commproc, packet->commproc_id, task->data_list, p_monitor);
            } else if (packet->request_type == PACKET_ACK_REQUEST) {
               task = packet->first_task;
               // @TODO: This could be done by listener already?
               sge_c_ack(packet, task, p_monitor);
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

#ifdef WITH_PYTHON_IN_QMASTER
            // Here the Python GIL is released
         }
#endif


         /*
          * do unlock
          */
         if (is_only_read_request) {
            SGE_UNLOCK(LOCK_READER, LOCK_READ);
         } else {
            SGE_UNLOCK(LOCK_READER, LOCK_WRITE);
         }

         if (packet->request_type == PACKET_GDI_REQUEST) {
            /*
             * Send the answer to the client
             */
            if (!packet->is_intern_request) {
               MONITOR_MESSAGES_OUT(p_monitor);
               sge_gdi_send_any_request(0, nullptr, packet->host, packet->commproc, packet->commproc_id,
                                         &(packet->pb), TAG_GDI_REQUEST, packet->response_id, nullptr);
               clear_packbuffer(&(packet->pb));
               sge_gdi_packet_free(&packet);
               /*
                * Code only for TS: 
                *
                * Following if-block will only be executed in testsuite if the qmaster
                * parameter __TEST_SLEEP_AFTER_REQUEST is defined. This will block the
                * reader thread if it handled a request. Only this makes sure that
                * other reader threads can handle incoming requests. Otherwise,
                * it might be possible that one reader threads handles all requests
                * on fast qmaster hosts if testsuite is not fast enough to generate
                * gdi requests.
                */
               if (mconf_get_enable_test_sleep_after_request()) {
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

         thread_output_profiling("reader thread profiling summary:\n", &next_prof_output);

         sge_monitor_output(p_monitor);
      }

      // pass the cancellation point at least once or stay here if shutdown was triggered
      bool shutdown_started = false;
      do {
         // pthread cancellation point
         int execute = 0;
         pthread_cleanup_push(sge_reader_cleanup_monitor, static_cast<void *>(p_monitor));
         cl_thread_func_testcancel(thread_config);
         pthread_cleanup_pop(execute); // cleanup monitor

         // shutdown in process?
         shutdown_started = sge_thread_has_shutdown_started();

         // if we will wait here than do not eat up all cpu time
         if (shutdown_started) {
            sge_usleep(25000);
         }
      } while (shutdown_started);
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before and after the call of cl_thread_func_testcancel()
}

