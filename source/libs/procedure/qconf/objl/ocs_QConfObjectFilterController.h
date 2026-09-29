#pragma once
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
 * @brief Driving the rendering of `qconf -sobjl` (CS-2819)
 */

#include "../ocs_QConfModelBase.h"
#include "ocs_QConfObjectFilterView.h"

namespace ocs {
   /** @brief `qconf -sobjl`: hand the matched names to the view
    * @ingroup libprocedure
    */
   class QConfObjectFilterController {
   protected:
      std::ostream &out_;   ///< where the view writes

   public:
      explicit QConfObjectFilterController(std::ostream &out) : out_(out) {}

      /** @brief Write the names the model matched
       * @param parameter the invocation; unused, the patterns were consumed by the model
       * @param model     the snapshot, holding the matched names
       * @param view      the rendering
       */
      void process_request([[maybe_unused]] QConfParameter &parameter, QConfModelBase &model,
                           QConfObjectFilterView &view) {
         view.show_names(out_, model.get_filtered_names());
      }
   };
}
