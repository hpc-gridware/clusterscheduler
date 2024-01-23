/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include <string>
#include "oge_topology.h"

#if defined(OGE_HWLOC)
namespace oge {

   // we do lazy initialization of the hwloc library
   bool topo_initialized = false;
   hwloc_topology_t topo_hwloc_topology = nullptr;

   /* @todo: the AGE/UGE code did this for every operation over and over again
    *        init + load topology, do some operation, hwloc_topology_destroy()
    *        what was the reason?
    */
   static bool topo_init() {
      bool ret = false;

      if (hwloc_topology_init(&topo_hwloc_topology) == 0) {
         if (hwloc_topology_load(topo_hwloc_topology) == 0) {
            ret = true;
         } // @todo error handling
      } // @todo error handling

      // even if initialization failed, treat it as initialized
      // we expect it to either succeed or always fail
      topo_initialized = true;
      return ret;
   }


/****** sge_binding_hlp/has_topology_information() *********************************
*  NAME
*     has_topology_information() -- Checks if current arch offers topology.
*
*  SYNOPSIS
*     bool has_topology_information()
*
*  FUNCTION
*     Checks if current architecture (on which this function is called)
*     offers processor topology information or not.
*
*  RESULT
*     bool - true if the arch offers topology information false if not
*
*  NOTES
*     MT-NOTE: has_topology_information() is not MT safe
*
*******************************************************************************/
   bool topo_has_topology_information()
   {
      bool ret = false;

      if (!topo_initialized) {
         topo_init();
      }

      // @todo: do this only once?
      // @todo: just check: hwloc_topology_get_depth(topology) != -1?;
      if (topo_hwloc_topology != nullptr) {
         auto support = hwloc_topology_get_support(topo_hwloc_topology);
         if (support->discovery->pu) { // @todo != nullptr ?
            ret = true;
         }
      }

      return ret;
   }

   bool topo_has_core_binding()
   {
      bool ret = false;

      if (!topo_initialized) {
         topo_init();
      }

      // @todo: do this only once?
      if (topo_hwloc_topology != nullptr) {
         auto support = hwloc_topology_get_support(topo_hwloc_topology);
         if (support->cpubind->set_proc_cpubind) { // @todo != nullptr ?
            ret = true;
         }
      }

      return ret;
   }

   hwloc_topology_t topo_get_hwloc_topology() {
      return topo_hwloc_topology;
   }

/****** sge_binding_hlp/topo_get_total_amount_of_threads() ***********************
*  NAME
*     topo_get_total_amount_of_threads() -- The total amount of hw supported threads.
*
*  SYNOPSIS
*     int topo_get_total_amount_of_threads()
*
*  FUNCTION
*     Returns the total amount of threads all CPUs on the host do
*     support.
*
*  RESULT
*     int - Total amount of harware supported threads the system supports.
*
*  NOTES
*     MT-NOTE: topo_get_total_amount_of_threads() is MT safe
*
*******************************************************************************/
   static int topo_get_total_amount_of_type(hwloc_obj_type_t type)
   {
      int amount = 0;
      if (topo_has_core_binding() && topo_has_topology_information()) {
         amount = hwloc_get_nbobjs_by_type(topo_hwloc_topology, type);
      }

      return amount;
   }

   int topo_get_total_amount_of_sockets() {
      return topo_get_total_amount_of_type(HWLOC_OBJ_PACKAGE);
   }

