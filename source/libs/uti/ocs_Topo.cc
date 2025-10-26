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
#include <tuple>
#include "ocs_Topo.h"
#include "sge_log.h"
#include "sge_rmon_macros.h"
#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/sge_conf.h"


// we do lazy initialization of the hwloc library
bool topo_initialized = false;
hwloc_topology_t topo_hwloc_topology = nullptr;
std::string fake_topo_file = NONE_STR;           //< active topology file or NONE

/* @todo: We might want to do the initialization for every operation, e.g.
 *        in execd retrieving topology as load value:
 *          - initialize hwloc
 *          - retrieve topology
 *          - destroy hwloc
 *         Reason: The topology might change, e.g. by disabling a core.
 *         See issue OGE-111.
 */

void ocs::Topo::set_fake_topo_file(std::string &topo_file) {
   DENTER(TOP_LAYER);

   // early exit if setting did not change
   if (topo_file == fake_topo_file) {
      DRETURN_VOID;
   }

   // HWLOC cleanup
   if (topo_initialized) {
      hwloc_topology_destroy(topo_hwloc_topology);
      topo_initialized = false;
   }

   // remember the current setting
   // next call that makes use of HWLOC will reinitialize the library
   fake_topo_file = topo_file;

   DRETURN_VOID;
}

/**
 * Initialize hwloc.
 * e.g "/home/ebablick/Clion/gcs0/ocs-testsuite/checktree_gcs/resources/hwloc-topo/AMD-Epyc-Zen5-c4d-highmem-384.xml"
 * @return true if the initialization succeeded, else false
 */
bool ocs::Topo::init() {
   DENTER(TOP_LAYER);
   bool ret = false;

   // Initialize HWLOC
   if (hwloc_topology_init(&topo_hwloc_topology) == 0) {
      int hwloc_error = 0;

      // Prepare loading of fake topology
      DPRINTF("using topo file %s\n", fake_topo_file.c_str());
      if (!fake_topo_file.empty() && fake_topo_file != NONE_STR && sge_is_file(fake_topo_file.c_str())) {
         hwloc_error = hwloc_topology_set_xml(topo_hwloc_topology, fake_topo_file.c_str());
         DTRACE;
      }

      // Load topology or detect topoloy
      if (hwloc_error == 0 && hwloc_topology_load(topo_hwloc_topology) == 0) {
         ret = true;
         DTRACE;
      } else {
         hwloc_topology_destroy(topo_hwloc_topology);
      }
   }

   // even if initialization failed, treat it as initialized
   // we expect it to either succeed or always fail
   topo_initialized = true;
   DRETURN(ret);
}

/** @brief Checks if executing host has topology information via HWLOC
 *
 * @return true if HWLOC is available
 */
bool ocs::Topo::has_topology_information() {
   bool ret = false;

   if (!topo_initialized) {
      init();
   }

   // @todo: do this only once?
   if (topo_hwloc_topology != nullptr) {
      auto support = hwloc_topology_get_support(topo_hwloc_topology);
      if (support->discovery->pu != 0) {
         ret = true;
      }
   }

   return ret;
}

hwloc_topology_t
ocs::Topo::get_hwloc_topology() {
   return topo_hwloc_topology;
}

/**
* Returns the total amount of a certain hwloc object type, e.g.
* the total amount of sockets, cores, pus (threads).
*
* @param[in] type e.g. HWLOC_OBJ_PACKAGE, HWLOC_OBJ_CORE, HWLOC_OBJ_PU
* @return total number of given objects
*/
int
ocs::Topo::get_total_amount_of_type(hwloc_obj_type_t type) {
   int amount = 0;
   if (ocs::Topo::has_topology_information()) {
      amount = hwloc_get_nbobjs_by_type(topo_hwloc_topology, type);
   }

   return amount;
}

/**
 * @return total number of sockets
 */
int
ocs::Topo::get_total_amount_of_sockets() {
   return get_total_amount_of_type(HWLOC_OBJ_PACKAGE);
}

