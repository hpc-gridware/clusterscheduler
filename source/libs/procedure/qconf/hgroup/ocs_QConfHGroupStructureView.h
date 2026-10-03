#pragma once
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
 * @brief The tree and resolved renderings of a host group (CS-2816)
 *
 * One class per header, one directory per sub-procedure, as `qstat` does it for
 * its four output formats.
 */

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

namespace ocs {
   /** @brief `qconf -shgrp_tree` and `-shgrp_resolved`
    *
    * A host group may contain other host groups, so there are two useful views:
    * the tree, which shows where each host comes from, and the resolved list,
    * which shows what the group finally means.
    *
    * Both need every host group, not the one named, which is why they cannot be
    * a row in qconf_read_table. Neither has a JSON form: the hand written option
    * blocks ignored `-fmt` and printed plain text, and that is kept.
    *
    * @ingroup libprocedure
    */
   class QConfHGroupStructureView : public QConfViewBase {
   public:
      explicit QConfHGroupStructureView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfHGroupStructureView() override = default;

      /** @brief Not reached: the structure controller renders through the two
       *         methods below, never through the generic name list */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm,
                          [[maybe_unused]] const qconf_object_desc &desc) override {
      }

      void show_tree(std::ostream &os, const lList *hgroup_list, const char *name);
      void show_resolved(std::ostream &os, const lListElem *hgroup, const lList *hgroup_list);
   };
}
