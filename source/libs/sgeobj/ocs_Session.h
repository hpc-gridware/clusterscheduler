#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2024 HPC-Gridware GmbH
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

#include <unordered_map>

#include <pthread.h>

#include "uti/sge_monitor.h"

#include "gdi/ocs_gdi_Packet.h"

#include "sge_qmaster_timed_event.h"

namespace ocs {
   class SessionManager {
   private:
      struct Session {
         u_long64 write_time;           //< time when write_unique_id was set
         u_long64 write_unique_id;      //< unique id for the last write event
      };

      static pthread_mutex_t mutex;                             //< mutex that saves access to the session_map
      static std::unordered_map<u_long64, Session> session_map; //< hashtable for sessions
      static u_long64 process_unique_id;                        //< unique id for the last processed event

      static void remove_unused();

   public:
      static constexpr u_long64 GDI_SESSION_NONE = 0LL;
      static u_long64 get_session_id(const char *user);

      static void set_write_unique_id(const u_long64 session_id, const u_long64 write_event_id);
      static void set_process_unique_id(const u_long64 process_event_id);
      static bool is_uptodate(const u_long64 session_id);

      static void session_cleanup_handler(te_event_t anEvent, monitoring_t *monitor);

      static void dump_all();
   };
}
