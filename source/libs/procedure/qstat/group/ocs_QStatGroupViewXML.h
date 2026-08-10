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
 * @brief XML rendering of `qstat -g c`
 */

#include <ostream>

#include "ocs_QStatGroupViewBase.h"

namespace ocs {
   /** @brief Renders `qstat -g c` as XML
    *
    * Like the other XML views this one builds a CULL tree from the hook calls
    * and lets the generic XML writer serialise it at the end.
    *
    * @ingroup libprocedure
    */
   class QStatGroupViewXML : public QStatGroupViewBase {
      lList *xml_elems = nullptr;   ///< The cluster queues collected so far
   public:
      /** @brief Build the XML view
       * @param parameter the call's parameters
       */
      explicit QStatGroupViewXML(const ProcedureParameter &parameter) : QStatGroupViewBase(parameter) {};
      ~QStatGroupViewXML() override { lFreeList(&xml_elems); }

      void report_started(std::ostream &os, QStatParameter &parameter) override;
      void report_finished(std::ostream &os, QStatParameter &parameter) override;
      void report_cqueue(std::ostream &os, const char* qname, Summary *cqueue_summary, QStatParameter &parameter) override;
   };
}
