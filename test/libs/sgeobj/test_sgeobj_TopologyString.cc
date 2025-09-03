/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include "ocs_TopologyString.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"
#include "uti/sge_dstring.h"

#include "sgeobj/ocs_HostTopology.h"

bool
test_find_first_unused_thread(const char *topology, int expected_pos, int expected_socket, int expected_core, int expected_thread) {
   DENTER(TOP_LAYER);

   int p, s, c, t;
   ocs::TopologyString topo(topology);
   bool ret = topo.find_first_unused_thread(&p, &s, &c, &t);
   if (expected_pos >= 0) {
      if (ret != true) {
         std::cerr << "Expected to find a thread in " << topology << ", but did not." << std::endl;
         return false;
      }
   }

   if (p != expected_pos) {
      std::cerr << "Expected to find a thread in " << topology << " at pos " << expected_pos << ", but got " << p << "." << std::endl;
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
   bool ret = test_find_first_unused_thread("(S(c(t)(t))(C(T)(t)))(S(C(t)(T))(c(t)(t)))", 6, 0, 1, 0);
   ret &= test_find_first_unused_thread("(s(c(t)(t)))(S(C(t)(T)))", 8, 1, 0, 1);

   // Asymmetric and additional letters for NUMA nodes and caches
   ret &= test_find_first_unused_thread("(n(s(x(c(t))(C(t)(T)))))(n(s(x(c(t)))))", 8, 0, 1, 1);

   // No threads available
   ret &= test_find_first_unused_thread("(s(c(t)(t)))(s(c(t)(t)))", -1, -1, -1, -1);
   DRETURN(ret);
}

bool
test_correct_topology_string(const std::string &name, char *topology, const char *expected_topology) {
   DENTER(TOP_LAYER);

   ocs::TopologyString topo(topology);
   topo.correct_topology_upper_lower();
   std::string topology_str = topo.to_string(true, true);
   if (strcmp(expected_topology, topology_str.c_str()) != 0) {
      std::cerr << name <<  ": Expected corrected topology string " << expected_topology << ", but got " << topology_str << "." << std::endl;
      DRETURN(false);
   }

   DRETURN(true);
}

bool
test_correct_topology_scenarios() {
   DENTER(TOP_LAYER);
   bool ret = true;

   char topology1[] = "(S(c(t)(t))(C(T)(t)))(S(C(t)(T))(c(t)(t)))";
   char topologyA[] = "(S(c(t)(t))(C(T)(t)))(S(C(t)(T))(c(t)(t)))";
   ret &= test_correct_topology_string("A", topology1, topologyA);

   char topology2[] = "(n(s(X(c(t))(C(t)(T)))))(n(s(x(c(t)))))";
   char topologyB[] = "(N(S(X(c(t))(C(t)(T)))))(n(s(x(c(t)))))";
   ret &= test_correct_topology_string("B", topology2, topologyB);

   char topology3[] = "(n(s(X(c(t)(T))(C(t)))))(N(s(x(C(t)))))";
   char topologyC[] = "(N(S(X(C(t)(T))(c(t)))))(n(s(x(c(t)))))";
   ret &= test_correct_topology_string("C", topology3, topologyC);

   char topology4[] = "(n(s(c(t)(t))))(n(s(c(T)(T))))";
   char topologyD[] = "(n(s(c(t)(t))))(N(S(C(T)(T))))";
   ret &= test_correct_topology_string("D", topology4, topologyD);

   char topology5[] = "(s(c(t)(t))(c(T)(t)))(s(c(t)(T))(c(t)(t)))";
   char topologyE[] = "(S(c(t)(t))(C(T)(t)))(S(C(t)(T))(c(t)(t)))";
   ret &= test_correct_topology_string("E", topology5, topologyE);

   DRETURN(ret);
}

bool
test_sort_by_characteristic_test(const std::string &name, ocs::TopologyString &topo,
                                 const std::string &sort_criteria, char sort_characteristic,
                                 const std::string &expected_after) {
   DENTER(TOP_LAYER);

#if 0
   std::string before = topo.to_string(true, false, false, false);
#endif

   topo.sort_tree(sort_criteria, sort_characteristic); // empty dies first
   std::string after = topo.to_string(true, false, false, false);

#if 0
   std::cout << topo.to_string(true, true, true, true, true) << std::endl;
   std::cout << before << std::endl;
   std::cout << after << std::endl;
#endif

   if (after != expected_after) {
      std::cerr << "test_sort_by_characteristic_scenario " << name << " failed: got " << after << " instead of " << expected_after << std::endl;
      DRETURN(false);
   }
   DRETURN(true);
}

bool
test_sort_by_characteristic_scenarios() {
   DENTER(TOP_LAYER);
   bool ret = true;

   // topo string with C-cores that are partially or completely used and one E core that is completely used
   // NSXYCTTYCtTycttYCTtYCTTYCTTYETETetETYETETETET
   ocs::TopologyString topoA("(N[size=134782259200](S(X[size=25165824](Y[size=1310720](C(T)(T)))(Y[size=1310720](C(t)(T)))(y[size=1310720](c(t)(t)))(Y[size=1310720](C(T)(t)))(Y[size=1310720](C(T)(T)))(Y[size=1310720](C(T)(T)))(Y[size=2097152](E(T))(E(T))(e(t))(E(T)))(Y[size=2097152](E(T))(E(T))(E(T))(E(T))))))");
   std::string sort_criteria = "NSAXYCET"; // all empty nodes first
   std::string expected_after = "NSXYETETETETYETETETetYCTTYCTTYCTTYCTtYCTtyctt";
   char sort_characteristic = 't';
   ret &= test_sort_by_characteristic_test("A" , topoA,  sort_criteria, sort_characteristic, expected_after);

   // nodes with the most utilized threads first
   ocs::TopologyString topoB("(N[size=134782259200](S(X[size=25165824](Y[size=1310720](C(T)(T)))(Y[size=1310720](C(t)(T)))(y[size=1310720](c(t)(t)))(Y[size=1310720](C(T)(t)))(Y[size=1310720](C(T)(T)))(Y[size=1310720](C(T)(T)))(Y[size=2097152](E(T))(E(T))(e(t))(E(T)))(Y[size=2097152](E(T))(E(T))(E(T))(E(T))))))");
   sort_criteria = "nsaxycet"; // all "full" nodes frist
   expected_after = "NSXycttYCtTYCtTYCTTYCTTYCTTYetETETETYETETETET";
   sort_characteristic = 't';
   ret &= test_sort_by_characteristic_test("B" , topoB,  sort_criteria, sort_characteristic, expected_after);

   // sorted all according to utilized threads except of Y caches
   ocs::TopologyString topoC("(N[size=134782259200](S(X[size=25165824](Y[size=1310720](C(T)(T)))(Y[size=1310720](C(t)(T)))(y[size=1310720](c(t)(t)))(Y[size=1310720](C(T)(t)))(Y[size=1310720](C(T)(T)))(Y[size=1310720](C(T)(T)))(Y[size=2097152](E(T))(E(T))(e(t))(E(T)))(Y[size=2097152](E(T))(E(T))(E(T))(E(T))))))");
   sort_criteria = "nsaxYcet"; // all "full" nodes frist
   expected_after = "NSXYETETETETYetETETETYCTTYCTTYCTTYCtTYCtTyctt";
   sort_characteristic = 't';
   ret &= test_sort_by_characteristic_test("C" , topoC,  sort_criteria, sort_characteristic, expected_after);

   DRETURN(ret);
}

bool
test_find_first_unused_thread(const char name, const std::string &topo_string,
                              const int expected_id, const int expected_socket, const int expected_core, const int expected_thread) {
   DENTER(TOP_LAYER);
   bool ret = true;

   ocs::TopologyString topo(topo_string);
   int id, socket, core, thread;
   if (topo.find_first_unused_thread(&id, &socket, &core, &thread)) {
      if (id != expected_id || socket != expected_socket || core != expected_core || thread != expected_thread) {
         std::cerr << "Scenario " << name << ": Expected to find unused thread at " << expected_id << ", " << expected_socket << ", " << expected_core << ", " << expected_thread <<
            ", but got " << id << ", " << socket << ", " << core << ", " << thread << "." << std::endl;
         ret = false;
      }
   } else {
      if (id != expected_id || socket != expected_socket || core != expected_core || thread != expected_thread) {
         std::cerr << "Scenario " << name << ": Expected not to find unused thread at " << expected_id << ", " << expected_socket << ", " << expected_core << ", " << expected_thread <<
            ", but got " << id << ", " << socket << ", " << core << ", " << thread << "." << std::endl;
         ret = false;
      }
   }
   DRETURN(ret);
}

bool test_find_first_unused_thread_scenarios() {
   DENTER(TOP_LAYER);
   bool ret = true;

   std::string topoA("(N(S(X(Y(C(T)(T)))(Y(C(t)(T)))(y(c(t)(t)))(Y(C(T)(t)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(E(T))(E(T))(e(t))(E(T)))(Y(E(T))(E(T))(E(T))(E(T))))))");
   ret &= test_find_first_unused_thread('A', topoA, 6, 0, 0, 0);

   std::string topoB("(N(S(X(y(c(t)(t)))(Y(C(t)(T)))(y(c(t)(t)))(Y(C(T)(t)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(E(T))(E(T))(e(t))(E(T)))(Y(E(T))(E(T))(E(T))(E(T))))))");
   ret &= test_find_first_unused_thread('B', topoB, 11, 0, 1, 1);

   std::string topoC("(n(s(x(y(c(t)(t)))(y(c(t)(t)))))(S(X(y(c(t)(t)))(Y(C(T)(t))))))");
   ret &= test_find_first_unused_thread('C', topoC, 20, 1, 1, 0);

   std::string topoD("(n(s(x(y(c(t)(t)))(y(c(t)(t)))))(s(x(y(c(t)(t)))(y(c(t)(t))))))");
   ret &= test_find_first_unused_thread('D', topoD, -1, -1, -1, -1);
   DRETURN(ret);
}

bool test_to_unused_internal_topoloy_string_scenario(char name, const std::string &topo_string, const std::string &expected_result) {
   DENTER(TOP_LAYER);
   bool ret = true;
   ocs::TopologyString topo(topo_string);
   std::string result = topo.to_unused_internal_topology_string();
   if (result != expected_result) {
      std::cerr << "Scenario " << name << ": Expected to get " << expected_result << ", but got " << result << "." << std::endl;
      ret = false;
   }
   DRETURN(ret);
}

bool test_to_unused_internal_topoloy_string() {
   DENTER(TOP_LAYER);
   bool ret = true;

   std::string topo_A("(n(S(X(y(c(t)(t)))(Y(C(T)(t)))))(S(X(y(c(t)(t)))(Y(C(T)(t))))))");
   std::string exp_A = "(N(S(X(Y(C(T)(T)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(Y(C(T)(T))))))";
   ret &= test_to_unused_internal_topoloy_string_scenario('A', topo_A, exp_A);

   DRETURN(ret);
}

bool test_mark_as_used_scenario(char name, const std::string &topo_string, const int id, bool mark_used, const std::string expected_result) {
   DENTER(TOP_LAYER);
   bool ret = true;
   ocs::TopologyString topo(topo_string);
   topo.mark_node_as_used_or_unused(id, mark_used);
   std::string result = topo.to_string(true, true, true, false, false, false);
   if (result != expected_result) {
      std::cerr << "Scenario " << name << ": Expected to get " << expected_result << ", but got " << result << "." << std::endl;
      ret = false;
   }
   DRETURN(ret);
}

bool test_mark_as_used_scenarios() {
   DENTER(TOP_LAYER);
   bool ret = true;
   std::string topo_A("(N(S(X(Y(C(T)(T)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(Y(C(T)(T))))))");
   std::string exp_A( "(N(S(X(y(c(t)(t)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(Y(C(T)(T))))))");
   ret &=test_mark_as_used_scenario('A', topo_A, 4, true, exp_A);

   std::string topo_B( "(N(S(X(Y(C(T)(T)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(Y(C(T)(T))))))");
   std::string exp_B(  "(N(S(X(Y(C(t)(T)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(Y(C(T)(T))))))");
   ret &=test_mark_as_used_scenario('B', topo_B, 6, true, exp_B);

   std::string topo_C( "(N(S(X(Y(C(T)(T)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(y(c(t)(t))))))");
   std::string exp_C(  "(N(S(X(Y(C(T)(T)))(Y(C(T)(T)))))(s(x(y(c(t)(t)))(y(c(t)(t))))))");
   ret &=test_mark_as_used_scenario('C', topo_C, 14, true, exp_C);
   DRETURN(ret);
}

bool test_mark_nodes_as_used_scenario(const std::string &name, const std::string &topo_string_A, const std::string &topo_string_B, const std::string &expected_result, bool mark_used) {
   DENTER(TOP_LAYER);
   bool ret = true;

   ocs::TopologyString topo_A(topo_string_A);
   ocs::TopologyString topo_B(topo_string_B);

   topo_A.mark_nodes_as_used_or_unused(topo_B, mark_used);
   std::string result_A = topo_A.to_string(true, true, true, false, false, false);
   if (result_A != expected_result) {
      std::cerr << "Scenario " << name << ": Expected to get " << expected_result << ", but got " << result_A << "." << std::endl;
      ret = false;
   }
   DRETURN(ret);
}

bool test_mark_nodes_as_unused() {
   DENTER(TOP_LAYER);
   bool ret = true;
   std::string topo1_A("(N(S(X(Y(C(T)(T)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(Y(C(T)(T))))))");
   std::string topo2_A( "(N(S(X(y(c(t)(t)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(Y(C(T)(T))))))");
   std::string exp_A1( "(N(S(X(y(c(t)(t)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(Y(C(T)(T))))))");
   std::string exp_A2("(N(S(X(Y(C(T)(T)))(Y(C(T)(T)))))(S(X(Y(C(T)(T)))(Y(C(T)(T))))))");
   // add used as used to another topo mask
   ret &= test_mark_nodes_as_used_scenario("A1a", topo1_A, topo2_A, exp_A1, true);

   // add used to a topo mask where the same is already marked as used
   ret &= test_mark_nodes_as_used_scenario("A1b", topo1_A, topo2_A, exp_A1, true);

   // remove used from a topo mask
   ret &= test_mark_nodes_as_used_scenario("A2", topo1_A, topo2_A, exp_A2, false);

   DRETURN(ret);
}

int main (int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_HostTopology");

   bool ret = test_find_first_unused_scenarios();
   ret &= test_correct_topology_scenarios();
   ret &= test_sort_by_characteristic_scenarios();
   ret &= test_find_first_unused_thread_scenarios();
   ret &= test_to_unused_internal_topoloy_string();
   ret &= test_mark_as_used_scenarios();
   ret &= test_mark_nodes_as_unused();

#if 0
   std::string unit = "CT";
   std::string strategy;
   std::string filter;
   std::string sort;
   std::string start = "C";
   std::string stop = "t";
   std::cout << "Result: " << topo1.get_packed_binding(2, unit, strategy, filter, sort, start, stop) << std::endl;
#endif
   if (!ret) {
      std::cerr << "Test failed." << std::endl;
      DRETURN(1);
   }
   DRETURN(0);
}
