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
 * @brief The changing half of the access list membership procedure, see the header
 */

#include <string>

#include "cull/cull_db.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "procedure/qconf/msg_qconf.h"
#include "msg_common.h"
#include "ocs_QConfMemberQmaster.h"
#include "sge_c_gdi.h"

namespace {
   /** @brief Write one access list through the object's own modifier
    *
    * A reduced element - the name and the whole new entry list - so nothing else
    * of the list is touched. The answer that matters to the caller is the status
    * and, on failure, the first message that says why; the generic confirmation
    * of the object layer is not passed on, because the per-user lines replace it,
    * as they did in the client.
    *
    * @param packet the client request
    * @param task the GDI task being answered
    * @param monitor for monitoring qmaster threads
    * @param object the access list row of the GDI object table
    * @param name the access list
    * @param entries the new entry list (`UE_Type`), taken over
    * @param add true to create the list, false to modify it
    * @param why receives the reason on failure, empty otherwise
    * @return the status of the write
    */
   uint32_t
   write_list(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor, gdi_object_t *object,
              const char *name, lList *entries, const bool add, std::string &why) {
      DENTER(TOP_LAYER);

      lListElem *elem = lCreateElem(US_Type);
      lSetString(elem, US_name, name);
      lSetList(elem, US_entries, entries);

      lList *alp = nullptr;
      lList *post_list = nullptr;
      const int ret = sge_gdi_add_mod_generic(packet, task, &alp, elem, add ? 1 : 0, object, packet->user,
                                              packet->host, ocs::gdi::Command::MOD_PROCEDURE,
                                              ocs::gdi::SubCommand::NONE, &post_list, monitor);
      why.clear();
      if (ret != STATUS_OK) {
         for_each_ep_lv(aep, alp) {
            if (lGetUlong(aep, AN_status) != STATUS_OK && lGetString(aep, AN_text) != nullptr) {
               why = lGetString(aep, AN_text);
               break;
            }
         }
      }
      lFreeList(&post_list);
      lFreeList(&alp);
      lFreeElem(&elem);

      DRETURN(static_cast<uint32_t>(ret));
   }

   /** @brief Add one answer with the quality its status implies
    * @param answer_list where it goes
    * @param status the status
    * @param text the message
    */
   void
   add_answer(lList **answer_list, const uint32_t status, const char *text) {
      answer_list_add(answer_list, text, status, (status == STATUS_OK) ? ANSWER_QUALITY_INFO : ANSWER_QUALITY_ERROR);
   }
}

/** @brief Add users to or remove them from access lists, see the header
 *
 * What the client's sge_client_add_user() and sge_client_del_user() did, with
 * the same messages, statuses and order - one answer per user and list, in the
 * order the users were named - but against the stored list and under the lock,
 * so a change made by someone else between a read and a write can no longer be
 * reverted, and there is no client read that RBAC could reduce.
 *
 * Adding to a list that does not exist creates it with every named user in it.
 * Removing from one reports it once, not once per user. A user named twice is
 * reported as added once and as already there for the repeat.
 *
 * The rules of the reserved lists `manager` and `operator` (CS-2394) are not
 * repeated here: userset_mod() applies them to the list as it would be written,
 * and its refusal comes back as the reason for every user of that write.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param parameter the arguments ARG_USERS, ARG_LISTS and ARG_ADD
 * @param answer_list receives one answer per user and list
 * @return true when every user could be added or removed
 */