/**
 * @return total number of cores
 */
int
ocs::Topo::get_total_amount_of_cores() {
   return get_total_amount_of_type(HWLOC_OBJ_CORE);
}

/**
 * @return total number of cores
 */
int
ocs::Topo::get_total_amount_of_threads() {
   return get_total_amount_of_type(HWLOC_OBJ_PU);
}

/**
 * @param[in] object a hwloc object, e.g. a package (socket)
 * @param[in] type the type we are searching for, e.g. core (HWLOC_OBJ_CORE)
 * @return the number of subobjects within an object, e.g. the number of cores within a specific package
 */
int
ocs::Topo::count_type_in_object(const hwloc_obj_t object, const hwloc_obj_type_t type) {
   int ret = 0;

   if (object != nullptr) {
      if (object->type == type) {
         ++ret;
      }
      // recursively search for type in sub objects
      for (unsigned int i = 0; i < object->arity; ++i) {
         ret += count_type_in_object(object->children[i], type);
      }
   }

   return ret;
}

/**
 * @param[in] socket_number
 * @see topo_count_type_in_object
 * @return the number of cores in the specified socket
 */
int ocs::Topo::get_amount_of_cores_for_socket(int socket_number) {
   int ret = 0;

#if defined(HWLOC)
   // This works also for inhomogeneous topologies, e.g. when some cores are disabled
   // Does not work for Solaris where cores bound to processor sets are not counted
   if (topo_has_topology_information()) {
      hwloc_obj_t socket;
      socket = hwloc_get_obj_by_type(topo_hwloc_topology, HWLOC_OBJ_SOCKET, socket_number);
      if (socket != nullptr) {
         ret = topo_count_type_in_object(socket, HWLOC_OBJ_CORE);
      }
   }
#else
   // Works only for homogeneous topologies, e.g. when all cores are enabled
   int socket = get_total_amount_of_sockets();
   int cores = get_total_amount_of_cores();
   ret = cores / socket;
#endif

   return ret;
}

/**
 * @param[in] socket_number
 * @param[in] core_number
 * @see topo_count_type_in_object
 * @return the number of threads in the specified core (on specified socket)
 */
int
ocs::Topo::get_amount_of_threads_for_core(int socket_number, int core_number) {
   int ret = 0;

   if (has_topology_information()) {
      hwloc_obj_t core;
      core = hwloc_get_obj_below_by_type(topo_hwloc_topology, HWLOC_OBJ_SOCKET, socket_number,
                                         HWLOC_OBJ_CORE, core_number);
      if (core != nullptr) {
         ret = count_type_in_object(core, HWLOC_OBJ_PU);
      }
   }

   return ret;
}

/**
 * Allocates and fills in an integer array proc_ids with the physical index numbers
 * of threads (PUs) for a specific socket and core.
 *
 * @note The caller is responsible for freeing (delete[]) the proc_ids array.
 *
 * @param[in] socket_number logical socket number (0..n)
 * @param[in] core_number  logical core number (0..n)
 * @param[out] proc_ids integer array with processor ids (allocated with new, caller has to free with delete[])
 * @param[out] amount  number of entries in proc_ids
 * @return true if the processor ids could be retrieved, else false
 */
bool
ocs::Topo::get_processor_ids(int socket_number, int core_number, int **proc_ids, int *amount) {
   bool ret = false;

   if (has_topology_information()) {
      hwloc_obj_t core;
      core = hwloc_get_obj_below_by_type(topo_hwloc_topology, HWLOC_OBJ_SOCKET,
                                         socket_number, HWLOC_OBJ_CORE, core_number);
      if (core != nullptr) {
         /* we can not rely on PUs being children of cores, see hwloc documentation,
          * from the introduction:
          *
          * Developers using the hwloc API or XML output for portable applications should therefore
          * be extremely careful to not make any assumptions about the structure of data that
          * is returned. For example, per the above reported PPC topology, it is not safe to assume
          * that PUs will always be descendants of cores.
          *
          * Therefore, we search recursively for PUs under the core.
          */
         *amount = get_amount_of_threads_for_core(socket_number, core_number);
         *proc_ids = new int[*amount];
         for (int i = 0; i < *amount; i++) {
            hwloc_obj_t pu;
            pu = hwloc_get_obj_below_by_type(topo_hwloc_topology, HWLOC_OBJ_CORE,
                                             core->logical_index, HWLOC_OBJ_PU, i);
            if (pu != nullptr) {
               (*proc_ids)[i] = pu->os_index;
            }
         }
         ret = true;
      }
   }

   return ret;
}

