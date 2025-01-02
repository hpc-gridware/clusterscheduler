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

#include "mir/sge_mirror.h"

#include <sgeobj/ocs_Session.h>

#include "ocs_MirrorReaderDataStore.h"

#include <sge_tq.h>

void ocs::MirrorReaderDataStore::subscribe_events() {
   sge_mirror_subscribe(evc, SGE_TYPE_ALL, nullptr, nullptr, nullptr, nullptr, nullptr);
   evc->ec_set_flush(evc, sgeE_ALL_EVENTS, true, 0);
   evc->ec_set_edtime(evc, 1);
}

void ocs::MirrorReaderDataStore::update_sessions_and_move_requests(const u_long64 unique_id){
   DENTER(TOP_LAYER);

   // Update the session with the unique ID of the last event
   SessionManager::set_process_unique_id(unique_id);

   // Move waiting reader requests that can now be handled
   // to the reader request queue (or global request queue if readers are disabled)
   const int moved_elements = sge_tq_move_from_to_if(ReaderWaitingRequestQueue, ReaderRequestQueue,
                                                    [](const void *always_nullptr, const void *task_void) -> int {
      // Find the packet stored in the task of the TQ
      const auto *task = *static_cast<sge_tq_task_t *const *>(task_void);
      const auto *packet = static_cast<ocs::GdiPacket *>(task->data);

      // Check if the session is up-to-date
      const u_long64 session_id = SessionManager::get_session_id(packet->user);
      const bool is_uptodate = SessionManager::is_uptodate(session_id);

      // Return the outcome so that the task is moved
      return is_uptodate ? 0 : -1;
   });

   // Wake up the reader request queue if there are tasks to handle
   if (moved_elements > 0) {
      DPRINTF("Moved %d elements so that they can now be handled by reader threads\n", moved_elements);
      sge_tq_wakeup_waiting(ReaderRequestQueue);
   }
   DRETURN_VOID;
}
