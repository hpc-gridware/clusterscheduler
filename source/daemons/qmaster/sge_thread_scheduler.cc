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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Starting and stopping the scheduler thread while qmaster runs
 */

#include <cstdint>
#include <cstdlib>

#include <pthread.h>
#include <cstring>
#include <cerrno>
#include <fcntl.h>

#include "comm/cl_commlib.h"

#include "uti/ocs_DebugParam.h"
#include "uti/ocs_cond.h"
#include "uti/ocs_gperf.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_os.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_time.h"

#include "evm/sge_event_master.h"

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_schedd_conf.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Select.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_CT.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_ET.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_UU.h"

#include "gdi/ocs_gdi_Packet.h"

#include "sched/sge_serf.h"
#include "sched/schedd_monitor.h"

#include <cinttypes>
#include "ocs_CategoryQmaster.h"
#include "ocs_ReportingFileWriter.h"
#include "sge_sched_order.h"
#include "sge_thread_main.h"
#include "sge_thread_scheduler.h"
#include "setup_qmaster.h"
#include "sge_sched_process_events.h"
#include "sge.h"
#include "msg_common.h"
#include "msg_qmaster.h"
#include "ocs_Bootstrap.h"
#include "ocs_gperf.h"

/** @brief Seconds the scheduler thread waits for work before looking around
 *
 * A wake-up is normally driven by the event trigger; this bounds how long the
 * thread sleeps when nothing happens at all.
 */
#define SCHEDULER_TIMEOUT_S 10

static char schedule_log_path[SGE_PATH_MAX + 1] = "";
static const char *schedule_log_file = "schedule";
static int SGE_TEST_DELAY_SCHEDULING = 0;

/** @brief @copybrief master_scheduler_class_t */
master_scheduler_class_t Master_Scheduler = {
        PTHREAD_MUTEX_INITIALIZER,
        false,
        0,
        true
};


static void
schedd_set_serf_log_file() {
   DENTER(TOP_LAYER);

   const char *cell_root = bootstrap_get_cell_root();

   if (!*schedule_log_path) {
      snprintf(schedule_log_path, sizeof(schedule_log_path), "%s/%s/%s", cell_root, "common", schedule_log_file);
      DPRINTF("schedule log path >>%s<<\n", schedule_log_path);
   }

   DRETURN_VOID;
}

