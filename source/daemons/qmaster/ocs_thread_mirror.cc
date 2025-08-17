/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"

#include "ocs_thread_mirror.h"
#include "ocs_MirrorReaderDataStore.h"
#include "ocs_MirrorListenerDataStore.h"
#include "sge_thread_main.h"
#include "msg_qmaster.h"

namespace ocs {
   static void *
   event_mirror_main(void *arg) {
      auto *mirror_thread = static_cast<ocs::MirrorDataStore *>(arg);

      return mirror_thread->main(arg);
   }

   void
   event_mirror_initialize() {
      DENTER(TOP_LAYER);

      /*
       * TODO: EB: corresponding destroy function is missing during shutdown
       */
      sge_tq_create(&ReaderRequestQueue);
      sge_tq_create(&ReaderWaitingRequestQueue);

      // create reader mirror
      auto reader_mirror_thread = new ocs::MirrorReaderDataStore();
      Main_Control.mirror_thread_pool.push_back(reader_mirror_thread);
      pthread_create(&reader_mirror_thread->thread, nullptr, event_mirror_main, reader_mirror_thread);
      DPRINTF("added event mirror thread for data store %d\n", reader_mirror_thread->data_store_id);

      // create listener mirror
      auto listener_mirror_thread = new ocs::MirrorListenerDataStore();
      Main_Control.mirror_thread_pool.push_back(listener_mirror_thread);
      pthread_create(&listener_mirror_thread->thread, nullptr, event_mirror_main, listener_mirror_thread);
      DPRINTF("added event mirror thread for data store %d\n", listener_mirror_thread->data_store_id);

      DRETURN_VOID;
   }

   /** @brief Block until all initial events have been handled by the mirror threads
    */
   void
   event_mirror_block_till_initial_events_handled() {
      for (auto mirror_thread: Main_Control.mirror_thread_pool) {
         mirror_thread->block_till_initial_events_handled();
      }
   }

   void
   event_mirror_terminate() {
      DENTER(TOP_LAYER);

      // trigger cancel and wakeup so that all threads can reach the cancellation point
      for (auto mirror_thread: Main_Control.mirror_thread_pool) {
         DPRINTF("triggered shutdown/wake up of mirror thread for data store %d\n", mirror_thread->data_store_id);

         pthread_cancel(mirror_thread->thread);
         mirror_thread->wakeup();
      }

      // wait till each thread returns from its main
      for (auto mirror_thread: Main_Control.mirror_thread_pool) {
         pthread_join(mirror_thread->thread, nullptr);
         DPRINTF("termination of thread for data store %d finished\n", mirror_thread->data_store_id);
         delete mirror_thread;
      }

      // empty the container
      Main_Control.mirror_thread_pool.clear();

      INFO(MSG_THREADPOOL_XTERMINATED_S, threadnames[EVENT_MIRROR_THREAD]);

      DRETURN_VOID;
   }
}
