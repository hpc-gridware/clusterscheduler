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
 * @brief Base view of `qselect`, and the interface the three output formats implement
 */

#include <ostream>

#include "ocs_ProcedureView.h"

namespace ocs {
   /** @brief Base view for `qselect`, and the interface the three formats implement
    *
    * `qselect` answers with nothing but the names of the queue instances that
    * match, so the interface is three hooks.
    *
    * @ingroup libprocedure
    */
   class QStatSelectViewBase : public ProcedureView {
   public:
      /** @brief Build a view for one qselect call
       * @param parameter the call's parameters
       */
      explicit QStatSelectViewBase(const ProcedureParameter &parameter) : ProcedureView(parameter) {};

      ~QStatSelectViewBase() override = default;

      /** @brief Begin the list
       * @param os stream to write to
       */
      virtual void report_started(std::ostream &os) = 0;

      /** @brief End the list
       * @param os stream to write to
       */
      virtual void report_finished(std::ostream &os) = 0;

      /** @brief Report one matching queue instance
       * @param os stream to write to
       * @param qname the queue instance
       */
      virtual void report_queue(std::ostream &os, const char* qname) = 0;
   };
}
