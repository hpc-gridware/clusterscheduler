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

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <random>

#include "uti/sge_time.h"

#include "sgeobj/ocs_Session.h"

// Function to generate a random string of given length
std::string generate_random_string(size_t length) {
    const std::string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> distribution(0, characters.size() - 1);

    std::string random_string;
    for (size_t i = 0; i < length; ++i) {
        random_string += characters[distribution(generator)];
    }
    return random_string;
}

static bool
check_session_performance(u_long64 sessions) {
   bool ret = true;
   std::vector<std::string> user_and_hostnames;

   for (u_long64 i = 0; i < sessions; i++) {
      user_and_hostnames.push_back(std::to_string(i));
      if (i % 50000 == 0) {
         std::cout << "." << std::flush;
      }
   }
   std::cout << std::endl;
   std::cout << "Generated " << sessions << " names" << std::endl;

   u_long64 start = sge_get_gmt64();

   // Create sessions
   for (auto &name: user_and_hostnames) {
      u_long64 session_id = ocs::SessionManager::get_session_id(name.c_str(), "hostname");
      ocs::SessionManager::set_write_unique_id(session_id, 1);
   }
   std::cout << "Created " << sessions << " sessions" << std::endl;

   // Update sessions
   for (u_long64 i = 0; i < sessions; i++) {
      ocs::SessionManager::set_process_unique_id(i);
   }
   std::cout << "Updated " << sessions << " sessions" << std::endl;

   // Check sessions
   for (auto &name: user_and_hostnames) {
      u_long64 session_id = ocs::SessionManager::get_session_id(name.c_str(), "hostname");
      ocs::SessionManager::is_uptodate_for_anyone(session_id);
   }
   std::cout << "Checked " << sessions << " sessions" << std::endl;

   // Remove sessions
   u_long64 end = sge_get_gmt64();
   u_long32 delta = end - start;
   std::cout << "Time for " << sessions << " sessions: " << delta << " us" << std::endl;

   // Check if the time is within reasonable limits (5 us per session (including creation, update, and check))
   if (delta > 5 * sessions) {
      std::cout << "ERROR: Time for " << sessions << " sessions is too high" << std::endl;
      ret = false;
   }

   return ret;
}

static bool
check_session_functionality() {
   bool ret = true;

   // Create a session
   u_long64 session_id = ocs::SessionManager::get_session_id(generate_random_string(1000).c_str(), "hostname");
   assert(session_id != ocs::SessionManager::GDI_SESSION_NONE);

   // Check if the session is up-to-date
   ret &= ocs::SessionManager::is_uptodate_for_anyone(session_id);

   // Set the write unique ID
   ocs::SessionManager::set_write_unique_id(session_id, 1);
   ocs::SessionManager::set_process_unique_id(0);

   // Check if the session is up-to-date
   if (ocs::SessionManager::is_uptodate_for_anyone(session_id) != false) {
      std::cout << "Session is up-to-date although this is not expected" << std::endl;
      ret = false;
   }

   // Set the process unique ID
   ocs::SessionManager::set_process_unique_id(1);

   // Check if the session is up-to-date
   if (ocs::SessionManager::is_uptodate_for_anyone(session_id) != true) {
      std::cout << "Session is not up-to-date although it should be" << std::endl;
      ret = false;
   }

   // Set the process unique ID
   ocs::SessionManager::set_process_unique_id(5);

   // Check if the session is up-to-date
   if (ocs::SessionManager::is_uptodate_for_anyone(session_id) != true) {
      std::cout << "Session is not up-to-date although it should be" << std::endl;
      ret = false;
   }

   return ret;
}

int
main(int argc, char *argv[]) {
   bool ret = true;

   ret &= check_session_functionality();
   ret &= check_session_performance(500 * 10000); // 500 users with 10000 hosts

   return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
