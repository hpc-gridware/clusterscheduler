/*___INFO__MARK_BEGIN_CLOSED__*/
/*___INFO__MARK_END_CLOSED__*/

#include "uti/sge_component.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_profiling.h"

#include "evm/sge_event_master.h"
#include "mir/sge_mirror.h"

#include "sgeobj/sge_report.h"

#include "oge_thread_event_mirror.h"
#include "setup_qmaster.h"
#include "sge_thread_main.h"

master_event_mirror_class_t master_event_mirror = {
        PTHREAD_MUTEX_INITIALIZER,  // mutex
        false,                      // is_running
        0,                          // thread_id
        true                        // use_bootstrap
};

void
oge_event_mirror_initialize() {
   cl_thread_settings_t *dummy_thread_p = nullptr;
   dstring thread_name = DSTRING_INIT;

   DENTER(TOP_LAYER);
   DPRINTF("event mirror functionality has been initialized\n");
   sge_dstring_sprintf(&thread_name, "%s%03d", threadnames[EVENT_MIRROR_THREAD], 0);
   cl_thread_list_setup(&(Main_Control.event_mirror_thread_pool), "event mirror thread pool");
   cl_thread_list_create_thread(Main_Control.event_mirror_thread_pool, &dummy_thread_p, cl_com_get_log_list(),
                                sge_dstring_get_string(&thread_name), 0, oge_event_mirror_main, nullptr, nullptr,
                                CL_TT_EVENT_MIRROR);
   sge_dstring_free(&thread_name);
   DRETURN_VOID;
}

void
oge_event_mirror_terminate() {
   DENTER(TOP_LAYER);

   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.event_mirror_thread_pool);
   while (thread != nullptr) {
      DPRINTF(SFN " gets canceled\n", thread->thread_name);
      cl_thread_list_delete_thread(Main_Control.event_mirror_thread_pool, thread);
      thread = cl_thread_list_get_first_thread(Main_Control.event_mirror_thread_pool);
   }
   DPRINTF("all " SFN " threads terminated\n", threadnames[EVENT_MIRROR_THREAD]);

   DRETURN_VOID;
}

struct event_mirror_control_t {
   pthread_mutex_t mutex;     ///< used to secure other attributes within this object
   pthread_cond_t cond_var;   ///< used to wait for new events
   bool exit;                 ///< true -> exit event processing
   bool triggered;            ///< true if new events are pending that need to get processed
   lList *new_events;         ///< new events that neet to get processed
};

event_mirror_control_t event_mirror_control = {
        PTHREAD_MUTEX_INITIALIZER,  // mutex
        PTHREAD_COND_INITIALIZER,   // cond_var
        false,                      // exit
        false,                      // triggered
        nullptr,                    // new_events
};

void
oge_event_mirror_event_update_func([[maybe_unused]] u_long32 ec_id, [[maybe_unused]] lList **alpp, lList *event_list) {
   DENTER(TOP_LAYER);
   static const char *mutex_name = "event_mirror_control_mutex";

   sge_mutex_lock(mutex_name, __func__, __LINE__, &event_mirror_control.mutex);

   // move the events from control of event master to the event mirror thread
   lListElem *new_event_obj = lFirstRW(event_list);
   if (event_mirror_control.new_events != nullptr) {
      lList *events = nullptr;
      lXchgList(new_event_obj, REP_list, &events);
      lAddList(event_mirror_control.new_events, &events);
   } else {
      lXchgList(new_event_obj, REP_list, &event_mirror_control.new_events);
   }
   event_mirror_control.triggered = true;
   DPRINTF("event update function oge_event_mirror_event_update_func() has been triggered\n");

   // trigger the event mirror thread to wake up and process the events
   pthread_cond_signal(&event_mirror_control.cond_var);

   sge_mutex_unlock(mutex_name, __func__, __LINE__, &event_mirror_control.mutex);
   DRETURN_VOID;
}

static void
oge_event_mirror_wait_for_event(sge_evc_class_t *evc, lList **event_list) {
   static const char *mutex_name = "event_mirror_control_mutex";
   static const long timeout_s = 10;
   static const long timeout_n = 0;
   int wait_ret;
   bool do_ack = false;

   DENTER(TOP_LAYER);

   // fetch events
   {
      sge_mutex_lock(mutex_name, __func__, __LINE__, &event_mirror_control.mutex);

      // if we did not receive a signal about new events then we will wait for one
      if (!event_mirror_control.triggered) {
         struct timespec ts{};
         u_long32 current_time = sge_get_gmt();
         ts.tv_sec = (long) current_time + timeout_s;
         ts.tv_nsec = timeout_n;

         // wait till we get notified by event master thread that there are new events
         wait_ret = pthread_cond_timedwait(&event_mirror_control.cond_var, &event_mirror_control.mutex, &ts);
         if (wait_ret != 0) {
            DPRINTF("timeout (or error) in oge_event_mirror_wait_for_event %d\n", wait_ret);
         }
      }

      // fetch the new events
      if (event_mirror_control.triggered) {
         *event_list = event_mirror_control.new_events;
         event_mirror_control.new_events = nullptr;
         event_mirror_control.triggered = false;
         do_ack = true;
      }

      sge_mutex_unlock(mutex_name, __func__, __LINE__, &event_mirror_control.mutex);
   }

   // acknowledge events
   if (do_ack) {
      if (lGetElemUlong(*event_list, ET_type, sgeE_ACK_TIMEOUT) != nullptr) {
         evc->ec_mark4registration(evc);
      }
      evc->ec_ack(evc);
   }

   DRETURN_VOID;
}

