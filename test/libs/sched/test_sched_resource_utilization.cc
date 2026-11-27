/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Unit tests for resource utilization in `libs/sched`
 */

#include <cstdio>
#include <limits>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_centry.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_RDE.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_RUE.h"

#include "sge_resource_utilization.h"
#include "sge_qeti.h"
#include "sge_select_queue.h"

// the empty handles a typed caller passes where the old interface took nullptr: a nullptr literal next to a handle
// would fit both the typed form and its transitional overload (N-X-2)
static const ocs::tom::ConstElement<ocs::tom::EH> no_host;
static const ocs::tom::ConstList<ocs::tom::ST> no_binding;
static const ocs::tom::ConstList<ocs::tom::RESL> no_rsmap;

/** @brief One booking in a resource utilization test
 *
 * Resource utilization is stored as a series of changes over time rather than
 * as a single number, so a test case is a list of bookings and the amounts they
 * add or remove.
 */
typedef struct {
   uint64_t    start_time;   ///< when the booking begins
   uint64_t    duration;     ///< how long it lasts
   double      uti;          ///< how much of the resource it takes
   const char *desc;         ///< what this booking is checking, printed on failure
} test_array_t;

static int s_fail = 0;

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
 *
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

/**
 * @brief Check utilization_max() for every interval of a test array
 * @param id the number of the next check; advanced by one per interval
 * @param cr the resource utilization diagram
 * @param ta the intervals with their expected utilization, ended by a start time of 0
 */
static void do_utilization_test(int *id, const ocs::tom::ConstElement<ocs::tom::RUE> cr, test_array_t *ta) {
   ocs::TopologyString binding_inuse;
   for (int i = 0; ta[i].start_time != 0; i++, (*id)++) {
      double uti = utilization_max(nullptr, nullptr, cr,
                                   ta[i].start_time, ta[i].duration,
                                   0.0, 0.0, 0.0, false, binding_inuse);
      CHECK(*id, ta[i].desc, uti == ta[i].uti);
   }
}

/**
 * @brief Check the points in time the qeti iterator gives for a resource utilization diagram
 *
 * The iterator walks a list of diagrams; it gets a copy of @p cr in a list of its own.
 *
 * @param id the number of the next check; advanced by one per point in time
 * @param cr the resource utilization diagram
 * @param qeti_expected_result the expected points in time, nullptr if the iterator must give none
 * @param desc the label of each expected point in time
 */
static void do_qeti_test(int *id, const ocs::tom::ConstElement<ocs::tom::RUE> cr,
                         uint64_t *qeti_expected_result, const char **desc) {
   auto cr_list = ocs::tom::ListPtr<ocs::tom::RUE>::create("");
   cr_list->push_back(cr.clone());
   sge_qeti_t *iter = sge_qeti_allocate2(*cr_list);

   if (qeti_expected_result == nullptr) {
      // expect no iteration — iterator must return 0 immediately
      uint64_t pe_time = sge_qeti_first(iter);
      CHECK(*id, "qeti is empty (no pending events)", pe_time == 0);
      (*id)++;
   } else {
      int i = 0;
      for (uint64_t pe_time = sge_qeti_first(iter);
           pe_time;
           pe_time = sge_qeti_next(iter), i++, (*id)++) {
         CHECK(*id, desc[i], qeti_expected_result[i] == pe_time);
      }
   }

   sge_qeti_release(&iter);
}

/**
 * @brief utilization_max() and the qeti iterator on a diagram of three bookings
 * @param id the number of the next check; advanced by every check
 */
static void test_normal_utilization(int *id) {
   /*
    *  8-|          --------    ----
    *    |
    *  4-|                  ----
    *    |
    *  0----------------------------------->
    *               |       |   |   |
    *              800     1000 1100 1200
    */
   static uint64_t qeti_expected[] = { 1200, 1100, 1000, 800 };
   static const char *qeti_desc[] = {
      "qeti[0]: time-event at 1200 (end of [1100,1200) reservation)",
      "qeti[1]: time-event at 1100 (end of [1000,1100) reservation)",
      "qeti[2]: time-event at 1000 (end of [800,1000) reservation)",
      "qeti[3]: time-event at 800 (start of first reservation)",
   };

   test_array_t test_array[] = {
      {600,   50, 0.0, "query window [600,650) before first reservation returns 0"},
      {800,    1, 8.0, "query at exact start of [800,1000) returns 8"},
      {1000,   1, 4.0, "query at boundary where [800,1000) ends and [1000,1100) begins returns 4"},
      {1100,   1, 8.0, "query at boundary where [1000,1100) ends and [1100,1200) begins returns 8"},
      {1000, 100, 4.0, "query spanning [1000,1100) single 4-slot reservation returns 4"},
      {1200, 150, 0.0, "query after all reservations have ended returns 0"},
      {700,  150, 8.0, "query window [700,850) overlapping [800,1000) 8-slot reservation returns 8"},
      {0, 0, 0.0, nullptr}
   };

   auto cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   cr->set<RUE_name>("slots");

   printf("\n--- normal utilization ---\n");

   utilization_add(*cr, 800,  200, 8, 100, 1, PE_TAG, "pe_slots", "STARTING",  false, false, no_binding, no_rsmap);
   utilization_add(*cr, 1000, 100, 4, 101, 1, PE_TAG, "pe_slots", "STARTING",  false, false, no_binding, no_rsmap);
   utilization_add(*cr, 1100, 100, 8, 102, 1, PE_TAG, "pe_slots", "RESERVING", false, false, no_binding, no_rsmap);

   do_utilization_test(id, *cr, test_array);
   do_qeti_test(id, *cr, qeti_expected, qeti_desc);

}

