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
 * @brief The action switches of `qconf`, see the header
 */

#include <cstring>
#include <ostream>
#include <string>
#include <vector>

#include "uti/sge_log.h"
#include "uti/ocs_Pattern.h"
#include "uti/sge_string.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_userset.h"
#include "sge.h"
#include "usage.h"
#include "procedure/qconf/action/ocs_QConfActionParameter.h"
#include "procedure/qconf/write/ocs_QConfWriteModelClient.h"
#include "qconf/ocs_QConfParameterClient.h"
#include "msg_common.h"
#include "msg_qconf.h"

#include "ocs_qconf_action.h"
#include "ocs_qconf_common.h"
#include "ocs_qconf_parse.h"

/** @brief Build the arguments of "-purge queue attr_list queue@host|queue@@hgroup"
 *
 * The command line is taken apart here, with the messages it always had; what
 * the action does with it is qmaster's (CS-2948).
 *
 * @param op the operation
 * @param action receives the arguments
 * @param alpp the answer list of the operation, receives the reason the command line is refused
 * @return true when the command line was usable, false after its message
 */
static bool
qconf_action_purge_args(const qconf_op &op, ocs::QConfActionParameter &action, lList **alpp) {
   /* "queue" is the only object type -purge knows */
   if (op.args[0] != SGE_OBJ_CQUEUE) {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_QCONF_MODIFICATIONOFOBJECTNOTSUPPORTED_S, op.args[0].c_str());
      return false;
   }
   if (op.args[1].empty()) {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, SFNMAX, MSG_QCONF_NOATTRIBUTEGIVEN);
      return false;
   }
   /* the object instance looks like queue@host or queue@@hgroup */
   const std::string &instance = op.args[2];
   const std::string::size_type at = instance.find('@');
   if (at == std::string::npos || at == 0 || at + 1 == instance.size()) {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_QCONF_GIVENOBJECTINSTANCEINCOMPLETE_S, instance.c_str());
      return false;
   }
   const std::string domain = instance.substr(at + 1);
   if (domain == "@/") {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_QCONF_MODIFICATIONOFHOSTNOTSUPPORTED_S, domain.c_str());
      return false;
   }
   action.set_arg(ocs::QConfActionParameter::ARG_QUEUE, instance.substr(0, at));
   action.set_arg(ocs::QConfActionParameter::ARG_ATTRIBUTES, op.args[1]);
   action.set_arg(ocs::QConfActionParameter::ARG_DOMAIN, domain);
   return true;
}

/** @brief Build the arguments of "-au"/"-du": users and access lists, as typed
 * @param op the operation
 * @param action receives the arguments
 * @param add true for "-au"
 * @return true
 */
static bool
qconf_action_members_args(const qconf_op &op, ocs::QConfActionParameter &action, const bool add) {
   action.set_arg(ocs::QConfActionParameter::ARG_USERS, op.args[0]);
   action.set_arg(ocs::QConfActionParameter::ARG_LISTS, op.args[1]);
   action.set_arg(ocs::QConfActionParameter::ARG_ADD, add ? ocs::QConfActionParameter::ARG_TRUE : "0");
   return true;
}

/** @brief Build the arguments of "-am"/"-ao"/"-dm"/"-do": users and one reserved list (CS-2394)
 * @param op the operation
 * @param action receives the arguments
 * @param list MANAGER_USERSET or OPERATOR_USERSET
 * @param add true to add the users
 * @return true
 */
static bool
qconf_action_manop_args(const qconf_op &op, ocs::QConfActionParameter &action, const char *list, const bool add) {
   action.set_arg(ocs::QConfActionParameter::ARG_USERS, op.args[0]);
   action.set_arg(ocs::QConfActionParameter::ARG_LISTS, list);
   action.set_arg(ocs::QConfActionParameter::ARG_ADD, add ? ocs::QConfActionParameter::ARG_TRUE : "0");
   return true;
}

/** @brief Build the arguments of the share tree node switches: the node list and the operation
 * @param op the operation
 * @param action receives the arguments
 * @param operation OP_ADD, OP_MODIFY or OP_DELETE
 * @return true
 */
static bool
qconf_action_stnode_args(const qconf_op &op, ocs::QConfActionParameter &action, const char *operation) {
   action.set_arg(ocs::QConfActionParameter::ARG_NODES, op.args[0]);
   action.set_arg(ocs::QConfActionParameter::ARG_OPERATION, operation);
   return true;
}

