/*___INFO__MARK_BEGIN_CLOSED__*/
/*___INFO__MARK_END_CLOSED__*/

#include <string>

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
#include "sge_lock.h"

struct event_mirror_control_t {
   const std::string mutex_name;       ///< unique mutex name
   pthread_mutex_t mutex;              ///< used to secure other attributes within this object
   pthread_cond_t cond_var;            ///< used to wait for new events
   bool exit;                          ///< true -> exit event processing
   bool triggered;                     ///< true if new events are pending that need to get processed
   lList *new_events;                  ///< new events that neet to get processed
};

struct event_mirror_argument_t {
   oge::DataStore::Id data_store_id;
};

event_mirror_control_t event_mirror_control = {
        "event_mirror_control_mutex",  // mutex name
        PTHREAD_MUTEX_INITIALIZER,     // mutex
        PTHREAD_COND_INITIALIZER,      // cond_var
        false,                         // exit
        false,                         // triggered
        nullptr,                       // new_events
};

master_event_mirror_class_t master_event_mirror = {
        PTHREAD_MUTEX_INITIALIZER,     // mutex
        false,                         // is_running
        0,                             // thread_id
        true                           // use_bootstrap
};

// trigger the event mirror thread to wake up either to handle events or to terminate
void
oge_event_mirror_wakeup() {
   sge_mutex_lock(event_mirror_control.mutex_name.c_str(), __func__, __LINE__, &event_mirror_control.mutex);
   pthread_cond_signal(&event_mirror_control.cond_var);
   sge_mutex_unlock(event_mirror_control.mutex_name.c_str(), __func__, __LINE__, &event_mirror_control.mutex);
}

void
oge_event_mirror_initialize() {
   DENTER(TOP_LAYER);

   cl_thread_settings_t *dummy_thread_p = nullptr;
   cl_thread_list_setup(&(Main_Control.event_mirror_thread_pool), "event mirror thread pool");
   DPRINTF("event mirror thread pool initialized\n");

   auto data_store_id = oge::DataStore::Id::READER_ALL;
   cl_thread_list_create_thread(Main_Control.event_mirror_thread_pool, &dummy_thread_p, cl_com_get_log_list(),
                                threadnames[EVENT_MIRROR_THREAD], 0, oge_event_mirror_main, nullptr,
                                (void *)data_store_id, CL_TT_EVENT_MIRROR);
   DPRINTF("added event mirror thread for %d\n", data_store_id);

   DRETURN_VOID;
}

void
oge_event_mirror_terminate() {
   DENTER(TOP_LAYER);

   // trigger pthread_cancel for each thread so that it will terminate when it reaches the cancellation point
   cl_thread_list_elem_t *thr_nxt = cl_thread_list_get_first_elem(Main_Control.event_mirror_thread_pool);
   cl_thread_list_elem_t *thr;
   while ((thr = thr_nxt) != nullptr) {
      thr_nxt = cl_thread_list_get_next_elem(thr);

      cl_thread_shutdown(thr->thread_config);
   }
   DPRINTF("triggered termination of all " SFN " threads\n", threadnames[EVENT_MIRROR_THREAD]);

   // wake up the mirror threads
   oge_event_mirror_wakeup();
   DPRINTF("triggered wake up of all " SFN " threads\n", threadnames[EVENT_MIRROR_THREAD]);

   // join the threads
   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.event_mirror_thread_pool);
   while (thread != nullptr) {
      cl_thread_list_delete_thread(Main_Control.event_mirror_thread_pool, thread);

      thread = cl_thread_list_get_first_thread(Main_Control.event_mirror_thread_pool);
   }
   DPRINTF("all " SFN " threads terminated\n", threadnames[EVENT_MIRROR_THREAD]);

   // trash the thread pool
   cl_thread_list_cleanup(&Main_Control.event_mirror_thread_pool);
   DPRINTF(SFQ " thread pool terminated\n", threadnames[EVENT_MIRROR_THREAD]);

   DRETURN_VOID;
}

void
oge_event_mirror_event_update_func([[maybe_unused]] u_long32 ec_id, [[maybe_unused]] lList **alpp, lList *event_list) {
   DENTER(TOP_LAYER);

   sge_mutex_lock(event_mirror_control.mutex_name.c_str(), __func__, __LINE__, &event_mirror_control.mutex);

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

   // trigger the event mirror thread to wake up
   pthread_cond_signal(&event_mirror_control.cond_var);

   sge_mutex_unlock(event_mirror_control.mutex_name.c_str(), __func__, __LINE__, &event_mirror_control.mutex);

   DRETURN_VOID;
}