/**
 * @brief utilization_max() and the qeti iterator on diagrams of overlapping, reserving and negative bookings
 * @param id the number of the next check; advanced by every check
 */
static void test_extensive_utilization(int *id) {
   auto cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   cr->set<RUE_name>("slots");

   // --- extensive: add unlimited reservations ---
   printf("\n--- extensive: add unlimited reservations ---\n");
   {
      /*
       *  8-|          |-------|             |-----......
       *    |
       *  4-|                  |---|----------------....
       *    |
       *  0-------------------------------------->
       *               |       |   |         |
       *              800     1000 1100      2000
       */
      static uint64_t qeti_expected[] = {
         std::numeric_limits<uint64_t>::max(), 2000, 1000, 800
      };
      static const char *qeti_desc[] = {
         "qeti[0]: UINT64_MAX (open-ended unlimited reservations extend to infinity)",
         "qeti[1]: time-event at 2000 (second unlimited reservation starts)",
         "qeti[2]: time-event at 1000 (now-assignment ends, reservations take over)",
         "qeti[3]: time-event at 800 (first now-assignment starts)",
      };

      test_array_t test_array[] = {
         {1000,                              100, 4.0,
          "query [1000,1100) under 4-slot now-assignment returns 4"},
         {1200, std::numeric_limits<uint64_t>::max(), 8.0,
          "infinite-duration query from 1200 returns 8 (two unlimited reservations eventually overlap)"},
         {200,  std::numeric_limits<uint64_t>::max(), 8.0,
          "infinite-duration query from 200 returns 8 (hits [800,1000) 8-slot peak)"},
         {700,                              150, 8.0,
          "query [700,850) overlapping [800,1000) 8-slot now-assignment returns 8"},
         {700,                              100, 0.0,
          "query [700,800) entirely before first assignment returns 0"},
         {3600,                             150, 8.0,
          "query [3600,3750) under two concurrent unlimited reservations (4+4) returns 8"},
         {1000,                            1000, 4.0,
          "query [1000,2000) with overlapping reservations — max is 4 before second unlimited starts"},
         {0, 0, 0.0, nullptr}
      };

      utilization_add(*cr, 800,  200, 8, 100, 1, PE_TAG, "pe_slots", "STARTING",  false, false, no_binding, no_rsmap);
      utilization_add(*cr, 1000, 100, 4, 101, 1, PE_TAG, "pe_slots", "STARTING",  false, false, no_binding, no_rsmap);
      utilization_add(*cr, 1100, std::numeric_limits<uint64_t>::max(), 4, 102, 1, PE_TAG, "pe_slots", "RESERVING", false, false, no_binding, no_rsmap);
      utilization_add(*cr, 2000, std::numeric_limits<uint64_t>::max(), 4, 103, 1, PE_TAG, "pe_slots", "RESERVING", false, false, no_binding, no_rsmap);

      do_utilization_test(id, *cr, test_array);
      do_qeti_test(id, *cr, qeti_expected, qeti_desc);
   }

   // --- extensive: partial remove ---
   printf("\n--- extensive: partial remove ---\n");
   {
      /*
       *  8-|          |-------|
       *    |
       *  4-|                                |-----......
       *    |
       *  0-------------------------------------->
       *               |       |   |         |
       *              800     1000          2000
       */
      static uint64_t qeti_expected[] = {
         std::numeric_limits<uint64_t>::max(), 2000, 1000, 800
      };
      static const char *qeti_desc[] = {
         "qeti[0]: UINT64_MAX (job103 open-ended reservation)",
         "qeti[1]: time-event at 2000 (job103 unlimited reservation starts)",
         "qeti[2]: time-event at 1000 (job100 now-assignment ends)",
         "qeti[3]: time-event at 800 (job100 now-assignment starts)",
      };

      test_array_t test_array[] = {
         {1000,                              100, 0.0,
          "query [1000,1100) after removing 4-slot now-assignment returns 0"},
         {1200, std::numeric_limits<uint64_t>::max(), 4.0,
          "infinite-duration query from 1200 with only job103 remaining returns 4"},
         {200,  std::numeric_limits<uint64_t>::max(), 8.0,
          "infinite-duration query from 200 still hits [800,1000) 8-slot peak"},
         {700,                              150, 8.0,
          "query [700,850) still overlaps [800,1000) now-assignment returns 8"},
         {700,                              100, 0.0,
          "query [700,800) before first assignment still returns 0"},
         {3600,                             150, 4.0,
          "query [3600,3750) under single remaining unlimited reservation returns 4"},
         {1000,                            1000, 0.0,
          "query [1000,2000) with job101 and job102 removed returns 0"},
         {0, 0, 0.0, nullptr}
      };

      utilization_add(*cr, 1000, 100,                               -4, 101, 1, PE_TAG, "pe_slots", "STARTING",  false, false, no_binding, no_rsmap);
      utilization_add(*cr, 1100, std::numeric_limits<uint64_t>::max(), -4, 102, 1, PE_TAG, "pe_slots", "RESERVING", false, false, no_binding, no_rsmap);

      do_utilization_test(id, *cr, test_array);
      do_qeti_test(id, *cr, qeti_expected, qeti_desc);
   }

   // --- extensive: remove all ---
   printf("\n--- extensive: remove all ---\n");
   {
      test_array_t test_array[] = {
         {1000,                              100, 0.0, "query [1000,1100) after removing all returns 0"},
         {1200, std::numeric_limits<uint64_t>::max(), 0.0, "infinite-duration query from 1200 after all removed returns 0"},
         {200,  std::numeric_limits<uint64_t>::max(), 0.0, "infinite-duration query from 200 after all removed returns 0"},
         {700,                              150, 0.0, "query [700,850) after all removed returns 0"},
         {700,                              100, 0.0, "query [700,800) after all removed returns 0"},
         {3600,                             150, 0.0, "query [3600,3750) after all removed returns 0"},
         {1000,                            1000, 0.0, "query [1000,2000) after all removed returns 0"},
         {0, 0, 0.0, nullptr}
      };

      utilization_add(*cr, 800,  200,                               -8, 100, 1, PE_TAG, "pe_slots", "STARTING",  false, false, no_binding, no_rsmap);
      utilization_add(*cr, 2000, std::numeric_limits<uint64_t>::max(), -4, 103, 1, PE_TAG, "pe_slots", "RESERVING", false, false, no_binding, no_rsmap);

      do_utilization_test(id, *cr, test_array);
      do_qeti_test(id, *cr, nullptr, nullptr);
   }

}

