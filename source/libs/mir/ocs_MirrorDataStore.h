#pragma once
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

#include <string>
#include "pthread.h"

#include "uti/sge_lock.h"

#include "sgeobj/ocs_DataStore.h"

#include "basis_types.h"
#include "evc/sge_event_client.h"

#include "ocs_thread_mirror.h"

namespace ocs {
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
      virtual void init_connection() = 0;
      virtual void init_event_mirror() = 0;
      virtual void subscribe_events() = 0;
      virtual void update_sessions_and_move_requests(u_long64 unique_id) = 0;
      virtual bool lock();
      virtual void unlock();

      static void event_mirror_update_func([[maybe_unused]] u_long32 ec_id, [[maybe_unused]] lList **answer_list, lList *event_list, void *arg);

      static void thread_cleanup_monitor(void *arg);
      static void thread_cleanup_event_client(void *arg);
      static void thread_cleanup_data_store([[maybe_unused]] void *unused);

      friend void event_mirror_initialize();
      friend void event_mirror_terminate();
   };
}