/** @brief "-au"/"-du" with users but no list: the message these two always had
 *
 * Every other action reports a short command line in the generic words of
 * sge_parser_get_next(); these two had a message of their own for the one case
 * where the users were given and the lists were not. The usage follows, as it
 * does for the generic report.
 *
 * @param op the truncated operation
 * @param alpp the answer list of the operation, receives the message
 * @return true when it was reported here, false to report it the generic way
 */
static bool
qconf_action_members_missing(const qconf_op &op, lList **alpp) {
   if (op.args.size() != 1) {
      return false;
   }
   qconf_answer(alpp, ANSWER_QUALITY_ERROR,
                op.is("-au") ? MSG_ANSWER_NOLISTNAMEPROVIDEDTOAUX_S : MSG_ANSWER_NOLISTNAMEPROVIDEDTODUX_S,
                op.args[0].c_str());
   return true;
}

/** @brief After a failed "-astnode"/"-mstnode": whether the usage follows the messages
 *
 * As before - when nothing was changed and a node argument is not of the form
 * "path=shares".
 *
 * @param op the operation
 * @return true when the usage is to be printed
 */
static bool
qconf_action_stnode_failed(const qconf_op &op) {
   bool usage = false;
   lList *args = nullptr;
   lString2List(op.args[0].c_str(), &args, ST_Type, ST_name, ", ");
   for_each_ep_lv(arg, args) {
      const char *text = lGetString(arg, ST_name);
      const char *eq = (text != nullptr) ? strchr(text, '=') : nullptr;
      int shares;
      if (eq == nullptr || eq == text || sscanf(eq + 1, "%d", &shares) != 1) {
         usage = true;
         break;
      }
   }
   lFreeList(&args);
   return usage;
}

/** @brief One switch served by qconf_run_action() */
struct qconf_action_entry {
   const char *switch_name;                                         ///< the switch as typed
   const char *action;                                              ///< the qconf action it runs
   ocs::gdi::Target target;                                         ///< the object list it changes
   bool (*args)(const qconf_op &op, ocs::QConfActionParameter &action, lList **alpp); ///< builds the arguments, or nullptr
   bool (*missing)(const qconf_op &op, lList **alpp);               ///< a message of its own for a short command line, or nullptr
   bool (*failed)(const qconf_op &op);                              ///< whether the usage follows a failure, or nullptr
};

/** @brief The switches that run a qconf action (CS-2962, CS-2967)
 *
 * "-purge" (CS-2948), the access list membership (CS-2959), "-clearusage"
 * (CS-2963) and the share tree nodes (CS-2966). Each row names the action and
 * the object list it is addressed to, which is the list qmaster's permission
 * check asks about, and the hooks that keep what each switch did on its own.
 */