static int test_rsmap_utilization();
static int test_rsmap_below();
static int test_below_window_before_closed_period();
static int test_binding_release();

/**
 * @brief Run the checks of this module test
 * @param argc unused
 * @param argv unused
 * @return 0 if every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_resource_utilization");
   component_set_daemonized(true);
   ocs::tom::install_model(ocs::tom::gcs_model());

   int id = 1;

   // --- baseline ---
   printf("\n--- baseline ---\n");
   {
      auto cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
      cr->set<RUE_name>("slots");

      ocs::TopologyString binding_inuse;
      double uti = utilization_max(nullptr, no_host, *cr, 1000, 100, 0.0, 0.0, 0.0, false, binding_inuse);
      CHECK(id, "utilization_max on empty cr returns 0.0", uti == 0.0);
      id++;

      do_qeti_test(&id, *cr, nullptr, nullptr);

   }

   test_normal_utilization(&id);
   test_extensive_utilization(&id);

   // --- concurrent accumulation ---
   printf("\n--- concurrent accumulation ---\n");
   {
      auto cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
      cr->set<RUE_name>("slots");

      utilization_add(*cr, 1500, 100, 4, 200, 1, PE_TAG, "pe_slots", "STARTING", false, false, no_binding, no_rsmap);
      utilization_add(*cr, 1500, 100, 4, 201, 1, PE_TAG, "pe_slots", "STARTING", false, false, no_binding, no_rsmap);

      ocs::TopologyString binding_inuse;
      double uti = utilization_max(nullptr, no_host, *cr, 1500, 100, 0.0, 0.0, 0.0, false, binding_inuse);
      CHECK(id, "two concurrent reservations (4+4) accumulate to 8.0", uti == 8.0);
      id++;

   }

   // the resource map tests came from V91_BRANCH with a counter of their own; fold it in
   s_fail += test_rsmap_utilization();
   s_fail += test_rsmap_below();
   s_fail += test_below_window_before_closed_period();
   s_fail += test_binding_release();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}

/**
 * @brief build a resource map instance list, as the granted resources carry it
 *
 * The arguments are identifier and amount pairs, ended by a nullptr identifier.
 *
 * @param id the first identifier, or nullptr for no instance at all
 * @param amount its amount
 * @param ... further identifier and amount pairs, ended by a nullptr identifier
 * @return the instance list, empty if @p id is nullptr
 */
static ocs::tom::ListPtr<ocs::tom::RESL>
make_rsmap(const char *id, uint32_t amount, ...) {
   ocs::tom::ListPtr<ocs::tom::RESL> ids;
   va_list ap;

   va_start(ap, amount);
   while (id != nullptr) {
      if (!ids) {
         ids = ocs::tom::ListPtr<ocs::tom::RESL>::create("rsmap");
      }
      ocs::tom::Element<ocs::tom::RESL> ep = ids->emplace_back<RESL_value>(id);
      ep.set<RESL_amount>(amount);
      id = va_arg(ap, const char *);
      if (id != nullptr) {
         amount = va_arg(ap, uint32_t);
      }
   }
   va_end(ap);

   return ids;
}

