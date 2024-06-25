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
#include <cerrno>
#include <fcntl.h>

#include "comm/cl_commlib.h"

#include "uti/sge_bootstrap.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_os.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_time.h"

#include "evm/sge_event_master.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_schedd_conf.h"

#include "gdi/sge_gdi_packet.h"

#include "sched/sge_serf.h"
#include "sched/schedd_monitor.h"

#include "basis_types.h"
#include "sge_sched_job_category.h"
#include "sge_sched_order.h"
#include "sge_thread_main.h"
#include "sge_thread_scheduler.h"
#include "setup_qmaster.h"
#include "sge_sched_process_events.h"

#include "msg_common.h"
#include "msg_qmaster.h"

#define SCHEDULER_TIMEOUT_S 10
#define SCHEDULER_TIMEOUT_N 0

static char schedule_log_path[SGE_PATH_MAX + 1] = "";
static const char *schedule_log_file = "schedule";
static int SGE_TEST_DELAY_SCHEDULING = 0;

master_scheduler_class_t Master_Scheduler = {
        PTHREAD_MUTEX_INITIALIZER,
        false,
        0,
        true
};


static void
schedd_set_serf_log_file() {
   const char *cell_root = bootstrap_get_cell_root();

   DENTER(TOP_LAYER);

   if (!*schedule_log_path) {
      snprintf(schedule_log_path, sizeof(schedule_log_path), "%s/%s/%s", cell_root, "common", schedule_log_file);
      DPRINTF("schedule log path >>%s<<\n", schedule_log_path);
   }

   DRETURN_VOID;
}

/* MT-NOTE: schedd_serf_record_func() is not MT safe */
static void
schedd_serf_record_func(u_long32 job_id, u_long32 ja_taskid, const char *state, u_long64 start_time, u_long64 end_time,
                        char level_char, const char *object_name, const char *name, double utilization) {
   FILE *fp;

   DENTER(TOP_LAYER);

   if (!(fp = fopen(schedule_log_path, "a"))) {
      DRETURN_VOID;
   }

   /* a new record */
   fprintf(fp, sge_U32CFormat":" sge_U32CFormat ":%s:" sge_u64 ":" sge_u64 ":%c:%s:%s:%f\n", sge_u32c(job_id),
           sge_u32c(ja_taskid), state, start_time, end_time - start_time, level_char, object_name,
           name, utilization);
   FCLOSE(fp);

   DRETURN_VOID;
FCLOSE_ERROR:
   DPRINTF(MSG_FILE_ERRORCLOSEINGXY_SS, schedule_log_path, strerror(errno));
   DRETURN_VOID;
}

/* MT-NOTE: schedd_serf_newline() is not MT safe */
static void
schedd_serf_newline() {
   FILE *fp;

   DENTER(TOP_LAYER);
   fp = fopen(schedule_log_path, "a");
   if (fp) {
      /* well, some kind of new line indicating a new schedule run */
      fprintf(fp, "::::::::\n");
      FCLOSE(fp);
   }
   DRETURN_VOID;
   FCLOSE_ERROR:
   DPRINTF(MSG_FILE_ERRORCLOSEINGXY_SS, schedule_log_path, strerror(errno));
   DRETURN_VOID;
}


static void
sge_scheduler_cleanup_monitor(void *arg) {
   DENTER(TOP_LAYER);
   auto monitor = static_cast<monitoring_t *>(arg);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

static void
sge_scheduler_cleanup_event_client(void *arg) {
   DENTER(TOP_LAYER);
   auto *evc = static_cast<sge_evc_class_t *>(arg);
   sge_mirror_shutdown(evc);
   DRETURN_VOID;
}

static void sge_scheduler_wait_for_event(sge_evc_class_t *evc, lList **event_list) {
   int wait_ret;
   bool do_ack = false;

   DENTER(TOP_LAYER);


   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);

   if (!Scheduler_Control.triggered) {
      struct timespec ts{};
      time_t current_time = time(nullptr);
      ts.tv_sec = current_time + SCHEDULER_TIMEOUT_S;
      ts.tv_nsec = SCHEDULER_TIMEOUT_N;

      wait_ret = pthread_cond_timedwait(&Scheduler_Control.cond_var, &Scheduler_Control.mutex, &ts);

      /*
       * if pthread_cond_timedwait returns 0, we were triggered by event master
       * otherwise we ran into a timeout or an error
       */
      if (wait_ret != 0) {
         DPRINTF("pthread_cond_timedwait for events failed %d\n", wait_ret);
      }
   }

   if (Scheduler_Control.triggered) {
      *event_list = Scheduler_Control.new_events;
      Scheduler_Control.new_events = nullptr;
      Scheduler_Control.triggered = false;
      do_ack = true;
   }

   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);

   if (do_ack) {
      if (lGetElemUlong(*event_list, ET_type, sgeE_ACK_TIMEOUT) != nullptr) {
         evc->ec_mark4registration(evc);
      }
      evc->ec_ack(evc);
   }

   DRETURN_VOID;
}