/* MT-NOTE: schedd_serf_record_func() is not MT safe */
static void
schedd_serf_record_func(uint32_t job_id, uint32_t ja_taskid, const char *state, uint64_t start_time, uint64_t end_time,
                        char level_char, const char *object_name, const char *name, double utilization) {
   DENTER(TOP_LAYER);

   FILE *fp;

   if (!(fp = fopen(schedule_log_path, "a"))) {
      DRETURN_VOID;
   }

   /* a new record */
   fprintf(fp, sge_u32":" sge_u32 ":%s:" sge_u64 ":" sge_u64 ":%c:%s:%s:%f\n", job_id,
           ja_taskid, state, start_time, end_time - start_time, level_char, object_name,
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
   DENTER(TOP_LAYER);

   FILE *fp;

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


/** @brief Release the monitoring state when the scheduler thread ends
 *
 * @param arg the thread argument
 */
static void
sge_scheduler_cleanup_monitor(void *arg) {
   DENTER(TOP_LAYER);
   auto monitor = static_cast<monitoring_t *>(arg);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

/** @brief Deregister and destroy the scheduler's event client when the thread ends
 *
 * @param arg the address of the scheduler's event client pointer (`sge_evc_class_t **`); nullptr afterwards
 */
static void
sge_scheduler_cleanup_event_client(void *arg) {
   DENTER(TOP_LAYER);
   auto **evc = static_cast<sge_evc_class_t **>(arg);
   sge_mirror_shutdown(evc);
   DRETURN_VOID;
}

/** @brief Release the scheduler's event subscription
 *
 * @param where_what the subscription to release
 */
static void
sge_scheduler_cleanup_where_what(void *where_what) {
   DENTER(TOP_LAYER);
   auto *where_what_p = static_cast<sge_where_what_t *>(where_what);
   free_what_and_where(where_what_p);
   DRETURN_VOID;
}

/** @brief Wait until the events for the next scheduling run have arrived
 *
 * @param evc the event client
 * @param event_list receives the events that arrived (`ET`); left as it is when the wait timed out
 */
static void
sge_scheduler_wait_for_event(sge_evc_class_t *evc, ocs::tom::ListPtr<ocs::tom::ET> &event_list) {
   DENTER(TOP_LAYER);

   int wait_ret;
   bool do_ack = false;


   sge_mutex_lock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);

   if (!Scheduler_Control.triggered) {
      wait_ret = ocs::uti::condition_timedwait(&Scheduler_Control.cond_var, &Scheduler_Control.mutex, SCHEDULER_TIMEOUT_S);

      /*
       * if pthread_cond_timedwait returns 0, we were triggered by event master
       * otherwise we ran into a timeout or an error
       */
      if (wait_ret != 0) {
         DPRINTF("pthread_cond_timedwait for events failed %d\n", wait_ret);
      }
   }

   if (Scheduler_Control.triggered) {
      // new_events is an ET list by construction (REP_list of an event report); no checked constructor here,
      // since it could throw with event_control_mutex locked
      event_list = ocs::tom::ListPtr<ocs::tom::ET>::from_typed(Scheduler_Control.new_events);
      Scheduler_Control.new_events = nullptr;
      Scheduler_Control.triggered = false;
      do_ack = true;
   }

   sge_mutex_unlock("event_control_mutex", __func__, __LINE__, &Scheduler_Control.mutex);

   if (do_ack) {
      if (event_list->find_first<ET_type>(sgeE_ACK_TIMEOUT)) {
         evc->ec_mark4registration(evc);
      }
      evc->ec_ack(evc);
   }

   DRETURN_VOID;
}

/**
 * @brief Setup and start the scheduler thread
 *
 * A call to this function initializes the scheduler thread if it is
 * not already running.
 * The first call to this function (during qmaster start) starts
 * the scheduler thread only if it is enabled in the bootstrap file.
 * Otherwise the scheduler will not be started.
 * Each subsequent call (triggered from GDI) will definitely start
 * the scheduler thread if it is not running.
 * Main routine for the created thread is sge_scheduler_main().
 * 'Master_Scheduler' is accessed by this function.
 *
 * @param answer_list answer list
 *
 * @note MT-NOTE: sge_scheduler_initialize() is MT safe
 *
 * @see #sge_scheduler_initialize, #sge_scheduler_cleanup_thread, #sge_scheduler_terminate, #sge_scheduler_main
 */
void
sge_scheduler_initialize(ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   // initialize the condition variable used in waiting for events triggering a scheduling run
   ocs::uti::condition_initialize(&Scheduler_Control.cond_var);

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
         start_thread = ocs::Bootstrap::get_scheduler_thread_count() > 0;
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
                                      cl_com_get_log_list(), to_cstr(SCHEDD_THREAD),
                                      Master_Scheduler.thread_id, sge_scheduler_main, nullptr, nullptr, CL_TT_SCHEDULER);

         /*
          * Increase the thread id so that the next instance of a scheduler will have a
          * different name and flag that scheduler is running
          */
         Master_Scheduler.thread_id++;
         Master_Scheduler.is_running = true;

         INFO(MSG_THREAD_XHASSTARTED_S, to_cstr(SCHEDD_THREAD));
         /* a success, so its status says so: qconf -at/-kt take their exit status
          * from it (CS-2979) */
         answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
      } else {
         INFO(MSG_THREAD_XSTARTDISABLED_S, to_cstr(SCHEDD_THREAD));
         answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_INFO);
      }
   } else {
      ERROR(MSG_THREAD_XISRUNNING_S, to_cstr(SCHEDD_THREAD));
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   }
   sge_mutex_unlock("master scheduler struct", __func__, __LINE__, &(Master_Scheduler.mutex));
   DRETURN_VOID;
}