/**
 * @brief how much of an identifier a diagram entry says is in use
 * @param rde the diagram entry
 * @param id the identifier
 * @return the amount in use, 0 if the entry does not name the identifier
 */
static uint32_t
rsmap_at(const ocs::tom::ConstElement<ocs::tom::RDE> rde, const char *id) {
   const auto ep = rde.find_first_in<RDE_resource_map_list, RESL_value>(id);
   return (ep != nullptr) ? ep.get<RESL_amount>() : 0;
}

/**
 * @brief the entry of the diagram which is in force at a point in time
 * @param cr the resource utilization
 * @param time the point in time
 * @return the entry, empty if the diagram starts later
 */
static ocs::tom::ConstElement<ocs::tom::RDE>
rde_at_time(const ocs::tom::ConstElement<ocs::tom::RUE> cr, const uint64_t time) {
   ocs::tom::ConstElement<ocs::tom::RDE> hit;
   ocs::tom::ConstElement<ocs::tom::RDE> ep;

   for (ep = cr.get<RUE_utilized>().front(); ep; ep = ep.next()) {
      if (ep.get<RDE_time>() > time) {
         break;
      }
      hit = ep;
   }

   return hit;
}

/**
 * @brief check how much of an identifier the diagram says is in use at a point in time
 * @param cr the resource utilization
 * @param time the point in time
 * @param id the identifier
 * @param expected the expected amount
 * @return 0 if the amount is the expected one, 1 otherwise
 */
static int
check_rsmap(const ocs::tom::ConstElement<ocs::tom::RUE> cr, const uint64_t time, const char *id,
            const uint32_t expected) {
   const auto rde = rde_at_time(cr, time);
   uint32_t got = (rde != nullptr) ? rsmap_at(rde, id) : 0;

   if (got != expected) {
      printf("   FAIL: at " sge_u64 " expected " sge_u32 " of %s, got " sge_u32 "\n",
             time, expected, id, got);
      return 1;
   }
   printf("   ok: at " sge_u64 " %s is used " sge_u32 " times\n", time, id, got);
   return 0;
}

/**
 * @brief the diagram records which instances of a resource map are held, over the window
 *
 * The amount alone cannot say which card a reservation took, so a job scheduled into the same
 * window has no way to avoid it. These are the cases the booking has to get right: instances
 * held for a window and given back at its end, two bookings of the same identifier adding up,
 * and a booking withdrawn again leaving nothing behind.
 */