/****** qmaster/threads/sge_scheduler_initialize() ***************************
*  NAME
*     sge_scheduler_initialize() -- setup and start the scheduler thread 
*
*  SYNOPSIS
*     void sge_scheduler_initialize(sge_gdi_ctx_class_t *ctx) 
*
*  FUNCTION
*     A call to this function initializes the scheduler thread if it is
*     not already running. 
*
*     The first call to this function (during qmaster start) starts
*     the scheduler thread only if it is enabled in the bootstrap file.
*     Otherwise the scheduler will not be started.
*
*     Each subsequent call (triggered from GDI) will definitely start
*     the scheduler thread if it is not running.
*
*     Main routine for the created thread is sge_scheduler_main().
*
*     'Master_Scheduler' is accessed by this function.
*     
*  INPUTS
*     sge_gdi_ctx_class_t *ctx - context object 
*     lList **answer_list      - answer list
*
*  RESULT
*     void - None
*
*  NOTES
*     MT-NOTE: sge_scheduler_initialize() is MT safe 
*
*  SEE ALSO
*     qmaster/threads/sge_scheduler_initialize() 
*     qmaster/threads/sge_scheduler_cleanup_thread() 
*     qmaster/threads/sge_scheduler_terminate() 
*     qmaster/threads/sge_scheduler_main() 
*******************************************************************************/
void
sge_scheduler_initialize(lList **answer_list) {
   DENTER(TOP_LAYER);

   /* initialize debugging instrumentation */
   {
      char *debug = getenv("SGE_TEST_DELAY_SCHEDULING");
      if (debug != nullptr) {
         SGE_TEST_DELAY_SCHEDULING = atoi(debug);
      }
   }

   sge_mutex_lock("master scheduler struct", __func__, __LINE__, &(Master_Scheduler.mutex));

   if (!Master_Scheduler.is_running) {
      bool start_thread = true;

      /* 
       * when this function is called the first time we will use the setting from
       * the bootstrap file to identify if the scheduler should be started or not
       * otherwise we have to start the thread due to a manual request through GDI.
       * There is no option. We have to start it.
       */
      if (Master_Scheduler.use_bootstrap) {
         start_thread = ((bootstrap_get_scheduler_thread_count() > 0) ? true : false);
         Master_Scheduler.use_bootstrap = false;
      }

      if (start_thread) {
         cl_thread_settings_t *dummy_thread_p = nullptr;

         /*
          * initialize the thread pool
          */
         cl_thread_list_setup(&(Main_Control.scheduler_thread_pool), "thread pool");

         /*
          * start the scheduler
          */
         cl_thread_list_create_thread(Main_Control.scheduler_thread_pool, &dummy_thread_p,
                                      cl_com_get_log_list(), threadnames[SCHEDD_THREAD],
                                      Master_Scheduler.thread_id, sge_scheduler_main, nullptr, nullptr, CL_TT_SCHEDULER);

         /*
          * Increase the thread id so that the next instance of a scheduler will have a 
          * different name and flag that scheduler is running
          */
         Master_Scheduler.thread_id++;
         Master_Scheduler.is_running = true;

         INFO(MSG_THREAD_XHASSTARTED_S, threadnames[SCHEDD_THREAD]);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_INFO);
      } else {
         INFO(MSG_THREAD_XSTARTDISABLED_S, threadnames[SCHEDD_THREAD]);
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_INFO);
      }
   } else {
      ERROR(MSG_THREAD_XISRUNNING_S, threadnames[SCHEDD_THREAD]);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   }
   sge_mutex_unlock("master scheduler struct", __func__, __LINE__, &(Master_Scheduler.mutex));
   DRETURN_VOID;
}

