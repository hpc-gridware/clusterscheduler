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
 *  See the License for the specific provisions governing your rights and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

/** @file
 * @brief The cache of hosts a matcher of a host group has already admitted
 */

#include "cull/cull.h"

namespace ocs {
   /** @brief Layer 3a: the hosts a matcher has already admitted
    *
    * A host admitted through a matcher is by definition **not** in the resolved
    * membership, so every one of its requests misses layer 2 - and without this
    * cache the complete recursive matcher walk would run on the entry path of
    * the qmaster, per task, per request. The cache is therefore not an optional
    * accelerator but part of the design; without it the cost of the feature is
    * permanent and paid on the hottest path there is.
    *
    * **This lives in the core although only the commercial edition fills it.**
    * Everything about the cache is core: the element type, the field on the host
    * group, the configured period, the resolved membership it prunes itself
    * against, and the event merge across which it has to be carried. What is
    * commercial is the one question of whether a host is admitted at all. In a
    * build without extensions no group ever carries a matcher, so nothing here
    * is ever entered - the same way the rest of the core stays inert.
    *
    * The class is a namespace with access control: all members are static, no
    * instance is ever created.
    *
    * Two kinds of access, guarded differently, and that split is what keeps the
    * design affordable. The frequent one - a hit - writes **one field** and
    * needs atomicity, not mutual exclusion. The rare one changes the
    * **structure** of the list and takes the one lock this module owns.
    */
   class MatchCache {
   public:
      static bool lookup(lListElem *hgroup, const char *hostname);

      static void insert(lListElem *hgroup, const char *hostname);

      static lList *detach(lListElem *hgroup);

      static void adopt(lListElem *hgroup, lList **cache);
   };
}