static int test_rsmap_utilization() {
   int ret = 0;

   printf("\n - test resource map instances in the diagram - \n\n");

   auto cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   cr->set<RUE_name>("gpu");

   // a reservation takes two instances of gpu0 from 1000 for 200 seconds
   auto two_of_gpu0 = make_rsmap("gpu0", 2, nullptr);
   utilization_add(*cr, 1000, 200, 2, 100, 1, HOST_TAG, "node01", "RESERVING", false, false,
                   no_binding, two_of_gpu0);

   printf("a reservation of gpu0 twice, from 1000 for 200s\n");
   ret += check_rsmap(*cr, 900, "gpu0", 0);
   ret += check_rsmap(*cr, 1000, "gpu0", 2);
   ret += check_rsmap(*cr, 1100, "gpu0", 2);
   ret += check_rsmap(*cr, 1200, "gpu0", 0);

   // a job inside that window takes one of gpu1, which has to leave gpu0 alone
   auto one_of_gpu1 = make_rsmap("gpu1", 1, nullptr);
   utilization_add(*cr, 1050, 100, 1, 101, 1, HOST_TAG, "node01", "STARTING", false, false,
                   no_binding, one_of_gpu1);

   printf("a job taking gpu1 once, from 1050 for 100s\n");
   ret += check_rsmap(*cr, 1000, "gpu1", 0);
   ret += check_rsmap(*cr, 1050, "gpu1", 1);
   ret += check_rsmap(*cr, 1100, "gpu1", 1);
   ret += check_rsmap(*cr, 1150, "gpu1", 0);
   ret += check_rsmap(*cr, 1100, "gpu0", 2);

   // a second booking of gpu0 inside the same window adds up
   auto one_of_gpu0 = make_rsmap("gpu0", 1, nullptr);
   utilization_add(*cr, 1100, 50, 1, 102, 1, HOST_TAG, "node01", "STARTING", false, false,
                   no_binding, one_of_gpu0);

   printf("a job taking gpu0 once more, from 1100 for 50s\n");
   ret += check_rsmap(*cr, 1050, "gpu0", 2);
   ret += check_rsmap(*cr, 1100, "gpu0", 3);
   ret += check_rsmap(*cr, 1150, "gpu0", 2);
   ret += check_rsmap(*cr, 1200, "gpu0", 0);

   // and withdrawing it leaves what the others hold
   utilization_add(*cr, 1100, 50, -1, 102, 1, HOST_TAG, "node01", "STARTING", false, false,
                   no_binding, one_of_gpu0);

   printf("that job is withdrawn again\n");
   ret += check_rsmap(*cr, 1100, "gpu0", 2);
   ret += check_rsmap(*cr, 1150, "gpu0", 2);
   ret += check_rsmap(*cr, 1200, "gpu0", 0);
   ret += check_rsmap(*cr, 1100, "gpu1", 1);

   // the reader: how much of an identifier is taken anywhere in a window. The diagram now has
   // gpu0 held twice over [1000,1200) and gpu1 once over [1050,1150).
   struct {
      uint64_t start;
      uint64_t duration;
      const char *id;
      uint32_t expected;
      const char *what;
   } windows[] = {
      {1000, 200, "gpu0", 2, "the whole window the reservation holds"},
      {1000, 200, "gpu1", 1, "a window covering the job as well"},
      {1000,  40, "gpu1", 0, "a window ending before the job starts"},
      { 800, 100, "gpu0", 0, "a window ending before anything starts"},
      {1200, 100, "gpu0", 0, "a window starting after everything ended"},
      { 900, 200, "gpu0", 2, "a window opening before and reaching into the reservation"},
      {1150,  50, "gpu1", 0, "a window opening exactly when the job ended"},
      {1100,  10, "gpu0", 2, "a window wholly inside the reservation"},
      {0, 0, nullptr, 0, nullptr}
   };

   printf("reading back what is taken over a window\n");
   for (int i = 0; windows[i].id != nullptr; i++) {
      auto taken = utilization_rsmap_max(*cr, 1000, windows[i].start, windows[i].duration);
      ocs::tom::ConstElement<ocs::tom::RESL> ep = taken->find_first<RESL_value>(windows[i].id);
      uint32_t got = (ep != nullptr) ? ep.get<RESL_amount>() : 0;

      if (got != windows[i].expected) {
         printf("   FAIL: %s: expected " sge_u32 " of %s over [" sge_u64 "," sge_u64 "),"
                " got " sge_u32 "\n",
                windows[i].what, windows[i].expected, windows[i].id, windows[i].start,
                windows[i].start + windows[i].duration, got);
         ret++;
      } else {
         printf("   ok: %s: " sge_u32 " of %s\n", windows[i].what, got, windows[i].id);
      }
   }

   // What a job holds right now is in the utilization of the moment as well as in the diagram,
   // so the two are combined by taking the larger and not by adding them up. gpu0 is held twice
   // over [1000,1200) in the diagram; saying the same in the now list must not make it four.
   cr->set<RUE_utilized_now_resource_map_list>(make_rsmap("gpu0", 2, "gpu2", 1, nullptr));

   printf("with the same instances also recorded as held right now\n");
   {
      auto taken = utilization_rsmap_max(*cr, 1000, 1000, 200);
      ocs::tom::ConstElement<ocs::tom::RESL> ep = taken->find_first<RESL_value>("gpu0");
      uint32_t got = (ep != nullptr) ? ep.get<RESL_amount>() : 0;
      if (got != 2) {
         printf("   FAIL: gpu0 is held twice, in both places, expected 2 got " sge_u32 "\n", got);
         ret++;
      } else {
         printf("   ok: gpu0 counts once, " sge_u32 "\n", got);
      }

      // an instance only the now list knows about still counts
      ocs::tom::ConstElement<ocs::tom::RESL> ep_resl = taken->find_first<RESL_value>("gpu2");
      got = (ep_resl != nullptr) ? ep_resl.get<RESL_amount>() : 0;
      if (got != 1) {
         printf("   FAIL: gpu2 is held right now, expected 1 got " sge_u32 "\n", got);
         ret++;
      } else {
         printf("   ok: gpu2 is held right now, " sge_u32 "\n", got);
      }
   }

   printf("and at queue end\n");
   {
      // every job holding something now has ended by then, so the now list does not count;
      // what remains is what the diagram says at its end, which here is nothing
      auto taken = utilization_rsmap_max(*cr, 1000, DISPATCH_TIME_QUEUE_END, 100);
      if (taken->size() != 0) {
         printf("   FAIL: something is taken at queue end, expected nothing\n");
         ret++;
      } else {
         printf("   ok: nothing is taken at queue end\n");
      }
   }

   printf("and for a window which begins after this moment\n");
   {
      // gpu2 is held now and nothing says it still is at 1200, so it does not count there
      auto taken = utilization_rsmap_max(*cr, 1000, 1200, 100);
      ocs::tom::ConstElement<ocs::tom::RESL> ep = taken->find_first<RESL_value>("gpu2");
      uint32_t got = (ep != nullptr) ? ep.get<RESL_amount>() : 0;
      if (got != 0) {
         printf("   FAIL: gpu2 is held now but the window is later, expected 0 got "
                sge_u32 "\n", got);
         ret++;
      } else {
         printf("   ok: what is held now does not count against a later window\n");
      }
   }

   // A map is configured on an exec host or on the global host, and a job takes it from
   // whichever has it, so both layers have to record the instances. A queue cannot carry a map
   // and must not record any.
   {
      auto global_cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
      global_cr->set<RUE_name>("gpu");
      utilization_add(*global_cr, 1000, 100, 1, 200, 1, GLOBAL_TAG, "global", "STARTING", false,
                      false, no_binding, one_of_gpu1);

      auto taken = utilization_rsmap_max(*global_cr, 1000, 1000, 100);
      ocs::tom::ConstElement<ocs::tom::RESL> ep = taken->find_first<RESL_value>("gpu1");
      if (ep == nullptr || ep.get<RESL_amount>() != 1) {
         printf("   FAIL: a map on the global host records nothing\n");
         ret++;
      } else {
         printf("   ok: a map on the global host records its instances\n");
      }
      global_cr.reset();

      auto queue_cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
      queue_cr->set<RUE_name>("gpu");
      utilization_add(*queue_cr, 1000, 100, 1, 201, 1, QUEUE_TAG, "a.q", "STARTING", false,
                      false, no_binding, one_of_gpu1);

      taken = utilization_rsmap_max(*queue_cr, 1000, 1000, 100);
      if (taken->size() != 0) {
         printf("   FAIL: a queue recorded resource map instances\n");
         ret++;
      } else {
         printf("   ok: a queue records no instances\n");
      }
   }

   two_of_gpu0.reset();
   one_of_gpu1.reset();
   one_of_gpu0.reset();
   cr.reset();

   if (ret == 0) {
      printf("\n - resource map instances in the diagram: ok -\n");
   }
   return ret;
}

