/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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

#include "sgeobj/ocs_Role.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_DataStore.h"

#include "spool/sge_spooling.h"

#include "evm/sge_event_master.h"

#include "sge_role_qmaster.h"
#include "sge_utility_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"

int
role_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *new_ep, lListElem *ep, int add,
         const char *ruser, const char *rhost, gdi_object_t *object,
         ocs::gdi::Command cmd, ocs::gdi::SubCommand sub_command,
         monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // ---- RL_name: copy and validate the role name on add
   if (add) {
      if (attr_mod_str(alpp, ep, new_ep, RL_name, object->object_name)) {
         // answer list already filled by attr_mod_str()
         DRETURN(STATUS_EUNKNOWN);
      }
   }
   const char *role_name = lGetString(new_ep, RL_name);

   if (add && verify_str_key(alpp, role_name, MAX_VERIFY_STRING, MSG_OBJ_ROLE, KEY_TABLE) != STATUS_OK) {
      // answer list already filled by verify_str_key()
      DRETURN(STATUS_EUNKNOWN);
   }

   // ---- RL_enabled
   attr_mod_bool(ep, new_ep, RL_enabled, "enabled");

   // ---- RL_user_list: verify all referenced usersets exist, then apply
   if (lGetPosViaElem(ep, RL_user_list, SGE_NO_ABORT) >= 0) {
      normalize_sublist(ep, RL_user_list);
      const lList *master_userset_list = *ocs::DataStore::get_master_list(SGE_TYPE_USERSET);
      if (userset_list_validate_acl_list(lGetList(ep, RL_user_list), alpp, master_userset_list) != STATUS_OK) {
         // answer list already filled by userset_list_validate_acl_list()
         DRETURN(STATUS_EUNKNOWN);
      }
      attr_mod_sub_list(alpp, new_ep, RL_user_list, US_name, ep, cmd, sub_command,
                        SGE_ATTR_USER_LISTS, SGE_OBJ_ROLE, 0, nullptr);
   }

   // ---- RL_parent_role_list: reject self-reference and unknown parents, then apply
   if (lGetPosViaElem(ep, RL_parent_role_list, SGE_NO_ABORT) >= 0) {
      normalize_sublist(ep, RL_parent_role_list);
      const lList *master_role_list = *ocs::DataStore::get_master_list(SGE_TYPE_RL);
      for_each_ep_lv(parent_ep, lGetList(ep, RL_parent_role_list)) {
         const char *parent_name = lGetString(parent_ep, ST_name);
         if (strcmp(parent_name, role_name) == 0) {
            ERROR(MSG_ROLE_SELF_REFERENCE_S, role_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
         if (!lGetElemStr(master_role_list, RL_name, parent_name)) {
            ERROR(MSG_ROLE_PARENT_DOESNOTEXIST_SS, parent_name, role_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
      }
      attr_mod_sub_list(alpp, new_ep, RL_parent_role_list, ST_name, ep, cmd, sub_command,
                        "parent_role_list", SGE_OBJ_ROLE, 0, nullptr);
   }

   // ---- RL_perm_list: validate syntax before applying
   if (lGetPosViaElem(ep, RL_perm_list, SGE_NO_ABORT) >= 0) {
      const char *perm_str = lGetString(ep, RL_perm_list);
      if (perm_str != nullptr && strcmp(perm_str, "NONE") != 0) {
         if (ocs::Role::PermRuleList rules; !ocs::Role::parse_perm_list(perm_str, rules, alpp)) {
            // answer list already filled by parse_perm_list()
            DRETURN(STATUS_EUNKNOWN);
         }
      }
   }
   attr_mod_zerostr(ep, new_ep, RL_perm_list, "perm_list");

   DRETURN(0);
}

int
role_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lList **alpp, lListElem *ep, gdi_object_t *object) {
   lList *answer_list = nullptr;

   DENTER(TOP_LAYER);

   bool dbret = spool_write_object(&answer_list, spool_get_default_context(), ep,
                                   lGetString(ep, RL_name), SGE_TYPE_RL, true);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, lGetString(ep, RL_name));
   }

   DRETURN(dbret ? 0 : 1);
}

int
role_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lListElem *old_ep, gdi_object_t *object,
             lList **ppList, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *role_name = lGetString(ep, RL_name);
   sge_add_event(0, old_ep ? sgeE_RL_MOD : sgeE_RL_ADD, 0, 0, role_name, nullptr, nullptr, ep, packet->gdi_session);

   DRETURN(0);
}

int
sge_del_role(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **alpp, char *ruser, char *rhost) {
   lList **master_role_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_RL);

   DENTER(TOP_LAYER);

   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if (lGetPosViaElem(ep, RL_name, SGE_NO_ABORT) < 0) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(RL_name), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *role_name = lGetString(ep, RL_name);
   if (!lGetElemStrRW(*master_role_list, RL_name, role_name)) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_ROLE, role_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   // refuse deletion if this role is still a parent of another role
   for_each_ep_lv(other_role, *master_role_list) {
      if (lGetSubStr(other_role, ST_name, role_name, RL_parent_role_list)) {
         ERROR(MSG_ROLE_STILL_REFERENCED_INPARENT_SS, role_name, lGetString(other_role, RL_name));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   sge_event_spool(alpp, 0, sgeE_RL_DEL, 0, 0, role_name, nullptr, nullptr,
                   nullptr, nullptr, nullptr, true, true, packet->gdi_session);
   lDelElemStr(master_role_list, RL_name, role_name);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, role_name, MSG_OBJ_ROLE);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}
