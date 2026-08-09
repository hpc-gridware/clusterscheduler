#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Controller of `qstat -j`: runs the request and drives the view
 */

#include "cull/cull.h"

#include "qstat/ocs_QStatModelBase.h"
#include "qstat/ocs_QStatParameter.h"

#include "ocs_QStatJobViewBase.h"

namespace ocs {
   /** @brief Runs one `qstat -j` request: fetch the selected jobs, report them in detail
    *
    * @ingroup libprocedure
    */
   class QStatJobController {
      std::ostream &out_;   ///< Where the view writes

   public:
      /** @brief Bind a controller to an output stream
       * @param out the stream the view will write to
       */
      explicit QStatJobController(std::ostream &out) : out_(out) {
      }

      virtual ~QStatJobController() = default;

      virtual void process_request(QStatParameter &parameter, QStatModelBase &model, QStatJobViewBase &view);
   };
}