/**
 * @brief Cleanup the scheduler thread
 *
 * Cleanup the scheduler thread.
 * This function has to be executed only by the scheduler thread.
 * Ideally it should be the last function executed when the
 * pthread cancellation point is passed.
 * 'Master_Scheduler' is accessed by this function.
 *
 * @note MT-NOTE: sge_scheduler_cleanup_thread() is MT safe
 *
 * @see #sge_scheduler_initialize, #sge_scheduler_cleanup_thread, #sge_scheduler_terminate, #sge_scheduler_main
 */
/** @brief Release what the scheduler thread allocated
 *
 * @param arg the thread argument
 */
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

/**
 * @brief Terminate the scheduler
 *
 * Terminates the scheduler if it was started previously. This
 * function will return only when it is sure that the pthread canceled.
 * 'Master_Scheduler' is accessed by this function.
 *
 * @param answer_list answer list
 *
 * @note MT-NOTE: sge_scheduler_terminate() is MT safe
 *
 * @see #sge_scheduler_initialize, #sge_scheduler_cleanup_thread, #sge_scheduler_terminate, #sge_scheduler_main
 */
void
sge_scheduler_terminate(ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
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

      pthread_join(thread_id, nullptr);

      INFO(MSG_THREAD_XTERMINATED_S, to_cstr(SCHEDD_THREAD));
      /* a success, so its status says so: qconf -at/-kt take their exit status
       * from it (CS-2979) */
      answer_list_add(answer_list, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   } else {
      sge_mutex_unlock("master scheduler struct", __func__, __LINE__, &(Master_Scheduler.mutex));

      ERROR(MSG_THREAD_XNOTRUNNING_S, to_cstr(SCHEDD_THREAD));
      answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   }

   DRETURN_VOID;
}

/** @brief Select from a list of the scheduler's mirror, with the results lSelect() gave
 *
 * The scheduler thread must not end in an exception, and its callers tell
 * "no list" from an empty one. So unlike ConstList<T>::select() this gives an
 * empty handle when nothing is selected, and an empty handle instead of an
 * exception when the selection fails (no memory, a field the source does not
 * hold) - as lSelect() did.
 *
 * @tparam T the object type
 * @param list the list of the mirror; an empty handle selects nothing
 * @param where the condition; an empty one selects every element
 * @param what the fields to copy
 * @param name the name of the result, or nullptr for the name of @p list
 * @return the selection with the reduced descriptor of @p what, an empty handle if nothing was selected or the
 *         selection failed
 */
template <ocs::tom::ObjectType T>
static ocs::tom::ListPtr<T>
ocs_scheduler_select(const ocs::tom::ConstList<T> list, const ocs::tom::Where<T> &where,
                     const ocs::tom::What<T> &what, const char *name = nullptr) {
   ocs::tom::ListPtr<T> selected;
   try {
      selected = list.select(where, what, name);
   } catch (const ocs::RuntimeException &) {
      // no memory: every allocation error of the list core is a ListCoreError, an ocs::RuntimeException
      return {};
   } catch (const ocs::tom::FieldException &) {
      return {};
   }
   if (selected && selected->empty()) {
      selected.reset();
   }
   return selected;
}

/** @brief Add the queue instances a condition selects to one of the scheduler's queue lists
 *
 * The first selection becomes the list, named @p name and with the reduced
 * descriptor of the selection; every further one is moved to its end.
 *
 * @param dst the queue list of the snapshot, created by the first selection
 * @param qinstance_list the queue instances of a cluster queue of the mirror
 * @param where the condition; an empty one selects every queue instance
 * @param what the fields to copy
 * @param name the name of the queue list
 */