/****** qmaster/threads/sge_scheduler_cleanup_thread() ********************
*  NAME
*     sge_scheduler_cleanup_thread() -- cleanup the scheduler thread 
*
*  SYNOPSIS
*     void sge_scheduler_cleanup_thread() 
*
*  FUNCTION
*     Cleanup the scheduler thread. 
*
*     This function has to be executed only by the scheduler thread.
*     Ideally it should be the last function executed when the
*     pthread cancellation point is passed.
*
*     'Master_Scheduler' is accessed by this function.
*     
*  INPUTS
*     void - None 
*
*  RESULT
*     void - none
*
*  NOTES
*     MT-NOTE: sge_scheduler_cleanup_thread() is MT safe 
*
*  SEE ALSO
*     qmaster/threads/sge_scheduler_initialize() 
*     qmaster/threads/sge_scheduler_cleanup_thread() 
*     qmaster/threads/sge_scheduler_terminate() 
*     qmaster/threads/sge_scheduler_main() 
*******************************************************************************/
void
sge_scheduler_cleanup_thread([[maybe_unused]] void *arg) {
   DENTER(TOP_LAYER);

   sge_mutex_lock("master scheduler struct", __func__, __LINE__, &(Master_Scheduler.mutex));

   if (Master_Scheduler.is_running) {
      /*
       * The scheduler thread itself executes this function (sge_scheduler_cleanup_thread())
       * at the cancellation point as part of the cleanup.
       * Therefore, it has to unset the thread config before the
       * cl_thread is deleted. Otherwise, we might run into a race condition when logging
       * is used after the call of cl_thread_list_delete_thread_without_join() 
       */
      cl_thread_unset_thread_config();

      /*
       * Delete the scheduler thread but don't wait for termination
       */
      cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.scheduler_thread_pool);
      cl_thread_list_delete_thread_without_join(Main_Control.scheduler_thread_pool, thread);

      /* 
       * Trash the thread pool 
       */
      cl_thread_list_cleanup(&Main_Control.scheduler_thread_pool);

      /*
       * now a new scheduler can start
       */
      Master_Scheduler.is_running = false;
   }

   sge_mutex_unlock("master scheduler struct", __func__, __LINE__, &(Master_Scheduler.mutex));

   DRETURN_VOID;
}

/****** qmaster/threads/sge_scheduler_terminate() ****************************
*  NAME
*     sge_scheduler_terminate() -- terminate the scheduler 
*
*  SYNOPSIS
*     void sge_scheduler_terminate(sge_gdi_ctx_class_t *ctx) 
*
*  FUNCTION
*     Terminates the scheduler if it was started previously. This
*     function will return only when it is sure that the pthread canceled. 
*
*     'Master_Scheduler' is accessed by this function.
*     
*  INPUTS
*     sge_gdi_ctx_class_t *ctx - context object 
*     lList **answer_list      - answer list
*
*  RESULT
*     void - None
*
*  NOTES
*     MT-NOTE: sge_scheduler_terminate() is MT safe 
*
*  SEE ALSO
*     qmaster/threads/sge_scheduler_initialize() 
*     qmaster/threads/sge_scheduler_cleanup_thread() 
*     qmaster/threads/sge_scheduler_terminate() 
*     qmaster/threads/sge_scheduler_main() 
*******************************************************************************/
void
sge_scheduler_terminate(lList **answer_list) {
   DENTER(TOP_LAYER);

   sge_mutex_lock("master scheduler struct", __func__, __LINE__, &(Master_Scheduler.mutex));

   if (Master_Scheduler.is_running) {
      cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.scheduler_thread_pool);
      pthread_t thread_id = *(thread->thread_pointer);

      pthread_cancel(thread_id);
      pthread_cond_signal(&Scheduler_Control.cond_var);

      /*
       * cl_thread deletion and cl_thread_pool deletion will be done at 
       * schedulers cancellation point in sge_scheduler_cleanup_thread() ...
       * ... therefore we have nothing more to do.
       */
      ;

      sge_mutex_unlock("master scheduler struct", __func__, __LINE__, &(Master_Scheduler.mutex));

      INFO(MSG_THREAD_XTERMINATED_S, threadnames[SCHEDD_THREAD]);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_INFO);
   } else {
      sge_mutex_unlock("master scheduler struct", __func__, __LINE__, &(Master_Scheduler.mutex));

      ERROR(MSG_THREAD_XNOTRUNNING_S, threadnames[SCHEDD_THREAD]);
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   }

   DRETURN_VOID;
}

