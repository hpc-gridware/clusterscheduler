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
 * @brief The rendering of `qconf -sobjl` (CS-2819)
 */

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

namespace ocs {
   /** @brief `qconf -sobjl`: the names the filter matched, one per line
    *
    * The matching itself happens in the model, because which name a queue entry
    * produces depends on the kind of the entry, not on the object - so what
    * arrives here is already a list of names, not a list of objects. There is no
    * JSON form yet (CS-2831); until then every format writes the plain list, as
    * the hand written switch did.
    *
    * @ingroup libprocedure
    */
   class QConfObjectFilterView : public QConfViewBase {
   public:
      explicit QConfObjectFilterView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfObjectFilterView() override = default;

      /** @brief Not reached: the names are written by show_names() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm) override {
      }

      /** @brief Write one matched name per line
       * @param os    the stream to write to
       * @param names the names the model matched, empty when nothing matched
       */
      void show_names(std::ostream &os, const std::vector<std::string> &names) {
         for (const auto &name : names) {
            os << name << "\n";
         }
      }
   };
}
