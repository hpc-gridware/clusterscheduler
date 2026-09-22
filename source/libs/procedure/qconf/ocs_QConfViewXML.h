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
 * @brief The XML view `qconf` does not have
 */

#include "ocs_QConfViewBase.h"

namespace ocs {
   /** @brief A view that writes nothing, because `qconf` has no XML output
    *
    * `-fmt` takes `plain` and `json` only. This exists so that the traits
    * struct of the server dispatch has an XML view to name, like every other
    * procedure; the request is refused earlier, in
    * QConfModelBase::make_snapshot().
    *
    * @ingroup libprocedure
    */
   class QConfViewXML : public QConfViewBase {
   public:
      explicit QConfViewXML(const ProcedureParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfViewXML() override = default;

      /** @brief Writes nothing; see the class description
       * @param os     unused
       * @param list   unused
       * @param key_nm unused
       */
      void show_name_list(std::ostream &os, const lList *list, int key_nm) override {
         (void) os; (void) list; (void) key_nm;
      }
   };
}
