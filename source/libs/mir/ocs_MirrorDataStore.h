#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024,2026 HPC-Gridware GmbH
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

/** @file
 * @brief Declarations for mirroring one mirroring thread per snapshot data store
 *
 * @see ocs_MirrorDataStore.cc
 */

#include <string>
#include "pthread.h"

#include "uti/sge_lock.h"

#include "sgeobj/ocs_DataStore.h"

#include <cinttypes>
#include "evc/sge_event_client.h"

#include "ocs_thread_mirror.h"

namespace ocs {
   /**
    * @brief One thread mirroring the master lists into one data store
    *
    * qmaster keeps several snapshots of the object model so readers do not
    * block writers. Each snapshot is fed by its own instance of this: the
    * thread registers as an event client, applies what arrives, and takes the
    * data store's lock only while it does so.
    *
    * The four pure virtual methods are what a concrete store has to supply -
    * how it connects, which events it wants, and what it does with sessions.
    */
   class MirrorDataStore {
   private:
      pthread_cond_t cond_var;                     ///< used to wait for new events and to wake up this thread
      volatile bool triggered;                     ///< true if new events are pending that need to get processed
      lList *new_events;                           ///< new events that need to get processed
      sge_locktype_t lock_type;                    ///< lock type used to secure the DS
      volatile bool did_handle_initial_events;     ///< true if the initial events have been handled and other threads can access the DS
      bool do_try_lock;                            ///< true if we should try to get the lock without waiting

   protected:
      pthread_mutex_t mutex;                       ///< used to secure other attributes within this object
      const std::string mutex_name;                ///< unique mutex name
      DataStore::Id data_store_id;                 ///< data store that is managed by this thread
      pthread_t thread{};                          ///< pthread that handles the mirroring
      sge_evc_class_t *evc = nullptr;              ///< event client that is used to subscribe to events

   public:
      explicit MirrorDataStore(ocs::DataStore::Id data_store_id, sge_locktype_t lock_type);
      virtual ~MirrorDataStore();

      virtual void wait_for_event(lList **event_list);
      virtual void wakeup();
      virtual void block_till_initial_events_handled();

      [[noreturn]] virtual void *main([[maybe_unused]] void *arg);
      /// Establish this thread's connection to qmaster
      virtual void init_connection() = 0;
      /// Set up the event mirror for this data store
      virtual void init_event_mirror() = 0;
      /// Subscribe the object types this data store needs
      virtual void subscribe_events() = 0;
      /**
       * @brief Release the requests that were waiting for this snapshot to catch up
       *
       * A client that just wrote may not be answered from a snapshot older than
       * its write; those requests wait until the snapshot reaches the id they
       * need.
       *
       * @param unique_id the write id this snapshot has now reached
       */
      virtual void update_sessions_and_move_requests(uint64_t unique_id) = 0;
      virtual bool lock();
      virtual void unlock();

      static void event_mirror_update_func([[maybe_unused]] uint32_t ec_id, [[maybe_unused]] lList **answer_list, lList *event_list, void *arg);

      static void thread_cleanup_monitor(void *arg);
      static void thread_cleanup_event_client(void *arg);
      static void thread_cleanup_data_store([[maybe_unused]] void *unused);

      /// Creates the mirror threads; needs access to the private members
      friend void event_mirror_initialize();
      /// Stops the mirror threads; needs access to the private members
      friend void event_mirror_terminate();
   };
}