static void
ocs_scheduler_append_selection(ocs::tom::ListPtr<ocs::tom::QU> &dst,
                               const ocs::tom::ConstList<ocs::tom::QU> qinstance_list,
                               const ocs::tom::Where<ocs::tom::QU> &where, const ocs::tom::What<ocs::tom::QU> &what,
                               const char *name) {
   auto selected = ocs_scheduler_select(qinstance_list, where, what, name);
   if (!selected) {
      return;
   }
   if (!dst) {
      dst = std::move(selected);
   } else {
      dst->splice(std::move(selected));
   }
}

/// The list length from which the scheduler thread hashes a string key field (see sge_scheduler_main())
static constexpr std::uint32_t SCHEDULER_STRING_HASH_MIN_ELEMS = 8;

/**
 * @brief Read a hash threshold of the scheduler thread from the environment
 * @param name the name of the environment variable
 * @param value receives the threshold if the variable holds a number
 * @return true if the variable is set to a number below UINT32_MAX
 */
static bool scheduler_hash_threshold_from_env(const char *name, std::uint32_t *value) {
   const char *env = getenv(name);
   if (env == nullptr || *env == '\0') {
      return false;
   }
   char *end = nullptr;
   const unsigned long number = strtoul(env, &end, 10);
   if (*end != '\0' || number >= UINT32_MAX) {
      return false;
   }
   *value = static_cast<std::uint32_t>(number);
   return true;
}

/**
 * @brief Main function of the scheduler thread
 *
 * Main function of the scheduler thread,
 *
 * @param arg pointer to the thread function (type cl_thread_settings_t*)
 *
 * @return always nullptr
 *
 * @note MT-NOTE: sge_scheduler_main() is MT safe
 *
 *       MT-NOTE: this is a thread function. Do NOT use this function
 *       MT-NOTE: in any other way!
 *
 * @see #sge_scheduler_initialize, #sge_scheduler_cleanup_thread, #sge_scheduler_terminate, #sge_scheduler_main
 */
