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

#include <iostream>
#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"
#include "uti/sge_dstring.h"

#include "sgeobj/ocs_HostTopology.h"

bool
test_find_first_unused_thread(const char *topology, int expected_pos, int expected_socket, int expected_core, int expected_thread) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(topology_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   sge_dstring_copy_string(&topology_dstr, topology);
   int p, s, c, t;
   bool ret = ocs::HostTopology::find_first_unused_thread(&topology_dstr, &p, &s, &c, &t);
   if (expected_pos >= 0) {
      if (ret != true) {
         std::cerr << "Expected to find a thread in " << topology << ", but did not." << std::endl;
         return false;
      }
   }

   if (p != expected_pos) {
      std::cerr << "Expected to find a thread in " << topology << " at pos " << expected_pos << ", but got" << p << "." << std::endl;
   }
   if (s != expected_socket || c != expected_core || t != expected_thread) {
      std::cerr << "Expected to find a thread in " << topology << "at " << expected_socket << "," << expected_core << "," << expected_thread <<
         ", but got " << s << "," << c << "," << t << "." << std::endl;
   }
   DRETURN(true);
}

bool
test_find_first_unused_scenarios() {
   DENTER(TOP_LAYER);

   // Symmetric without other information
   bool ret = test_find_first_unused_thread("ScttCTtSCtTctt", 5, 0, 1, 0);
   ret &= test_find_first_unused_thread("scttSCtT", 7, 1, 0, 1);

   // Asymmetric and additional letters for NUMA nodes and caches
   ret &= test_find_first_unused_thread("nsxctCtTnsxct", 7, 0, 1, 1);

   // No threads available
   ret &= test_find_first_unused_thread("scttsctt", -1, -1, -1, -1);
   DRETURN(ret);
}

bool
test_correct_topology_string(char *topology, const char *expected_topology) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(topology_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   sge_dstring_copy_string(&topology_dstr, topology);
   ocs::HostTopology::correct_topology_upper_lower(&topology_dstr);
   if (strcmp(expected_topology, sge_dstring_get_string(&topology_dstr)) != 0) {
      std::cerr << "Expected corrected topology string " << expected_topology << ", but got " << topology << "." << std::endl;
      DRETURN(false);
   }

   DRETURN(true);
}

bool
test_correct_topology_scenarios() {
   DENTER(TOP_LAYER);

   char topology1[] = "ScttCTtSCtTctt";
   bool ret = test_correct_topology_string(topology1, "ScttCTtSCtTctt");

   char topology2[] = "nsXctCtTnsxct";
   ret &= test_correct_topology_string(topology2, "NSXctCtTNsXct");

   char topology3[] = "nsXctTCtnsxct";
   ret &= test_correct_topology_string(topology3, "NSXCtTctNsXct");

   char topology4[] = "nscttnscTT";
   ret &= test_correct_topology_string(topology4, "NscttNSCTT");

   char topology5[] = "scttcTtsctTctt";
   ret &= test_correct_topology_string(topology5, "ScttCTtSCtTctt");

   DRETURN(ret);
}

