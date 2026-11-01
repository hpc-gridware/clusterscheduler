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
 * @brief The control switches of `qconf`, see the header
 */

#include <cstring>
#include <string>

#include "uti/sge_component.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/sge_utility.h"
#include "gdi/ocs_gdi_Client.h"
#include "msg_common.h"
#include "msg_qconf.h"

#include "ocs_qconf_control.h"

/** @brief Hand a kill request to qmaster and take its answer
 * @param lp the targets, or nullptr for every one of them
 * @param action_flag what to shut down, see ocs::gdi::Client::gdi_kill()
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 when qmaster reported an error
 */
static int
qconf_control_kill(lList *lp, const int action_flag, lList **alpp) {
   lList *alp = ocs::gdi::Client::gdi_kill(lp, action_flag);
   return qconf_answer_take(alpp, &alp);
}

/** @brief "-ks": shut down the scheduler
 * @param op unused
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 otherwise
 */
static int
qconf_control_kill_scheduler([[maybe_unused]] const qconf_op &op, lList **alpp) {
   return qconf_control_kill(nullptr, SCHEDD_KILL, alpp);
}

/** @brief "-km": shut down qmaster
 * @param op unused
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 otherwise
 */
static int
qconf_control_kill_master([[maybe_unused]] const qconf_op &op, lList **alpp) {
   return qconf_control_kill(nullptr, MASTER_KILL, alpp);
}

/** @brief "-kec id,...|all": shut down event clients; "all" means every dynamic one
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 otherwise
 */
static int
qconf_control_kill_event_clients(const qconf_op &op, lList **alpp) {
   if (op.args[0] == "all") {
      return qconf_control_kill(nullptr, EVENTCLIENT_KILL, alpp);
   }
   lList *lp = nullptr;
   lString2List(op.args[0].c_str(), &lp, ID_Type, ID_str, ", ");
   const int ret = qconf_control_kill(lp, EVENTCLIENT_KILL, alpp);
   lFreeList(&lp);
   return ret;
}

/** @brief Start or stop qmaster threads by name
 * @param op the operation, its argument the thread names
 * @param trigger SGE_THREAD_TRIGGER_START or SGE_THREAD_TRIGGER_STOP
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 otherwise
 */
static int
qconf_control_thread(const qconf_op &op, const uint32_t trigger, lList **alpp) {
   lList *lp = nullptr;
   lString2List(op.args[0].c_str(), &lp, ID_Type, ID_str, ", ");
   for_each_rw_lv(ep, lp) {
      lSetUlong(ep, ID_action, trigger);
   }
   const int ret = qconf_control_kill(lp, THREAD_START, alpp);
   lFreeList(&lp);
   return ret;
}

/** @brief "-at name,...": start qmaster threads
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 otherwise
 */
static int
qconf_control_start_thread(const qconf_op &op, lList **alpp) {
   return qconf_control_thread(op, SGE_THREAD_TRIGGER_START, alpp);
}

/** @brief "-kt name,...": stop qmaster threads
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 otherwise
 */
static int
qconf_control_stop_thread(const qconf_op &op, lList **alpp) {
   return qconf_control_thread(op, SGE_THREAD_TRIGGER_STOP, alpp);
}

/** @brief "-ke host,...|all" and "-kej host,...|all": shut down execution daemons, with their jobs for "-kej"
 *
 * The option table matches every "-k..." that no other row takes; what follows
 * the "-k" decides, as it always did.
 *
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 otherwise
 */
static int
qconf_control_kill_execds(const qconf_op &op, lList **alpp) {
   int action_flag = EXECD_KILL;
   if (op.token == "-kej") {
      action_flag |= JOB_KILL;
   } else if (op.token != "-ke") {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, MSG_ANSWER_XISNOTAVALIDOPTIONY_SU, op.token.c_str(),
                   component_get_component_id());
      return -1;
   }
   if (op.args.empty()) {
      qconf_answer(alpp, ANSWER_QUALITY_ERROR, "%s", MSG_QCONF_NEEDAHOSTNAMEORALL);
      return 1;
   }
   if (op.args[0] == "all") {
      return qconf_control_kill(nullptr, action_flag, alpp);
   }
   lList *lp = nullptr;
   lString2List(op.args[0].c_str(), &lp, EH_Type, EH_name, ", ");
   const int ret = qconf_control_kill(lp, action_flag, alpp);
   lFreeList(&lp);
   return ret;
}

/** @brief "-cq queue,...": clean queues
 * @param op the operation
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 otherwise
 */
static int
qconf_control_clean_queues(const qconf_op &op, lList **alpp) {
   lList *lp = nullptr;
   lString2List(op.args[0].c_str(), &lp, ID_Type, ID_str, ", ");
   for_each_rw_lv(ep, lp) {
      lSetUlong(ep, ID_action, QI_DO_CLEAN);
   }
   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CQ_LIST, ocs::gdi::Command::TRIGGER,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
   lFreeList(&lp);
   return qconf_answer_take(alpp, &alp);
}

/** @brief "-tsm": trigger a scheduler monitoring run
 * @param op unused
 * @param alpp the answer list of the operation
 * @return 0 on success, 1 otherwise
 */
static int
qconf_control_trigger_monitoring([[maybe_unused]] const qconf_op &op, lList **alpp) {
   lList *alp = ocs::gdi::Client::gdi_tsm();
   return qconf_answer_take(alpp, &alp);
}

/** @brief One switch served by qconf_run_control() */
struct qconf_control_entry {
   const char *spec_name;                             ///< the row of the option table it is matched by
   int (*run)(const qconf_op &op, lList **alpp);      ///< 0 on success, 1 on failure, -1 when the usage follows
};

/** @brief The control switches (CS-2986)
 *
 * Keyed by the row of the option table the parser matched, not by the token:
 * "-kec" and "-k" are prefix rows, so "-kec" serves every token that starts with
 * it and "-k" every other "-k...", "-ke" and "-kej" among them.
 */
static constexpr qconf_control_entry qconf_control_table[] = {
   {"-ks",  qconf_control_kill_scheduler},
   {"-km",  qconf_control_kill_master},
   {"-kec", qconf_control_kill_event_clients},
   {"-at",  qconf_control_start_thread},
   {"-kt",  qconf_control_stop_thread},
   {"-k",   qconf_control_kill_execds},
   {"-cq",  qconf_control_clean_queues},
   {"-tsm", qconf_control_trigger_monitoring},
};

QConfOpResult
qconf_run_control(const qconf_op &op, lList **alpp) {
   DENTER(TOP_LAYER);

   if (op.spec == nullptr) {
      DRETURN(QConfOpResult::NOT_HANDLED);
   }
   const qconf_control_entry *entry = nullptr;
   for (const qconf_control_entry &e : qconf_control_table) {
      if (strcmp(op.spec->name, e.spec_name) == 0) {
         entry = &e;
         break;
      }
   }
   if (entry == nullptr) {
      DRETURN(QConfOpResult::NOT_HANDLED);
   }
   if (qconf_missing_argument(op, alpp)) {
      DRETURN(QConfOpResult::FAILED_USAGE);
   }

   const int ret = entry->run(op, alpp);
   if (ret < 0) {
      DRETURN(QConfOpResult::FAILED_USAGE);
   }
   DRETURN(ret != 0 ? QConfOpResult::FAILED : QConfOpResult::OK);
}
