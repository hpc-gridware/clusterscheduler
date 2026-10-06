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

/** @file
 * @brief A stored procedure that modifies state, against a RUNNING qmaster
 *
 * CS-2822 gives a modifying procedure its own GDI command, puts it through the
 * same permission checks a MOD passes, and binds every dispatch entry to the
 * command *and* the target it may arrive under. None of that can be checked
 * without a qmaster: the binding is enforced where the request is dispatched,
 * and the permission checks need a data store and a live packet.
 *
 * So this runs real GDI requests, the way a client would, and asserts both
 * directions:
 *
 *   - the round trip works - a calendar really changes, from the text a read
 *     would have rendered, which is what the procedure consumes
 *   - every way of addressing it wrongly is refused, and refused *before*
 *     anything is written
 *
 * The second half is the point. A procedure that modifies while wearing the
 * read command would be checked by host only - no modify permission, no admin
 * host - and one addressed to a different object list would be checked against
 * an object it does not touch. Both must be impossible, and "impossible" is
 * only worth believing if something tries.
 *
 * Runs as a manager (the testsuite user is one). The refusals under test are
 * not about this user's rights; a manager must be refused too, because what is
 * wrong is the addressing, not the caller.
 *
 * Not wired into a plain "ctest" run: it needs a cluster. Registered DISABLED,
 * like test_qmaster_hgroup_reserved, and installed into testbin/ so a testsuite
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

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_ClientBase.h"

#include "procedure/calendar/ocs_CalendarModModelClient.h"

#include "basis_types.h"

/* Both values are taken from calendars the testsuite already uses, so a failure
 * here is never a rejected syntax. */
static const char *CAL_NAME = "test_cs2822_calendar";
static const char *WEEK_BEFORE = "mon-sun=0-24=on";
static const char *WEEK_AFTER = "mon-fri=0-23=suspended";

static int s_fail = 0;

/** @brief The flat text of a calendar, in the form a read renders it
 *
 * Built by hand rather than fetched and re-rendered: the point of the write
 * procedure is that it takes text, so a test should be explicit about the text
 * it sends. The reader separates name and value by whitespace, so the padding
 * is for the eye only.
 *
 * @param name the calendar name
 * @param week its week calendar
 * @return the text to hand to the procedure
 */
static std::string
calendar_text(const char *name, const char *week) {
   /* The names are the ones CAL_fields declares - "calendar_name", "year" and
    * "week" - not the CULL field names. Taking them from the struct instead of
    * the attribute names is how a first version of this test failed with
    * "unknown attribute name". */
   return std::string("calendar_name ") + name + "\n"
          + "year          NONE\n"
          + "week          " + week + "\n";
}

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

/** @brief A model that names the writing procedure under the READ command
 *
 * The one thing a client must not be able to get away with. Everything else
 * about the request is correct, so a refusal can only come from the binding.
 */
class WrongCommandModel : public ocs::ProcedureModel {
public:
   [[nodiscard]] ocs::gdi::Command get_command() const override {
      return ocs::gdi::Command::GET_PROCEDURE;
   }

   [[nodiscard]] ocs::gdi::Target get_target() const override {
      return ocs::gdi::Target::CAL_LIST;
   }
};

/** @brief A model that addresses the writing procedure to the wrong object list
 *
 * The permission checks would then ask about cluster queues while the procedure
 * writes a calendar.
 */
class WrongTargetModel : public ocs::ProcedureModel {
public:
   [[nodiscard]] ocs::gdi::Command get_command() const override {
      return ocs::gdi::Command::MOD_PROCEDURE;
   }

   [[nodiscard]] ocs::gdi::Target get_target() const override {
      return ocs::gdi::Target::CQ_LIST;
   }
};

/** @brief Create a calendar over plain GDI
 * @param name the calendar name
 * @param week its week calendar
 * @return true when it was created
 */
static bool
add_calendar(const char *name, const char *week) {
   lList *lp = nullptr;
   lListElem *ep = lAddElemStr(&lp, CAL_name, name, CAL_Type);
   lSetString(ep, CAL_week_calendar, week);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CAL_LIST, ocs::gdi::Command::ADD,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
   const bool ok = !answer_list_has_error(&alp);
   lFreeList(&alp);
   lFreeList(&lp);
   return ok;
}

/** @brief Delete a calendar over plain GDI
 * @param name the calendar name
 * @return true when it was deleted
 */
static bool
del_calendar(const char *name) {
   lList *lp = nullptr;
   lAddElemStr(&lp, CAL_name, name, CAL_Type);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CAL_LIST, ocs::gdi::Command::DEL,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
   const bool ok = !answer_list_has_error(&alp);
   lFreeList(&alp);
   lFreeList(&lp);
   return ok;
}

/** @brief Read a calendar's week calendar back
 * @param name the calendar name
 * @return its week calendar, empty when the calendar is absent
 */
static std::string
get_week_calendar(const char *name) {
   lList *lp = nullptr;
   lCondition *where = lWhere("%T(%I==%s)", CAL_Type, CAL_name, name);
   lEnumeration *what = lWhat("%T(ALL)", CAL_Type);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CAL_LIST, ocs::gdi::Command::GET,
                                          ocs::gdi::SubCommand::NONE, &lp, where, what);
   lFreeWhere(&where);
   lFreeWhat(&what);
   lFreeList(&alp);

   /* select by name rather than lFirst(): a GET may return more than asked for */
   const lListElem *found = lGetElemStr(lp, CAL_name, name);
   const char *value = (found != nullptr) ? lGetString(found, CAL_week_calendar) : nullptr;
   std::string result = (value != nullptr) ? value : "";
   lFreeList(&lp);
   return result;
}

