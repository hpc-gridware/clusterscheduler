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
 * @brief Unit tests for the default requests of a parallel job in `libs/sched`
 *
 * parallel_rc_slots_by_time() evaluates the default request of every consumable a job did not
 * request. It used to write the default value into the complex definition it found in
 * a->centry_list. In qmaster that list is the master complex list (job verification with
 * `qsub -w e|v`, advance reservations), so the master complex entries were modified without an
 * event, and with BerkeleyDB spooling the value could end up in the spooled record (CS-3196).
 *
 * The tests pin both halves of the contract: the complex definitions are not touched, and the
 * default requests still limit the slots exactly as before.
 *
 * The implicit request slots=1 was a function-local static, created on first use without any
 * synchronisation while the qmaster worker threads and the scheduler thread match jobs at the
 * same time (CS-482). It is kept in the assignment now; the tests pin that each assignment owns
 * its own request, that it limits the slots as before and that assignment_release() frees it.
 */

#include <climits>
#include <cstdio>
#include <cstring>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_centry.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_RUE.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "sge_select_queue.h"

static int failures = 0;

/**
 * @brief Record the outcome of one check
 * @param id the id of the check, printed as `[Tnn]`
 * @param what what the check is about
 * @param ok whether the check held
 */
static void
check(const char *id, const char *what, const bool ok) {
   if (ok) {
      printf("ok    [%s] %s\n", id, what);
   } else {
      printf("FAIL  [%s] %s\n", id, what);
      failures++;
   }
}

/**
 * @brief Compare two strings which may be nullptr
 * @param a the first string
 * @param b the second string
 * @return true if both are nullptr or both are equal
 */
static bool
same_string(const char *a, const char *b) {
   if (a == nullptr || b == nullptr) {
      return a == b;
   }
   return strcmp(a, b) == 0;
}

/**
 * @brief Add a consumable complex definition with a default request to a complex list
 * @param centry_list the complex list
 * @param name the name of the complex
 * @param defaultval the default request
 */
static void
add_consumable(const ocs::tom::List<ocs::tom::CE> centry_list, const char *name, const char *defaultval) {
   auto ep = centry_list.emplace_back<CE_name>(name);
   ep.set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::INT));
   ep.set<CE_relop>(CMPLXLE_OP);
   ep.set<CE_requestable>(REQU_YES);
   ep.set<CE_consumable>(CONSUMABLE_YES);
   ep.set<CE_defaultval>(defaultval);
}

/**
 * @brief Add a capacity and an empty utilization for a consumable
 * @param total_list the capacities (`CE`)
 * @param rue_list the utilization (`RUE`)
 * @param name the name of the consumable
 * @param total the capacity
 */
static void
add_capacity(const ocs::tom::List<ocs::tom::CE> total_list, const ocs::tom::List<ocs::tom::RUE> rue_list,
             const char *name, const double total) {
   char buf[64];
   snprintf(buf, sizeof(buf), "%.0f", total);
   auto tep = total_list.emplace_back<CE_name>(name);
   tep.set<CE_doubleval>(total);
   tep.set<CE_stringval>(buf);
   auto uep = rue_list.emplace_back<RUE_name>(name);
   uep.set<RUE_utilized_now>(0.0);
}

/**
 * @brief The default requests of a job are evaluated without writing into the complex definitions
 *
 * Two consumables with a default request, none of them requested by the job: `dr_a` (default 2,
 * capacity 10, enough for 5 slots) and `dr_b` (default 3, capacity 9, enough for 3 slots). The
 * global level is matched twice, as the scheduler does for every candidate.
 */
static void
test_default_request_leaves_centry_untouched() {
   auto centry_list = ocs::tom::ListPtr<ocs::tom::CE>::create("");
   add_consumable(*centry_list, "dr_a", "2");
   add_consumable(*centry_list, "dr_b", "3");

   // what a complex definition carries before the match - the master list's entries have no value
   const auto a_before = centry_list->find_first<CE_name>("dr_a");
   const auto b_before = centry_list->find_first<CE_name>("dr_b");
   const char *a_stringval = a_before.get<CE_stringval>();
   const double a_doubleval = a_before.get<CE_doubleval>();
   const char *b_stringval = b_before.get<CE_stringval>();
   const double b_doubleval = b_before.get<CE_doubleval>();

   auto total_list = ocs::tom::ListPtr<ocs::tom::CE>::create("");
   auto rue_list = ocs::tom::ListPtr<ocs::tom::RUE>::create("");
   add_capacity(*total_list, *rue_list, "dr_a", 10.0);
   add_capacity(*total_list, *rue_list, "dr_b", 9.0);

   // a parallel job without any resource request
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   auto pe = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   pe->set<PE_name>("test_pe");

   sge_assignment_t a{};
   a.job = *job;
   a.pe = *pe;
   a.pe_name = "test_pe";
   a.centry_list = *centry_list;
   a.start = DISPATCH_TIME_NOW;
   a.now = 1000;
   a.duration = 60;

   for (int round = 1; round <= 2; round++) {
      int slots = 0;
      bool found_master_host = false;
      const auto result = parallel_rc_slots_by_time(&a, &slots, *total_list, *rue_list,
                                                    ocs::tom::ConstList<ocs::tom::HL>(), false,
                                                    ocs::tom::ConstElement<ocs::tom::EH>(),
                                                    ocs::tom::Element<ocs::tom::QU>(), DOMINANT_LAYER_GLOBAL, 0.0,
                                                    GLOBAL_TAG, false, false, found_master_host, false,
                                                    SGE_GLOBAL_NAME, false);

      char id[16];
      snprintf(id, sizeof(id), "T%02d", round * 10 + 1);
      check(id, "the default requests match", result == DISPATCH_OK);
      snprintf(id, sizeof(id), "T%02d", round * 10 + 2);
      check(id, "the default requests limit the slots to the scarcer consumable (3)", slots == 3);

      const auto a_after = centry_list->find_first<CE_name>("dr_a");
      const auto b_after = centry_list->find_first<CE_name>("dr_b");
      snprintf(id, sizeof(id), "T%02d", round * 10 + 3);
      check(id, "CE_stringval of the complex definition dr_a is unchanged",
            same_string(a_after.get<CE_stringval>(), a_stringval));
      snprintf(id, sizeof(id), "T%02d", round * 10 + 4);
      check(id, "CE_doubleval of the complex definition dr_a is unchanged",
            a_after.get<CE_doubleval>() == a_doubleval);
      snprintf(id, sizeof(id), "T%02d", round * 10 + 5);
      check(id, "CE_stringval of the complex definition dr_b is unchanged",
            same_string(b_after.get<CE_stringval>(), b_stringval));
      snprintf(id, sizeof(id), "T%02d", round * 10 + 6);
      check(id, "CE_doubleval of the complex definition dr_b is unchanged",
            b_after.get<CE_doubleval>() == b_doubleval);
   }

   assignment_release(&a);
}