[[noreturn]] void *
sge_scheduler_main(void *arg) {
   DENTER(TOP_LAYER);

   auto *thread_config = (cl_thread_settings_t *) arg;
   uint64_t next_prof_output = 0;
   monitoring_t monitor;
   sge_evc_class_t *evc = nullptr;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   sge_where_what_t where_what{};
#ifdef WITH_GPERF
   sge_gperf_per_thread_t gperf_data;
#endif

   // set thread name and id used by logging an others
   const char *thread_name = thread_config->thread_name;
   int thread_id = thread_config->thread_id;
   component_set_thread_name(thread_name);
   component_set_thread_id(thread_id);
   DPRINTF(SFN "(%d) started\n", thread_name, thread_id);

   /* initialize commlib thread */
   cl_thread_func_startup(thread_config);

   /* initialize monitoring */
   sge_monitor_init(&monitor, thread_config->thread_name, SCH_EXT, SCT_WARNING, SCT_ERROR,
                    ocs::ReportingFileWriter::create_monitoring_records);
   sge_qmaster_thread_init(SCHEDD, SCHEDD_THREAD, true);

   /* register at profiling module */
   set_thread_name(pthread_self(), "Scheduler Thread");
   conf_update_thread_profiling("Scheduler Thread");
   DPRINTF(SFN " started\n", thread_config->thread_name);

   /* initialize schedd_runlog logging */
   schedd_set_schedd_log_file();

   // The run copies of this thread hash short string-keyed lists from 8 elements on, apart from the other threads:
   // the scheduler looks up complex entries, queue and resource names in short lists over and over again (measured
   // with the list API module test, CS-3103). OCS_TOM_SCHED_HASH_MIN_ELEMS_STRING and _HOST override the string and
   // the host threshold of this thread, so that thresholds can be compared without rebuilding.
   ocs::tom::set_thread_string_hash_threshold(SCHEDULER_STRING_HASH_MIN_ELEMS);
   if (std::uint32_t value; scheduler_hash_threshold_from_env("OCS_TOM_SCHED_HASH_MIN_ELEMS_STRING", &value)) {
      ocs::tom::set_thread_string_hash_threshold(value);
   }
   if (std::uint32_t value; scheduler_hash_threshold_from_env("OCS_TOM_SCHED_HASH_MIN_ELEMS_HOST", &value)) {
      ocs::tom::set_thread_host_hash_threshold(value);
   }

#ifdef WITH_GPERF
   sge_gperf_per_thread_init(gperf_data);
#endif

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
   ocs::DataStore::select_active_ds(ocs::DataStore::Id::SCHEDULER);

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
      bool do_shutdown = false;
      bool handled_events = false;
      ocs::tom::ListPtr<ocs::tom::ET> event_list;
      int execute = 0;

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
      MONITOR_IDLE_TIME(sge_scheduler_wait_for_event(evc, event_list), (&monitor), mconf_get_monitoring_options());

      /* If we lost connection we have to register again */
      if (evc->ec_need_new_registration(evc)) {
         event_list.reset();
         if (evc->ec_register(evc, false, nullptr)) {
            DPRINTF("re-registered at event master!\n");
         }
      }

      if (event_list) {
         /* check for shutdown */
         do_shutdown = static_cast<bool>(event_list->find_first<ET_type>(sgeE_SHUTDOWN));

         /* update mirror and free data */
         if (!do_shutdown && sge_mirror_process_event_list(evc, *event_list) == SGE_EM_OK) {
            handled_events = true;
            DPRINTF("events handled\n");
         } else {
            DPRINTF("events contain shutdown event - ignoring events\n");
         }
         event_list.reset();
      }

      /* if we actually got events, start the scheduling run and further event processing */
      if (handled_events) {
         ocs::tom::ListPtr<ocs::tom::AN> answer_list;
         // the snapshot owns its copies; they are freed at the end of the run, see below
         scheduler_all_data_t copy{};
         // the mirror's lists; the cluster queue, job, host and pe lists hold the reduced elements of the subscription
         const auto master_cqueue_list = ocs::DataStore::master_list<SGE_TYPE_CQUEUE>();
         const auto master_job_list = ocs::DataStore::master_list<SGE_TYPE_JOB>();
         const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
         const auto master_project_list = ocs::DataStore::master_list<SGE_TYPE_PROJECT>();
         const auto master_exechost_list = ocs::DataStore::master_list<SGE_TYPE_EXECHOST>();
         const auto master_rqs_list = ocs::DataStore::master_list<SGE_TYPE_RQS>();
         const auto master_centry_list = ocs::DataStore::master_list<SGE_TYPE_CENTRY>();
         const auto master_ckpt_list = ocs::DataStore::master_list<SGE_TYPE_CKPT>();
         const auto master_user_list = ocs::DataStore::master_list<SGE_TYPE_USER>();
         const auto master_ar_list = ocs::DataStore::master_list<SGE_TYPE_AR>();
         const auto master_pe_list = ocs::DataStore::master_list<SGE_TYPE_PE>();
         const auto master_hgrp_list = ocs::DataStore::master_list<SGE_TYPE_HGROUP>();
         const auto master_sharetree_list = ocs::DataStore::master_list<SGE_TYPE_SHARETREE>();
         const auto master_config_list = ocs::DataStore::master_list<SGE_TYPE_CONFIG>();
         const auto master_category_list = ocs::DataStore::master_list<SGE_TYPE_CATEGORY>();

         /* delay scheduling for test purposes, see issue GE-3306 */
         if (SGE_TEST_DELAY_SCHEDULING > 0) {
            sleep(SGE_TEST_DELAY_SCHEDULING);
         }

         if (DPRINTF_IS_ACTIVE) {
            dstring ds;
            char buffer[128];

            sge_dstring_init(&ds, buffer, sizeof(buffer));
            DPRINTF("================[SCHEDULING-EPOCH %s]==================\n", sge_ctime64(0, &ds));
            sge_dstring_free(&ds);
         }

         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM6);
         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM7);

         /*
          * If there were new events then
          * copy/filter data necessary for the scheduler run
          * and run the scheduler method
          */
         // the selections keep the reduced descriptor of the subscription
         copy.dept_list = ocs_scheduler_select(master_userset_list, where_what.where_dept, where_what.what_acldept);
         copy.acl_list = ocs_scheduler_select(master_userset_list, where_what.where_acl, where_what.what_acldept);

         DPRINTF("RAW CQ:%d, J:%d, H:%d, C:%d, A:%d, D:%d, P:%d, CKPT:%d,"
                  " US:%d, PR:%d, RQS:%d, AR:%d, S:nd:%d/lf:%d\n",
                 static_cast<uint32_t>(master_cqueue_list.size()),
                 static_cast<uint32_t>(master_job_list.size()),
                 static_cast<uint32_t>(master_exechost_list.size()),
                 static_cast<uint32_t>(master_centry_list.size()),
                 static_cast<uint32_t>(copy.acl_list->size()),
                 static_cast<uint32_t>(copy.dept_list->size()),
                 static_cast<uint32_t>(master_project_list.size()),
                 static_cast<uint32_t>(master_ckpt_list.size()),
                 static_cast<uint32_t>(master_user_list.size()),
                 static_cast<uint32_t>(master_project_list.size()),
                 static_cast<uint32_t>(master_rqs_list.size()),
                 static_cast<uint32_t>(master_ar_list.size()),
                 master_sharetree_list.count_nodes<STN_children>(),
                 master_sharetree_list.count_leaves<STN_children>()
                 );

