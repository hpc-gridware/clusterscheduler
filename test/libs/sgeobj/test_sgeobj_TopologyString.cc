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

#if 0
bool
test_construction_of_topo_string_symmetric() {
   DENTER(TOP_LAYER);

   bool ret = true;

   ocs::TopologyString topo1("NXSYCtTYCTTYetETYETETNSYCTTYCTTYETETYETET");
   //ocs::TopologyString topo1("SCttCTTETETETETSCTTCTTETETETET");
   //ocs::TopologyString topo1("SCTTTTCTTTTSCTTTTCTTTT");
   topo1.parse_to_tree();
   topo1.print();

   DRETURN(ret);
}
#endif

bool
test_sort_by_characteristic() {
   DENTER(TOP_LAYER);

   ocs::TopologyString topo1("(N[size=134782259200](S(X[size=25165824](Y[size=1310720](C(T)(T)))(Y[size=1310720](C(t)(T)))(y[size=1310720](c(t)(t)))(Y[size=1310720](C(T)(t)))(Y[size=1310720](C(T)(T)))(Y[size=1310720](C(T)(T)))(Y[size=2097152](E(T))(E(T))(e(t))(E(T)))(Y[size=2097152](E(T))(E(T))(E(T))(E(T))))))");
   topo1.parse_to_tree();

   topo1.print();
   std::cout << std::endl;

   topo1.print(true);
   std::cout << std::endl;

   topo1.print(true, true, true);
   std::cout << std::endl;

   topo1.sort_tree("NSAXYCET", 't'); // empty dies first
   topo1.print(true, true, true, true, true);

   DRETURN(true);
}

int main (int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_HostTopology");

   //bool ret = test_construction_of_topo_string_symmetric();
   bool ret = test_sort_by_characteristic();

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
