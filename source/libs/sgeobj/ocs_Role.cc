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

bool
ocs::Role::parse_perm_list(const char *perm_list_str, PermRuleList &rules, lList **answer_list) {
   DENTER(TOP_LAYER);

   rules.clear();

   // "NONE" means an empty rule set — always valid
   if (perm_list_str == nullptr || strcmp(perm_list_str, "NONE") == 0) {
      DRETURN(true);
   }

   auto trim = [](const std::string &s) -> std::string {
      auto start = s.find_first_not_of(" \t\r\n");
      if (start == std::string::npos) return "";
      auto end = s.find_last_not_of(" \t\r\n");
      return s.substr(start, end - start + 1);
   };

   // split perm_list by commas into individual rule strings
   const std::string perm_list(perm_list_str);
   size_t pos = 0;
   while (true) {
      const size_t comma = perm_list.find(',', pos);
      const std::string rule_str = trim(perm_list.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));

      if (!rule_str.empty()) {
         // count colons to report an accurate field count on error
         int n_colons = 0;
         for (const char c : rule_str) if (c == ':') ++n_colons;

         if (n_colons < 5) {
            ERROR(MSG_ROLE_PERMLIST_NFIELDS_SI, rule_str.c_str(), n_colons + 1);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }

         // split on the first 5 colons; field 6 receives everything after the 5th colon
         PermRule rule;
         std::string *field_ptrs[6] = {
            &rule.source, &rule.origin, &rule.operation,
            &rule.object_type, &rule.object_key, &rule.value_constraint
         };
         size_t rpos = 0;
         for (int i = 0; i < 5; ++i) {
            const size_t colon = rule_str.find(':', rpos);
            *field_ptrs[i] = trim(rule_str.substr(rpos, colon - rpos));
            rpos = colon + 1;
         }
         rule.value_constraint = trim(rule_str.substr(rpos));

         // validate that no field is empty
         for (int i = 0; i < 6; ++i) {
            if (field_ptrs[i]->empty()) {
               ERROR(MSG_ROLE_PERMLIST_EMPTYFIELD_SI, rule_str.c_str(), i + 1);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }
         }

         rules.push_back(std::move(rule));
      }

      if (comma == std::string::npos) break;
      pos = comma + 1;
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