void
oge_event_mirror_cleanup_thread([[maybe_unused]] void *arg) {
   static const char *mutex_name = "master event mirror";
   DENTER(TOP_LAYER);

   sge_mutex_lock(mutex_name, __func__, __LINE__, &master_event_mirror.mutex);

   if (master_event_mirror.is_running) {
      // delete thread config before thread itself is deleted
      cl_thread_unset_thread_config();

      // delete event mirror thread (but do not wait for termination!)
      cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.scheduler_thread_pool);
      cl_thread_list_delete_thread_without_join(Main_Control.scheduler_thread_pool, thread);

      // trash the thread pool
      cl_thread_list_cleanup(&Main_Control.event_mirror_thread_pool);

      // shutdown of old thread finished. now a new event mirror can be  started
      master_event_mirror.is_running = false;
   }

   sge_mutex_unlock(mutex_name, __func__, __LINE__, &master_event_mirror.mutex);

   DRETURN_VOID;
}

static void
oge_event_mirror_cleanup_monitor(monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

static void
oge_event_mirror_cleanup_event_client(sge_evc_class_t *evc) {
   DENTER(TOP_LAYER);
   sge_mirror_shutdown(evc);
   DRETURN_VOID;
}

void *
oge_event_mirror_main(void *arg) {
   DENTER(TOP_LAYER);
   lList *alp = nullptr; // answer_list

   // initialize commlib thread
   auto *thread_config = (cl_thread_settings_t *) arg;
   cl_thread_func_startup(thread_config);

   // set thread name and id used by logging an others
   const char *thread_name = thread_config->thread_name;
   int thread_id = thread_config->thread_id;
   component_set_thread_name(thread_name);
   component_set_thread_id(thread_id);
   DPRINTF(SFN "(%d) started\n", thread_name, thread_id);

   // initialize monitoring
   monitoring_t monitor;
   sge_monitor_init(&monitor, thread_name, NONE_EXT, NO_WARNING, NO_ERROR);
   sge_qmaster_thread_init(QMASTER, EVENT_MIRROR_THREAD, true);
   DPRINTF("initialized monitoring\n");

   // register at profiling module
   set_thread_name(pthread_self(), "Event Mirror Thread");
   conf_update_thread_profiling("Event Mirror Thread");
   DPRINTF("registered at profiling module\n");

   // set profiling parameters
   time_t next_prof_output = 0;
   prof_set_level_name(SGE_PROF_CUSTOM0, "main", nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM1, "wait", nullptr);
   prof_set_level_name(SGE_PROF_CUSTOM2, "mirror", nullptr);
   DPRINTF("set profiling levels\n");

   // prepare as an event client/mirror
   sge_evc_class_t *evc = nullptr;
   bool local_ret = sge_gdi2_evc_setup(&evc, EV_ID_EVENT_MIRROR, &alp, thread_name);
   DPRINTF("prepared event client/mirror mechanism\n");

   // register as event mirror and subscribe events
   if (local_ret) {
      sge_mirror_initialize(evc, OBJ_STATE_READER, &oge_event_mirror_event_update_func, &sge_mod_event_client,
                            &sge_add_event_client, &sge_remove_event_client, &sge_handle_event_ack);
      evc->ec_register(evc, false, nullptr, &monitor);
      evc->ec_set_busy_handling(evc, EV_BUSY_UNTIL_RELEASED);
      DPRINTF("registered at event mirror\n");

      // DS should be a copy of the master DS - wee need all events
      sge_mirror_subscribe(evc, SGE_TYPE_ALL, nullptr, nullptr, nullptr, nullptr, nullptr);
   }

   // enter main loop
   if (local_ret) {
      while (true) {
         lList *event_list = nullptr;
         bool handled_events = false;

         // wait for new events
         MONITOR_IDLE_TIME(oge_event_mirror_wait_for_event(evc, &event_list), (&monitor),
                           mconf_get_monitor_time(), mconf_is_monitor_message());

         // if we lost connection we have to register again
         if (evc->ec_need_new_registration(evc)) {
            DPRINTF("event mirror thread lost connection to event master thread");
            lFreeList(&event_list);
            if (evc->ec_register(evc, false, nullptr, &monitor) == true) {
               DPRINTF("re-registered at event master!\n");
            }
         }

         // process received events
         if (event_list != nullptr) {
            bool do_shutdown = (lGetElemUlong(event_list, ET_type, sgeE_SHUTDOWN) != nullptr);

            if (do_shutdown == false && sge_mirror_process_event_list(evc, event_list) == SGE_EM_OK) {
               handled_events = true;
               DPRINTF("events handled\n");
            } else {
               DPRINTF("events contain shutdown event - ignoring events\n");
            }
            lFreeList(&event_list);
         }

         // actions if events where processed
         if (handled_events == true) {
            thread_output_profiling("scheduler thread profiling summary:\n", &next_prof_output);
            sge_monitor_output(&monitor);
         }

         // reset the busy state to get more events
         evc->ec_set_busy(evc, 0);
         evc->ec_commit(evc, nullptr);

         // pthread cancellation point (functions are pushed in reverse order of execution)
         int execute = 0;
         pthread_cleanup_push(oge_event_mirror_cleanup_thread, nullptr);
         pthread_cleanup_push((void (*)(void *)) oge_event_mirror_cleanup_monitor, (void *) &monitor);
         pthread_cleanup_push((void (*)(void *)) oge_event_mirror_cleanup_event_client, (void *) evc);
         cl_thread_func_testcancel(thread_config);
         pthread_cleanup_pop(execute);
         pthread_cleanup_pop(execute);
         pthread_cleanup_pop(execute);
         DPRINTF("passed cancellation point\n");
      }
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before and after the call of cl_thread_func_testcancel()
   DRETURN(nullptr);
}
