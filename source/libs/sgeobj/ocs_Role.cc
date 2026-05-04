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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_Role.h"

#include "msg_common.h"

lListElem *
ocs::Role::locate(const lList *role_list, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(lGetElemStrRW(role_list, RL_name, name));
}

bool
ocs::Role::validate(lListElem *role, lList **answer_list, bool startup) {
   DENTER(TOP_LAYER);

   if (const char *name = lGetString(role, RL_name);
       verify_str_key(answer_list, name, MAX_VERIFY_STRING, MSG_OBJ_ROLE, KEY_TABLE) != STATUS_OK) {
      DRETURN(false);
   }

   DRETURN(true);
}

lListElem *
ocs::Role::create_template() {
   DENTER(TOP_LAYER);

   lListElem *role = lCreateElem(RL_Type);
   if (role == nullptr) {
      DRETURN(nullptr);
   }

   lSetString(role, RL_name, "template");
   lSetBool(role, RL_enabled, true);
   lSetString(role, RL_perm_list, "NONE");

   DRETURN(role);
}
