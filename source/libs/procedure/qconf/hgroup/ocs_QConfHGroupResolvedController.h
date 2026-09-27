#pragma once
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
 * @brief Driving `qconf -shgrp_resolved` (CS-2816)
 *
 * One class per header, one directory per sub-procedure, as `qstat` does it for
 * its four output formats.
 */

#include "../ocs_QConfController.h"
#include "ocs_QConfHGroupStructureView.h"

namespace ocs {
   /** @brief `qconf -shgrp_resolved`
    * @ingroup libprocedure
    */
   class QConfHGroupResolvedController {
   protected:
      std::ostream &out_;   ///< where the view writes

   public:
      explicit QConfHGroupResolvedController(std::ostream &out) : out_(out) {}

      void process_request(QConfParameter &parameter, QConfModelBase &model,
                           QConfHGroupStructureView &view);
   };
}
