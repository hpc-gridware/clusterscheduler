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
 * @brief Driving the rendering of `qconf -sds` (CS-2819)
 */

#include "../ocs_QConfModelBase.h"
#include "ocs_QConfSickView.h"

namespace ocs {
   /** @brief `qconf -sds`: hand the report to the view
    * @ingroup libprocedure
    */
   class QConfSickController {
   protected:
      std::ostream &out_;   ///< where the view writes

   public:
      explicit QConfSickController(std::ostream &out) : out_(out) {}

      /** @brief Write the report the model built
       * @param parameter the invocation; unused, everything is in the model
       * @param model     the snapshot, holding the report
       * @param view      the rendering
       */
      void process_request([[maybe_unused]] QConfParameter &parameter, QConfModelBase &model,
                           QConfSickView &view) {
         view.show_report(out_, model.get_sick_report());
      }
   };
}