static void
oge_event_mirror_wait_for_event(sge_evc_class_t *evc, lList **event_list) {
   static const long timeout_s = 10;
   static const long timeout_n = 0;
   int wait_ret;
   bool do_ack = false;

   DENTER(TOP_LAYER);

   // fetch events
   {
      sge_mutex_lock(event_mirror_control.mutex_name.c_str(), __func__, __LINE__, &event_mirror_control.mutex);

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

      sge_mutex_unlock(event_mirror_control.mutex_name.c_str(), __func__, __LINE__, &event_mirror_control.mutex);
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

#if 0
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
#endif

static void
oge_event_mirror_cleanup_monitor(void *arg) {
   DENTER(TOP_LAYER);
   auto *monitor = (monitoring_t *)arg;
   sge_monitor_free(monitor);
   DRETURN_VOID;
}

static void
oge_event_mirror_cleanup_event_client(void *arg) {
   DENTER(TOP_LAYER);
   auto *evc = (sge_evc_class_t *)arg;
   sge_mirror_shutdown(evc);
   DRETURN_VOID;
}

static void
oge_event_mirror_cleanup_data_store([[maybe_unused]] void *unused) {
   DENTER(TOP_LAYER);
   oge::DataStore::free_all_master_lists();
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
   auto thread_id = thread_config->thread_id;
   auto data_store_id = (oge::DataStore::Id)(long)(thread_config->thread_user_data);
   component_set_thread_name(thread_name);
   component_set_thread_id(thread_id);
   DPRINTF(SFN "(%d) started handling data store %ld\n", thread_name, thread_id, data_store_id);

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

   // this thread will use the READER data store
   oge::DataStore::select_active_ds(data_store_id);

   // prepare as an event client/mirror
   sge_evc_class_t *evc = nullptr;
   bool local_ret = sge_gdi2_evc_setup(&evc, EV_ID_EVENT_MIRROR, &alp, thread_name);
   DPRINTF("prepared event client/mirror mechanism\n");

   // register as event mirror and subscribe events
   if (local_ret) {
      sge_mirror_initialize(evc, &oge_event_mirror_event_update_func, &sge_mod_event_client,
                            &sge_add_event_client, &sge_remove_event_client, &sge_handle_event_ack);
      evc->ec_register(evc, false, nullptr, &monitor);
      evc->ec_set_busy_handling(evc, EV_BUSY_UNTIL_RELEASED);
      DPRINTF("registered at event mirror\n");

      // DS should be a copy of the master DS - wee need all events
      sge_mirror_subscribe(evc, SGE_TYPE_ALL, nullptr, nullptr, nullptr, nullptr, nullptr);
   }

   // enter main loop
   if (local_ret) {

      // do not exit even if shutdown event is received. We want th thread only to terminate in the cancellation
      // point to enforce that: other threads (accessing the data store) terminate before us and we need to do the
      // cleanup (free of data store memory and more) at the cancellation point
      while (true) {
         lList *event_list = nullptr;

         // wait for new events
         MONITOR_IDLE_TIME(oge_event_mirror_wait_for_event(evc, &event_list), (&monitor),
                           mconf_get_monitor_time(), mconf_is_monitor_message());

         // if we lost connection we have to register again
         if (evc->ec_need_new_registration(evc)) {
            DPRINTF("event mirror thread lost connection to event master thread\n");
            lFreeList(&event_list);
            if (evc->ec_register(evc, false, nullptr, &monitor) == true) {
               DPRINTF("re-registered at event master!\n");
            }
         }

         // did we receive a shutdown event?
         bool do_shutdown = false;
         if (event_list != nullptr) {
            do_shutdown = (lGetElemUlong(event_list, ET_type, sgeE_SHUTDOWN) != nullptr);

            if (do_shutdown) {
               DPRINTF("received event to shutdown\n");
            }

            lFreeList(&event_list);
         }

         // handle events
         const long wait_time = 200;
         const long max_wait_time = 2000;
         long remaining_wait_time = max_wait_time;
         bool did_handle_events = false;
         bool got_lock = false;
         bool do_try_lock = true;

         while (!got_lock) {
            // acquire lock. first try to get the lock. Only if we have to wait to long we will enforce to get the lock
            if (do_try_lock) {
               got_lock = SGE_TRY_LOCK(LOCK_READ_ALL_DS, LOCK_WRITE);
            } else {
               SGE_LOCK(LOCK_READ_ALL_DS, LOCK_WRITE);
               got_lock = true;
            }

            if (got_lock) {
               // we got the lock and can process the events
               sge_mirror_error mirror_ret = sge_mirror_process_event_list(evc, event_list);
               lFreeList(&event_list);

               if (mirror_ret == SGE_EM_OK) {
                  did_handle_events = true;
                  DPRINTF("events handled successfully\n");
               } else {
                  DPRINTF("error during event processing\n");
               }

               // release lock
               SGE_UNLOCK(LOCK_READ_ALL_DS, LOCK_WRITE);
            } else {
               // we did not get the lock. wait a short time before retry. if the max wait time is consumed
               // then continue with a hard lock instead of a try lock
               sge_usleep(wait_time);
               remaining_wait_time -= wait_time;
               if (remaining_wait_time <= 0) {
                  do_try_lock = false;
               }
            }
         }

         // actions if events where processed
         if (did_handle_events == true) {
            thread_output_profiling("scheduler thread profiling summary:\n", &next_prof_output);
            sge_monitor_output(&monitor);
         }

         // reset the busy state to get more events
         evc->ec_set_busy(evc, 0);
         evc->ec_commit(evc, nullptr);

         // pthread cancellation point (functions are pushed in reverse order of execution)
         int execute = 0;
#if 0
         pthread_cleanup_push(oge_event_mirror_cleanup_thread, nullptr);
#endif
         pthread_cleanup_push(oge_event_mirror_cleanup_monitor, &monitor);
         pthread_cleanup_push(oge_event_mirror_cleanup_data_store, nullptr);
         pthread_cleanup_push(oge_event_mirror_cleanup_event_client, evc);
         cl_thread_func_testcancel(thread_config);
         pthread_cleanup_pop(execute); // event client registration
         pthread_cleanup_pop(execute); // data store that was filled by this mirror
         pthread_cleanup_pop(execute); // monitor
#if 0
         pthread_cleanup_pop(execute); // thread itself
#endif
         DPRINTF("passed cancellation point\n");
      }
   }

   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before and after the call of cl_thread_func_testcancel()
   DRETURN(nullptr);
}
