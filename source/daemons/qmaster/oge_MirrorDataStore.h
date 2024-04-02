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

#include "sgeobj/oge_DataStore.h"

#include "basis_types.h"
#include "evc/sge_event_client.h"

#include "oge_thread_mirror.h"

namespace oge {
   class MirrorDataStore {
   private:
      pthread_mutex_t mutex;              ///< used to secure other attributes within this object
      pthread_cond_t cond_var;            ///< used to wait for new events and to wakeup this thread
      const std::string mutex_name;       ///< unique mutex name
      volatile bool triggered;            ///< true if new events are pending that need to get processed
      lList *new_events;                  ///< new events that neet to get processed
      oge::DataStore::Id data_store_id;   ///< data store that is managed by this thread
      pthread_t thread{};                 ///< pthread that handles the mirroring
      sge_locktype_t lock_type;           ///< lock type used to secure the DS

      static void thread_cleanup_monitor(void *arg);
      static void thread_cleanup_event_client(void *arg);
      static void thread_cleanup_data_store([[maybe_unused]] void *unused);

   protected:
      sge_evc_class_t *evc = nullptr;

   public:
      explicit MirrorDataStore(oge::DataStore::Id data_store_id, sge_locktype_t lock_type);
      virtual ~MirrorDataStore() = default;

      virtual void wait_for_event(lList **event_list);
      virtual void wakeup();

      [[noreturn]] virtual void *main([[maybe_unused]] void *arg);
      virtual void subscribe_events() = 0;

      static void event_mirror_update_func([[maybe_unused]] u_long32 ec_id, [[maybe_unused]] lList **answer_list, lList *event_list, void *arg);

      friend void event_mirror_initialize();
      friend void event_mirror_terminate();
   };
}
