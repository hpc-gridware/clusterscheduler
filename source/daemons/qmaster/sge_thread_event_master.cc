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

#include <cstring>
#include <pthread.h>

#include "uti/sge_bootstrap.h"
#include "uti/ocs_cond.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_thread_ctrl.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_report.h"

#include "comm/cl_commlib.h"

#include "gdi/ocs_gdi_Packet.h"

#include "evm/sge_event_master.h"

#include "basis_types.h"
#include "ocs_ReportingFileWriter.h"
#include "setup_qmaster.h"
#include "sge_qmaster_timed_event.h"
#include "sge_thread_main.h"
#include "sge_thread_event_master.h"
#include "msg_qmaster.h"

static void
sge_event_master_cleanup_monitor(void *arg) {
   DENTER(TOP_LAYER);
   auto *monitor = static_cast<monitoring_t *>(arg);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

static void
sge_event_master_cleanup_report_list(void *arg) {
   DENTER(TOP_LAYER);
   auto *list = static_cast<lList **>(arg);
   lFreeList(list);
   DRETURN_VOID;
}

void
sge_event_master_initialize() {
   DENTER(TOP_LAYER);

   DPRINTF("event master functionality has been initialized\n");

   cl_thread_list_setup(&(Main_Control.event_master_thread_pool), "event master thread pool");
   cl_thread_settings_t *dummy_thread_p = nullptr;
   cl_thread_list_create_thread(Main_Control.event_master_thread_pool, &dummy_thread_p, cl_com_get_log_list(),
                                threadnames[EVENT_MASTER_THREAD], 0, sge_event_master_main, nullptr, nullptr,
                                CL_TT_EVENT_MASTER);
   DRETURN_VOID;
}

void
sge_event_master_terminate() {
   DENTER(TOP_LAYER);
   /*
    * trigger pthread_cancel for each thread so that further
    * shutdown process will be faster
    */
   cl_thread_list_elem_t *thr;
   cl_thread_list_elem_t *thr_nxt = cl_thread_list_get_first_elem(Main_Control.event_master_thread_pool);
   while ((thr = thr_nxt) != nullptr) {
      thr_nxt = cl_thread_list_get_next_elem(thr);

      cl_thread_shutdown(thr->thread_config);
   }

   // wakeup event master thread
   sge_event_master_flush_requests(true);

   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.event_master_thread_pool);
   while (thread != nullptr) {
      DPRINTF(SFN " gets canceled\n", thread->thread_name);
      cl_thread_list_delete_thread(Main_Control.event_master_thread_pool, thread);
      thread = cl_thread_list_get_first_thread(Main_Control.event_master_thread_pool);
   }
   DPRINTF("all " SFN " threads terminated\n", threadnames[EVENT_MASTER_THREAD]);
   INFO(MSG_THREAD_XTERMINATED_S, threadnames[EVENT_MASTER_THREAD]);

   DRETURN_VOID;
}

[[noreturn]] void *
sge_event_master_main(void *arg) {
   auto *thread_config = static_cast<cl_thread_settings_t *>(arg);
   monitoring_t monitor;
   monitoring_t *p_monitor = &monitor;
   u_long64 next_prof_output = 0;

   DENTER(TOP_LAYER);

   // set thread name and id used by logging an others
   const char *thread_name = thread_config->thread_name;
   const int thread_id = thread_config->thread_id;
   component_set_thread_name(thread_name);
   component_set_thread_id(thread_id);
   DPRINTF(SFN "(%d) started\n", thread_name, thread_id);

   // init monitoring
   cl_thread_func_startup(thread_config);
   sge_monitor_init(p_monitor, thread_config->thread_name, EMAT_EXT, EVENT_MASTER_THREAD_WARNING,
                    EVENT_MASTER_THREAD_ERROR, ocs::ReportingFileWriter::create_monitoring_records);
   sge_qmaster_thread_init(QMASTER, EVENT_MASTER_THREAD, true);

   // register at profiling module
   set_thread_name(pthread_self(), "Event Master Thread");
   conf_update_thread_profiling("Event Master Thread");

   lList *report_list = lCreateListHash("report list", REP_Type, false);
   lListElem *report = lCreateElem(REP_Type);
   lSetUlong(report, REP_type, NUM_REP_REPORT_EVENTS);
   lSetHost(report, REP_host, component_get_qualified_hostname());
   lAppendElem(report_list, report);

   while (true) {
      thread_start_stop_profiling();

      // did a new event arrive which has a flush time of 0 seconds?
      MONITOR_IDLE_TIME(sge_event_master_wait_next(), p_monitor, mconf_get_monitoring_options());
      MONITOR_MESSAGES(p_monitor);
      MONITOR_EDT_COUNT(p_monitor);
      MONITOR_CLIENT_COUNT(p_monitor, sge_get_num_event_clients());

      sge_event_master_process_requests(p_monitor);
      sge_event_master_send_events(report, report_list, p_monitor);
      sge_monitor_output(p_monitor);

      thread_output_profiling("event master thread profiling summary:\n", &next_prof_output);

      // pthread cancellation point
      int execute = 0;
      pthread_cleanup_push(sge_event_master_cleanup_monitor, static_cast<void *>(&monitor));
      pthread_cleanup_push(sge_event_master_cleanup_report_list, static_cast<void *>(&report_list));
      pthread_cleanup_push(sge_cleanup_event_master_control, nullptr);
      cl_thread_func_testcancel(thread_config);
      pthread_cleanup_pop(execute);
      pthread_cleanup_pop(execute);
      pthread_cleanup_pop(execute);

      // event master shall be active also during shutdown until the thread is explicitly cancelled
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before and after the call of cl_thread_func_testcancel()
}

