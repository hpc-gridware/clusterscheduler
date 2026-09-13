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
 * @brief Host group members that describe a set of hosts instead of naming one
 */

#include "cull/cull.h"

#include "uti/sge_dstring.h"

namespace ocs {
   /** @brief What a matcher means, as opposed to how it is recognised
    *
    * A host group member may name one host, refer to another group, or - since
    * CS-2680 - **describe** a set of hosts: `host:gpu*` admits every host whose
    * name matches, without that host having to be entered anywhere.
    *
    * Recognising a matcher is a prefix comparison and lives in the core, in
    * `ocs::classify_member()` and its neighbours in `uti/ocs_Pattern.h`, because
    * the host name normaliser needs it and every host value of the system passes
    * through that. Everything that *interprets* a matcher lives behind this
    * interface, and the implementation behind it is selected at build time: the
    * commercial tree carries the real one, this tree a stub that refuses.
    *
    * The class is a namespace with access control - all members are static, no
    * instance is ever created.
    *
    * The interface is deliberately **coarse**. Each entry point answers a whole
    * question rather than a step towards one; there is none for normalising a
    * payload, for detecting a catch-all or for comparing one matcher against one
    * name, because exposing those would move the semantics into this tree one
    * function at a time.
    */
   class Matcher {
   public:
      static bool prepare(const char *member, const lListElem *hgroup, dstring *out, lList **answer_list);

      static bool expand(const lList *member_list, const lList *candidates, lList **hosts,
                         lList **answer_list);

      static bool report_ineffective(const lList *master_hgroup_list, lList **answer_list);
   };
}
