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
 * @brief Driving `qconf -sc` (CS-2817)
 */

#include "../ocs_QConfModelBase.h"
#include "ocs_QConfTableView.h"

namespace ocs {
   /** @brief `qconf -sc`: hand the whole complex entry list to the table view
    *
    * Not derived from QConfController: its view type is not QConfViewBase, so
    * process_request() cannot be an override of it - the same reason qstat gives
    * each output shape a controller of its own.
    *
    * @ingroup libprocedure
    */
   class QConfTableController {
   protected:
      std::ostream &out_;   ///< where the view writes

   public:
      explicit QConfTableController(std::ostream &out) : out_(out) {}

      void process_request(QConfParameter &parameter, QConfModelBase &model,
                           QConfTableView &view);
   };
}