int
ocs::Topo::add_hw_for_logical_id(hwloc_bitmap_t cpuset, int socket_id, int core_id, int thread_id) {
   if (!has_topology_information()) {
      return -1;
   }

   // find the core via HWLOC
   hwloc_obj_t core = hwloc_get_obj_below_by_type(topo_hwloc_topology, HWLOC_OBJ_SOCKET, socket_id, HWLOC_OBJ_CORE, core_id);
   if (core == nullptr) {
      return -1;
   }

   // check if  thread_id is correct
   //SGE_ASSERT(thread_id < topo_count_type_in_object(core , HWLOC_OBJ_PU));

   // find the PU (thread) via HWLOC
   hwloc_obj_t pu = hwloc_get_obj_below_by_type(topo_hwloc_topology, HWLOC_OBJ_CORE, core->logical_index, HWLOC_OBJ_PU, thread_id);
   if (pu == nullptr) {
      return -1;
   }

   // add the PU (thread) to the cpuset
   hwloc_bitmap_or(cpuset, cpuset, pu->cpuset);

   // return the OS index of the PU (thread)
   return pu->os_index;
}

/**
 * Returns a topology string with the topology of the current machine.
 * Sockets are printed as "S", cores as "C", threads as "T", e.g.
 * a 2 socket machine with 4 cores each and each core having 2 threads has
 * the toploogy "SCTTCTTCTTCTTSCTTCTTCTTCTT".
 * @param[out] str_topology reference to a string object which will hold the topology
 * @return true if the topology could be retrieved, else false
 */
bool
ocs::Topo::get_topology(std::string &str_topology) {
   if (!has_topology_information()) {
      return false;
   }

   constexpr char TOPO_SOCKET{'S'};
   constexpr char TOPO_CORE{'C'};
   constexpr char TOPO_THREAD{'T'};

   int num_sockets = get_total_amount_of_sockets();
   for (int socket = 0; socket < num_sockets; ++socket) {
      str_topology += TOPO_SOCKET;

      int num_cores = get_amount_of_cores_for_socket(socket);
      for (int core = 0; core < num_cores; ++core) {
         str_topology += TOPO_CORE;

         int num_threads = get_amount_of_threads_for_core(socket, core);
         // report threads only when there is more than 1 thread
         if (num_threads > 1) {
            for (int thread = 0; thread < num_threads; ++thread) {
               str_topology += TOPO_THREAD;
            }
         }
      }
   }

   if (str_topology.empty()) {
      return false;
   }

   return true;
}

/**
 * Returns a topology string with the topology of the current machine.
 * Sockets are printed as "S", cores as "C", threads as "T", e.g.
 * a 2 socket machine with 4 cores each and each core having 2 threads has
 * the toploogy "SCTTCTTCTTCTTSCTTCTTCTTCTT".
 * @note The topology string gets dynamically allocated. It is in the responsibility
 *       of the caller to free it!
 * @param[out] topology pointer to the topology c-string
 * @param[out] length  length of the topology c-string
 * @return true if the topology could be retrieved, else false
 */
bool
ocs::Topo::get_topology(char **topology, int *length) {
   std::string str_topology;
   bool ret = get_topology(str_topology);

   if (ret) {
      // @todo: do we have to free a *topology != nullptr?
      *topology = strdup(str_topology.c_str());
      *length = str_topology.length();
   } else {
      *length = 0;
   }

   return ret;
}