bool
qconf_member_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                            const ocs::QConfActionParameter &parameter, lList **answer_list) {
   DENTER(TOP_LAYER);

   const std::string users_arg = parameter.get_arg(ocs::QConfActionParameter::ARG_USERS);
   const std::string lists_arg = parameter.get_arg(ocs::QConfActionParameter::ARG_LISTS);

   /* the client never sends an incomplete request; this guards the action
    * against a caller that is not qconf */
   if (users_arg.empty() || lists_arg.empty()) {
      answer_list_add(answer_list, MSG_QCONF_MEMBERINCOMPLETE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   gdi_object_t *object = get_gdi_object(ocs::gdi::Target::US_LIST);
   if (object == nullptr) {
      answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   // separated as the client always separated them
   lList *users = nullptr;
   lString2List(users_arg.c_str(), &users, UE_Type, UE_name, ", ");
   lList *lists = nullptr;
   lString2List(lists_arg.c_str(), &lists, US_Type, US_name, ", ");

   const bool add = (parameter.get_arg(ocs::QConfActionParameter::ARG_ADD) == ocs::QConfActionParameter::ARG_TRUE);
   bool all_ok = true;
   dstring buffer = DSTRING_INIT;

   for_each_ep_lv(list_arg, lists) {
      const char *list_name = lGetString(list_arg, US_name);
      const lListElem *stored = lGetElemStr(*ocs::DataStore::get_master_list(SGE_TYPE_USERSET), US_name, list_name);

      if (stored == nullptr && !add) {
         add_answer(answer_list, STATUS_EEXIST, sge_dstring_sprintf(&buffer, MSG_ACL_ACLDOESNOTEXIST_S, list_name));
         all_ok = false;
         continue;
      }

      /* Decide who has to change. "changed" is also what tells the report below
       * which users the write carried, so a user named twice is reported once as
       * changed and once as already in (or not in) the list. */
      lList *entries = (stored != nullptr) ? lCopyList("", lGetList(stored, US_entries)) : nullptr;
      lList *changed = nullptr;
      for_each_ep_lv(user_arg, users) {
         const char *user_name = lGetString(user_arg, UE_name);
         const bool present = lGetElemStr(entries, UE_name, user_name) != nullptr;
         if (add && !present) {
            lAddElemStr(&entries, UE_name, user_name, UE_Type);
            lAddElemStr(&changed, UE_name, user_name, UE_Type);
         } else if (!add && present) {
            lDelElemStr(&entries, UE_name, user_name);
            lAddElemStr(&changed, UE_name, user_name, UE_Type);
         }
      }

      uint32_t write_status = STATUS_OK;
      std::string why;
      if (stored == nullptr || lGetNumberOfElem(changed) > 0) {
         write_status = write_list(packet, task, monitor, object, list_name, entries, stored == nullptr, why);
         entries = nullptr;
      }
      lFreeList(&entries);

      // report in the order the users were named
      for_each_ep_lv(user_arg, users) {
         const char *user_name = lGetString(user_arg, UE_name);
         lListElem *carried = lGetElemStrRW(changed, UE_name, user_name);
         uint32_t status;
         const char *text;
         if (carried == nullptr) {
            status = add ? STATUS_EEXIST : STATUS_EEXIST + 1;
            text = sge_dstring_sprintf(&buffer, add ? MSG_ACL_USERINACL_SS : MSG_ACL_USERNOTINACL_SS, user_name,
                                       list_name);
         } else {
            lRemoveElem(changed, &carried);
            status = write_status;
            if (status == STATUS_OK) {
               text = sge_dstring_sprintf(&buffer, add ? MSG_ACL_ADDTOACL_SS : MSG_ACL_DELFROMACL_SS, user_name,
                                          list_name);
            } else if (!why.empty()) {
               text = sge_dstring_copy_string(&buffer, why.c_str());
            } else {
               text = sge_dstring_sprintf(&buffer, add ? MSG_ACL_CANTADDTOACL_SS : MSG_ACL_CANTDELFROMACL_SS,
                                          user_name, list_name);
            }
         }
         if (status != STATUS_OK) {
            all_ok = false;
         }
         add_answer(answer_list, status, text);
      }
      lFreeList(&changed);
   }

   sge_dstring_free(&buffer);
   lFreeList(&lists);
   lFreeList(&users);

   DRETURN(all_ok);
}
