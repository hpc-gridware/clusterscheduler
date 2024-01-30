#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#if defined(OGE_HWLOC)

#include <string>
#include <hwloc.h>

namespace oge {
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
