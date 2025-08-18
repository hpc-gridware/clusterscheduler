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

#include "uti/ocs_Systemd.h"
#include "uti/ocs_Topo.h"

namespace ocs {
   extern bool g_use_systemd;
   extern ocs::uti::SystemdProperties_t g_systemd_properties;

   void shepherd_systemd_init();
   void move_shepherd_child_to_job_scope(pid_t pid);

   void shepherd_systemd_signal_job(int signal, bool only_main);
#if defined(OCS_HWLOC)
   void add_binding_to_systemd_properties(hwloc_const_bitmap_t cpuset);
#endif
} // namespace ocs