   int topo_get_total_amount_of_threads() {
      return topo_get_total_amount_of_type(HWLOC_OBJ_PU);
   }

/****** sge_binding_hlp/topo_get_total_amount_of_cores() ********************************
*  NAME
*     get_total_amount_of_cores() -- Fetches the total amount of cores on system.
*
*  SYNOPSIS
*     int get_total_amount_of_cores()
*
*  FUNCTION
*     Returns the total amount of cores per socket.
*
*  RESULT
*     int - Total amount of cores installed on the system.
*
*  NOTES
*     MT-NOTE: get_total_amount_of_cores() is MT safe
*
*******************************************************************************/
   int topo_get_total_amount_of_cores() {
      return topo_get_total_amount_of_type(HWLOC_OBJ_PU);
   }

static int topo_count_type_in_object(const hwloc_obj_t object, const hwloc_obj_type_t type)
{
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



/****** sge_binding_hlp/get_amount_of_cores() **************************************
*  NAME
*     topo_get_amount_of_cores_for_socket() -- Get amount of cores per socket.
*
*  SYNOPSIS
*     int topo_get_amount_of_cores_for_socket(int socket_number)
*
*  FUNCTION
*     Returns the amount of cores for a specific socket.
*
*  INPUTS
*     int socket_number - Physical socket number starting at 0.
*
*  RESULT
*     int - Amount of cores for the given socket or 0.
*
*  NOTES
*     MT-NOTE: topo_get_amount_of_cores_for_socket() is MT safe
*
*******************************************************************************/
   int topo_get_amount_of_cores_for_socket(int socket_number)
   {
      int ret = 0;

      if (topo_has_core_binding() && topo_has_topology_information()) {
         hwloc_obj_t socket;
         socket = hwloc_get_obj_by_type(topo_hwloc_topology, HWLOC_OBJ_SOCKET, socket_number);
         if (socket != nullptr) {
            ret = topo_count_type_in_object(socket, HWLOC_OBJ_CORE);
         }
      }

      return ret;
   }

/****** sge_binding_hlp/topo_get_amount_of_threads_for_core() **************************************
*  NAME
*     topo_get_amount_of_threads_for_core() -- Get amount of threads a specific core supports.
*
*  SYNOPSIS
*     int topo_get_amount_of_threads_for_core(int socket_number, int core_number)
*
*  FUNCTION
*     Returns the amount of threads a specific core supports.
*
*  INPUTS
*     int socket_number - Physical socket number starting at 0.
*     int core_number   - Physical core number on socket starting at 0.
*
*  RESULT
*     int - Amount of threads a specific core supports.
*
*  NOTES
*     MT-NOTE: topo_get_amount_of_threads_for_core() is MT safe
*
*******************************************************************************/
   int topo_get_amount_of_threads_for_core(int socket_number, int core_number) {
      int ret = 0;

      if (topo_has_core_binding() && topo_has_topology_information()) {
         hwloc_obj_t core;
         core = hwloc_get_obj_below_by_type(topo_hwloc_topology, HWLOC_OBJ_SOCKET, socket_number,
                                            HWLOC_OBJ_CORE, core_number);
         if (core != nullptr) {
            ret = topo_count_type_in_object(core, HWLOC_OBJ_PU);
         }
      }

      return ret;
   }

/****** sge_binding_hlp/topo_get_processor_ids() ******************************
*  NAME
*     topo_get_processor_ids() -- Get internal processor ids for a specific core.
*
*  SYNOPSIS
*     bool topo_get_processor_ids(int socket_number, int core_number, int**
*     proc_ids, int* amount)
*
*  FUNCTION
*     Get the Linux internal processor ids for a given core (specified by a socket,
*     core pair).
*
*  INPUTS
*     int socket_number - Logical socket number (starting at 0 without holes)
*     int core_number   - Logical core number on the socket (starting at 0 without holes)
*
*  OUTPUTS
*     int** proc_ids    - Array of Linux internal processor ids.
*     int* amount       - Size of the proc_ids array.
*
*  RESULT
*     bool - Returns true when processor ids where found otherwise false.
*
*  NOTES
*     MT-NOTE: topo_get_processor_ids() is MT safe
*
*******************************************************************************/
   bool topo_get_processor_ids(int socket_number, int core_number, int** proc_ids, int* amount)
   {
      bool ret = false;

      if (topo_has_core_binding() && topo_has_topology_information()) {
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
             * Therefore, we fetch the first PU, get its parent (usually the core)
             * and iterate over its children (the PUs).
             */
            hwloc_obj_t pu;
            pu = hwloc_get_obj_below_by_type(topo_hwloc_topology, HWLOC_OBJ_CORE,
                                             core->logical_index, HWLOC_OBJ_PU, 0);
            if (pu != nullptr) {
               hwloc_obj_t parent = pu->parent;
               *amount = parent->arity;
               *proc_ids = new int[*amount];
               for (int i = 0; i < *amount; ++i) {
                  (*proc_ids)[i] = parent->children[i]->os_index;
               }
               ret = true;
            }
         }
      }

      return ret;
   }

/****** sge_binding_hlp/topo_get_topology() ***********************************
*  NAME
*     topo_get_topology() -- Creates the topology string for the current host.
*
*  SYNOPSIS
*     bool topo_get_topology(char** topology, int* length)
*
*  FUNCTION
*     Creates the topology string for the current host. When it was created
*     it has top be freed from outside.
*
*  INPUTS
*     char** topology - The topology string for the current host.
*     int* length     - The length of the topology string.
*
*  RESULT
*     bool - when true the topology string could be generated (and memory
*            is allocated otherwise false
*
*  NOTES
*     MT-NOTE: topo_get_topology() is MT safe
*
*******************************************************************************/
   bool topo_get_topology(std::string &str_topology)
   {
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

   bool topo_get_topology(char** topology, int* length)
   {
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