#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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
 * @brief The scheduler's side of core binding
 */

#include "tom/ocs_Types.h"

#include "sgeobj/tom/ocs_tom_EH.h"
#include "tom/ocs_Element.h"

#include "sge_select_queue.h"

namespace ocs {
   /** @brief Decides which topology units a job gets on a host */
   class BindingSchedd {
      static bool
      find_initial_in_use(const sge_assignment_t *a, ocs::tom::ConstElement<ocs::tom::EH> host,
                          TopologyString &host_in_use);

      static bool
      find_final_in_use(const sge_assignment_t *a, TopologyString& binding_in_use, TopologyString& binding_in_use_sorted);

      static bool
      ignore_binding(const sge_assignment_t *a, ocs::tom::ConstElement<ocs::tom::EH> host);

      static int
      slots_reduced_to_available_maximum(const sge_assignment_t *a, int slots_max_available);
   public:

      static double
      test_strategy(const sge_assignment_t *a, ocs::tom::ConstElement<ocs::tom::EH> host, double slots,
                    const TopologyString &binding_in_use);

      static int
      apply_strategy(sge_assignment_t *a, int slots, ocs::tom::ConstElement<ocs::tom::EH> host,
                     TopologyString& topo_in_use);

      static bool
      copy_strategy(const sge_assignment_t *a);
   };
}
