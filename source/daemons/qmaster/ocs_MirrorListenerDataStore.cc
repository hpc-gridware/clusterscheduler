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

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_userset.h"

#include "mir/sge_mirror.h"

#include "ocs_MirrorListenerDataStore.h"

namespace ocs {
   MirrorListenerDataStore::MirrorListenerDataStore() : MirrorDataStore(DataStore::Id::LISTENER, LOCK_LISTENER) {
      ;
   }

   void MirrorListenerDataStore::subscribe_events() {
      sge_mirror_subscribe(evc, SGE_TYPE_ADMINHOST, nullptr, nullptr, nullptr, nullptr, nullptr);
      evc->ec_set_flush(evc, sgeE_ADMINHOST_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_ADMINHOST_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_ADMINHOST_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_ADMINHOST_DEL, true, 0);

      sge_mirror_subscribe(evc, SGE_TYPE_SUBMITHOST, nullptr, nullptr, nullptr, nullptr, nullptr);
      evc->ec_set_flush(evc, sgeE_SUBMITHOST_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_SUBMITHOST_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_SUBMITHOST_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_SUBMITHOST_DEL, true, 0);

      sge_mirror_subscribe(evc, SGE_TYPE_HGROUP, nullptr, nullptr, nullptr, nullptr, nullptr);
      evc->ec_set_flush(evc, sgeE_HGROUP_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_HGROUP_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_HGROUP_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_HGROUP_DEL, true, 0);

      sge_mirror_subscribe(evc, SGE_TYPE_MANAGER, nullptr, nullptr, nullptr, nullptr, nullptr);
      evc->ec_set_flush(evc, sgeE_MANAGER_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_MANAGER_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_MANAGER_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_MANAGER_DEL, true, 0);

      sge_mirror_subscribe(evc, SGE_TYPE_OPERATOR, nullptr, nullptr, nullptr, nullptr, nullptr);
      evc->ec_set_flush(evc, sgeE_OPERATOR_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_OPERATOR_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_OPERATOR_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_OPERATOR_DEL, true, 0);

      // We need just the name to see if a host is an execution host
      lEnumeration *eh_what = lWhat("%T(%I)", EH_Type, EH_name);
      sge_mirror_subscribe(evc, SGE_TYPE_EXECHOST, nullptr, nullptr, nullptr, nullptr, eh_what);
      lFreeWhat(&eh_what);
      evc->ec_set_flush(evc, sgeE_EXECHOST_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_EXECHOST_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_EXECHOST_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_EXECHOST_DEL, true, 0);

      // We need the usernames of ACL lists and the ACL name itself to check if certain operations are allowed
      lEnumeration *us_what = lWhat("%T(%I%I->(%I))", US_Type, US_name, US_entries, UE_name);
      sge_mirror_subscribe(evc, SGE_TYPE_USERSET, nullptr, nullptr, nullptr, nullptr, us_what);
      lFreeWhat(&us_what);
      evc->ec_set_flush(evc, sgeE_USERSET_LIST, true, 0);
      evc->ec_set_flush(evc, sgeE_USERSET_ADD, true, 0);
      evc->ec_set_flush(evc, sgeE_USERSET_MOD, true, 0);
      evc->ec_set_flush(evc, sgeE_USERSET_DEL, true, 0);

      evc->ec_set_edtime(evc, 15);
      // no need to call evc->ec_commit(). This is done directly after this method returns.
   }
}
