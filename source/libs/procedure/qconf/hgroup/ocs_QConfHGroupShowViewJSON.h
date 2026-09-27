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
 * @brief The JSON rendering of `qconf -shgrp` (CS-2816)
 *
 * One class per header, one directory per sub-procedure, as `qstat` does it for
 * its four output formats.
 */

#include "../ocs_QConfViewJSON.h"

namespace ocs {
   /** @brief `qconf -shgrp -fmt json`: the member list with its classes split out
    *
    * CS-2680 gave the machine readable form three keys of their own - hosts,
    * hostgroups and matchers - so that a program need not guess the class of an
    * entry from its text. They are written *beside* the unchanged `hostlist`,
    * which is the definition as stored and what `-Mhgrp` reads back, so the
    * round trip stays idempotent.
    *
    * Only a group carrying a matcher gets them. Splitting the entries of every
    * other group would change the output of installations that never heard of
    * the feature, and a member list without matchers holds nothing a reader
    * cannot already tell apart by the leading `@`.
    *
    * @ingroup libprocedure
    */
   class QConfHGroupShowViewJSON : public QConfViewJSON {
   public:
      explicit QConfHGroupShowViewJSON(const QConfParameter &parameter) : QConfViewJSON(parameter) {}
      ~QConfHGroupShowViewJSON() override = default;

      void show_object(std::ostream &os, const lListElem *ep, const qconf_object_desc &desc) override;
   };
}
