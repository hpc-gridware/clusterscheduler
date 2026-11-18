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
 * @brief RBAC roles
 */

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_Role.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_RL.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "spool/sge_spooling.h"

#include "evm/sge_event_master.h"

#include "sge_role_qmaster.h"
#include "sge_utility_qmaster.h"
#include "sge_persistence_qmaster.h"
#include "msg_common.h"
#include "msg_qmaster.h"

/** @brief Apply one attribute change to a RBAC role
 *
 * The modifier of RBAC roles, bound in ocs_gdi_hooks<ocs::tom::RL>; see ocs_gdi_add_mod() in
 * sge_c_gdi.cc for the sequence it is called from.
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param new_ep the role as it is to become
 * @param ep the reduced element the client sent; its lists are normalized and consumed
 * @param add 1 for add, 0 for modify
 * @param ruser the requesting user
 * @param rhost the requesting host
 * @param object the table entry for this object type
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
role_mod(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
         const ocs::tom::Element<ocs::tom::RL> new_ep, const ocs::tom::Element<ocs::tom::RL> ep, int add,
         const char *ruser, const char *rhost, gdi_object_t *object, ocs::gdi::Command cmd,
         ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);


   // ---- RL_name: copy and validate the role name on add
   if (add) {
      // a nullptr name is refused by verify_obj_name() below
      ocs::tom::copy_if_present<RL_name>(new_ep, ep);
   }
   const char *role_name = ocs::tom::get<RL_name>(new_ep);

   if (add && verify_obj_name(alpp, role_name, MAX_VERIFY_STRING, MSG_OBJ_ROLE) != STATUS_OK) {
      // answer list already filled by verify_obj_name()
      DRETURN(STATUS_EUNKNOWN);
   }

   // ---- RL_enabled
   ocs::tom::copy_if_present<RL_enabled>(new_ep, ep);

   // ---- RL_user_list: verify all referenced usersets exist, then apply
   if (ocs::tom::has<RL_user_list>(ep)) {
      normalize_sublist<RL_user_list>(ep);
      const ocs::tom::ConstList<ocs::tom::US> master_userset_list(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET));
      if (userset_list_validate_acl_list(ocs::tom::get<RL_user_list>(ep), alpp, master_userset_list) != STATUS_OK) {
         // answer list already filled by userset_list_validate_acl_list()
         DRETURN(STATUS_EUNKNOWN);
      }
      attr_mod_sub_list<RL_user_list, US_name>(alpp, new_ep, ep, sub_command, SGE_ATTR_USER_LISTS, SGE_OBJ_ROLE);
   }

   // ---- RL_parent_role_list: reject self-reference and unknown parents, then apply
   if (ocs::tom::has<RL_parent_role_list>(ep)) {
      normalize_sublist<RL_parent_role_list>(ep);
      const ocs::tom::ConstList<ocs::tom::RL> master_role_list(*ocs::DataStore::get_master_list(SGE_TYPE_RL));
      for (const ocs::tom::ConstElement<ocs::tom::ST> parent_ep : ocs::tom::get<RL_parent_role_list>(ep)) {
         const char *parent_name = ocs::tom::get<ST_name>(parent_ep);
         if (strcmp(parent_name, role_name) == 0) {
            ERROR(MSG_ROLE_SELF_REFERENCE_S, role_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
         if (!ocs::tom::find_first<RL_name>(master_role_list, parent_name)) {
            ERROR(MSG_ROLE_PARENT_DOESNOTEXIST_SS, parent_name, role_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
         if (ocs::Role::would_create_cycle(role_name, parent_name, master_role_list)) {
            ERROR(MSG_ROLE_PARENT_CYCLE_SS, parent_name, role_name);
            answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_EUNKNOWN);
         }
      }
      attr_mod_sub_list<RL_parent_role_list, ST_name>(alpp, new_ep, ep, sub_command, "parent_role_list",
                                                      SGE_OBJ_ROLE);
   }

   // ---- RL_perm_list: validate syntax before applying
   if (ocs::tom::has<RL_perm_list>(ep)) {
      const char *perm_str = ocs::tom::get<RL_perm_list>(ep);
      if (perm_str != nullptr && strcmp(perm_str, "NONE") != 0) {
         if (ocs::Role::PermRuleList rules; !ocs::Role::parse_perm_list(perm_str, rules, alpp)) {
            // answer list already filled by parse_perm_list()
            DRETURN(STATUS_EUNKNOWN);
         }
      }
   }
   ocs::tom::copy_if_present<RL_perm_list>(new_ep, ep);

   DRETURN(0);
}

/** @brief Write a RBAC role to the spool
 *
 * The writer of RBAC roles, bound in ocs_gdi_hooks<ocs::tom::RL> (sge_c_gdi.cc).
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param alpp receives messages for the caller
 * @param ep the object
 * @param object the table entry for this object type
 * @return 0 on success
 */
