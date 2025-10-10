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

#include "cull/cull.h"

#include "sge_select_queue.h"

namespace ocs {
   class BindingSchedd {
      static bool
      find_initial_in_use(const sge_assignment_t *a, const lListElem *host, TopologyString &host_in_use);

      static bool
      find_final_in_use(const sge_assignment_t *a, TopologyString& binding_in_use, TopologyString& binding_in_use_sorted);

      static bool
      ignore_binding(const sge_assignment_t *a, const lListElem *host);
   public:

      static double
      test_strategy(const sge_assignment_t *a, const lListElem *host, double slots, const TopologyString &binding_in_use);

      static int
      apply_strategy(sge_assignment_t *a, int slots, const lListElem *host, TopologyString& topo_in_use);

      static bool
      copy_strategy(const sge_assignment_t *a);
   };
}