static constexpr qconf_action_entry qconf_action_table[] = {
   {"-purge", ocs::QConfActionParameter::QUEUE_PURGE, ocs::gdi::Target::CQ_LIST,
    qconf_action_purge_args, nullptr, nullptr},
   {"-au", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST,
    [](const qconf_op &op, ocs::QConfActionParameter &a, [[maybe_unused]] lList **alpp) { return qconf_action_members_args(op, a, true); },
    qconf_action_members_missing, nullptr},
   {"-du", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST,
    [](const qconf_op &op, ocs::QConfActionParameter &a, [[maybe_unused]] lList **alpp) { return qconf_action_members_args(op, a, false); },
    qconf_action_members_missing, nullptr},
   {"-am", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST,
    [](const qconf_op &op, ocs::QConfActionParameter &a, [[maybe_unused]] lList **alpp) { return qconf_action_manop_args(op, a, MANAGER_USERSET, true); },
    nullptr, nullptr},
   {"-ao", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST,
    [](const qconf_op &op, ocs::QConfActionParameter &a, [[maybe_unused]] lList **alpp) { return qconf_action_manop_args(op, a, OPERATOR_USERSET, true); },
    nullptr, nullptr},
   {"-dm", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST,
    [](const qconf_op &op, ocs::QConfActionParameter &a, [[maybe_unused]] lList **alpp) { return qconf_action_manop_args(op, a, MANAGER_USERSET, false); },
    nullptr, nullptr},
   {"-do", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST,
    [](const qconf_op &op, ocs::QConfActionParameter &a, [[maybe_unused]] lList **alpp) { return qconf_action_manop_args(op, a, OPERATOR_USERSET, false); },
    nullptr, nullptr},
   {"-astnode", ocs::QConfActionParameter::SHARETREE_NODES, ocs::gdi::Target::STN_LIST,
    [](const qconf_op &op, ocs::QConfActionParameter &a, [[maybe_unused]] lList **alpp) { return qconf_action_stnode_args(op, a, ocs::QConfActionParameter::OP_ADD); },
    nullptr, qconf_action_stnode_failed},
   {"-mstnode", ocs::QConfActionParameter::SHARETREE_NODES, ocs::gdi::Target::STN_LIST,
    [](const qconf_op &op, ocs::QConfActionParameter &a, [[maybe_unused]] lList **alpp) { return qconf_action_stnode_args(op, a, ocs::QConfActionParameter::OP_MODIFY); },
    nullptr, qconf_action_stnode_failed},
   {"-dstnode", ocs::QConfActionParameter::SHARETREE_NODES, ocs::gdi::Target::STN_LIST,
    [](const qconf_op &op, ocs::QConfActionParameter &a, [[maybe_unused]] lList **alpp) { return qconf_action_stnode_args(op, a, ocs::QConfActionParameter::OP_DELETE); },
    nullptr, nullptr},
   {"-clearusage", ocs::QConfActionParameter::USAGE_CLEAR, ocs::gdi::Target::UU_LIST, nullptr, nullptr, nullptr},
};

/** @brief One switch that changes the members of a reserved host group (CS-2985) */
struct qconf_reserved_hgroup_entry {
   const char *switch_name;   ///< the switch as typed
   const char *group;         ///< ADMIN_HOSTGROUP or SUBMIT_HOSTGROUP
   bool add;                  ///< true to add the members, false to remove them
   const char *what;          ///< the list's name in the per-member report
};

/** @brief The switches that change the members of the reserved host groups
 *
 * The host group half of the membership switches, served by the action
 * HGROUP_MEMBERS since CS-2985, as the access list half "-am"/"-ao"/"-dm"/"-do"
 * is by USERSET_MEMBERS since CS-2959.
 */
static constexpr qconf_reserved_hgroup_entry qconf_reserved_hgroup_table[] = {
   {"-ah", ADMIN_HOSTGROUP,  true,  "administrative host"},
   {"-as", SUBMIT_HOSTGROUP, true,  "submit host"},
   {"-dh", ADMIN_HOSTGROUP,  false, "administrative host"},
   {"-ds", SUBMIT_HOSTGROUP, false, "submit host"},
};

/** @brief Add members to or remove them from a reserved host group, through the action HGROUP_MEMBERS (CS-2985)
 *
 * The client's part is what it always was: take the list apart, resolve each
 * host name - a host group reference and a matcher are passed as typed, the
 * class of an entry is decided before anything is resolved (CS-2680, N-I-9) -
 * and report. A name that does not resolve is reported and left out; the rest of
 * the list is still changed, and the operation fails. qmaster changes the group
 * under the lock, as one delta for the whole list, so the list is changed as a
 * whole or not at all (CS-2754).
 *
 * What qmaster says on the way is printed first; then each member the change
 * carried is reported on a line of its own on @p os, the success report the
 * switches have had since CS-2645.
 *
 * @param os    the stream the per-member lines go to
 * @param param the invocation, for its execution context
 * @param op    the operation
 * @param entry the row of the switch
 * @param alpp  the answer list of the operation
 * @return what became of the operation
 */
