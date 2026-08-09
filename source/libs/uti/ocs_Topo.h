#pragma once
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

/** @file
 * @brief The host's CPU topology, as discovered through hwloc
 */

#if defined(OCS_HWLOC) || defined(BINDING_SOLARIS)

#include <string>
#include <hwloc.h>
#include <vector>

namespace ocs {
   /**
    * @brief The host's CPU topology, discovered once through hwloc
    *
    * Static only, initialised lazily on first use. Provides two things: a
    * *topology string* describing the machine, and cpusets for binding a job
    * to specific hardware.
    *
    * The topology string encodes the hierarchy with one letter per object -
    * sockets, cores and threads - so a whole machine layout fits in a value
    * that can be reported as a complex and matched against a request.
    * #make_cpuset walks the same alphabet in reverse to turn a binding string
    * back into a cpuset.
    */
   class Topo {

      static int get_total_amount_of_type(hwloc_obj_type_t type);

      static int count_type_in_object(const hwloc_obj_t object, const hwloc_obj_type_t type);

   public:
      /**
       * @brief Performance and efficiency core detection
       *
       * On hybrid CPUs hwloc reports several "kinds" of core. Each kind is
       * given a letter so it can appear in the topology string.
       */
      class CpuKind {
         using LetterBitmap = std::tuple<char, hwloc_bitmap_t>;
         static std::vector<LetterBitmap> cpu_kinds;
      public:
         /// Ask hwloc which CPU kinds are present and assign each a letter
         static void detect_via_hwloc();
         /// Release the bitmaps allocated by #detect_via_hwloc
         static void release_data();
         static char get_letter_for_core(hwloc_topology_t topology, hwloc_obj_t core);
      };

      static void set_fake_topo_file(std::string &topo_file);
      static bool init();

      static bool has_topology_information();

      /**
       * @brief Can jobs be bound to cores on this host?
       * @return true when binding is supported and the topology is known
       */
      static bool has_core_binding();

      static hwloc_topology_t get_hwloc_topology();

      static void get_sub_topology(std::string& topo_string, hwloc_topology_t topology, hwloc_obj_t obj, int depth, bool no_data_nodes);

      static bool get_new_topology(std::string &topology, bool data_nodes = false, bool enable_hwloc = true);
      static bool get_topology(std::string &topology);
      static bool get_topology(char **topology, int *length); // @todo switch to the func above

      static bool get_processor_ids(int socket_number, int core_number, int **proc_ids, int *amount);

      static int add_hw_for_logical_id(hwloc_bitmap_t cpuset, int socket_id, int core_id, int thread_id);

      static int get_amount_of_cores_for_socket(int socket_number);

      static int get_amount_of_threads_for_core(int socket_number, int core_number);

      static int get_total_amount_of_cores();

      static int get_total_amount_of_threads();

      static int get_total_amount_of_sockets();

      static void make_cpuset(hwloc_bitmap_t cpuset, const std::string &binding_to_use);
      };
}

#endif