#if 0
         /* rebuild all job categories
          * - when the scheduler config changed
          * - when the projects were added/modified/deleted
          * - when jobs were added/modified/deleted
          * - when usersets were added/modified/deleted
          * */
         sge_rebuild_job_category(master_job_list, master_userset_list,
                                  master_project_list, master_rqs_list);
#endif

         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM7);
         double prof_init = prof_get_measurement_wallclock(SGE_PROF_CUSTOM7, true, nullptr);
         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM7);

         // if we received a new global configuration, we need to merge the global and local one
         if (st_get_flag_new_global_conf()) {
            const char *hostname = component_get_qualified_hostname();
            const char *cell_root = bootstrap_get_cell_root();
            uint32_t progid = component_get_component_id();

            const auto global = master_config_list.find_first<CONF_name>(ocs::tom::Host(SGE_GLOBAL_NAME));
            const auto local = master_config_list.find_first<CONF_name>(ocs::tom::Host(hostname));
            merge_configuration(nullptr, progid, cell_root, global, local);

            // reset the flag
            st_set_flag_new_global_conf(false);
         }

         /*
          * job categories are reset here, we need
          *  - an update of the rejected field for every new run
          *  - the resource request dependent urgency contribution is cached
          *    per job category
          */
#if 0
         sge_reset_job_category();
