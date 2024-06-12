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
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"

#include "comm/cl_commlib.h"

#include "uti/sge_profiling.h"
#include "uti/sge_os.h"
#include "uti/sge_thread_ctrl.h"

#include "gdi/sge_security.h"
#include "gdi/sge_gdi_packet.h"

#include "basis_types.h"
#include "setup_qmaster.h"
#include "sge_calendar_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "sge_qmaster_timed_event.h"
#include "sge_host_qmaster.h"
#include "sge_userprj_qmaster.h"
#include "sge_give_jobs.h"
#include "sge_advance_reservation_qmaster.h"
#include "sge_thread_main.h"
#include "sge_thread_timer.h"
#include "sge_qmaster_heartbeat.h"
#include "sge_job_enforce_limit.h"
#include "sge_qmod_qmaster.h"
#include "reschedule.h"
#include "sge_job_qmaster.h"
#include "sge_log.h"

static void
sge_timer_cleanup_monitor(monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

/****** qmaster/sge_thread_timer/sge_timer_register_event_handler() *************
*  NAME
*     sge_timer_register_event_handler() -- register event handlers
*
*  SYNOPSIS
*     void sge_timer_register_event_handler(void) 
*
*  FUNCTION
*    registers event handlers
*
*  SEE ALSO
*     sge_thread_timer/sge_timer_start_periodic_tasks
*******************************************************************************/
void
sge_timer_register_event_handler() {
   DENTER(TOP_LAYER);

   /* 
    * recurring events 
    */

   te_register_event_handler(sge_store_job_number, TYPE_JOB_NUMBER_EVENT);

   te_register_event_handler(sge_store_ar_id, TYPE_AR_ID_EVENT);

   te_register_event_handler(sge_load_value_cleanup_handler, TYPE_LOAD_VALUE_CLEANUP_EVENT);

   te_register_event_handler(sge_zombie_job_cleanup_handler, TYPE_ZOMBIE_JOB_CLEANUP_EVENT);

   te_register_event_handler(sge_automatic_user_cleanup_handler, TYPE_AUTOMATIC_USER_CLEANUP_EVENT);

   te_register_event_handler(sge_security_event_handler, TYPE_SECURITY_EVENT);

   /* 
    * one time events
    */

   te_register_event_handler(sge_job_resend_event_handler, TYPE_JOB_RESEND_EVENT);

   te_register_event_handler(sge_job_enfoce_limit_handler, TYPE_ENFORCE_LIMIT_EVENT);

   te_register_event_handler(sge_calendar_event_handler, TYPE_CALENDAR_EVENT);

   te_register_event_handler(resend_signal_event, TYPE_SIGNAL_RESEND_EVENT);

   te_register_event_handler(reschedule_unknown_event, TYPE_RESCHEDULE_UNKNOWN_EVENT);

   te_register_event_handler(sge_ar_event_handler, TYPE_AR_EVENT);


   DRETURN_VOID;
}

/****** qmaster/sge_thread_timer/sge_timer_start_periodic_tasks() ************************
*  NAME
*     sge_timer_start_periodic_tasks() -- Start periodic qmaster tasks. 
*
*  SYNOPSIS
*     static void sge_timer_start_periodic_tasks(void) 
*
*  FUNCTION
*     Start periodic qmaster tasks. Periodic tasks are implemented as recurring
*     events. 
*
*  INPUTS
*     void - none 
*
*  RESULT
*     void - none 
*
*  NOTES
*******************************************************************************/
void sge_timer_start_periodic_tasks() {
   te_event_t ev = nullptr;

   DENTER(TOP_LAYER);

   /* recurring events */
   ev = te_new_event(sge_gmt32_to_gmt64(15), TYPE_JOB_NUMBER_EVENT, RECURRING_EVENT, 0, 0, "job_number_changed");
   te_add_event(ev);
   te_free_event(&ev);

   ev = te_new_event(sge_gmt32_to_gmt64(15), TYPE_AR_ID_EVENT, RECURRING_EVENT, 0, 0, "ar_id_changed");
   te_add_event(ev);
   te_free_event(&ev);

   ev = te_new_event(sge_gmt32_to_gmt64(15), TYPE_LOAD_VALUE_CLEANUP_EVENT, RECURRING_EVENT, 0, 0, "load-value-cleanup");
   te_add_event(ev);
   te_free_event(&ev);

   ev = te_new_event(sge_gmt32_to_gmt64(30), TYPE_ZOMBIE_JOB_CLEANUP_EVENT, RECURRING_EVENT, 0, 0, "zombie-job-cleanup");
   te_add_event(ev);
   te_free_event(&ev);

   ev = te_new_event(sge_gmt32_to_gmt64(60), TYPE_AUTOMATIC_USER_CLEANUP_EVENT, RECURRING_EVENT, 0, 0, "automatic-user-cleanup");
   te_add_event(ev);
   te_free_event(&ev);

   ev = te_new_event(sge_gmt32_to_gmt64(10), TYPE_SECURITY_EVENT, RECURRING_EVENT, 0, 0, "security-event");
   te_add_event(ev);
   te_free_event(&ev);

   DRETURN_VOID;
}

void
sge_timer_initialize(monitoring_t *monitor) {
   cl_thread_settings_t *dummy_thread_p = nullptr;
   lList *answer_list = nullptr;

   DENTER(TOP_LAYER);

   te_init();
   DPRINTF("timed event module has been initialized\n");
   heartbeat_initialize();
   DPRINTF("heartbeat module initialized\n");
   ar_initialize_timer(&answer_list, monitor);
   answer_list_output(&answer_list);
   DPRINTF("ar and corresponding timers are initialized\n");
   calendar_initalize_timer(monitor);
   DPRINTF("queue states and corresponding timers are initialized due to calendar settings\n");
   host_initalitze_timer();
   DPRINTF("reschedule unknown timer have been initialized\n");
   sge_timer_register_event_handler();
   DPRINTF("timer are registered at timed event module\n");
   sge_timer_start_periodic_tasks();
   DPRINTF("periodic tasks are registered at timed event module\n");
   sge_initialize_persistance_timer();
   DPRINTF("persistence timer initialized at timed event module\n");
   sge_setup_job_resend();
   DPRINTF("job resend functionality initialized\n");
   sge_add_check_limit_trigger();
   DPRINTF("added timer event to check load reports and possibly to enforce limits\n");

   DPRINTF(SFN " related initialisation has been done\n", threadnames[TIMER_THREAD]);

   cl_thread_list_setup(&(Main_Control.timer_thread_pool), "timer thread pool");
   cl_thread_list_create_thread(Main_Control.timer_thread_pool, &dummy_thread_p, cl_com_get_log_list(),
                                threadnames[TIMER_THREAD], 0, sge_timer_main, nullptr, nullptr, CL_TT_TIMER);
   DRETURN_VOID;
}

void
sge_timer_terminate() {
   DENTER(TOP_LAYER);

   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.timer_thread_pool);
   while (thread != nullptr) {
      DPRINTF("getting canceled\n");
      cl_thread_list_delete_thread(Main_Control.timer_thread_pool, thread);

      thread = cl_thread_list_get_first_thread(Main_Control.timer_thread_pool);
   }
   DPRINTF("all " SFN " threads terminated\n", threadnames[TIMER_THREAD]);

   te_shutdown();

   DPRINTF(SFN " related cleanup has been done\n", threadnames[TIMER_THREAD]);

   DRETURN_VOID;
}

