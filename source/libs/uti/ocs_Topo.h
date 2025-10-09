#pragma once
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

#if defined(OCS_HWLOC) || defined(BINDING_SOLARIS)

#include <string>
#include <hwloc.h>
#include <vector>

namespace ocs {
   class Topo {

      static int get_total_amount_of_type(hwloc_obj_type_t type);

      static int count_type_in_object(const hwloc_obj_t object, const hwloc_obj_type_t type);

   public:
      class CpuKind {
         using LetterBitmap = std::tuple<char, hwloc_bitmap_t>;
         static std::vector<LetterBitmap> cpu_kinds;
      public:
         static void detect_via_hwloc();
         static void release_data();
         static char get_letter_for_core(hwloc_topology_t topology, hwloc_obj_t core);
      };

      static void set_fake_topo_file(std::string &topo_file);
      static bool init();

      static bool has_topology_information();

      static bool has_core_binding();

      static hwloc_topology_t get_hwloc_topology();

      static void get_sub_topology(std::string& topo_string, hwloc_topology_t topology, hwloc_obj_t obj, int depth, bool no_data_nodes, bool close_numa);

      static bool get_new_topology(std::string &topology, bool data_nodes = false);
      static bool get_topology(std::string &topology);
      static bool get_topology(char **topology, int *length); // @todo switch to the func above

      static bool get_processor_ids(int socket_number, int core_number, int **proc_ids, int *amount);

      static int add_hw_for_logical_id(hwloc_bitmap_t cpuset, int socket_id, int core_id, int thread_id);

      static int get_amount_of_cores_for_socket(int socket_number);

      static int get_amount_of_threads_for_core(int socket_number, int core_number);

      static int get_total_amount_of_cores();

      static int get_total_amount_of_threads();

      static int get_total_amount_of_sockets();
   };
}

#endif
