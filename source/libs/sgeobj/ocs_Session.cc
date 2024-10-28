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
#include <string>
#include <unordered_map>
#include <vector>

#include "basis_types.h"

#include "uti/sge_time.h"
#include "uti/sge_rmon_macros.h"

#include "ocs_Session.h"

pthread_mutex_t ocs::SessionManager::mutex = PTHREAD_MUTEX_INITIALIZER;
std::unordered_map<u_long64, ocs::SessionManager::Session> ocs::SessionManager::session_map;

/**
 * @brief Get a session ID that is unique for all requests of a user.
 *
 * Session ID 0 has a special meaning (GDI_SESSION_NONE). It cannot be used
 * to identify requests for a user therefore we use only one session for the
 * users whose hash would be 0 and 1.
 *
 * @param packet GDI packet
 * @return session ID
 */
u_long64
ocs::SessionManager::get_session_id(sge_gdi_packet_class_t *packet) {
   std::hash<std::string> hasher;
   std::string hash_input(packet->user);
   u_long64 session_id = hasher(hash_input);

   // avoid the use of session ID 0 (GDI_SESSION_NONE)
   // in has a hash is 0, we use 1 as session ID. This means
   // that there might be only one session for the two users whose hash is 0 and 1
   if (session_id == GDI_SESSION_NONE) {
      session_id = 1;
   }
   return session_id;
}

/**
 * @brief Set the unique ID for the last write event processed for the session.
 *
 * The unique ID is used to check if a session is up-to-date.
 *
 * Updates also the time when the session was accessed the last time.
 *
 * Implicitly creates a session if it does not exist.
 *
 * @param session_id session ID
 * @param write_event_id unique ID for the last write event
 */
void
ocs::SessionManager::set_write_unique_id(u_long64 session_id, u_long64 write_event_id) {
   Session s{};
   u_long64 time = sge_get_gmt64();

   pthread_mutex_lock(&mutex);
   auto it = session_map.find(session_id);
   if (it != session_map.end()) {
      s = it->second;
   }
   s.write_unique_id = write_event_id;

   // update the time when the session was accessed the last time
   if (session_id != GDI_SESSION_NONE) {
      s.write_time = time;
   }

   session_map[session_id] = s;
   pthread_mutex_unlock(&mutex);
}

/**
 * @brief Set the unique ID for the last event processed for the session.
 *
 * *processed* means 'handled by the mirror thread' of the corresponding DS.
 * If session_id is GDI_SESSION_NONE, the process_unique_id is set for all sessions.
 *
 * Implicitly creates a session if it does not exist and if the session_id is
 * a real session ID and not GDI_SESSION_NONE.
 *
 * Updates the time when the session was accessed the last time but only for
 * specific sessions.
 *
 * @param session_id session ID
 * @param process_event_id unique ID for the last event
 */
void
ocs::SessionManager::set_process_unique_id(u_long64 session_id, u_long64 process_event_id) {
   Session s{};

   pthread_mutex_lock(&mutex);
   if (session_id != GDI_SESSION_NONE) {
      auto it = session_map.find(session_id);
      if (it != session_map.end()) {
         s = it->second;
      }
      s.process_unique_id = process_event_id;
      session_map[session_id] = s;
   } else {
      for (auto & it : session_map) {
         s = it.second;
         s.process_unique_id = process_event_id;
         session_map[it.first] = s;
      }
   }
   pthread_mutex_unlock(&mutex);
}

/**
 * @brief Check if a sessions allows access for a reader
 *
 * @param session_id session ID
 * @param is_uptodate true if the access is allowed
 * @return true if the session exists
 */
bool
ocs::SessionManager::is_uptodate(u_long64 session_id, bool *is_uptodate) {
   bool ret = true;

   pthread_mutex_lock(&mutex);
   auto it = session_map.find(session_id);
   if (it != session_map.end()) {
      auto [write_time, write_event_id, process_event_id] = it->second;

      // all write events have been processed (==) => true
      // more events are processed than there are expected write events (<) => true
      // there are still write events that we expect that have not been processed (>) => false
      *is_uptodate = (write_event_id <= process_event_id);
      ret = true;
   } else {
      // session does not exist, and we have to handle a RO-request
      // We can be sure that there is no previous write request because
      // otherwise the session would exit => session is up-to-date
      *is_uptodate = true;
      ret = false;
   }
   pthread_mutex_unlock(&mutex);
   return ret;
}

/**
 * @brief Remove all sessions that have not been used for 15 minutes.
 */
void
ocs::SessionManager::remove_unused() {
   u_long64 time = sge_get_gmt64();
   u_long64 time_threshold = time - 15 * 60 * 1000000;

   pthread_mutex_lock(&mutex);
   for (auto it = session_map.begin(); it != session_map.end(); ) {

      // delete sessions that have a write time (== all expect for GDI_SESSION_NONE)
      // and that is older than 15 minutes (time since last write happened)
      if (it->second.write_time != 0 && it->second.write_time < time_threshold) {
         it = session_map.erase(it);
      } else {
         ++it;
      }
   }
   pthread_mutex_unlock(&mutex);
}

/**
 * @brief Event handler that removes unused sessions.
 *
 * @param anEvent event
 * @param monitor monitoring object
 */
void
ocs::SessionManager::session_cleanup_handler(te_event_t anEvent, monitoring_t *monitor) {
   DENTER(TOP_LAYER);
   remove_unused();
   DRETURN_VOID;
}

/**
 * @brief Dump all sessions to the debug output.
 */
void
ocs::SessionManager::dump_all() {
   DENTER(TOP_LAYER);
   pthread_mutex_lock(&mutex);
   for (auto & [session_id, session] : session_map) {
      DPRINTF("session %20lu: write_time=%16lu, write_unique_id=%lu, process_unique_id=%lu\n",
              session_id, session.write_time, session.write_unique_id, session.process_unique_id);
   }
   pthread_mutex_unlock(&mutex);
   DRETURN_VOID;
}

