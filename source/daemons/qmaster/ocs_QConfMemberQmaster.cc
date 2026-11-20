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
#include <vector>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_object.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_UE.h"
#include "sgeobj/tom/ocs_tom_US.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_HR.h"
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
    * @param entries the new entry list (`UE`), taken over; may be an empty handle
    * @param add true to create the list, false to modify it
    * @param why receives the reason on failure, empty otherwise
    * @return the status of the write
    */
   uint32_t
   write_list(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor, gdi_object_t *object,
              const char *name, ocs::tom::ListPtr<ocs::tom::UE> &&entries, const bool add, std::string &why) {
      DENTER(TOP_LAYER);

      auto taken_entries = std::move(entries);
      auto elem = ocs::tom::ElementPtr<ocs::tom::US>::create();
      ocs::tom::set<US_name>(elem, name);
      ocs::tom::set<US_entries>(elem, std::move(taken_entries));

      ocs::tom::ListPtr<ocs::tom::AN> alp;
      // the on_success hook of access lists leaves nothing for after the request
      const int ret = ocs_gdi_add_mod<ocs::tom::US>(packet, task, &alp, *elem, add ? 1 : 0, object, packet->user,
                                                    packet->host, ocs::gdi::Command::MOD_PROCEDURE,
                                                    ocs::gdi::SubCommand::NONE, nullptr, monitor);
      why.clear();
      if (ret != STATUS_OK) {
         for (const auto aep : *alp) {
            if (ocs::tom::get<AN_status>(aep) != STATUS_OK && ocs::tom::get<AN_text>(aep) != nullptr) {
               why = ocs::tom::get<AN_text>(aep);
               break;
            }
         }
      }
      alp.reset();
      elem.reset();

      DRETURN(static_cast<uint32_t>(ret));
   }

   /** @brief Add one answer with the quality its status implies
    * @param answer_list where it goes
    * @param status the status
    * @param text the message
    */
   void
   add_answer(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const uint32_t status, const char *text) {
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
                            const ocs::QConfActionParameter &parameter, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
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
   const auto users = ocs::tom::ListPtr<ocs::tom::UE>::from_string<UE_name>(users_arg.c_str(), ", ");
   const auto lists = ocs::tom::ListPtr<ocs::tom::US>::from_string<US_name>(lists_arg.c_str(), ", ");

   const bool add = (parameter.get_arg(ocs::QConfActionParameter::ARG_ADD) == ocs::QConfActionParameter::ARG_TRUE);
   bool all_ok = true;
   dstring buffer = DSTRING_INIT;

   const auto master_userset_list = ocs::DataStore::master_list<SGE_TYPE_USERSET>();
   for (const auto list_arg : *lists) {
      const char *list_name = ocs::tom::get<US_name>(list_arg);
      const auto stored = master_userset_list.find_first<US_name>(list_name);

      if (stored == nullptr && !add) {
         add_answer(answer_list, STATUS_EEXIST, sge_dstring_sprintf(&buffer, MSG_ACL_ACLDOESNOTEXIST_S, list_name));
         all_ok = false;
         continue;
      }

      /* Decide who has to change. "changed" is also what tells the report below
       * which users the write carried, so a user named twice is reported once as
       * changed and once as already in (or not in) the list. */
      ocs::tom::ListPtr<ocs::tom::UE> entries;
      if (stored) {
         entries = ocs::tom::get<US_entries>(stored).clone("");
      }
      // the lists are created with the first entry, as the untyped emplace_back() created them
      ocs::tom::ListPtr<ocs::tom::UE> changed;
      for (const auto user_arg : *users) {
         const char *user_name = ocs::tom::get<UE_name>(user_arg);
         const bool present = static_cast<bool>(entries->find_first<UE_name>(user_name));
         if (add && !present) {
            if (!entries) {
               entries = ocs::tom::ListPtr<ocs::tom::UE>::create("");
            }
            entries->emplace_back<UE_name>(user_name);
            if (!changed) {
               changed = ocs::tom::ListPtr<ocs::tom::UE>::create("");
            }
            changed->emplace_back<UE_name>(user_name);
         } else if (!add && present) {
            entries->erase<UE_name>(user_name);
            // an entry list left empty is freed, as the untyped erase() freed it
            if (entries->empty()) {
               entries.reset();
            }
            if (!changed) {
               changed = ocs::tom::ListPtr<ocs::tom::UE>::create("");
            }
            changed->emplace_back<UE_name>(user_name);
         }
      }

      uint32_t write_status = STATUS_OK;
      std::string why;
      if (!stored || !changed->empty()) {
         write_status = write_list(packet, task, monitor, object, list_name, std::move(entries), !stored, why);
      }
      entries.reset();

      // report in the order the users were named
      for (const auto user_arg : *users) {
         const char *user_name = ocs::tom::get<UE_name>(user_arg);
         const auto carried = changed->find_first<UE_name>(user_name);
         uint32_t status;
         const char *text;
         if (!carried) {
            status = add ? STATUS_EEXIST : STATUS_EEXIST + 1;
            text = sge_dstring_sprintf(&buffer, add ? MSG_ACL_USERINACL_SS : MSG_ACL_USERNOTINACL_SS, user_name,
                                       list_name);
         } else {
            changed->erase(carried);
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
      changed.reset();
   }

   sge_dstring_free(&buffer);

   DRETURN(all_ok);
}

/** @brief Add members to or remove them from a reserved host group, see the header
 *
 * What the client's mod_reserved_hgroup() did over GDI until CS-2985: one modify
 * of the host group list carrying the group name and every member to add or
 * remove, as a delta - APPEND or REMOVE, the mechanism behind "-aattr"/"-dattr" -
 * so the members are merged under the lock and two concurrent changes cannot
 * lose each other (CS-2438). Now the delta is built here, which makes the
 * membership of the reserved host groups take the path the reserved access
 * lists took with CS-2959.
 *
 * The request is one change for the whole list (CS-2754), so it is atomic: a
 * list naming one member that is already there, or one that is not there to be
 * removed, changes nothing. ocs_gdi_add_mod() passes on only the last of
 * several reasons, so such a list is refused naming one of them.
 *
 * What hgroup_mod() says on the way - a matcher normalised by the domain rules,
 * a duplicate collapsed, the scope of a matcher in a reserved group (CS-2680) -
 * is passed on. Its generic confirmation is not: the client reports each member
 * on a line of its own instead (CS-2645).
 *
 * The host names arrive resolved: the client resolves them, with the messages
 * it always had, and passes host group references and matchers as typed.
 *
 * @param packet the client request, for the user and host the change is logged under
 * @param task the GDI task being answered
 * @param monitor for monitoring qmaster threads
 * @param parameter the arguments ARG_GROUP, ARG_HOSTS and ARG_ADD
 * @param answer_list receives what qmaster says about the change, but not its generic confirmation
 * @return true when the change was made
 */
bool
qconf_hgroup_member_from_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                                   const ocs::QConfActionParameter &parameter,
                                   ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   const std::string group = parameter.get_arg(ocs::QConfActionParameter::ARG_GROUP);
   const std::string hosts_arg = parameter.get_arg(ocs::QConfActionParameter::ARG_HOSTS);

   /* the client never sends an incomplete request or another group; this
    * guards the action against a caller that is not qconf */
   if (hosts_arg.empty()) {
      answer_list_add(answer_list, MSG_QCONF_HGROUPMEMBERINCOMPLETE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }
   if (group != ADMIN_HOSTGROUP && group != SUBMIT_HOSTGROUP) {
      dstring buffer = DSTRING_INIT;
      answer_list_add(answer_list, sge_dstring_sprintf(&buffer, MSG_QCONF_HGROUPMEMBERNOTRESERVED_S, group.c_str()),
                      STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      sge_dstring_free(&buffer);
      DRETURN(false);
   }

   gdi_object_t *object = get_gdi_object(ocs::gdi::Target::HGRP_LIST);
   if (object == nullptr) {
      answer_list_add(answer_list, MSG_UNKNOWN_OBJECT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(false);
   }

   /* the delta: the group name and every member to add or remove */
   // created with the first member, as the untyped emplace_back() created it
   ocs::tom::ListPtr<ocs::tom::HR> hosts;
   std::string::size_type begin = 0;
   while (begin <= hosts_arg.size()) {
      const std::string::size_type end = hosts_arg.find(ocs::QConfActionParameter::LINE_SEPARATOR, begin);
      const std::string host = hosts_arg.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
      if (!host.empty()) {
         if (!hosts) {
            hosts = ocs::tom::ListPtr<ocs::tom::HR>::create("");
         }
         hosts->emplace_back<HR_name>(ocs::tom::Host(host.c_str()));
      }
      if (end == std::string::npos) {
         break;
      }
      begin = end + 1;
   }
   auto delta = ocs::tom::ElementPtr<ocs::tom::HGRP>::create();
   ocs::tom::set<HGRP_name>(delta, group.c_str());
   ocs::tom::set<HGRP_host_list>(delta, std::move(hosts));

   const bool add = (parameter.get_arg(ocs::QConfActionParameter::ARG_ADD) == ocs::QConfActionParameter::ARG_TRUE);
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   // the on_success hook of host groups leaves nothing for after the request
   const int ret = ocs_gdi_add_mod<ocs::tom::HGRP>(packet, task, &alp, *delta, 0, object, packet->user, packet->host,
                                                   ocs::gdi::Command::MOD_PROCEDURE,
                                                   add ? ocs::gdi::SubCommand::APPEND : ocs::gdi::SubCommand::REMOVE,
                                                   nullptr, monitor);
   if (ret == STATUS_OK) {
      /* the generic confirmation of the object layer is the last answer */
      if (const auto last = alp->back()) {
         alp->erase(last);
      }
   }
   answer_list_append_list(answer_list, &alp);
   delta.reset();

   DRETURN(ret == STATUS_OK);
}