bool
test_add_or_remove_used_threads_test(const char *topology, const char *topology_in_use, bool do_add, const char *expected_topology) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(topology_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   sge_dstring_copy_string(&topology_dstr, topology);
   DSTRING_STATIC(topology_in_use_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   sge_dstring_copy_string(&topology_in_use_dstr, topology_in_use);

   if (do_add) {
      ocs::HostTopology::add_used_threads(&topology_dstr, &topology_in_use_dstr);
   } else {
      ocs::HostTopology::remove_used_threads(&topology_dstr, &topology_in_use_dstr);
   }

   if (strcmp(expected_topology, sge_dstring_get_string(&topology_dstr)) != 0) {
      std::cerr << "Expected modified topology string " << expected_topology << ", but got " << sge_dstring_get_string(&topology_dstr) << "." << std::endl;
      DRETURN(false);
   }

   DRETURN(true);
}

bool
test_add_or_remove_used_threads_scenarios() {
   DENTER(TOP_LAYER);

   bool ret = true;

   // Add threads
   ret &= test_add_or_remove_used_threads_test("SCTT", "SCTt", true, "SCTt");
   ret &= test_add_or_remove_used_threads_test("SCTT", "SCtt", true, "sctt");
   ret &= test_add_or_remove_used_threads_test("SCtT", "SCTt", true, "sctt");

   // Remove threads
   ret &= test_add_or_remove_used_threads_test("SCTt", "SCTt", false, "SCTT");
   ret &= test_add_or_remove_used_threads_test("sctt", "SCtt", false, "SCTT");
   ret &= test_add_or_remove_used_threads_test("sctt", "SCTt", false, "SCtT");


   DRETURN(ret);
}

bool
test_add_used_thread_with_pos_test(const char *topology, bool do_add, int pos, const char *expected_topology) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(topology_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   sge_dstring_copy_string(&topology_dstr, topology);

   if (do_add) {
      ocs::HostTopology::add_used_thread(&topology_dstr, pos);
   } else {
      ocs::HostTopology::remove_used_thread(&topology_dstr, pos);
   }

   if (strcmp(expected_topology, sge_dstring_get_string(&topology_dstr)) != 0) {
      std::cerr << "Expected modified topology string " << expected_topology << ", but got " << sge_dstring_get_string(&topology_dstr) << "." << std::endl;
      DRETURN(false);
   }

   DRETURN(true);
}

bool
test_add_or_remove_used_threads_with_pos_scenarios() {
   DENTER(TOP_LAYER);

   bool ret = true;

   // Add threads by position
   ret &= test_add_used_thread_with_pos_test("SCTT", true, 2, "SCtT");
   ret &= test_add_used_thread_with_pos_test("SCTT", true, 3, "SCTt");
   ret &= test_add_used_thread_with_pos_test("SCtT", true, 3, "sctt");
   ret &= test_add_used_thread_with_pos_test("SCtT", true, 2, "SCtT");

   // Remove threads by position
   ret &= test_add_used_thread_with_pos_test("SCtT", false, 2, "SCTT");
   ret &= test_add_used_thread_with_pos_test("SCTt", false, 3, "SCTT");
   ret &= test_add_used_thread_with_pos_test("sctt", false, 3, "SCtT");
   ret &= test_add_used_thread_with_pos_test("SCtT", false, 3, "SCtT");

   DRETURN(ret);
}

bool
test_remove_all_used_threads_test(char *topology, const char *expected_topology) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(topology_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   sge_dstring_copy_string(&topology_dstr, topology);
   ocs::HostTopology::remove_all_used_threads(&topology_dstr);
   if (strcmp(expected_topology, sge_dstring_get_string(&topology_dstr)) != 0) {
      std::cerr << "Expected all used threads removed from topology but got " << topology << "." << std::endl;
      DRETURN(false);
   }

   DRETURN(true);
}

bool
test_remove_all_used_threads_scenarios() {
   DENTER(TOP_LAYER);

   bool ret = true;

   // Remove all used threads
   char topology1[] = "SCtT";
   ret &= test_remove_all_used_threads_test(topology1, "SCTT");

   char topology2[] = "NsXctCtTNsXct";
   ret &= test_remove_all_used_threads_test(topology2, "NSXCTCTTNSXCT");


   DRETURN(ret);
}

bool
test_correct_topology_missing_threads_test(char *topology, const char *expected_topology) {
   DENTER(TOP_LAYER);

   DSTRING_STATIC(topology_dstr, ocs::HostTopology::MAX_TOPOLOGY_LENGTH);
   sge_dstring_copy_string(&topology_dstr, topology);
   ocs::HostTopology::correct_topology_missing_threads(&topology_dstr);
   if (strcmp(expected_topology, sge_dstring_get_string(&topology_dstr)) != 0) {
      std::cerr << "Expected corrected topology string " << expected_topology << ", but got " << topology << "." << std::endl;
      DRETURN(false);
   }

   DRETURN(true);
}

bool
test_correct_topology_missing_threads_scenarios() {
   DENTER(TOP_LAYER);

   bool ret = true;

   // Correct topology by adding missing threads
   char topology1[] = "scCcC";
   ret &= test_correct_topology_missing_threads_test(topology1, "sctCTctCT");

   char topology2[] = "scCEEscTTetT";
   ret &= test_correct_topology_missing_threads_test(topology2, "sctCTETETscTTetT");

   DRETURN(ret);
}

int main (int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_HostTopology");

   bool ret = test_find_first_unused_scenarios();
   ret &= test_correct_topology_scenarios();
   ret &= test_add_or_remove_used_threads_scenarios();
   ret &= test_add_or_remove_used_threads_with_pos_scenarios();
   ret &= test_remove_all_used_threads_scenarios();
   ret &= test_correct_topology_missing_threads_scenarios();

   if (!ret) {
      std::cerr << "Test failed." << std::endl;
      DRETURN(1);
   }
   DRETURN(0);
}