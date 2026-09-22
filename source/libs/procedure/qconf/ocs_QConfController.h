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
 * @brief Driving one `qconf` read operation from model to view
 */

#include <ostream>

#include "ocs_QConfModelBase.h"
#include "ocs_QConfViewBase.h"

namespace ocs {
   /** @brief Hands what the model fetched to the view
    * @ingroup libprocedure
    */
   class QConfController {
   private:
      std::ostream &out_;   ///< where the view writes

   public:
      /** @brief Bind a controller to an output stream
       * @param out the stream, `std::cout` in a client and a response buffer in qmaster
       */
      explicit QConfController(std::ostream &out) : out_(out) {}
      virtual ~QConfController() = default;

      virtual void process_request(QConfParameter &parameter, QConfModelBase &model,
                                   QConfViewBase &view);
   };
}