std::vector<ocs::Topo::CpuKind::LetterBitmap> ocs::Topo::CpuKind::cpu_kinds;

void
ocs::Topo::CpuKind::detect_via_hwloc() {
   DENTER(TOP_LAYER);

   hwloc_topology_t topology = ocs::Topo::get_hwloc_topology(); //< HWLOC handle

   // Are there more than one CPU kinds?
   // If not then it is not guaranteed that the CPU kind is available. 0 might be returned.
   const int nr = hwloc_cpukinds_get_nr(topology, 0);

   // If we have more than one entry then the entries are sorted according to
   // the efficiency of the CPU kind, with the most efficient last!
   // The most efficient CPU kind will get the letter C
   char letter = 'C' - 1;

   // Find more info about each CPU type. We especially need the bitmap
   int last_linux_capacity = -1;
   int linux_capacity = -1;
   for (int i = nr - 1; i >= 0; i--) {
      struct hwloc_info_s *info {};
      unsigned nr_infos = 0;
      int efficiency;

      hwloc_bitmap_t cpu_bitmap = hwloc_bitmap_alloc();
      int err = hwloc_cpukinds_get_info(topology, i, cpu_bitmap, &efficiency, &nr_infos, &info, 0);
      if (err == 0) {
         // Print CPU kind information for debugging
         char *cpusets;
         hwloc_bitmap_asprintf(&cpusets, cpu_bitmap);
         DPRINTF("CPU-Type #%u: efficiency=%d,cpuset=%s\n", i, efficiency, cpusets);
         free(cpusets);

         // This would give more details like: Max-Frequency, Base-Frequency, LinuxCapacity => Core Type
         // Increate the letter for the core type only if the LinuxCapacity increased by at least 5%
         bool increase_letter = true;
         for (unsigned j = 0; j < nr_infos; j++) {
            if (info[j].name != nullptr && strcmp(info[j].name, "LinuxCapacity") != 0) {
               DPRINTF("  %s = %s\n", info[j].name, info[j].value);
               continue;
            }
            try {
               linux_capacity = std::stoi(info[j].value);
            } catch (...) {
               linux_capacity = -1;
            }
            if (linux_capacity != -1) {
               if (last_linux_capacity != -1 && 0.95 * last_linux_capacity <= linux_capacity) {
                  increase_letter = false;
               }
               last_linux_capacity = linux_capacity;
               //break;
            }
            DPRINTF("  %s = %s (increase_letter=%s)\n", info[j].name, info[j].value, increase_letter ? "true" : "false");
         }

         // Increase the letter for the next CPU kind if needed (see above)
         if (increase_letter) {
            if (letter == 'C') {
               letter += 2; // skip 'D' which is reserved for another power core type
            } else {
               letter++;
            }
         }

         // Add the CPU kind to the list
         cpu_kinds.emplace_back(letter, cpu_bitmap);

      }
   }
}

void ocs::Topo::CpuKind::release_data() {
   DENTER(TOP_LAYER);
   for (auto &kind : cpu_kinds) {
      hwloc_bitmap_free(std::get<1>(kind));
   }
   cpu_kinds.clear();
   DRETURN_VOID;
}

char
ocs::Topo::CpuKind::get_letter_for_core(hwloc_topology_t topology, hwloc_obj_t core) {
   DENTER(TOP_LAYER);

   // If we have no CPU kinds then return 'C' as default
   if (cpu_kinds.empty()) {
      DRETURN('C');
   }

   // Get the CPU kind for the given object
   for (const auto &kind : cpu_kinds) {
      if (hwloc_bitmap_isincluded(core->cpuset, std::get<1>(kind))) {
         DRETURN(std::get<0>(kind));
      }
   }

   // If we did not find a matching CPU kind then return 'C'
   DRETURN('C');
}