/**
 * @brief a resource map of two cards, each shared twice: "gpu=4(gpu0 gpu0 gpu1 gpu1)"
 *
 * Each identifier also carries a characteristic "rack", both cards being in rack r1, so that
 * grouping by the identifier and grouping by a characteristic can be told apart.
 */
static ocs::tom::ElementPtr<ocs::tom::CE>
make_two_cards() {
   auto def = ocs::tom::ElementPtr<ocs::tom::CE>::create();
   def->set<CE_name>("gpu");
   def->set<CE_valtype>(static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));

   for (const char *id : {"gpu0", "gpu1"}) {
      ocs::tom::Element<ocs::tom::RESL> resl = def->emplace_back_in<CE_resource_map_list, RESL_value>(id);
      resl.set<RESL_amount>(2);
      ocs::tom::Element<ocs::tom::CE> prop = resl.emplace_back_in<RESL_properties, CE_name>("rack");
      prop.set<CE_stringval>("r1");
   }

   return def;
}

/**
 * @brief Check when utilization_rsmap_below() says a group of a resource map has an amount free
 * @param def the complex entry of the resource map
 * @param cr the resource utilization diagram
 * @param key_name the grouping, "id" or a characteristic
 * @param amount the instances wanted from one group
 * @param expected the point in time expected
 * @param what the label of the check
 * @return 0 if the point in time is the expected one, 1 otherwise
 */
static int
check_below(const ocs::tom::ConstElement<ocs::tom::CE> def, const ocs::tom::ConstElement<ocs::tom::RUE> cr,
            const char *key_name, uint32_t amount, uint64_t expected, const char *what) {
   uint64_t got = utilization_rsmap_below(def, cr, key_name, amount);

   if (got != expected) {
      printf("   FAIL: %s: expected " sge_u64 ", got " sge_u64 "\n", what, expected, got);
      return 1;
   }
   printf("   ok: %s\n", what);
   return 0;
}

/**
 * @brief when one group of a resource map has a number of instances free, and keeps them
 *
 * "Four free shares" and "four free shares of one card" are different questions. These are the
 * cases which tell them apart: a whole card wanted while the shares are spread over both, a
 * gap which closes again before the end, and a group which never comes free at all.
 */
