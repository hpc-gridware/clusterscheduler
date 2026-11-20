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
 * @brief The structure check of GDI request data, against a RUNNING qmaster (CS-3125)
 *
 * qmaster checks in its listener thread that the data of a request has the
 * types its target and command expect - a reduced descriptor of the type is
 * fine - before a worker takes it. Before, a sublist of another type reached
 * the object functions: with elements qmaster aborted at the first field
 * access, an empty one was stored in the master object.
 *
 * The requests are built by hand, the way a manipulated client could send
 * them, and each must be refused while qmaster keeps running: a list of the
 * wrong type at the top level, and an empty and a non-empty sublist of the
 * wrong type in a reduced exec host element (the global host, modified only
 * in its access list - and refused, so it is not changed).
 *
 * Not wired into a plain "ctest" run: it needs a cluster. Registered DISABLED,
 * like test_qmaster_procedure_mod, and installed into testbin/ so a testsuite
 * check can drive it.
 */

#include <cstdio>
#include <cstring>
#include <string>

#include "uti/ocs_ProgName.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_CAL.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "tom/ocs_Element.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_ClientBase.h"

#include "basis_types.h"
#include "cull/cull_test_api.h"

static const char *CAL_NAME = "test_cs3125_calendar";
static const char *WEEK = "mon-sun=0-24=on";

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
 * @param expr the condition that must hold
 */
#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/** @brief Send one request and report whether qmaster refused it
 *
 * Any error answers a request qmaster did not take; refused by the
 * structure check means a syntax error - the old code refused some of
 * these requests too, later and for other reasons (a missing field, the
 * caller's rights), and those must not count.
 *
 * @tparam T the object type the data list is sent as
 * @param target the target
 * @param command the command
 * @param lp the data list; taken over and freed
 * @param syntax_only count only a refusal with STATUS_ESYNTAX
 * @return true when the answer holds an error (of status STATUS_ESYNTAX with syntax_only)
 */
template <ocs::tom::ObjectType T>
static bool
refused(const ocs::gdi::Target target, const ocs::gdi::Command command, ocs::tom::ListPtr<T> &&lp,
        const bool syntax_only = true) {
   auto data = std::move(lp);
   auto alp = ocs::gdi::Client::sge_gdi(target, command, ocs::gdi::SubCommand::NONE, data);
   bool error = false;
   for (const lListElem *aep = lFirst(alp); aep != nullptr; aep = lNext(aep)) {
      if (lGetUlong(aep, AN_quality) == ANSWER_QUALITY_ERROR &&
          (!syntax_only || lGetUlong(aep, AN_status) == STATUS_ESYNTAX)) {
         error = true;
      }
   }
   if (!error) {
      answer_list_output(&alp);
   }
   alp.reset();
   data.reset();
   return error;
}

/** @brief Add or delete the test calendar over plain GDI
 * @param command ADD or DEL
 * @return true when qmaster accepted it
 */
static bool
calendar_request(const ocs::gdi::Command command) {
   auto lp = ocs::tom::ListPtr<ocs::tom::CAL>::create("calendars");
   ocs::tom::Element<ocs::tom::CAL> ep = lp->emplace_back<CAL_name>(CAL_NAME);
   if (command == ocs::gdi::Command::ADD) {
      ocs::tom::set<CAL_week_calendar>(ep, WEEK);
   }
   return !refused(ocs::gdi::Target::CAL_LIST, command, std::move(lp), false);
}

/** @brief Whether the test calendar can be read back - qmaster is alive and answers
 * @return true when the calendar is found
 */
static bool
calendar_readable() {
   ocs::tom::ListPtr<ocs::tom::CAL> lp;
   const auto where = ocs::tom::Where<ocs::tom::CAL>::from_typed(lWhere("%T(%I==%s)", CAL_Type, CAL_name, CAL_NAME));
   auto alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CAL_LIST, ocs::gdi::Command::GET,
                                        ocs::gdi::SubCommand::NONE, lp, where, ocs::tom::what_all<ocs::tom::CAL>());
   alp.reset();
   const bool found = ocs::tom::find_first<CAL_name>(*lp, CAL_NAME) != nullptr;
   lp.reset();
   return found;
}

/** @brief A modify request of the global host, reduced to its name and access list
 * @param acl the list to send as EH_acl - of the wrong type on purpose; taken over
 * @return the request list
 */
static lList *
global_host_with_acl(lList *acl) {
   lEnumeration *what = lWhat("%T(%I %I)", EH_Type, EH_name, EH_acl);
   const lDescr *descr = nullptr;
   lReduceDescr(&descr, EH_Type, what);
   lFreeWhat(&what);
   lList *lp = lCreateList("host", descr);
   lListElem *ep = lCreateElem(descr);
   lSetHost(ep, EH_name, "global");
   lSetList(ep, EH_acl, acl);
   lAppendElem(lp, ep);
   ocs_cull_descr_release(&descr);   // the list and the element hold their own references
   return lp;
}

/**
 * @brief Entry point
 * @param argc unused
 * @param argv unused
 * @return 0 when every check passed, 1 on a failure, 99 when no cluster is reachable
 */
int
main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_qmaster_request_structure");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QCONF, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      printf("\nSKIP - no cluster reachable (this test needs a running qmaster)\n");
      DRETURN(99);
   }

   // a well-typed request passes the check
   calendar_request(ocs::gdi::Command::DEL);   // in case an earlier run was interrupted
   CHECK(1, "a well-typed request is accepted", calendar_request(ocs::gdi::Command::ADD));

   // T02: a list of the wrong type at the top level - string elements sent as calendars; the old code
   // refused it too, later, for a missing field - only the syntax status tells the structure check
   {
      lList *lp = nullptr;
      lAddElemStr(&lp, ST_name, CAL_NAME, ST_Type);
      // sent as calendars on purpose, so the list is taken over unchecked
      CHECK(2, "a calendar modify request with string elements is refused as a syntax error",
            refused(ocs::gdi::Target::CAL_LIST, ocs::gdi::Command::MOD,
                    ocs::tom::ListPtr<ocs::tom::CAL>::from_typed(lp)));
   }

   // T03: an empty access list of the wrong type in the global host - it used to be stored
   {
      lList *acl = lCreateList("acl", ST_Type);
      lList *lp = global_host_with_acl(acl);
      // the sublist is of the wrong type on purpose, so the list is taken over unchecked
      CHECK(3, "an empty sublist of the wrong type is refused",
            refused(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::MOD,
                    ocs::tom::ListPtr<ocs::tom::EH>::from_typed(lp)));
   }

   // T04: a non-empty access list of the wrong type - it used to abort qmaster
   {
      lList *acl = nullptr;
      lAddElemStr(&acl, ST_name, "no_such_userset", ST_Type);
      lList *lp = global_host_with_acl(acl);
      // the sublist is of the wrong type on purpose, so the list is taken over unchecked
      CHECK(4, "a non-empty sublist of the wrong type is refused",
            refused(ocs::gdi::Target::EH_LIST, ocs::gdi::Command::MOD,
                    ocs::tom::ListPtr<ocs::tom::EH>::from_typed(lp)));
   }

   CHECK(5, "qmaster still answers after the refused requests", calendar_readable());
   CHECK(6, "the test calendar could be removed again", calendar_request(ocs::gdi::Command::DEL));

   ocs::gdi::ClientBase::shutdown();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
