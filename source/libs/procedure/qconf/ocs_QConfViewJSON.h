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
 * @brief The JSON rendering of a `qconf` read operation
 */

#include "ocs_QConfViewBase.h"

namespace ocs {
   /** @brief The name list as a JSON array in the schema envelope (CS-2313a)
    * @ingroup libprocedure
    */
   class QConfViewJSON : public QConfViewBase {
   public:
      explicit QConfViewJSON(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfViewJSON() override = default;

      void show_name_list(std::ostream &os, const lList *list, int key_nm) override;
   };
}