/****** qmaster/threads/sge_scheduler_main() **********************************
*  NAME
*     sge_scheduler_main() -- main function of the scheduler thread 
*
*  SYNOPSIS
*     void * sge_scheduler_main(void *arg) 
*
*  FUNCTION
*     Main function of the scheduler thread, 
*
*  INPUTS
*     void *arg - pointer to the thread function (type cl_thread_settings_t*) 
*
*  RESULT
*     void * - always nullptr
*
*  NOTES
*     MT-NOTE: sge_scheduler_main() is MT safe 
*
*     MT-NOTE: this is a thread function. Do NOT use this function
*     MT-NOTE: in any other way!
*
*  SEE ALSO
*     qmaster/threads/sge_scheduler_initialize() 
*     qmaster/threads/sge_scheduler_cleanup_thread() 
*     qmaster/threads/sge_scheduler_terminate() 
*     qmaster/threads/sge_scheduler_main() 
*******************************************************************************/
[[noreturn]] void *
sge_scheduler_main(void *arg) {
   auto *thread_config = (cl_thread_settings_t *) arg;
   u_long64 next_prof_output = 0;
   monitoring_t monitor;
   sge_evc_class_t *evc = nullptr;
   lList *alp = nullptr;
   sge_where_what_t where_what;

   DENTER(TOP_LAYER);
   memset(&where_what, 0, sizeof(where_what));

   // set thread name and id used by logging an others
   const char *thread_name = thread_config->thread_name;
   int thread_id = thread_config->thread_id;
   component_set_thread_name(thread_name);
   component_set_thread_id(thread_id);
   DPRINTF(SFN "(%d) started\n", thread_name, thread_id);

   /* initialize commlib thread */
   cl_thread_func_startup(thread_config);

   /* initialize monitoring */
   sge_monitor_init(&monitor, thread_config->thread_name, SCH_EXT, SCT_WARNING, SCT_ERROR);
   sge_qmaster_thread_init(SCHEDD, SCHEDD_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "Scheduler Thread");
   conf_update_thread_profiling("Scheduler Thread");
   DPRINTF(SFN " started\n", thread_config->thread_name);

   /* initialize schedd_runlog logging */
   schedd_set_schedd_log_file();

   /* set profiling parameters */
   prof_set_level_name(SGE_PROF_EVENTMASTER, nullptr, nullptr);
   prof_set_level_name(SGE_PROF_SPOOLING, nullptr, nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM0, "scheduler", nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM1, "pending ticket calculation", nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM3, "job sorting", nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM4, "job dispatching", nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM5, "send orders", nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM6, "scheduler event loop", nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM7, "copy lists", nullptr);
   prof_set_level_name(SGE_PROF_SCHEDLIB4, nullptr, nullptr);

   /* set-up needed for 'schedule' file */
   serf_init(schedd_serf_record_func, schedd_serf_newline);
   schedd_set_serf_log_file();

   // this thread will use the SCHEDULER data store
   oge::DataStore::select_active_ds(oge::DataStore::Id::SCHEDULER);

   /*
    * prepare event client/mirror mechanism
    */
   sge_gdi2_evc_setup(&evc, EV_ID_SCHEDD, &alp, "scheduler");
   DPRINTF("prepared event client/mirror mechanism\n");

   /*
    * register as event mirror
    */
   sge_mirror_initialize(evc, &event_update_func, &sge_mod_event_client,
                         &sge_add_event_client, &sge_remove_event_client, &sge_handle_event_ack, nullptr);
   evc->ec_register(evc, false, nullptr);
   evc->ec_set_busy_handling(evc, EV_BUSY_UNTIL_RELEASED);
   DPRINTF("registered at event mirror\n");

   /*
    * subscribe necessary data
    */
   ensure_valid_what_and_where(&where_what);
   subscribe_scheduler(evc, &where_what);
   DPRINTF("subscribed necessary data from event master\n");

   /* 
    * schedulers main loop
    */
   while (true) {
      bool handled_events = false;
      lList *event_list = nullptr;
      int execute = 0;
      lList *orders = nullptr;

      bool do_start = sconf_get_profiling();
      prof_start_stop(SGE_PROF_OTHER, nullptr, do_start);
      prof_start_stop(SGE_PROF_PACKING, nullptr, do_start);
      prof_start_stop(SGE_PROF_EVENTCLIENT, nullptr, do_start);
      prof_start_stop(SGE_PROF_MIRROR, nullptr, do_start);
      prof_start_stop(SGE_PROF_GDI, nullptr, do_start);
      prof_start_stop(SGE_PROF_HT_RESIZE, nullptr, do_start);
      prof_start_stop(SGE_PROF_CUSTOM0, nullptr, do_start);
      prof_start_stop(SGE_PROF_CUSTOM1, nullptr, do_start);
      prof_start_stop(SGE_PROF_CUSTOM3, nullptr, do_start);
      prof_start_stop(SGE_PROF_CUSTOM4, nullptr, do_start);
      prof_start_stop(SGE_PROF_CUSTOM5, nullptr, do_start);
      prof_start_stop(SGE_PROF_CUSTOM6, nullptr, do_start);
      prof_start_stop(SGE_PROF_CUSTOM7, nullptr, do_start);
      prof_start_stop(SGE_PROF_SCHEDLIB4, nullptr, do_start);

      /*
       * Wait for new events
       */
      MONITOR_IDLE_TIME(sge_scheduler_wait_for_event(evc, &event_list), (&monitor), mconf_get_monitor_time(),
                        mconf_is_monitor_message());

      /* If we lost connection we have to register again */
      if (evc->ec_need_new_registration(evc)) {
         lFreeList(&event_list);
         if (evc->ec_register(evc, false, nullptr)) {
            DPRINTF("re-registered at event master!\n");
         }
      }

      if (event_list != nullptr) {
         /* check for shutdown */
         bool do_shutdown = (lGetElemUlong(event_list, ET_type, sgeE_SHUTDOWN) != nullptr);

         /* update mirror and free data */
         if (!do_shutdown && sge_mirror_process_event_list(evc, event_list) == SGE_EM_OK) {
            handled_events = true;
            DPRINTF("events handled\n");
         } else {
            DPRINTF("events contain shutdown event - ignoring events\n");
         }
         lFreeList(&event_list);
      }

      /* if we actually got events, start the scheduling run and further event processing */
      if (handled_events) {
         lList *answer_list = nullptr;
         scheduler_all_data_t copy;
         const lList *master_cqueue_list = *oge::DataStore::get_master_list(SGE_TYPE_CQUEUE);
         const lList *master_job_list = *oge::DataStore::get_master_list(SGE_TYPE_JOB);
         const lList *master_userset_list = *oge::DataStore::get_master_list(SGE_TYPE_USERSET);
         const lList *master_project_list = *oge::DataStore::get_master_list(SGE_TYPE_PROJECT);
         const lList *master_exechost_list = *oge::DataStore::get_master_list(SGE_TYPE_EXECHOST);
         const lList *master_rqs_list = *oge::DataStore::get_master_list(SGE_TYPE_RQS);
         const lList *master_centry_list = *oge::DataStore::get_master_list(SGE_TYPE_CENTRY);
         const lList *master_ckpt_list = *oge::DataStore::get_master_list(SGE_TYPE_CKPT);
         const lList *master_user_list = *oge::DataStore::get_master_list(SGE_TYPE_USER);
         const lList *master_ar_list = *oge::DataStore::get_master_list(SGE_TYPE_AR);
         const lList *master_pe_list = *oge::DataStore::get_master_list(SGE_TYPE_PE);
         const lList *master_hgrp_list = *oge::DataStore::get_master_list(SGE_TYPE_HGROUP);
         const lList *master_sharetree_list = *oge::DataStore::get_master_list(SGE_TYPE_SHARETREE);

         /* delay scheduling for test purposes, see issue GE-3306 */
         if (SGE_TEST_DELAY_SCHEDULING > 0) {
            sleep(SGE_TEST_DELAY_SCHEDULING);
         }

         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM6);
         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM7);

         if (DPRINTF_IS_ACTIVE) {
            dstring ds;
            char buffer[128];

            sge_dstring_init(&ds, buffer, sizeof(buffer));
            DPRINTF("================[SCHEDULING-EPOCH %s]==================\n", sge_ctime64(0, &ds));
            sge_dstring_free(&ds);
         }

         /*
          * If there were new events then
          * copy/filter data necessary for the scheduler run
          * and run the scheduler method
          */
         memset(&copy, 0, sizeof(copy));

         copy.dept_list = lSelect("", master_userset_list, where_what.where_dept, where_what.what_acldept);
         copy.acl_list = lSelect("", master_userset_list, where_what.where_acl, where_what.what_acldept);

         DPRINTF("RAW CQ:%d, J:%d, H:%d, C:%d, A:%d, D:%d, P:%d, CKPT:%d,"
                  " US:%d, PR:%d, RQS:%d, AR:%d, S:nd:%d/lf:%d\n",
                 lGetNumberOfElem(master_cqueue_list),
                 lGetNumberOfElem(master_job_list),
                 lGetNumberOfElem(master_exechost_list),
                 lGetNumberOfElem(master_centry_list),
                 lGetNumberOfElem(copy.acl_list),
                 lGetNumberOfElem(copy.dept_list),
                 lGetNumberOfElem(master_project_list),
                 lGetNumberOfElem(master_ckpt_list),
                 lGetNumberOfElem(master_user_list),
                 lGetNumberOfElem(master_project_list),
                 lGetNumberOfElem(master_rqs_list),
                 lGetNumberOfElem(master_ar_list),
                 lGetNumberOfNodes(nullptr, master_sharetree_list, STN_children),
                 lGetNumberOfLeafs(nullptr, master_sharetree_list, STN_children)
                 );

         sge_rebuild_job_category(master_job_list, master_userset_list,
                                  master_project_list, master_rqs_list);

         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM7);
         double prof_init = prof_get_measurement_wallclock(SGE_PROF_CUSTOM7, true, nullptr);
         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM7);

         sge_before_dispatch(evc);

         /* prepare data for the scheduler itself */
         copy.host_list = lCopyList("", master_exechost_list);

         /*
          * Within the scheduler we do only need QIs
          */
         {
            const lListElem *cqueue;
            lEnumeration *what_queue3 = nullptr;

            for_each_ep(cqueue, master_cqueue_list) {
               const lList *qinstance_list = lGetList(cqueue, CQ_qinstances);
               lList *t;

               if (!qinstance_list) {
                  continue;
               }

               /* all_queue_list contains all queue instances with state and full queue name only */
               if (!what_queue3) {
                  what_queue3 = lWhat("%T(%I%I)", lGetListDescr(qinstance_list), QU_full_name, QU_state);
               }
               t = lSelect("t", qinstance_list, nullptr, what_queue3);
               if (t) {
                  if (copy.all_queue_list == nullptr) {
                     copy.all_queue_list = lCreateList("all", lGetListDescr(t));
                  }
                  lAppendList(copy.all_queue_list, t);
                  lFreeList(&t);
               }

               t = lSelect("t", qinstance_list, where_what.where_queue, where_what.what_queue2);
               if (t) {
                  if (copy.queue_list == nullptr) {
                     copy.queue_list = lCreateList("enabled", lGetListDescr(t));
                  }
                  lAppendList(copy.queue_list, t);
                  lFreeList(&t);
               }

               t = lSelect("t", qinstance_list, where_what.where_queue2, where_what.what_queue2);
               if (t) {
                  if (copy.dis_queue_list == nullptr) {
                     copy.dis_queue_list = lCreateList("disabled", lGetListDescr(t));
                  }
                  lAppendList(copy.dis_queue_list, t);
                  lFreeList(&t);
               }
            }
            if (what_queue3) {
               lFreeWhat(&what_queue3);
            }
         }

         if (sconf_is_job_category_filtering()) {
            copy.job_list = sge_category_job_copy(copy.queue_list, &orders, evc->monitor_next_run);
         } else {
            copy.job_list = lCopyList("", master_job_list);
         }

         /* no need to copy these lists, they are read only used */
         copy.centry_list = master_centry_list;
         copy.ckpt_list = master_ckpt_list;
         copy.hgrp_list = master_hgrp_list;

         /* these lists need to be copied because they are modified during scheduling run */
         copy.share_tree = lCopyList("", master_sharetree_list);
         copy.pe_list = lCopyList("", master_pe_list);
         copy.user_list = lCopyList("", master_user_list);
         copy.project_list = lCopyList("", master_project_list);
         copy.rqs_list = lCopyList("", master_rqs_list);
         copy.ar_list = lCopyList("", master_ar_list);

         /* report number of reduced and raw (in brackets) lists */
         DPRINTF("Q:" sge_uu32 ", AQ:" sge_uu32 " J:" sge_uu32 "(" sge_uu32 "), H:" sge_uu32 "(" sge_uu32 "), C:" sge_uu32
                  ", A:" sge_uu32 ", D:" sge_uu32 ", P:" sge_uu32 ", CKPT:" sge_uu32 ", US:" sge_uu32 ", PR:" sge_uu32
                  ", RQS:" sge_uu32 ", AR:" sge_uu32 ", S:nd:%d/lf:%d\n",
                 lGetNumberOfElem(copy.queue_list),
                 lGetNumberOfElem(copy.all_queue_list),
                 lGetNumberOfElem(copy.job_list),
                 lGetNumberOfElem(master_job_list),
                 lGetNumberOfElem(copy.host_list),
                 lGetNumberOfElem(master_exechost_list),
                 lGetNumberOfElem(copy.centry_list),
                 lGetNumberOfElem(copy.acl_list),
                 lGetNumberOfElem(copy.dept_list),
                 lGetNumberOfElem(copy.pe_list),
                 lGetNumberOfElem(copy.ckpt_list),
                 lGetNumberOfElem(copy.user_list),
                 lGetNumberOfElem(copy.project_list),
                 lGetNumberOfElem(copy.rqs_list),
                 lGetNumberOfElem(copy.ar_list),
                 lGetNumberOfNodes(nullptr, copy.share_tree, STN_children),
                 lGetNumberOfLeafs(nullptr, copy.share_tree, STN_children)
                 );

         if (getenv("SGE_ND")) {
            printf("Q:" sge_uu32 ", AQ:" sge_uu32 " J:" sge_uu32 "(" sge_uu32 "), H:" sge_uu32 "(" sge_uu32 "), C:" sge_uu32
                   ", A:" sge_uu32 ", D:" sge_uu32 ", P:" sge_uu32 ", CKPT:" sge_uu32 ", US:" sge_uu32 ", PR:" sge_uu32
                   ", RQS:" sge_uu32 ", AR:" sge_uu32 ", S:nd:%d/lf:%d\n",
                   lGetNumberOfElem(copy.queue_list),
                   lGetNumberOfElem(copy.all_queue_list),
                   lGetNumberOfElem(copy.job_list),
                   lGetNumberOfElem(master_job_list),
                   lGetNumberOfElem(copy.host_list),
                   lGetNumberOfElem(master_exechost_list),
                   lGetNumberOfElem(copy.centry_list),
                   lGetNumberOfElem(copy.acl_list),
                   lGetNumberOfElem(copy.dept_list),
                   lGetNumberOfElem(copy.pe_list),
                   lGetNumberOfElem(copy.ckpt_list),
                   lGetNumberOfElem(copy.user_list),
                   lGetNumberOfElem(copy.project_list),
                   lGetNumberOfElem(copy.rqs_list),
                   lGetNumberOfElem(copy.ar_list),
                   lGetNumberOfNodes(nullptr, copy.share_tree, STN_children),
                   lGetNumberOfLeafs(nullptr, copy.share_tree, STN_children)
            );
         } else {
            schedd_log("-------------START-SCHEDULER-RUN-------------", nullptr, evc->monitor_next_run);
         }

         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM7);
         double prof_copy = prof_get_measurement_wallclock(SGE_PROF_CUSTOM7, true, nullptr);
         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM7);

         scheduler_method(evc, &answer_list, &copy, &orders);
         answer_list_output(&answer_list);

         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM7);
         double prof_run = prof_get_measurement_wallclock(SGE_PROF_CUSTOM7, true, nullptr);
         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM7);

         /* ... which gets deleted after using */
         lFreeList(&(copy.host_list));
         lFreeList(&(copy.queue_list));
         lFreeList(&(copy.dis_queue_list));
         lFreeList(&(copy.all_queue_list));
         lFreeList(&(copy.job_list));
         lFreeList(&(copy.acl_list));
         lFreeList(&(copy.dept_list));
         lFreeList(&(copy.pe_list));
         lFreeList(&(copy.share_tree));
         lFreeList(&(copy.user_list));
         lFreeList(&(copy.project_list));
         lFreeList(&(copy.rqs_list));
         lFreeList(&(copy.ar_list));

         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM7);
         double prof_free = prof_get_measurement_wallclock(SGE_PROF_CUSTOM7, true, nullptr);

         /*
          * need to sync with event master thread
          * if schedd configuration changed then settings in evm can be adjusted
          */
         if (sconf_is_new_config()) {
            /* set scheduler interval / event delivery interval */
            u_long32 interval = sconf_get_schedule_interval();
            if ((u_long32) evc->ec_get_edtime(evc) != interval) {
               evc->ec_set_edtime(evc, interval);
            }

            /* set job / ja_task event flushing */
            set_job_flushing(evc);

            /* no need to ec_commit here - we do it when resetting the busy state */

            /* now we handled the new schedd config - no need to do it twice */
            sconf_reset_new_config();
         }

         /* block till master handled all GDI orders */
         sge_schedd_block_until_orders_processed(nullptr);
         schedd_order_destroy();

         /*
          * Stop profiling for "schedd run total" and the subcategories
          */
         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM6);
         double prof_total = prof_get_measurement_wallclock(SGE_PROF_CUSTOM6, true, nullptr);

         if (prof_is_active(SGE_PROF_CUSTOM6)) {
            PROFILING("PROF: schedd run took: %.3f s (init: %.3f s, copy: %.3f s, "
                                  "run:%.3f, free: %.3f s, jobs: " sge_uu32 ", categories: %d/%d)",
                    prof_total, prof_init, prof_copy, prof_run, prof_free,
                    lGetNumberOfElem(*oge::DataStore::get_master_list(SGE_TYPE_JOB)), sge_category_count(),
                    sge_cs_category_count());
         }
         if (getenv("SGE_ND") != nullptr) {
            printf("--------------STOP-SCHEDULER-RUN-------------\n");
         } else {
            schedd_log("--------------STOP-SCHEDULER-RUN-------------", nullptr, evc->monitor_next_run);
         }

         thread_output_profiling("scheduler thread profiling summary:\n", &next_prof_output);

         sge_monitor_output(&monitor);
      }

      /* reset the busy state */
      evc->ec_set_busy(evc, 0);
      evc->ec_commit(evc, nullptr);

      /* stop logging into schedd_runlog (enabled via -tsm) */
      evc->monitor_next_run = false;

      /*
       * pthread cancellation point
       *
       * sge_scheduler_cleanup_thread() is the last function which should be called, so it is pushed first
       */
      pthread_cleanup_push(sge_scheduler_cleanup_thread, nullptr);
      pthread_cleanup_push(sge_scheduler_cleanup_monitor, static_cast<void *>(&monitor));
      pthread_cleanup_push(sge_scheduler_cleanup_event_client, static_cast<void *>(evc));
      cl_thread_func_testcancel(thread_config);
      pthread_cleanup_pop(execute);
      pthread_cleanup_pop(execute);
      pthread_cleanup_pop(execute);
      DPRINTF("passed cancellation point\n");
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before the call of cl_thread_func_testcancel()
}

