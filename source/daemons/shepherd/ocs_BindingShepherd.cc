//*___INFO__MARK_BEGIN_NEW__*/
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

#include "uti/sge_stdlib.h"

#include "sgeobj/ocs_BindingInstance.h"

#include "ocs_BindingShepherd.h"
#include "ocs_shepherd_systemd.h"
#include "config_file.h"
#include "err_trace.h"

#if defined(OCS_HWLOC)

/** @brief Try to bind the process strictly to the given cpuset.
 *
 * If this fails we try a non-strict binding. If this also fails we return false.
 *
 * @param cpuset The CPU set containing the processor IDs.
 * @return true if the binding was successful, false otherwise.
 */
bool
ocs::BindingShepherd::bind_process_to_cpuset(hwloc_const_bitmap_t cpuset) {
   // Early exit if no topology informationis available
   if (!Topo::has_topology_information()) {
      return false;
   }

   // we only need core binding capabilities, no topology is required
   hwloc_topology_t topology = Topo::get_hwloc_topology();
   if (hwloc_set_cpubind(topology, cpuset, HWLOC_CPUBIND_STRICT) == 0) {
      shepherd_trace("strict cpu binding succeeded");
      return true;
   }
   shepherd_trace("strict cpu binding is not available");

   // strict binding failed, try non-strict binding
   if (hwloc_set_cpubind(topology, cpuset, 0) == 0) {
      shepherd_trace("non strict cpu binding succeeded");
      return true;
   }

   shepherd_trace("cpu binding failed");
   return false;
}

#endif

#if defined(OCS_HWLOC) && !defined(SOLARIS)

int
ocs::BindingShepherd::do_thread_binding() {
   // Read binding_to_use from config file. None means no bining is done for the job.
   char *binding_to_use = get_conf_val("binding_to_use");
   if (binding_to_use == nullptr) {
      shepherd_trace("do_thread_binding: \"binding_to_use\" parameter not found in config file");
      return -1;
   }
   if (strcasecmp(NONE_STR, binding_to_use) == 0) {
      shepherd_trace("do_thread_binding: binding is disabled (no socket/core/thread specified)");
      return 0;
   }

   // Read binding instance from config file
   // Exit in case binding instance is not SET. Then the binding is done outside of shepherd.
   char *binding_instance_str = get_conf_val("binding_instance");
   if (binding_instance_str == nullptr) {
      shepherd_trace("do_thread_binding: \"binding_instance\" parameter not found in config file");
      return -1;
   }
   BindingInstance::Instance binding_instance = BindingInstance::from_string(binding_instance_str);
   if (binding_instance != BindingInstance::Instance::SET) {
      shepherd_trace("do_thread_binding: binding instance is not SET. nothing to do here.");
      return 0;
   }

   // Read the cpuset from config file
   char *cpuset_str = get_conf_val("binding_cpuset");
   hwloc_bitmap_t cpuset = hwloc_bitmap_alloc();
   if (cpuset_str != nullptr && strcasecmp(NONE_STR, cpuset_str) != 0) {
      if (hwloc_bitmap_sscanf(cpuset, cpuset_str) == 0) {
         shepherd_trace("do_thread_binding: parsed cpuset from config file: \"%s\"", cpuset_str);
      } else {
         shepherd_trace("do_thread_binding: couldn't parse cpuset from config file: \"%s\"", cpuset_str);
         return -1;
      }
   }

   // Binding will either be done via systemd or via HWLOC API directly
   if (g_use_systemd) {
      add_binding_to_systemd_properties(cpuset);
      shepherd_trace("do_thread_binding: applied thread binding via systemd");
      hwloc_bitmap_free(cpuset);
      return 0;
   }

   if (bind_process_to_cpuset(cpuset)) {
      shepherd_trace("do_thread_binding: applied thread binding via hwloc");
      hwloc_bitmap_free(cpuset);
      return 0;
   }

   // binding failed
   hwloc_bitmap_free(cpuset);
   return -1;
}

#endif