/**
 * @brief The implicit slots request is owned by the assignment and still limits the slots
 *
 * The global level offers 2 slots and the consumable `dr_a` (default 2, capacity 10) enough for 5,
 * so the implicit slots request is the one that limits the job to 2 slots.
 */
static void
test_implicit_slots_request_in_assignment() {
   auto centry_list = ocs::tom::ListPtr<ocs::tom::CE>::create("");
   add_consumable(*centry_list, SGE_ATTR_SLOTS, "0");
   add_consumable(*centry_list, "dr_a", "2");

   auto total_list = ocs::tom::ListPtr<ocs::tom::CE>::create("");
   auto rue_list = ocs::tom::ListPtr<ocs::tom::RUE>::create("");
   add_capacity(*total_list, *rue_list, SGE_ATTR_SLOTS, 2.0);
   add_capacity(*total_list, *rue_list, "dr_a", 10.0);

   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   auto pe = ocs::tom::ElementPtr<ocs::tom::PE>::create();
   pe->set<PE_name>("test_pe");

   sge_assignment_t a{};
   a.job = *job;
   a.pe = *pe;
   a.pe_name = "test_pe";
   a.centry_list = *centry_list;
   a.start = DISPATCH_TIME_NOW;
   a.now = 1000;
   a.duration = 60;

   check("T31", "a new assignment has no implicit slots request", !a.implicit_slots_request);

   for (int round = 1; round <= 2; round++) {
      int slots = 0;
      bool found_master_host = false;
      const auto result = parallel_rc_slots_by_time(&a, &slots, *total_list, *rue_list,
                                                    ocs::tom::ConstList<ocs::tom::HL>(), false,
                                                    ocs::tom::ConstElement<ocs::tom::EH>(),
                                                    ocs::tom::Element<ocs::tom::QU>(), DOMINANT_LAYER_GLOBAL, 0.0,
                                                    GLOBAL_TAG, false, false, found_master_host, false,
                                                    SGE_GLOBAL_NAME, false);
      char id[16];
      snprintf(id, sizeof(id), "T%02d", 30 + round * 2);
      check(id, "the implicit slots request matches", result == DISPATCH_OK);
      snprintf(id, sizeof(id), "T%02d", 31 + round * 2);
      check(id, "the implicit slots request limits the slots to the global slots (2)", slots == 2);
   }

   check("T36", "the implicit slots request is created in the assignment", static_cast<bool>(a.implicit_slots_request));
   const auto request = ocs_assignment_implicit_slots_request(&a);
   check("T37", "the implicit slots request is slots=1 of type INT",
         same_string(request.get<CE_name>(), SGE_ATTR_SLOTS) && same_string(request.get<CE_stringval>(), "1") &&
         request.get<CE_doubleval>() == 1.0 &&
         request.get<CE_valtype>() == static_cast<uint32_t>(ocs::CEntry::Type::INT));
   check("T38", "the assignment keeps one implicit slots request",
         ocs_assignment_implicit_slots_request(&a).raw() == request.raw());

   sge_assignment_t b{};
   check("T39", "another assignment has an implicit slots request of its own",
         ocs_assignment_implicit_slots_request(&b).raw() != request.raw());

   assignment_release(&a);
   assignment_release(&b);
   check("T40", "assignment_release() frees the implicit slots request",
         !a.implicit_slots_request && !b.implicit_slots_request);
}

/**
 * @brief Run the tests
 * @return 0 if all checks held, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sched_default_request");

   ocs::tom::install_model(ocs::tom::gcs_model());

   test_default_request_leaves_centry_untouched();
   test_implicit_slots_request_in_assignment();

   printf("\n%s: %d failure(s)\n", failures == 0 ? "PASSED" : "FAILED", failures);
   DRETURN(failures == 0 ? 0 : 1);
}
