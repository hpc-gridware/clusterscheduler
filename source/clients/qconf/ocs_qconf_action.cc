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
#include <string>

#include "uti/sge_log.h"
#include "uti/sge_string.h"
#include "sgeobj/sge_answer.h"
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

/** @brief Build the arguments of "-purge queue attr_list queue@host|queue@@hgroup"
 *
 * The command line is taken apart here, with the messages it always had; what
 * the action does with it is qmaster's (CS-2948).
 *
 * @param op the operation
 * @param action receives the arguments
 * @return true when the command line was usable, false after its message
 */
static bool
qconf_action_purge_args(const qconf_op &op, ocs::QConfActionParameter &action) {
   /* "queue" is the only object type -purge knows */
   if (op.args[0] != SGE_OBJ_CQUEUE) {
      ERROR(MSG_QCONF_MODIFICATIONOFOBJECTNOTSUPPORTED_S, op.args[0].c_str());
      return false;
   }
   if (op.args[1].empty()) {
      ERROR(SFNMAX, MSG_QCONF_NOATTRIBUTEGIVEN);
      return false;
   }
   /* the object instance looks like queue@host or queue@@hgroup */
   const std::string &instance = op.args[2];
   const std::string::size_type at = instance.find('@');
   if (at == std::string::npos || at == 0 || at + 1 == instance.size()) {
      ERROR(MSG_QCONF_GIVENOBJECTINSTANCEINCOMPLETE_S, instance.c_str());
      return false;
   }
   const std::string domain = instance.substr(at + 1);
   if (domain == "@/") {
      ERROR(MSG_QCONF_MODIFICATIONOFHOSTNOTSUPPORTED_S, domain.c_str());
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
 * where the users were given and the lists were not.
 *
 * @param op the truncated operation
 * @return true when it was reported here, false to report it the generic way
 */
static bool
qconf_action_members_missing(const qconf_op &op) {
   if (op.args.size() != 1) {
      return false;
   }
   ERROR(op.is("-au") ? MSG_ANSWER_NOLISTNAMEPROVIDEDTOAUX_S : MSG_ANSWER_NOLISTNAMEPROVIDEDTODUX_S,
         op.args[0].c_str());
   sge_usage(QCONF, stderr);
   return true;
}

/** @brief After a failed "-astnode"/"-mstnode": the usage, when an argument was malformed
 *
 * Printed as before - when nothing was changed and a node argument is not of
 * the form "path=shares".
 *
 * @param op the operation
 */
static void
qconf_action_stnode_failed(const qconf_op &op) {
   lList *args = nullptr;
   lString2List(op.args[0].c_str(), &args, ST_Type, ST_name, ", ");
   for_each_ep_lv(arg, args) {
      const char *text = lGetString(arg, ST_name);
      const char *eq = (text != nullptr) ? strchr(text, '=') : nullptr;
      int shares;
      if (eq == nullptr || eq == text || sscanf(eq + 1, "%d", &shares) != 1) {
         sge_usage(QCONF, stderr);
         break;
      }
   }
   lFreeList(&args);
}

/** @brief One switch served by qconf_run_action() */
struct qconf_action_entry {
   const char *switch_name;                                         ///< the switch as typed
   const char *action;                                              ///< the qconf action it runs
   ocs::gdi::Target target;                                         ///< the object list it changes
   QConfClientCheck check;                                          ///< the check it makes in the client
   bool (*args)(const qconf_op &op, ocs::QConfActionParameter &action); ///< builds the arguments, or nullptr
   bool (*missing)(const qconf_op &op);                             ///< a message of its own for a short command line, or nullptr
   void (*failed)(const qconf_op &op);                              ///< what it adds after a failure, or nullptr
};

/** @brief The switches that run a qconf action (CS-2962, CS-2967)
 *
 * "-purge" (CS-2948), the access list membership (CS-2959), "-clearusage"
 * (CS-2963) and the share tree nodes (CS-2966). Each row names the action and
 * the object list it is addressed to, which is the list the permission check
 * asks about, and the hooks that keep what each switch did on its own.
 */
static constexpr qconf_action_entry qconf_action_table[] = {
   {"-purge", ocs::QConfActionParameter::QUEUE_PURGE, ocs::gdi::Target::CQ_LIST, QConfClientCheck::NONE,
    qconf_action_purge_args, nullptr, nullptr},
   {"-au", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST, QConfClientCheck::NONE,
    [](const qconf_op &op, ocs::QConfActionParameter &a) { return qconf_action_members_args(op, a, true); },
    qconf_action_members_missing, nullptr},
   {"-du", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST, QConfClientCheck::NONE,
    [](const qconf_op &op, ocs::QConfActionParameter &a) { return qconf_action_members_args(op, a, false); },
    qconf_action_members_missing, nullptr},
   {"-am", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST, QConfClientCheck::NONE,
    [](const qconf_op &op, ocs::QConfActionParameter &a) { return qconf_action_manop_args(op, a, MANAGER_USERSET, true); },
    nullptr, nullptr},
   {"-ao", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST, QConfClientCheck::NONE,
    [](const qconf_op &op, ocs::QConfActionParameter &a) { return qconf_action_manop_args(op, a, OPERATOR_USERSET, true); },
    nullptr, nullptr},
   {"-dm", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST, QConfClientCheck::NONE,
    [](const qconf_op &op, ocs::QConfActionParameter &a) { return qconf_action_manop_args(op, a, MANAGER_USERSET, false); },
    nullptr, nullptr},
   {"-do", ocs::QConfActionParameter::USERSET_MEMBERS, ocs::gdi::Target::US_LIST, QConfClientCheck::NONE,
    [](const qconf_op &op, ocs::QConfActionParameter &a) { return qconf_action_manop_args(op, a, OPERATOR_USERSET, false); },
    nullptr, nullptr},
   {"-astnode", ocs::QConfActionParameter::SHARETREE_NODES, ocs::gdi::Target::STN_LIST, QConfClientCheck::NONE,
    [](const qconf_op &op, ocs::QConfActionParameter &a) { return qconf_action_stnode_args(op, a, ocs::QConfActionParameter::OP_ADD); },
    nullptr, qconf_action_stnode_failed},
   {"-mstnode", ocs::QConfActionParameter::SHARETREE_NODES, ocs::gdi::Target::STN_LIST, QConfClientCheck::NONE,
    [](const qconf_op &op, ocs::QConfActionParameter &a) { return qconf_action_stnode_args(op, a, ocs::QConfActionParameter::OP_MODIFY); },
    nullptr, qconf_action_stnode_failed},
   {"-dstnode", ocs::QConfActionParameter::SHARETREE_NODES, ocs::gdi::Target::STN_LIST, QConfClientCheck::NONE,
    [](const qconf_op &op, ocs::QConfActionParameter &a) { return qconf_action_stnode_args(op, a, ocs::QConfActionParameter::OP_DELETE); },
    nullptr, nullptr},
   {"-clearusage", ocs::QConfActionParameter::USAGE_CLEAR, ocs::gdi::Target::UU_LIST,
    QConfClientCheck::MANAGER_ON_ADMIN_HOST, nullptr, nullptr, nullptr},
};

QConfOpResult
qconf_run_action(const ocs::QConfParameterClient &param, const qconf_op &op, const char *username,
                 const char *hostname) {
   DENTER(TOP_LAYER);

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
       entry->missing != nullptr && entry->missing(op)) {
      DRETURN(QConfOpResult::FAILED);
   }
   if (qconf_missing_argument(op)) {
      DRETURN(QConfOpResult::FAILED);
   }

   qconf_client_check(entry->check, username, hostname);

   ocs::QConfActionParameter action(entry->action);
   if (entry->args != nullptr && !entry->args(op, action)) {
      DRETURN(QConfOpResult::FAILED);
   }

   if (param.get_exec_context() != ocs::ProcedureParameter::ExecContext::SERVER) {
      ERROR(MSG_QCONF_MODNEEDSSERVERCTX_S, entry->switch_name);
      DRETURN(QConfOpResult::FAILED);
   }

   ocs::QConfWriteModelClient model(entry->target);
   lList *answer_list = nullptr;
   const bool ok = model.make_snapshot(&answer_list, action);
   const int shown = show_answer_list(answer_list);
   lFreeList(&answer_list);

   if (!ok || shown != 0) {
      if (entry->failed != nullptr) {
         entry->failed(op);
      }
      DRETURN(QConfOpResult::FAILED);
   }
   DRETURN(QConfOpResult::OK);
}
