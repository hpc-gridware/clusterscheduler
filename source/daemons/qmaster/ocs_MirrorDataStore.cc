/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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

#include <functional>
#include <iostream>

#include "uti/sge_component.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_profiling.h"
#include "uti/sge_log.h"

#include "evm/sge_event_master.h"
#include "mir/sge_mirror.h"

#include "sgeobj/sge_report.h"

#include "ocs_MirrorDataStore.h"
#include "setup_qmaster.h"

namespace ocs {
   /**
    * pthread cleanup handler to release the monitor
    * @param arg Monitor pointer
    */
   void
   MirrorDataStore::thread_cleanup_monitor(void *arg) {
      DENTER(TOP_LAYER);
      auto *monitor = (monitoring_t *) arg;
      sge_monitor_free(monitor);
      DRETURN_VOID;
   }

   /**
    * pthread cleanup handler to shutdown the event client
    * @param arg Event client pointer
    */
   void
   MirrorDataStore::thread_cleanup_event_client(void *arg) {
      DENTER(TOP_LAYER);
      auto *evc = (sge_evc_class_t *) arg;
      sge_mirror_shutdown(evc);
      DRETURN_VOID;
   }

   /**
    * pthread cleanup handler to free all master lists of the mirrored data store
    * @param unused
    */
   void
   MirrorDataStore::thread_cleanup_data_store([[maybe_unused]] void *unused) {
      DENTER(TOP_LAYER);
      DataStore::free_all_master_lists();
      DRETURN_VOID;
   }

   /**
    * Will be called by event master to pass a list of events to a mirror thread (main method of this class).
    * Triggers also the wakeup of the mirror thread so that it can process the received events.
    *
    * @param ec_id Event client ID
    * @param answer_list AN_Type list that might be used to report errors to the event mirror.
    * @param event_list List of new events
    * @param arg Passthrough argument - here it is the instance of a MirrorDataStore subclass
    */
   void
   MirrorDataStore::event_mirror_update_func([[maybe_unused]] u_long32 ec_id, [[maybe_unused]] lList **answer_list,
                                             lList *event_list, void *arg) {
      DENTER(TOP_LAYER);
      auto *mirror_thread = static_cast<MirrorDataStore *>(arg);

      sge_mutex_lock(mirror_thread->mutex_name.c_str(), __func__, __LINE__, &mirror_thread->mutex);

      // move the events from control of event master to the event mirror thread
      lListElem *new_event_obj = lFirstRW(event_list);
      if (mirror_thread->new_events != nullptr) {
         lList *events = nullptr;
         lXchgList(new_event_obj, REP_list, &events);
         lAddList(mirror_thread->new_events, &events);
      } else {
         lXchgList(new_event_obj, REP_list, &mirror_thread->new_events);
      }

      // wakeup the mirror
      mirror_thread->triggered = true;
      pthread_cond_signal(&mirror_thread->cond_var);

      sge_mutex_unlock(mirror_thread->mutex_name.c_str(), __func__, __LINE__, &mirror_thread->mutex);

      DRETURN_VOID;
   }

   /** @brief Block executing thread till initial events are handled.
    * Recheck every 100ms.
    */
   void
   MirrorDataStore::block_till_initial_events_handled() {
      DENTER(TOP_LAYER);
      volatile bool wait = true;

      do {
         if (wait) {
            sge_usleep(25000);
         }
         DPRINTF("still waiting for initial events to be handled\n");
         sge_mutex_lock(mutex_name.c_str(), __func__, __LINE__, &mutex);
         wait = !did_handle_initial_events;
         sge_mutex_unlock(mutex_name.c_str(), __func__, __LINE__, &mutex);
      } while (wait);
      DRETURN_VOID;
   }