static int test_rsmap_below() {
   int ret = 0;

   printf("\n - test when a group of a resource map comes free - \n\n");

   auto def = make_two_cards();
   auto cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   cr->set<RUE_name>("gpu");

   // nothing booked at all: any group can serve the request straight away
   ret += check_below(*def, *cr, "id", 2, DISPATCH_TIME_NOW, "an empty diagram answers now");

   // one share of each card held to 1000 and 2000. Two shares in total are free the whole
   // time, but never two of one card until the first is given back.
   auto one_of_gpu0 = make_rsmap("gpu0", 1, nullptr);
   auto one_of_gpu1 = make_rsmap("gpu1", 1, nullptr);
   utilization_add(*cr, 0, 1000, 1, 100, 1, HOST_TAG, "node01", "STARTING", false, false,
                   no_binding, one_of_gpu0);
   utilization_add(*cr, 0, 2000, 1, 101, 1, HOST_TAG, "node01", "STARTING", false, false,
                   no_binding, one_of_gpu1);

   ret += check_below(*def, *cr, "id", 1, DISPATCH_TIME_NOW,
                      "one share is free on both cards now");
   ret += check_below(*def, *cr, "id", 2, 1000,
                      "a whole card comes free when the share on gpu0 is given back");

   // grouped by a characteristic both cards are one group, so the shares add up and two of
   // the group are free from the start
   ret += check_below(*def, *cr, "rack", 2, DISPATCH_TIME_NOW,
                      "grouped by rack the two free shares are in one group");
   ret += check_below(*def, *cr, "rack", 3, 1000,
                      "three of the group need the gpu0 share back");
   ret += check_below(*def, *cr, "rack", 4, 2000,
                      "the whole group needs both shares back");

   // A gap which closes again must not be offered. On a fresh diagram gpu0 is free from 1000,
   // taken again from 1500 to 2500 and free for good after that, while gpu1 is held to 3000.
   // The gap at 1000 fits a whole card but does not last, so the answer is gpu0 at 2500.
   auto gap = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   gap->set<RUE_name>("gpu");
   auto two_of_gpu0 = make_rsmap("gpu0", 2, nullptr);
   utilization_add(*gap, 0, 1000, 1, 102, 1, HOST_TAG, "node01", "STARTING", false, false,
                   no_binding, one_of_gpu0);
   utilization_add(*gap, 1500, 1000, 2, 103, 1, HOST_TAG, "node01", "STARTING", false, false,
                   no_binding, two_of_gpu0);
   utilization_add(*gap, 0, 3000, 1, 104, 1, HOST_TAG, "node01", "STARTING", false, false,
                   no_binding, one_of_gpu1);

   ret += check_below(*def, *gap, "id", 2, 2500,
                      "the gap at 1000 closes again, so gpu0 answers at 2500 instead");

   // a group still taken at the end of the diagram never comes free, but another group which
   // is free throughout still answers
   auto full = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   full->set<RUE_name>("gpu");
   auto both_of_gpu0 = make_rsmap("gpu0", 2, nullptr);
   utilization_add(*full, 0, std::numeric_limits<uint64_t>::max(), 2, 105, 1, HOST_TAG, "node01", "STARTING", false,
                   false, no_binding, both_of_gpu0);

   ret += check_below(*def, *full, "id", 2, DISPATCH_TIME_NOW,
                      "gpu0 never comes free, gpu1 is untouched and answers now");

   auto both_cards = make_rsmap("gpu0", 2, "gpu1", 2, nullptr);
   auto all_full = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   all_full->set<RUE_name>("gpu");
   utilization_add(*all_full, 0, std::numeric_limits<uint64_t>::max(), 4, 104, 1, HOST_TAG, "node01", "STARTING", false,
                   false, no_binding, both_cards);

   ret += check_below(*def, *all_full, "id", 2, std::numeric_limits<uint64_t>::max(),
                      "when no card ever comes free there is nothing to reserve");

   one_of_gpu0.reset();
   one_of_gpu1.reset();
   two_of_gpu0.reset();
   both_of_gpu0.reset();
   both_cards.reset();
   cr.reset();
   gap.reset();
   full.reset();
   all_full.reset();
   def.reset();

   if (ret == 0) {
      printf("\n - when a group comes free: ok -\n");
   }
   return ret;
}

/**
 * @brief a free window in front of a calendar closed period is where a reservation belongs
 *
 * A calendar which closes the queue later on books the whole queue for that period, so the
 * diagram ends with a blocked stretch. Searching backwards from the end finds only the moment
 * the queue reopens, and every free window in front of it is passed over - CS-1000. A job which
 * fits into such a window has to be reserved there instead.
 *
 *  1-|----          ----------
 *    |
 *  0-|    ----------          ---------->
 *      100     200        1000      2000
 */
static int test_below_window_before_closed_period() {
   int ret = 0;

   struct {
      uint64_t    duration;
      uint64_t    expected;
      const char *what;
   } cases[] = {
      {300, 200,  "a job which fits into the window starts when the running one ends"},
      {800, 200,  "a job which fills the window exactly still starts there"},
      {801, 2000, "a job one second too long waits for the queue to open again"},
      {0, 0, nullptr}
   };

   printf("\n - test a reservation in front of a calendar closed period - \n\n");

   auto cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   cr->set<RUE_name>("slots");

   // the running job, and the stretch the calendar blocks off later on
   utilization_add(*cr, 100, 100, 1, 1, 1, PE_TAG, "slots", "STARTING", false, false, no_binding, no_rsmap);
   utilization_add(*cr, 1000, 1000, 1, 2, 1, PE_TAG, "slots", "STARTING", false, false, no_binding, no_rsmap);

   for (int i = 0; cases[i].what != nullptr; i++) {
      sge_assignment_t a{};
      a.now = 50;
      a.duration = cases[i].duration;

      ocs::TopologyString binding_inuse;
      // one slot in total and the job wants it, so anything above zero is in the way
      uint64_t when = utilization_below(&a, no_host, *cr, 0.0, 1.0, 1.0, "test_queue", false,
                                        binding_inuse);
      if (when != cases[i].expected) {
         printf("   FAIL: %s: expected " sge_u64 ", got " sge_u64 "\n",
                cases[i].what, cases[i].expected, when);
         ret++;
      } else {
         printf("   ok: %s\n", cases[i].what);
      }
   }

   return ret;
}

