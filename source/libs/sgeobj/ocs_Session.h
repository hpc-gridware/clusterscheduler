#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief GDI sessions, giving a client read-after-write consistency
 */

#include <unordered_map>

#include <pthread.h>

#include "uti/sge_monitor.h"

#include "gdi/ocs_gdi_Packet.h"

#include "sge_qmaster_timed_event.h"

namespace ocs {
   /**
    * @brief Read-after-write consistency for clients that use the snapshot data stores
    *
    * A snapshot store lags the read-write store by the event delivery. Without
    * this a client that just submitted a job could ask a reader thread and not
    * see it. Each user gets a session recording the id of their last write; a
    * later read waits until the snapshot has caught up to that id.
    */
   class SessionManager {
   private:
      struct Session {
         uint64_t write_time;           ///< time when write_unique_id was set
         uint64_t write_unique_id;      ///< unique id for the last write event
      };

      static pthread_mutex_t mutex;                             ///< mutex that saves access to the session_map
      static std::unordered_map<uint64_t, Session> session_map; ///< hashtable for sessions
      static uint64_t process_unique_id;                        ///< unique id for the last processed event

      static void remove_unused();

   public:
      /// No session: the request has nothing to wait for and may be answered from any snapshot
      static constexpr uint64_t GDI_SESSION_NONE = 0LL;
      static uint64_t get_session_id(const char *user);

      static void set_write_unique_id(const uint64_t session_id, const uint64_t write_event_id);
      static void set_process_unique_id(const uint64_t process_event_id);
      static bool is_uptodate(const uint64_t session_id);

      static void session_cleanup_handler(te_event_t anEvent, monitoring_t *monitor);

      static void dump_all();
   };
}
