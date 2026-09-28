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
 * @brief The rendering of `qconf -sq` (CS-2818)
 */

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

namespace ocs {
   /** @brief `qconf -sq`: cluster queues, queue instances, or a template
    *
    * One switch with four shapes, which is why the descriptor table cannot
    * express it and it is a sub-procedure of its own:
    *
    * - no argument at all prints a **template**, an object that does not exist
    *   in the cluster,
    * - a plain queue pattern prints the **cluster queue** configuration,
    * - a pattern naming a host prints the matching **queue instances**,
    * - a pattern naming a queue domain (`all.q@@group`) resolves the group and
    *   prints the instances on its hosts.
    *
    * The last two write one JSON array for everything matched, so that a query
    * hitting several hosts stays a single document; in plain output each
    * instance is a block of its own, blank-line separated, as it always was.
    *
    * @ingroup libprocedure
    */
   class QConfCQueueShowView : public QConfViewBase {
   public:
      explicit QConfCQueueShowView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfCQueueShowView() override = default;

      /** @brief Not reached: the queues are written by show_queues() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm) override {
      }

      bool show_queues(std::ostream &os, lList **answer_list, const QConfModelBase &model);

   protected:
      bool write_qinstance_list(std::ostream &os, lList **answer_list, const lList *qi_list);
      bool write_cqueue(std::ostream &os, lList **answer_list, const lListElem *cqueue);
   };
}