/**
 * @brief Counts the cores a diagram entry holds as bound
 *
 * A used core is written in lower case in the topology string of RDE_binding_inuse.
 *
 * @param cr the slots entry of a host
 * @param time the point in time
 * @return the number of used cores at that time
 */
static int
binding_used_cores(const ocs::tom::ConstElement<ocs::tom::RUE> cr, const uint64_t time) {
   const auto rde = rde_at_time(cr, time);
   const char *binding = rde ? rde.get<RDE_binding_inuse>() : nullptr;
   int used = 0;

   if (binding != nullptr) {
      for (const char *c = binding; *c != '\0'; c++) {
         if (*c == 'c') {
            used++;
         }
      }
   }
   return used;
}

/**
 * @brief Checks the number of bound cores at a point in time
 *
 * @param cr the slots entry of a host
 * @param time the point in time
 * @param expected the expected number of used cores
 * @return 0 when the number matches, 1 otherwise
 */
static int
check_binding(const ocs::tom::ConstElement<ocs::tom::RUE> cr, const uint64_t time,
              const int expected) {
   const auto rde = rde_at_time(cr, time);
   const char *binding = rde ? rde.get<RDE_binding_inuse>() : nullptr;
   const int got = binding_used_cores(cr, time);

   if (got != expected) {
      printf("   FAIL: at " sge_u64 " expected %d bound cores, got %d (%s)\n",
             time, expected, got, binding != nullptr ? binding : "NONE");
      return 1;
   }
   printf("   ok: at " sge_u64 " %d bound cores (%s)\n", time, got,
          binding != nullptr ? binding : "NONE");
   return 0;
}

/**
 * @brief Creates a binding_to_use list with one topology string
 *
 * @param topology the internal topology string, used nodes in lower case
 * @return the list
 */
static ocs::tom::ListPtr<ocs::tom::ST>
make_binding(const char *topology) {
   auto list = ocs::tom::ListPtr<ocs::tom::ST>::create("binding_to_use");
   list->emplace_back<ST_name>(topology);
   return list;
}

/**
 * @brief Giving back a booking also gives back the cores bound with it
 *
 * The same call books and gives back, the second with a negated amount, e.g. when an advance
 * reservation is deleted. The cores bound with the booking have to be free again afterwards,
 * else every later request needing them is refused on that host.
 *
 * @return the number of failed checks
 */
static int test_binding_release() {
   int ret = 0;

   printf("\n - test the release of bound cores - \n\n");

   auto cr = ocs::tom::ElementPtr<ocs::tom::RUE>::create();
   cr->set<RUE_name>("slots");

   // two sockets with one core and two threads each: core 0 and core 1
   auto core0 = make_binding("(N(s(x(y(c(t)(t)))))(S(X(Y(C(T)(T))))))");
   auto core1 = make_binding("(N(S(X(Y(C(T)(T)))))(s(x(y(c(t)(t))))))");

   printf("a reservation binding core 0, from 1000 for 200s\n");
   utilization_add(*cr, 1000, 200, 1, 100, 1, HOST_TAG, "node01", "RESERVING", false, false,
                   core0, no_rsmap);
   ret += check_binding(*cr, 900, 0);
   ret += check_binding(*cr, 1000, 1);
   ret += check_binding(*cr, 1100, 1);
   ret += check_binding(*cr, 1200, 0);

   printf("a reservation binding core 1, from 1100 for 200s\n");
   utilization_add(*cr, 1100, 200, 1, 101, 1, HOST_TAG, "node01", "RESERVING", false, false,
                   core1, no_rsmap);
   ret += check_binding(*cr, 1000, 1);
   ret += check_binding(*cr, 1100, 2);
   ret += check_binding(*cr, 1200, 1);
   ret += check_binding(*cr, 1300, 0);

   printf("the first reservation is deleted\n");
   utilization_add(*cr, 1000, 200, -1, 100, 1, HOST_TAG, "node01", "RESERVING", false, false,
                   core0, no_rsmap);
   ret += check_binding(*cr, 1000, 0);
   ret += check_binding(*cr, 1100, 1);
   ret += check_binding(*cr, 1200, 1);
   ret += check_binding(*cr, 1300, 0);
   ret += check_binding(*cr, 5000, 0);

   printf("the second reservation is deleted\n");
   utilization_add(*cr, 1100, 200, -1, 101, 1, HOST_TAG, "node01", "RESERVING", false, false,
                   core1, no_rsmap);
   ret += check_binding(*cr, 1000, 0);
   ret += check_binding(*cr, 1100, 0);
   ret += check_binding(*cr, 1300, 0);
   ret += check_binding(*cr, 5000, 0);
   const auto entries = cr->get<RUE_utilized>().size();
   if (entries != 0) {
      printf("   FAIL: the diagram still has %zu entries\n", static_cast<size_t>(entries));
      ret++;
   }

   return ret;
}
