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
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/cull/sge_str_ST_L.h"
#include "sgeobj/ocs_Role.h"

#include "msg_common.h"

lListElem *
ocs::Role::locate(const lList *role_list, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(lGetElemStrRW(role_list, RL_name, name));
}

bool
ocs::Role::validate(const lListElem *role, lList **answer_list, bool startup) {
   DENTER(TOP_LAYER);

   if (const char *name = lGetString(role, RL_name);
       verify_str_key(answer_list, name, MAX_VERIFY_STRING, MSG_OBJ_ROLE, KEY_TABLE) != STATUS_OK) {
      DRETURN(false);
   }

   DRETURN(true);
}

void
ocs::Role::check_integrity(const lList *role_list, const lList *userset_list, lList **answer_list) {
   DENTER(TOP_LAYER);

   for_each_ep_lv(role, role_list) {
      const char *role_name = lGetString(role, RL_name);

      // check each user_list entry against the master userset list
      for_each_ep_lv(user_ep, lGetList(role, RL_user_list)) {
         const char *userset_name = lGetString(user_ep, US_name);
         if (!lGetElemStr(userset_list, US_name, userset_name)) {
            WARNING(MSG_ROLE_INTEGRITY_USERSET_SS, role_name, userset_name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_WARNING);
         }
      }

      // check each parent_role_list entry against the master role list
      for_each_ep_lv(parent_ep, lGetList(role, RL_parent_role_list)) {
         const char *parent_name = lGetString(parent_ep, ST_name);
         if (!lGetElemStr(role_list, RL_name, parent_name)) {
            WARNING(MSG_ROLE_INTEGRITY_PARENT_SS, role_name, parent_name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_WARNING);
         }
      }
   }

   DRETURN_VOID;
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