   /**
    * Block till event master has new events to get processed.
    * MirrorDataStore::wakeup() can be used to wakeup a thread that is blocking in this call.
    *
    * @param event_list List of new events
    */
   void
   MirrorDataStore::wait_for_event(lList **event_list) {
      int pthread_ret = 0;
      bool do_ack = false;

      DENTER(TOP_LAYER);

      // fetch events
      {
         sge_mutex_lock(mutex_name.c_str(), __func__, __LINE__, &mutex);

         // if we did not receive a signal about new events then we will wait for one
         if (!triggered) {
#if 0
            static const time_t timeout_s = 15;
            static const time_t timeout_n = 0;
            struct timespec ts{};
            time_t current_time = time(nullptr);
            ts.tv_sec = current_time + timeout_s;
            ts.tv_nsec = timeout_n;

            // wait till we get notified by event master thread that there are new events

            DPRINTF("will wait for events now (with timeout)\n");
            pthread_ret = pthread_cond_timedwait(&cond_var, &mutex, &ts);
#else
            DPRINTF("will wait for events now (without timeout)\n");
            pthread_ret = pthread_cond_wait(&cond_var, &mutex);
#endif
         }

         DPRINTF("woke up (pthread_ret=%d, triggered=%s, #events=%d)\n",
                 pthread_ret, triggered ? "true" : "false", lGetNumberOfElem(new_events));

         // fetch the new events
         if (triggered) {
            *event_list = new_events;
            new_events = nullptr;
            triggered = false;
            do_ack = true;
         }

         sge_mutex_unlock(mutex_name.c_str(), __func__, __LINE__, &mutex);
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

   /**
    * Constructor.
    *
    * @param data_store_id Data store that will be filled with data.
    * @param lock_type Lock that should be used to secure the data store
    */
   MirrorDataStore::MirrorDataStore(DataStore::Id data_store_id, sge_locktype_t lock_type) :
           mutex(PTHREAD_MUTEX_INITIALIZER),
           cond_var(PTHREAD_COND_INITIALIZER),
           mutex_name("event_mirror_control_mutex"),
           triggered(false),
           new_events(nullptr),
           data_store_id(data_store_id),
           lock_type(lock_type),
           did_handle_initial_events(false) {
      // derived classes have to specify the lock to be used, and it must be different from LOCK_GLOBAL
      SGE_ASSERT(lock_type != LOCK_GLOBAL);
   }

   MirrorDataStore::~MirrorDataStore() {
      pthread_mutex_destroy(&mutex);
      pthread_cond_destroy(&cond_var);
      lFreeList(&new_events);
   }

   /**
    * Mirror threads main routine.
    *
    * Does the setup of a qmaster thread (with monitor, profiling, ...) but without commlib. Can therefor not
    * communicate with other daemons or clients.
    *
    * This thread subscribes as event client at the event master thread and replicates those data subscribed in
    * subscribe_events() to that data store that has been passed to the constructor of this class. Access to the data
    * store is synchronized using the lock object that was also specified during construction of this object.
    *
    * In the main loop of the thread the thread waits for new events. Within the first second after new events have
    * been received the thread tries to get a write lock to the data store. If this is not possible then a regular
    * write lock is requested.
    *
    * The wait time can therefore be much more that one second if:
    *    - there are either threads already accessing the data store
    *    - there are threads waiting in the FIFO lock list because they tried to acquire the lock before this thread.
    *
    * As soon as the thread gets the lock it will process the events to update the data store and notify the event
    * master about the processed events.
    *
    * After that it will pass the cancellation point and restart in the main loop.
    *
    * @param arg The usual void* argument of pthreads.
    * @return Will never return.
    */
   [[noreturn]] void *
   MirrorDataStore::main([[maybe_unused]] void *arg) {
      DENTER(TOP_LAYER);
      lList *alp = nullptr;

      auto thread_name = threadnames[EVENT_MIRROR_THREAD];
      auto thread_id = (DataStore::Id) (long) data_store_id;

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
      u_long64 next_prof_output = 0;
      prof_set_level_name(SGE_PROF_CUSTOM0, "main", nullptr);
      prof_set_level_name(SGE_PROF_CUSTOM1, "wait", nullptr);
      prof_set_level_name(SGE_PROF_CUSTOM2, "mirror", nullptr);
      DPRINTF("set profiling levels\n");

      // this thread will use the READER data store
      DataStore::select_active_ds(data_store_id);

      // prepare as an event client/mirror
      std::string mirror_name{thread_name};
      mirror_name += '-' + std::to_string(data_store_id);
      ev_registration_id reg_id = DataStore::get_ev_id_for_data_store(data_store_id);
      bool local_ret = sge_gdi2_evc_setup(&evc, reg_id, &alp, mirror_name.c_str());
      DPRINTF("prepared event client/mirror mechanism\n");

      // register as event mirror and subscribe events
      if (local_ret) {
         sge_mirror_initialize(evc, MirrorDataStore::event_mirror_update_func, &sge_mod_event_client,
                               &sge_add_event_client, &sge_remove_event_client, &sge_handle_event_ack, this);

         evc->ec_register(evc, false, nullptr);
         evc->ec_set_busy_handling(evc, EV_BUSY_UNTIL_RELEASED);

         // Subscribe events that are required for the data store
         subscribe_events();

         evc->ec_commit(evc, nullptr);

         DPRINTF("registered at event mirror and subscribed events\n");
      }

      // do not exit even if event client shutdown event is received. We want the thread only to terminate in the cancellation
      // point to enforce that: other threads (accessing the data store) terminate before us and we need to do the
      // cleanup (free of data store memory and more) at the cancellation point

      // the thread shall only terminate in pthread_testcancel() to ensure that proper cleanup is done
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
      pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);

      bool do_qmaster_shutdown = false;
      while (true) {
         lList *event_list = nullptr;

         // wait for new events
         MONITOR_IDLE_TIME(wait_for_event(&event_list), (&monitor),
                           mconf_get_monitor_time(), mconf_is_monitor_message());

         // if we lost connection we have to register again
         if (evc->ec_need_new_registration(evc)) {
            DPRINTF("event mirror thread lost connection to event master thread\n");
            lFreeList(&event_list);
            if (evc->ec_register(evc, false, nullptr)) {
               DPRINTF("re-registered at event master!\n");
            }
         }

         // did we receive a shutdown event?
         bool do_shutdown = false;
         if (event_list != nullptr) {
            // if we receive a shutdown event for the event client we will re-register with the next iteration of the main loop
            bool do_event_client_shutdown = (lGetElemUlong(event_list, ET_type, sgeE_SHUTDOWN) != nullptr);

            // if we receive a qmaster shutdown event we will terminate this qmaster thread
            do_qmaster_shutdown = (lGetElemUlong(event_list, ET_type, sgeE_QMASTER_GOES_DOWN) != nullptr);
            do_shutdown = do_event_client_shutdown || do_qmaster_shutdown;

            if (do_shutdown) {
               DPRINTF("received event to shutdown\n");

               // no need to handle the events if we shut down afterward
               lFreeList(&event_list);
            }
         }

         // handle events (if shutdown is not pending)
         if (!do_shutdown) {
            const long wait_time = 20000;
            const long max_wait_time = mconf_get_max_ds_deviation() * 1000;
            long remaining_wait_time = max_wait_time;
            bool did_handle_events = false;
            bool got_lock = false;
            bool do_try_lock = (max_wait_time > wait_time);

            while (!got_lock) {
               // acquire lock. first try to get the lock. Only if we have to wait to long we will enforce to get the lock
               if (do_try_lock) {
                  got_lock = SGE_TRY_LOCK(lock_type, LOCK_WRITE);
               } else {
                  SGE_LOCK(lock_type, LOCK_WRITE);
                  got_lock = true;
               }

               if (got_lock) {
                  // we got the lock and can process the events
                  sge_mirror_error mirror_ret = sge_mirror_process_event_list(evc, event_list);
                  lFreeList(&event_list);

                  if (mirror_ret == SGE_EM_OK) {
                     did_handle_initial_events = true;
                     did_handle_events = true;
                  } else {
                     DPRINTF("error during event processing\n");
                  }

                  // release lock
                  SGE_UNLOCK(lock_type, LOCK_WRITE);
               } else {
                  // we did not get the lock. wait a short time before retry. if the max wait time is consumed
                  // then continue with a hard lock instead of a try lock
                  INFO("waiting for lock in event mirror thread. remaining_wait_time=%ld", remaining_wait_time);
                  sge_usleep(wait_time);
                  remaining_wait_time -= wait_time;
                  if (remaining_wait_time <= 0) {
                     do_try_lock = false;
                  }
               }
            }

            // actions if events where processed
            if (did_handle_events) {
               thread_output_profiling("scheduler thread profiling summary:\n", &next_prof_output);
               sge_monitor_output(&monitor);
            }
         }

         // reset the busy state to get more events
         evc->ec_set_busy(evc, 0);
         evc->ec_commit(evc, nullptr);

         // pthread cancellation point (functions are pushed in reverse order of execution)
         // As soon as we know that qmaster will shut down we will not reiterate the main loop
         // We just wait here for the final termination signal to do the cleanup.
         do {
            int execute = 0;
            pthread_cleanup_push(thread_cleanup_monitor, static_cast<void *>(&monitor));
            pthread_cleanup_push(thread_cleanup_data_store, nullptr);
            pthread_cleanup_push(thread_cleanup_event_client, static_cast<void *>(evc));
            pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
            pthread_testcancel();
            pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
            pthread_cleanup_pop(execute); // event client registration
            pthread_cleanup_pop(execute); // data store that was filled by this mirror
            pthread_cleanup_pop(execute); // monitor

            if (do_qmaster_shutdown) {
               sge_usleep(50000);
            }
         } while (do_qmaster_shutdown);
         DPRINTF("passed cancellation point\n");
      }

      // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
      // pthread_cleanup_push()/pthread_cleanup_pop() before and after the call of cl_thread_func_testcancel()
   }

   /**
    * Trigger the event mirror thread to wakeup.
    *
    * Mirror threads wait for new events in MirrorDataStore::wait_for_event(). This method is used by event master
    * to wakeup a mirror thread so that it processes the events that event master wants to deliver.
    *
    * Also used during shutdown of qmaster to wakeup the thread.
    */
   void
   MirrorDataStore::wakeup() {
      DENTER(TOP_LAYER);
      DPRINTF("wakeup call of event mirror to process events\n");
      sge_mutex_lock(mutex_name.c_str(), __func__, __LINE__, &mutex);
      pthread_cond_signal(&cond_var);
      sge_mutex_unlock(mutex_name.c_str(), __func__, __LINE__, &mutex);
      DRETURN_VOID;
   }
}
