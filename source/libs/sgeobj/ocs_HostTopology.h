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

#include "uti/sge_dstring.h"
#include "cull/cull.h"

namespace ocs {
   class HostTopology {
      static void add_or_remove_used_threads(dstring *topology_dstr, const dstring *topology_in_use_dstr, bool do_add);
   public:
      static void correct_topology_upper_lower(dstring *topology_dstr);
      static void correct_topology_missing_threads(dstring *topology_dstr);
      static bool find_first_unused_thread(const dstring *topology_dstr, int *pos, int *socket, int *core, int *thread);
      static bool find_first_unused_thread(const dstring *topology_dstr, int *pos);

      static void add_used_threads(dstring *topology_dstr, const dstring *topology_in_use_dstr);
      static void add_used_threads(lListElem *elem, int topology_nm, dstring *topology_in_use_dstr);
      static void add_used_thread(dstring *topology_dstr, int pos);

      static void remove_used_threads(dstring *topology_dstr, const dstring *topology_in_use_dstr);
      static void remove_used_thread(dstring *topology_dstr, int pos);
      static void remove_all_used_threads(dstring *topology_dstr);

      static void elem_add_binding(lListElem *elem, int nm, const char *binding_now, const char *binding_to_use);
      static void elem_remove_binding(lListElem *elem, int nm, const char *binding_now, const char *binding_to_use);
      };
}