#endif

         // prepare data for the scheduler itself
         copy.host_list = master_exechost_list.clone();

         // within the scheduler we only need QIs create one big qinstance list
         {
            // the selections keep the reduced descriptor of the subscription; a reduced descriptor keeps the
            // positions of the full type, so the selection of the full name and the state serves every list
            ocs::tom::What<ocs::tom::QU> what_queue3;
            try {
               what_queue3 = ocs::tom::what<QU_full_name, QU_state>();
            } catch (const ocs::tom::ListCoreError &) {
               // no memory: all_queue_list stays empty, as it did when lWhat() returned nullptr
            }

            for (const auto cqueue : master_cqueue_list) {
               const auto qinstance_list = cqueue.get<CQ_qinstances>();

               if (qinstance_list == nullptr) {
                  continue;
               }

               /* all_queue_list contains all queue instances with state and full queue name only */
               if (what_queue3) {
                  ocs_scheduler_append_selection(copy.all_queue_list, qinstance_list,
                                                 ocs::tom::Where<ocs::tom::QU>(), what_queue3, "all");
               }

               // ensure_valid_what_and_where()
               ocs_scheduler_append_selection(copy.queue_list, qinstance_list, where_what.where_queue,
                                              where_what.what_queue2, "enabled");

               // ensure_valid_what_and_where()
               ocs_scheduler_append_selection(copy.dis_queue_list, qinstance_list, where_what.where_queue2,
                                              where_what.what_queue2, "disabled");
            }
         }

         copy.category_list = master_category_list.clone();
         copy.job_list = master_job_list.clone();

         // We have copied the category list and the job list, so we need to update also the cached category pointers
         ocs::CategoryQmaster::refresh_cat_data_all_jobs(*copy.category_list, *copy.job_list);

         /* no need to copy these lists, they are read only used */
         copy.centry_list = master_centry_list;
         copy.ckpt_list = master_ckpt_list;
         copy.hgrp_list = master_hgrp_list;

         /* these lists need to be copied because they are modified during scheduling run */
         copy.share_tree = master_sharetree_list.clone();
         copy.pe_list = master_pe_list.clone();
         copy.user_list = master_user_list.clone();
         copy.project_list = master_project_list.clone();
         copy.rqs_list = master_rqs_list.clone();
         copy.ar_list = master_ar_list.clone();

         /* report number of reduced and raw (in brackets) lists */
         DPRINTF("Q:" sge_u32 ", AQ:" sge_u32 ", DQ:" sge_u32 ", J:" sge_u32 "(" sge_u32 "), H:" sge_u32 "(" sge_u32 "), C:" sge_u32
                  ", A:" sge_u32 ", D:" sge_u32 ", P:" sge_u32 ", CKPT:" sge_u32 ", US:" sge_u32 ", PR:" sge_u32
                  ", RQS:" sge_u32 ", AR:" sge_u32 ", S:nd:%d/lf:%d\n",
                 static_cast<uint32_t>(copy.queue_list->size()),
                 static_cast<uint32_t>(copy.all_queue_list->size()),
                 static_cast<uint32_t>(copy.dis_queue_list->size()),
                 static_cast<uint32_t>(copy.job_list->size()),
                 static_cast<uint32_t>(master_job_list.size()),
                 static_cast<uint32_t>(copy.host_list->size()),
                 static_cast<uint32_t>(master_exechost_list.size()),
                 static_cast<uint32_t>(copy.centry_list.size()),
                 static_cast<uint32_t>(copy.acl_list->size()),
                 static_cast<uint32_t>(copy.dept_list->size()),
                 static_cast<uint32_t>(copy.pe_list->size()),
                 static_cast<uint32_t>(copy.ckpt_list.size()),
                 static_cast<uint32_t>(copy.user_list->size()),
                 static_cast<uint32_t>(copy.project_list->size()),
                 static_cast<uint32_t>(copy.rqs_list->size()),
                 static_cast<uint32_t>(copy.ar_list->size()),
                 copy.share_tree->count_nodes<STN_children>(),
                 copy.share_tree->count_leaves<STN_children>()
                 );

         bool in_nd_mode = ocs::DebugParam::is_component_in_nd_mode();
         if (!in_nd_mode) {
            schedd_log("-------------START-SCHEDULER-RUN-------------", nullptr, evc->monitor_next_run);
         }

         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM7);
         double prof_copy = prof_get_measurement_wallclock(SGE_PROF_CUSTOM7, true, nullptr);
         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM7);

#ifdef WITH_GPERF
         bool disable_gperf = false;
         if (evc->monitor_next_run) {
            std::string gperf_name = mconf_get_gperf_name();
            std::string gperf_threads = mconf_get_gperf_threads();
            std::string gperf_thread_name = thread_name;

            // do profile only when scheduler is triggered via qconf -tsm
            g_scheduler_use_gperftools = sge_gperf_start_profiling(gperf_data, gperf_thread_name, gperf_threads, gperf_name);
            disable_gperf = true;
         }
#endif

         scheduler_method(evc, &answer_list, &copy);

#ifdef WITH_GPERF
         if (disable_gperf) {
            g_scheduler_use_gperftools = sge_gperf_stop_profiling(gperf_data);
         }