static QConfOpResult
qconf_run_reserved_hgroup(std::ostream &os, const ocs::QConfParameterClient &param, const qconf_op &op,
                          const qconf_reserved_hgroup_entry &entry, lList **alpp) {
   DENTER(TOP_LAYER);

   if (qconf_missing_argument(op, alpp)) {
      DRETURN(QConfOpResult::FAILED_USAGE);
   }
   if (param.get_exec_context() != ocs::ProcedureParameter::ExecContext::SERVER) {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_QCONF_MODNEEDSSERVERCTX_S, entry.switch_name);
      DRETURN(QConfOpResult::FAILED);
   }

   std::string arg = op.args[0];
   lList *names = nullptr;
   parse_name_list_to_cull(entry.add ? "host to add" : "host to del", &names, HR_Type, HR_name, arg.data());

   bool ok = true;
   std::vector<std::string> members;
   for_each_rw_lv(name_ep, names) {
      const char *name = lGetHost(name_ep, HR_name);
      if (name == nullptr) {
         ok = false;
         continue;
      }
      if (ocs::classify_member(name) == ocs::MemberClass::LITERAL_HOST) {
         if (sge_resolve_host(name_ep, HR_name) != CL_RETVAL_OK) {
            qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_SGETEXT_CANTRESOLVEHOST_S, name);
            ok = false;
            continue;
         }
         name = lGetHost(name_ep, HR_name);
      }
      members.emplace_back(name);
   }
   lFreeList(&names);
   if (members.empty()) {
      DRETURN(ok ? QConfOpResult::OK : QConfOpResult::FAILED);
   }

   std::string hosts;
   for (const std::string &member : members) {
      if (!hosts.empty()) {
         hosts += ocs::QConfActionParameter::LINE_SEPARATOR;
      }
      hosts += member;
   }
   ocs::QConfActionParameter action(ocs::QConfActionParameter::HGROUP_MEMBERS);
   action.set_arg(ocs::QConfActionParameter::ARG_GROUP, entry.group);
   action.set_arg(ocs::QConfActionParameter::ARG_HOSTS, hosts);
   action.set_arg(ocs::QConfActionParameter::ARG_ADD, entry.add ? ocs::QConfActionParameter::ARG_TRUE : "0");

   ocs::QConfWriteModelClient model(ocs::gdi::Target::HGRP_LIST);
   lList *answer_list = nullptr;
   const bool changed = model.make_snapshot(&answer_list, action);
   if (qconf_answer_take(alpp, &answer_list) != 0 || !changed) {
      DRETURN(QConfOpResult::FAILED);
   }

   /* what qmaster said comes first, then one line per member */
   qconf_answer_print(alpp);
   for (const std::string &member : members) {
      qconf_info(os, entry.add ? MSG_QCONF_XADDEDTOYLIST_SS : MSG_QCONF_XREMOVEDFROMYLIST_SS, member.c_str(),
                 entry.what);
   }

   DRETURN(ok ? QConfOpResult::OK : QConfOpResult::FAILED);
}

QConfOpResult
qconf_run_action(std::ostream &os, const ocs::QConfParameterClient &param, const qconf_op &op, lList **alpp) {
   DENTER(TOP_LAYER);

   for (const qconf_reserved_hgroup_entry &e : qconf_reserved_hgroup_table) {
      if (op.token == e.switch_name) {
         DRETURN(qconf_run_reserved_hgroup(os, param, op, e, alpp));
      }
   }

   const qconf_action_entry *entry = nullptr;
   for (const qconf_action_entry &e : qconf_action_table) {
      if (op.token == e.switch_name) {
         entry = &e;
         break;
      }
   }
   if (entry == nullptr) {
      DRETURN(QConfOpResult::NOT_HANDLED);
   }
   if (op.spec != nullptr && op.args.size() < static_cast<size_t>(op.spec->min_args) &&
       entry->missing != nullptr && entry->missing(op, alpp)) {
      DRETURN(QConfOpResult::FAILED_USAGE);
   }
   if (qconf_missing_argument(op, alpp)) {
      DRETURN(QConfOpResult::FAILED_USAGE);
   }


   ocs::QConfActionParameter action(entry->action);
   if (entry->args != nullptr && !entry->args(op, action, alpp)) {
      DRETURN(QConfOpResult::FAILED);
   }

   if (param.get_exec_context() != ocs::ProcedureParameter::ExecContext::SERVER) {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_QCONF_MODNEEDSSERVERCTX_S, entry->switch_name);
      DRETURN(QConfOpResult::FAILED);
   }

   ocs::QConfWriteModelClient model(entry->target);
   lList *answer_list = nullptr;
   const bool ok = model.make_snapshot(&answer_list, action);
   const int shown = qconf_answer_take(alpp, &answer_list);

   if (!ok || shown != 0) {
      DRETURN((entry->failed != nullptr && entry->failed(op)) ? QConfOpResult::FAILED_USAGE : QConfOpResult::FAILED);
   }
   DRETURN(QConfOpResult::OK);
}
