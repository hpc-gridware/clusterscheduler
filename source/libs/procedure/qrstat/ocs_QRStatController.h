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
 * @brief Controller of `qrstat`: runs the request and drives the view
 */

#include "ocs_QRStatParameterClient.h"
#include "ocs_QRStatModelBase.h"
#include "ocs_QRStatViewBase.h"

namespace ocs {
   /** @brief Runs one `qrstat` request: fetch the advance reservations, report them
    *
    * @ingroup libprocedure
    */
   class QRStatController {
      std::ostream &out_;   ///< Where the view writes
   public:
      /** @brief Bind a controller to an output stream
       * @param out the stream the view will write to
       */
      explicit QRStatController(std::ostream &out) : out_(out) {};

      virtual ~QRStatController() = default;

      virtual void process_request(QRStatParameter &parameter, QRStatModelBase &model, QRStatViewBase &view);

   };
}