// NSX SNX SXN
void ocs::Topo::get_sub_topology(std::string& topo_string, hwloc_topology_t topology, hwloc_obj_t obj, int depth, bool data_nodes) {
   bool close_group = false;
   bool close_numa_group = false;

   // NUMA nodes are part of a separate tree within HWLOC
   if (data_nodes) {
      if (obj->memory_arity > 0) {
         hwloc_obj_t mem_obj = obj->memory_first_child;

         if (mem_obj->type == HWLOC_OBJ_NUMANODE) {
            // begin a NUMA node group
            topo_string += "(N[size=";
            topo_string += std::to_string(mem_obj->attr->numanode.local_memory);
            topo_string += "]";

            // we are now inside a NUMA node that need to get closed
            close_numa_group = true;
         }
      }
   }

   // Now handle all Packages, Cores, Threads and also Caches (which are not part of the memory subtree)
   switch (obj->type) {
      case HWLOC_OBJ_PACKAGE:
         topo_string += "(S";
         close_group = true;
         break;
      case HWLOC_OBJ_DIE:
         if (data_nodes) {
            topo_string += "(A";
            close_group = true;
         }
         break;
      case HWLOC_OBJ_CORE: {
         topo_string += "("; // C or E foolows next
         topo_string += CpuKind::get_letter_for_core(topology, obj);
         close_group = true;
         break;
      }
      case HWLOC_OBJ_PU:
         topo_string += "(T";
         close_group = true;
         break;

      // Note: for following objects obj->attr->osdev.type would return the memory in bytes
      case HWLOC_OBJ_L3CACHE:
         if (data_nodes) {
            topo_string += "(X[size=";
            topo_string += std::to_string(obj->attr->cache.size);
            topo_string += "]";
            close_group = true;
         }
         break;
      case HWLOC_OBJ_L2CACHE:
         if (data_nodes) {
            topo_string += "(Y[size=";
            topo_string += std::to_string(obj->attr->cache.size);
            topo_string += "]";
            close_group = true;
         }
         break;
      default:
         break;
   }

   // walk along the hardware tree that also contains cache information
   if (obj->arity > 0) {
      for (unsigned i = 0; i < obj->arity; i++) {
         get_sub_topology(topo_string, topology, obj->children[i], depth + 1, data_nodes);
      }
   }

   // If we are in a package, core, thread or cache, we close it here
   if (close_group) {
      topo_string += ")";
   }

   // If we opened a NUMA node within a socket then we close it here
   if (close_numa_group) {
      topo_string += ")";
   }
}

bool
ocs::Topo::get_new_topology(std::string &topo_str, bool data_nodes) {
   if (!has_topology_information()) {
      return false;
   }

   CpuKind::detect_via_hwloc();
   get_sub_topology(topo_str, topo_hwloc_topology, hwloc_get_root_obj(topo_hwloc_topology), 0, data_nodes);
   CpuKind::release_data();
   return true;
}

void
ocs::Topo::make_cpuset(hwloc_bitmap_t cpuset, const std::string &binding_to_use) {
   DENTER(TOP_LAYER);

   if (cpuset == nullptr || binding_to_use.empty()) {
      DRETURN_VOID;
   }

   hwloc_bitmap_zero(cpuset);

   int socket_id = -1;
   int core_id = -1;
   int thread_id = -1;
   for (const auto c : binding_to_use) {
      switch (c) {
         case 'S':
         case 's':
            socket_id++;
            core_id = -1; // reset core and thread id for new socket
            thread_id = -1;
            break;
         case 'C':
         case 'E':
         case 'c':
         case 'e':
            core_id++;
            thread_id = -1; // reset thread id for new core
            break;
         case 'T':
            thread_id++;
            break;
         case 't':
            thread_id++;
            Topo::add_hw_for_logical_id(cpuset, socket_id, core_id, thread_id);
            break;
         default:
            thread_id++;
            break;
      }
   }
   DRETURN_VOID;
}

#endif