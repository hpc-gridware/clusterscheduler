#pragma once
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

#if defined(OCS_HWLOC) || defined(BINDING_SOLARIS)

#include <string>
#include <hwloc.h>

namespace ocs {
   bool topo_has_topology_information();

   bool topo_has_core_binding();

   hwloc_topology_t topo_get_hwloc_topology();

   bool topo_get_topology(std::string &topology);

   bool topo_get_topology(char **topology, int *length); // @todo switch to the func above
   bool topo_get_processor_ids(int socket_number, int core_number, int **proc_ids, int *amount);

   int topo_get_amount_of_cores_for_socket(int socket_number);

   int topo_get_amount_of_threads_for_core(int socket_number, int core_number);

   int topo_get_total_amount_of_cores();

   int topo_get_total_amount_of_threads();

   int topo_get_total_amount_of_sockets();
}

#endif
