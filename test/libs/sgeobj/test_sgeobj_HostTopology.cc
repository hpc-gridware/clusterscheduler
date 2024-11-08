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

#include <iostream>

#include "sgeobj/ocs_HostTopology.h"

int main (int argc, char *argv[]) {
   std::string topo_mask = "ScTTCtTsctTtt";
   ocs::HostTopology ht = ocs::HostTopology(topo_mask);

   std::cout << topo_mask << std::endl;
   std::cout << ht.getTopoMask() << std::endl;
   std::cout << ht.getFullTopoMask() << std::endl;
   return 0;
}