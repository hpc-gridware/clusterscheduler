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
#include "uti/ocs_Topo.h"

#include "sgeobj/ocs_HostTopology.h"

#include <hwloc.h>
#include <hwloc/helper.h>



// (N(S(X(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(D(T))(D(T))(D(T))(D(T)))(Y(D(T))(D(T))(D(T))(D(T))))))
// (N(S(X(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T))))))(N(S(X(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T))))))
// (N(S(X(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T))))))(N(S(X(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T)))(Y(C(T)(T))))))


int main (int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_sgeobj_HostTopology");

   bool ret = true;

   if (!ocs::Topo::has_core_binding()) {
      std::cerr << "Your kernel seems not to offer core binding capabilities for HWLOC!" << std::endl;
      DRETURN(1);
   }

   std::string old_topology_string;
   ocs::Topo::get_topology(old_topology_string);
   std::cout << old_topology_string << std::endl;

   std::string topology_str;
   ocs::Topo::get_new_topology(topology_str, true);
   std::cout << "Topology string: " << topology_str << std::endl;

   if (!ret) {
      std::cerr << "Test failed." << std::endl;
      DRETURN(1);
   }
   DRETURN(0);
}