#endif

         answer_list_output(&answer_list);

         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM7);
         double prof_run = prof_get_measurement_wallclock(SGE_PROF_CUSTOM7, true, nullptr);
         PROF_START_MEASUREMENT(SGE_PROF_CUSTOM7);

         // free category cache within the category list
         ocs::CategoryQmaster::reset_tmp_data(*copy.category_list);
         copy.category_list.reset();

         // ... which gets deleted after using; here and not at the end of the scope, so the free is measured
         copy.host_list.reset();
         copy.queue_list.reset();
         copy.dis_queue_list.reset();
         copy.all_queue_list.reset();
         copy.job_list.reset();
         copy.acl_list.reset();
         copy.dept_list.reset();
         copy.pe_list.reset();
         copy.share_tree.reset();
         copy.user_list.reset();
         copy.project_list.reset();
         copy.rqs_list.reset();
         copy.ar_list.reset();

         PROF_STOP_MEASUREMENT(SGE_PROF_CUSTOM7);
         double prof_free = prof_get_measurement_wallclock(SGE_PROF_CUSTOM7, true, nullptr);

         /*
          * need to sync with event master thread
          * if scheduler configuration changed then settings in evm can be adjusted
          */
         if (sconf_is_new_config()) {
            /* set scheduler interval / event delivery interval */
            uint32_t interval = sconf_get_schedule_interval();
            if ((uint32_t) evc->ec_get_edtime(evc) != interval) {
               evc->ec_set_edtime(evc, interval);
            }

            /* set job / ja_task event flushing */
            set_job_flushing(evc);

            /* no need to ec_commit here - we do it when resetting the busy state */

            /* now we handled the new scheduler config - no need to do it twice */
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
                      "run:%.3f, free: %.3f s, jobs: " sge_u32 ", categories: " sge_u32 "/%d)",
                      prof_total, prof_init, prof_copy, prof_run, prof_free,
                      static_cast<uint32_t>(master_job_list.size()),
                      static_cast<uint32_t>(master_category_list.size()), 0);
         }
         if (in_nd_mode) {
            printf("--------------STOP-SCHEDULER-RUN-------------\n");
         } else {
            schedd_log("--------------STOP-SCHEDULER-RUN-------------", nullptr, evc->monitor_next_run);
         }

         thread_output_profiling("scheduler thread profiling summary:", &next_prof_output);

         sge_monitor_output(&monitor);
      }

      /* reset the busy state */
      evc->ec_set_busy(evc, 0);
      evc->ec_commit(evc, nullptr);

      /* stop logging into schedd_runlog (enabled via -tsm) */
      schedd_log_close();
      evc->monitor_next_run = false;

      // pthread cancellation point (functions are pushed in reverse order of execution)
      // As soon as we know that qmaster will shut down we will not reiterate the main loop
      // We just wait here for the final termination signal to do the cleanup.
      // sge_scheduler_cleanup_thread() is the last function which should be called, so it is pushed first
      do {
         pthread_cleanup_push(sge_scheduler_cleanup_thread, nullptr);
         pthread_cleanup_push(sge_scheduler_cleanup_monitor, static_cast<void *>(&monitor));
         pthread_cleanup_push(sge_scheduler_cleanup_event_client, static_cast<void *>(&evc));
         pthread_cleanup_push(sge_scheduler_cleanup_where_what, static_cast<void *>(&where_what));
         cl_thread_func_testcancel(thread_config);
         pthread_cleanup_pop(execute);
         pthread_cleanup_pop(execute);
         pthread_cleanup_pop(execute);
         pthread_cleanup_pop(execute);
         DPRINTF("passed cancellation point\n");

         if (do_shutdown) {
            sge_usleep(50000);
         }
         // we shut down scheduler thread via SHUTDOWN event
         // will need to change to the usual do {} while (sge_thread_has_shutdown_started()) loop
         // when we separate mirroring and scheduling into different threads
      } while (do_shutdown);
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before the call of cl_thread_func_testcancel()
}

/** @brief Is the scheduler thread currently running?
 * @return true when it is
 */
bool
sge_scheduler_is_running() {
   return Master_Scheduler.is_running;
}

