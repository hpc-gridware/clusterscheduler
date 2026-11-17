/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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
 * @brief The read-only data store the listener threads answer from
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_userset.h"

#include "mir/sge_mirror.h"

#include "tom/ocs_Select.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "ocs_MirrorListenerDataStore.h"

/**
 * @brief Subscribe the events the listener threads answer from: host groups, the names of the execution
 *        hosts and the usersets with the names of their entries
 */
void ocs::MirrorListenerDataStore::subscribe_events() {
   DENTER(TOP_LAYER);

   sge_mirror_subscribe(evc, SGE_TYPE_HGROUP, nullptr, nullptr, nullptr, nullptr, nullptr);
   evc->ec_set_flush(evc, sgeE_HGROUP_LIST, true, 0);
   evc->ec_set_flush(evc, sgeE_HGROUP_ADD, true, 0);
   evc->ec_set_flush(evc, sgeE_HGROUP_MOD, true, 0);
   evc->ec_set_flush(evc, sgeE_HGROUP_DEL, true, 0);

   // CS-2394: managers/operators are members of the reserved "manager"/"operator"
   // usersets, so the USERSET subscription below covers them.

   // We need just the name to see if a host is an execution host
   const auto eh_what = ocs::tom::what<EH_name>();
   sge_mirror_subscribe(evc, SGE_TYPE_EXECHOST, nullptr, nullptr, nullptr, nullptr, eh_what.get());
   evc->ec_set_flush(evc, sgeE_EXECHOST_LIST, true, 0);
   evc->ec_set_flush(evc, sgeE_EXECHOST_ADD, true, 0);
   evc->ec_set_flush(evc, sgeE_EXECHOST_MOD, true, 0);
   evc->ec_set_flush(evc, sgeE_EXECHOST_DEL, true, 0);

   // We need the usernames of ACL lists and the ACL name itself to check if certain operations are allowed
   const auto us_what = ocs::tom::what<US_name>().with<US_entries>(ocs::tom::what<UE_name>());
   sge_mirror_subscribe(evc, SGE_TYPE_USERSET, nullptr, nullptr, nullptr, nullptr, us_what.get());
   evc->ec_set_flush(evc, sgeE_USERSET_LIST, true, 0);
   evc->ec_set_flush(evc, sgeE_USERSET_ADD, true, 0);
   evc->ec_set_flush(evc, sgeE_USERSET_MOD, true, 0);
   evc->ec_set_flush(evc, sgeE_USERSET_DEL, true, 0);

   evc->ec_set_edtime(evc, 15);
   // no need to call evc->ec_commit(). This is done directly after this method returns.

   DRETURN_VOID;
}