int
role_spool(ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::tom::ListPtr<ocs::tom::AN> *alpp,
           const ocs::tom::ConstElement<ocs::tom::RL> ep, gdi_object_t *object) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> answer_list;

   const bool dbret = spool_write_object(&answer_list, spool_get_default_context(), ep,
                                   ocs::tom::get<RL_name>(ep), SGE_TYPE_RL);
   answer_list_output(&answer_list);

   if (!dbret) {
      answer_list_add_sprintf(alpp, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                              MSG_PERSISTENCE_WRITE_FAILED_S, ocs::tom::get<RL_name>(ep));
   }

   DRETURN(dbret ? 0 : 1);
}

/** @brief Announce a RBAC role change once it is safely spooled
 *
 * The on_success of RBAC roles, bound in ocs_gdi_hooks<ocs::tom::RL> (sge_c_gdi.cc).
 *
 * @param packet the client request
 * @param task the GDI task being answered
 * @param ep the object
 * @param old_ep the object as it was, or an empty handle on add
 * @param object the table entry for this object type
 * @param post what is left for after the last object of the request, unused
 * @param monitor for monitoring qmaster threads
 * @return 0 on success
 */
int
role_success(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::Element<ocs::tom::RL> ep,
             const ocs::tom::ConstElement<ocs::tom::RL> old_ep, gdi_object_t *object,
             [[maybe_unused]] ocs_gdi_post_processing *post, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   const char *role_name = ocs::tom::get<RL_name>(ep);
   sge_add_event(0, old_ep ? sgeE_RL_MOD : sgeE_RL_ADD, 0, 0, role_name, nullptr, nullptr, ep, packet->gdi_session);

   DRETURN(0);
}

/** @brief Delete a role from the master role list
 *
 * @param packet the GDI packet holding the request
 * @param task the GDI task within that packet
 * @param ep an element carrying at least the `RL_name` of the role to delete
 * @param alpp used to return error messages
 * @param ruser the user who requested the deletion
 * @param rhost the host the request came from
 * @return `STATUS_OK`, or one of `STATUS_EUNKNOWN` / `STATUS_EEXIST` with
 *         `alpp` filled
 */
int
sge_del_role(ocs::gdi::Packet *packet, ocs::gdi::Task *task, const ocs::tom::ConstElement<ocs::tom::RL> ep,
             ocs::tom::ListPtr<ocs::tom::AN> *alpp, char *ruser, char *rhost) {
   DENTER(TOP_LAYER);

   // raw out-parameter of the DataStore API: ocs::tom::erase() below frees the list when it becomes empty
   lList **master_role_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_RL);

   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   if (!ocs::tom::has<RL_name>(ep)) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(RL_name), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   const char *role_name = ocs::tom::get<RL_name>(ep);
   const ocs::tom::ConstList<ocs::tom::RL> role_list(*master_role_list);
   if (!role_list.find_first<RL_name>(role_name)) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, MSG_OBJ_ROLE, role_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   // refuse deletion if this role is still a parent of another role
   for (const auto other_role : role_list) {
      if (ocs::tom::find_first_in<RL_parent_role_list, ST_name>(other_role, role_name)) {
         ERROR(MSG_ROLE_STILL_REFERENCED_INPARENT_SS, role_name, ocs::tom::get<RL_name>(other_role));
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         DRETURN(STATUS_EUNKNOWN);
      }
   }

   sge_event_spool(alpp, 0, sgeE_RL_DEL, 0, 0, role_name, nullptr, nullptr,
                   nullptr, nullptr, nullptr, true, true, packet->gdi_session);
   ocs::tom::erase<RL_name>(master_role_list, role_name);

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, role_name, MSG_OBJ_ROLE);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}

