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
 * @brief The route wording of `qconf -shgrp_why` (CS-2816)
 *
 * One class per header, one directory per sub-procedure, as `qstat` does it for
 * its four output formats.
 */

#include "../ocs_QConfModelBase.h"
#include "../ocs_QConfViewBase.h"

namespace ocs {
   /** @brief `qconf -shgrp_why`: by which route a host is a member
    *
    * The route was decided in the model, where the graph and the answer list are;
    * this only words it. No JSON form, as above.
    *
    * @ingroup libprocedure
    */
   class QConfHGroupWhyView : public QConfViewBase {
   public:
      explicit QConfHGroupWhyView(const QConfParameter &parameter) : QConfViewBase(parameter) {}
      ~QConfHGroupWhyView() override = default;

      /** @brief Not reached, see QConfHGroupStructureView::show_name_list() */
      void show_name_list([[maybe_unused]] std::ostream &os, [[maybe_unused]] const lList *list,
                          [[maybe_unused]] int key_nm,
                          [[maybe_unused]] const qconf_object_desc &desc) override {
      }

      void show_why(std::ostream &os, const QConfModelBase::HGroupWhy &why);
   };
}
