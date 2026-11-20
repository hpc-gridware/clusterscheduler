/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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
 * @brief The object model of the Cluster Scheduler: views of the generated tables of all object types
 *
 * The model is a constant over the generated constexpr tables of
 * `ocs_tom_all.h`; the indexes for the lookups are built by install_model()
 * in the list library. Building them at compile time here (a constexpr sort
 * over about 1100 names) was rejected: it costs compile time, adds index
 * arrays and a second, wider interface to install_model() for a sort that
 * takes well under a millisecond once at start.
 * Only this object references the tables as a whole, and it is linked as soon
 * as gcs_model() is called; no static initializer is needed.
 */

#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"

namespace ocs::tom {

   /**
    * @brief The object model of the Cluster Scheduler: every object type of `all_types.json` and the sub-types
    *        of their list and object fields
    *
    * Install it with `ocs::tom::install_model(ocs::tom::gcs_model())` next to `lInit(nmv)`.
    *
    * @return the model; its tables live for the whole run of the program
    */
   const Model &gcs_model() noexcept {
      static constexpr Model model{all_type_tables, all_sub_types};
      return model;
   }

} // namespace ocs::tom
