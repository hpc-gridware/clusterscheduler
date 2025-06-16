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
#include "ocs_topology.h"

namespace ocs {

   // we do lazy initialization of the hwloc library
   bool topo_initialized = false;
   hwloc_topology_t topo_hwloc_topology = nullptr;

   /* @todo: We might want to do the initialization for every operation, e.g.
    *        in execd retrieving topology as load value:
    *          - initialize hwloc
    *          - retrieve topology
    *          - destroy hwloc
    *         Reason: The topology might change, e.g. by disabling a core.
    *         See issue OGE-111.
    */

   /**
    * Initialize hwloc.
    * @return true if the initialization succeeded, else false
    */
   static bool topo_init() {
      bool ret = false;

      if (hwloc_topology_init(&topo_hwloc_topology) == 0) {
         if (hwloc_topology_load(topo_hwloc_topology) == 0) {
            ret = true;
         } else {
            hwloc_topology_destroy(topo_hwloc_topology);
         }
      }

      // even if initialization failed, treat it as initialized
      // we expect it to either succeed or always fail
      topo_initialized = true;
      return ret;
   }


/**
 * Checks if current architecture (on which this function is called)
 * offers processor topology information or not.
 *
 * @return true if the arch offers topology information, false if not
 */
   bool topo_has_topology_information() {
      bool ret = false;

      if (!topo_initialized) {
         topo_init();
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

   /**
    * Checks if core binding is possible on the current host / architecture.
    *
    * @return true, if core binding is possible, else false
    */
   bool topo_has_core_binding() {
      bool ret = false;

      if (!topo_initialized) {
         topo_init();
      }

      // @todo: do this only once?
      if (topo_hwloc_topology != nullptr) {
         auto support = hwloc_topology_get_support(topo_hwloc_topology);
         if (support->cpubind->set_proc_cpubind != 0) {
            ret = true;
         }
      }

      return ret;
   }

   hwloc_topology_t topo_get_hwloc_topology() {
      return topo_hwloc_topology;
   }

/**
 * Returns the total amount of a certain hwloc object type, e.g.
 * the total amount of sockets, cores, pus (threads).
 *
 * @param[in] type e.g. HWLOC_OBJ_PACKAGE, HWLOC_OBJ_CORE, HWLOC_OBJ_PU
 * @return total number of given objects
 */
   static int topo_get_total_amount_of_type(hwloc_obj_type_t type) {
      int amount = 0;
      if (topo_has_topology_information()) {
         amount = hwloc_get_nbobjs_by_type(topo_hwloc_topology, type);
      }

      return amount;
   }

   /**
    * @return total number of sockets
    */
   int topo_get_total_amount_of_sockets() {
      return topo_get_total_amount_of_type(HWLOC_OBJ_PACKAGE);
   }

   /**
    * @return total number of cores
    */
   int topo_get_total_amount_of_cores() {
      return topo_get_total_amount_of_type(HWLOC_OBJ_CORE);
   }

   /**
    * @return total number of cores
    */
   int topo_get_total_amount_of_threads() {
      return topo_get_total_amount_of_type(HWLOC_OBJ_PU);
   }

#if 1
   /**
    * @param[in] object a hwloc object, e.g. a package (socket)
    * @param[in] type the type we are searching for, e.g. core (HWLOC_OBJ_CORE)
    * @return the number of subobjects within an object, e.g. the number of cores within a specific package
    */
   static int topo_count_type_in_object(const hwloc_obj_t object, const hwloc_obj_type_t type) {
      int ret = 0;

      if (object != nullptr) {
         if (object->type == type) {
            ++ret;
         }
         // recursively search for type in sub objects
         for (unsigned int i = 0; i < object->arity; ++i) {
            ret += topo_count_type_in_object(object->children[i], type);
         }
      }

      return ret;
   }
#endif

   /**
    * @param[in] socket_number
    * @see topo_count_type_in_object
    * @return the number of cores in the specified socket
    */
   int topo_get_amount_of_cores_for_socket(int socket_number) {
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
      int socket = ocs::topo_get_total_amount_of_sockets();
      int cores =  ocs::topo_get_total_amount_of_cores();
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
   int topo_get_amount_of_threads_for_core(int socket_number, int core_number) {
      int ret = 0;

#if 1
      if (topo_has_topology_information()) {
         hwloc_obj_t core;
         core = hwloc_get_obj_below_by_type(topo_hwloc_topology, HWLOC_OBJ_SOCKET, socket_number,
                                            HWLOC_OBJ_CORE, core_number);
         if (core != nullptr) {
            ret = topo_count_type_in_object(core, HWLOC_OBJ_PU);
         }
      }
#else
      int cores =  ocs::topo_get_total_amount_of_cores();
      int threads =  ocs::topo_get_total_amount_of_threads();
      ret = threads / cores;
#endif

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
   bool topo_get_processor_ids(int socket_number, int core_number, int **proc_ids, int *amount) {
      bool ret = false;

      if (topo_has_topology_information()) {
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
            *amount = topo_get_amount_of_threads_for_core(socket_number, core_number);
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

   /**
    * Returns a topology string with the topology of the current machine.
    * Sockets are printed as "S", cores as "C", threads as "T", e.g.
    * a 2 socket machine with 4 cores each and each core having 2 threads has
    * the toploogy "SCTTCTTCTTCTTSCTTCTTCTTCTT".
    * @param[out] str_topology reference to a string object which will hold the topology
    * @return true if the topology could be retrieved, else false
    */
   bool topo_get_topology(std::string &str_topology) {
      bool ret = false;

      if (topo_has_topology_information()) {
         constexpr char TOPO_SOCKET{'S'};
         constexpr char TOPO_CORE{'C'};
         constexpr char TOPO_THREAD{'T'};

         int num_sockets = topo_get_total_amount_of_sockets();
         for (int socket = 0; socket < num_sockets; ++socket) {
            str_topology += TOPO_SOCKET;

            int num_cores = topo_get_amount_of_cores_for_socket(socket);
            for (int core = 0; core < num_cores; ++core) {
               str_topology += TOPO_CORE;

               int num_threads = topo_get_amount_of_threads_for_core(socket, core);
               // report threads only when there is more than 1 thread
               if (num_threads > 1) {
                  for (int thread = 0; thread < num_threads; ++thread) {
                     str_topology += TOPO_THREAD;
                  }
               }
            }
         }

         if (!str_topology.empty()) {
            ret = true;
         }
      }

      return ret;
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
   bool topo_get_topology(char **topology, int *length) {
      std::string str_topology;
      bool ret = topo_get_topology(str_topology);

      if (ret) {
         // @todo: do we have to free a *topology != nullptr?
         *topology = strdup(str_topology.c_str());
         *length = str_topology.length();
      } else {
         *length = 0;
      }

      return ret;
   }

}
#endif