/****** qmaster/sge_qmaster_timed_event/timed_event_thread() ***********************
*  NAME
*     timed_event_thread() -- Deliver timed events due
*
*  SYNOPSIS
*     static void* timed_event_thread(void* anArg) 
*
*  FUNCTION
*     Check whether system clock has been put back. If so, adjust event due
*     times. Check if event list does contain events. If so, fetch first event
*     and check whether it is due. If there is a due event, search event handler
*     table for a matching event handler and invoke it.
*
*     After event delivery an event with event mode 'ONE_TIME_EVENT' will be
*     removed. An event with event mode 'RECURRING_EVENT' will be delivered
*     repeatedly.
*
*     The event list MUST be sorted in ascending event due time order.
*
*  INPUTS
*     void* anArg - not used 
*
*  RESULT
*     void* - none 
*
*  NOTES
*     MT-NOTE: 'timed_event_thread()' is a thread function. Do NOT use this
*     MT-NOTE: function in any other way!
*     MT-NOTE:
*     MT-NOTE: If the event list is empty, 'timed_event_thread()' will wait until
*     MT-NOTE: an event has been added.
*     MT-NOTE: 
*     MT-NOTE: If no event is due, i.e. the due date of the next event does lie
*     MT-NOTE: ahead, 'timed_event_thread()' does wait until the next event does
*     MT-NOTE: become due, or an event which is due earlier has been added. If
*     MT-NOTE: an event has been deleted while waiting ('Event_Control.deleted'
*     MT-NOTE: equals 'true'), skip the current event and start over. The
*     MT-NOTE: deleted event maybe the event 'timed_event_thread()' has been
*     MT-NOTE: waiting for.
*     MT-NOTE:
*     MT-NOTE: Before 'te_scan_table_and_deliver()' is invoked,
*     MT-NOTE: 'Event_Control.mutex' MUST be unlocked. Otherwise, a deadlock
*     MT-NOTE: may occur due to recursive mutex locking.
*
*******************************************************************************/
[[noreturn]] void *
sge_timer_main(void *arg) {
   auto *thread_config = (cl_thread_settings_t *) arg;
   monitoring_t monitor;
   monitoring_t *p_monitor = &monitor;

   lListElem *le = nullptr;
   te_event_t te = nullptr;
   u_long64 now;
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
   sge_monitor_init(p_monitor, thread_config->thread_name, TET_EXT, TET_WARNING, TET_ERROR);
   sge_qmaster_thread_init(QMASTER, TIMER_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "TEvent Thread");
   conf_update_thread_profiling("TEvent Thread");

   while (true) {
      int execute = 0;
      thread_start_stop_profiling();

      sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

      te_check_time(sge_get_gmt64());

      Event_Control.last = sge_get_gmt64();

      MONITOR_IDLE_TIME(te_wait_empty(), p_monitor, mconf_get_monitor_time(),
                        mconf_is_monitor_message());
      MONITOR_MESSAGES(p_monitor);

      MONITOR_TET_COUNT(p_monitor);
      MONITOR_TET_EVENT(p_monitor, lGetNumberOfElem(Event_Control.list));

      le = lFirstRW(Event_Control.list);
      te = te_event_from_list_elem(le);
      now = Event_Control.next = sge_get_gmt64();

      if (te->when > now) {
         Event_Control.next = te->when;
         Event_Control.deleted = false;
         MONITOR_IDLE_TIME(te_wait_next(te, now), p_monitor, mconf_get_monitor_time(),
                           mconf_is_monitor_message());

         if ((Event_Control.next < te->when) || Event_Control.deleted) {
            DPRINTF("%s: event list changed - next:" sge_u64" --> start over\n", __func__, Event_Control.next);

            sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

            te_free_event(&te);
            sge_monitor_output(p_monitor);
            continue;
         }
      }

      MONITOR_TET_EXEC(p_monitor);

      lDechainElem(Event_Control.list, le);
      lFreeElem(&le);

      sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Event_Control.mutex);

      te_scan_table_and_deliver(te, p_monitor);
      te_free_event(&te);

      sge_monitor_output(p_monitor);
      thread_output_profiling("timed event thread profiling summary:\n",
                              &next_prof_output);

      /* pthread cancellation point */
      do {
         pthread_cleanup_push((void (*)(void *)) sge_timer_cleanup_monitor,
                              (void *) p_monitor);
            cl_thread_func_testcancel(thread_config);
         pthread_cleanup_pop(execute);
         if (sge_thread_has_shutdown_started()) {
            DPRINTF("waiting for termination\n");
            sleep(1);
         }
      } while (sge_thread_has_shutdown_started());
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before and after the call of cl_thread_func_testcancel()
}