/** @brief Send one procedure call and report whether qmaster refused it
 * @param model the model deciding command and target
 * @param parameter the call's parameters
 * @return true when the call was refused
 */
static bool
procedure_refused(ocs::ProcedureModel &model, ocs::ProcedureParameter &parameter) {
   lList *alp = nullptr;
   const bool ok = model.make_snapshot(&alp, parameter);
   const bool refused = !ok || answer_list_has_error(&alp);
   lFreeList(&alp);
   return refused;
}

int
main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_qmaster_procedure_mod");
   lList *alp = nullptr;

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QCONF, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      printf("\nSKIP - no cluster reachable (this test needs a running qmaster)\n");
      DRETURN(99);
   }

   /* A calendar of our own, so the test neither depends on nor disturbs one the
    * cluster came with. Removed again at the end. */
   del_calendar(CAL_NAME);  /* in case an earlier run was interrupted */
   CHECK(1, "a calendar to work on could be created", add_calendar(CAL_NAME, WEEK_BEFORE));
   CHECK(2, "it starts with the expected week calendar", get_week_calendar(CAL_NAME) == WEEK_BEFORE);

   /* ---- the round trip ---------------------------------------------------- */
   {
      ocs::CalendarModParameter parameter;
      parameter.set_object_text(calendar_text(CAL_NAME, WEEK_AFTER));
      ocs::CalendarModModelClient model;

      lList *call_alp = nullptr;
      const bool ok = model.make_snapshot(&call_alp, parameter);
      if (!ok) {
         answer_list_output(&call_alp);
      }
      lFreeList(&call_alp);

      CHECK(3, "the modifying procedure was accepted", ok);
      CHECK(4, "the calendar carries the new week calendar", get_week_calendar(CAL_NAME) == WEEK_AFTER);
   }

   /* ---- every wrong way of addressing it --------------------------------- */
   {
      /* back to the known value, so each refusal can be checked against it */
      ocs::CalendarModParameter reset;
      reset.set_object_text(calendar_text(CAL_NAME, WEEK_BEFORE));
      ocs::CalendarModModelClient reset_model;
      lList *reset_alp = nullptr;
      const bool reset_ok = reset_model.make_snapshot(&reset_alp, reset);
      lFreeList(&reset_alp);
      /* The call has to be asserted, not only its effect: checking the value
       * alone would also pass when nothing ever changed, which is exactly how
       * this check passed while T03 and T04 were failing. */
      CHECK(5, "reset to the starting value worked",
            reset_ok && get_week_calendar(CAL_NAME) == WEEK_BEFORE);

      /* the writing procedure under the reading command */
      {
         ocs::CalendarModParameter parameter;
         parameter.set_object_text(calendar_text(CAL_NAME, WEEK_AFTER));
         WrongCommandModel model;
         CHECK(6, "writing procedure under GET_PROCEDURE refused",
               procedure_refused(model, parameter));
      }

      /* the writing procedure addressed to a different object list */
      {
         ocs::CalendarModParameter parameter;
         parameter.set_object_text(calendar_text(CAL_NAME, WEEK_AFTER));
         WrongTargetModel model;
         CHECK(7, "writing procedure addressed to CQ_LIST refused",
               procedure_refused(model, parameter));
      }

      /* a reading procedure under the writing command: sub-procedure "" is the
       * plain qconf read entry, which declares GET_PROCEDURE */
      {
         ocs::CalendarModParameter parameter;
         parameter.set_object_text(calendar_text(CAL_NAME, WEEK_AFTER));
         parameter.set_sub_procedure_name("");
         ocs::CalendarModModelClient model;
         CHECK(8, "reading procedure under MOD_PROCEDURE refused",
               procedure_refused(model, parameter));
      }

      /* a procedure that does not exist at all */
      {
         ocs::CalendarModParameter parameter;
         parameter.set_object_text(calendar_text(CAL_NAME, WEEK_AFTER));
         parameter.set_sub_procedure_name("no_such_sub_procedure");
         ocs::CalendarModModelClient model;
         CHECK(9, "unknown procedure under MOD_PROCEDURE refused",
               procedure_refused(model, parameter));
      }

      /* The decisive one: a guard that reports an error after writing would be
       * worse than no guard. */
      CHECK(10, "none of the refused calls changed the calendar",
            get_week_calendar(CAL_NAME) == WEEK_BEFORE);
   }

   /* ---- what the procedure itself rejects -------------------------------- */
   {
      /* an empty text has no object in it and must not reach the write lock */
      ocs::CalendarModParameter parameter;
      ocs::CalendarModModelClient model;
      CHECK(11, "a call with an empty object text is refused", procedure_refused(model, parameter));

      /* text naming a calendar that does not exist */
      ocs::CalendarModParameter missing;
      missing.set_object_text(calendar_text("test_cs2822_no_such_calendar", WEEK_AFTER));
      ocs::CalendarModModelClient missing_model;
      CHECK(12, "modifying a calendar that does not exist is refused",
            procedure_refused(missing_model, missing));
   }

   CHECK(13, "the test calendar could be removed again", del_calendar(CAL_NAME));

   ocs::gdi::ClientBase::shutdown();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
