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
 * @brief Driving `qconf -shgrp` (CS-2816)
 *
 * One class per header, one directory per sub-procedure, as `qstat` does it for
 * its four output formats.
 */

#include "../ocs_QConfController.h"

namespace ocs {
   /** @brief `qconf -shgrp`: pick the group out of the snapshot and render it
    *
    * The model fetches every host group, because three of the four host group
    * views resolve references and need the graph. This one wants a single
    * element out of it, so it cannot use QConfController, which would see a list
    * and write a list of names. Whether the group exists was settled in the
    * model, where there is an answer list to say so in.
    *
    * @ingroup libprocedure
    */
   class QConfHGroupShowController : public QConfController {
   public:
      explicit QConfHGroupShowController(std::ostream &out) : QConfController(out) {}
      ~QConfHGroupShowController() override = default;

      void process_request(QConfParameter &parameter, QConfModelBase &model,
                           QConfViewBase &view) override;
   };
}
