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
 * @brief JSON rendering of `qstat -g c`
 */

#include "ocs_QStatGroupViewBase.h"

namespace ocs {
   /** @brief Renders `qstat -g c` as JSON
    *
    * @ingroup libprocedure
    */
   class QStatGroupViewJSON : public QStatGroupViewBase {
      int indent = 0;             ///< Current indentation depth
      bool first_queue = true;    ///< Whether no cluster queue has been written yet, so no comma is needed
   public:
      /** @brief Build the JSON view
       * @param parameter the call's parameters
       */
      explicit QStatGroupViewJSON(const ProcedureParameter &parameter) : QStatGroupViewBase(parameter) {};
      ~QStatGroupViewJSON() override = default;

      void report_started(std::ostream &os, QStatParameter &parameter) override;
      void report_finished(std::ostream &os, QStatParameter &parameter) override;
      void report_cqueue(std::ostream &os, const char* qname, Summary *cqueue_summary, QStatParameter &parameter) override;
   };
}
