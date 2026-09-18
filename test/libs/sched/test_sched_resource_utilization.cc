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
#include "sgeobj/cull/sge_all_listsL.h"

#include "sge_resource_utilization.h"
#include "sge_qeti.h"

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

static void do_utilization_test(int *id, lListElem *cr, test_array_t *ta) {
   ocs::TopologyString binding_inuse;
   for (int i = 0; ta[i].start_time != 0; i++, (*id)++) {
      double uti = utilization_max(nullptr, nullptr, cr,
                                   ta[i].start_time, ta[i].duration,
                                   0.0, 0.0, 0.0, false, binding_inuse);
      CHECK(*id, ta[i].desc, uti == ta[i].uti);
   }
}

static void do_qeti_test(int *id, lListElem *cr,
                         uint64_t *qeti_expected_result, const char **desc) {
   lList *cr_list = lCreateList("", RUE_Type);
   lAppendElem(cr_list, cr);
   sge_qeti_t *iter = sge_qeti_allocate2(cr_list);

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

   lDechainElem(cr_list, cr);
   sge_qeti_release(&iter);
   lFreeList(&cr_list);
}

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

   lListElem *cr = lCreateElem(RUE_Type);
   lSetString(cr, RUE_name, "slots");

   printf("\n--- normal utilization ---\n");

   utilization_add(cr, 800,  200, 8, 100, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr, nullptr);
   utilization_add(cr, 1000, 100, 4, 101, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr, nullptr);
   utilization_add(cr, 1100, 100, 8, 102, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr, nullptr);

   do_utilization_test(id, cr, test_array);
   do_qeti_test(id, cr, qeti_expected, qeti_desc);

   lFreeElem(&cr);
}

static void test_extensive_utilization(int *id) {
   lListElem *cr = lCreateElem(RUE_Type);
   lSetString(cr, RUE_name, "slots");

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

      utilization_add(cr, 800,  200, 8, 100, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr, nullptr);
      utilization_add(cr, 1000, 100, 4, 101, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr, nullptr);
      utilization_add(cr, 1100, std::numeric_limits<uint64_t>::max(), 4, 102, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr, nullptr);
      utilization_add(cr, 2000, std::numeric_limits<uint64_t>::max(), 4, 103, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr, nullptr);

      do_utilization_test(id, cr, test_array);
      do_qeti_test(id, cr, qeti_expected, qeti_desc);
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

      utilization_add(cr, 1000, 100,                               -4, 101, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr, nullptr);
      utilization_add(cr, 1100, std::numeric_limits<uint64_t>::max(), -4, 102, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr, nullptr);

      do_utilization_test(id, cr, test_array);
      do_qeti_test(id, cr, qeti_expected, qeti_desc);
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

      utilization_add(cr, 800,  200,                               -8, 100, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr, nullptr);
      utilization_add(cr, 2000, std::numeric_limits<uint64_t>::max(), -4, 103, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr, nullptr);

      do_utilization_test(id, cr, test_array);
      do_qeti_test(id, cr, nullptr, nullptr);
   }

   lFreeElem(&cr);
}

static int test_rsmap_utilization();
static int test_rsmap_below();

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_resource_utilization");
   component_set_daemonized(true);
   lInit(nmv);

   int id = 1;

   // --- baseline ---
   printf("\n--- baseline ---\n");
   {
      lListElem *cr = lCreateElem(RUE_Type);
      lSetString(cr, RUE_name, "slots");

      ocs::TopologyString binding_inuse;
      double uti = utilization_max(nullptr, nullptr, cr, 1000, 100, 0.0, 0.0, 0.0, false, binding_inuse);
      CHECK(id, "utilization_max on empty cr returns 0.0", uti == 0.0);
      id++;

      do_qeti_test(&id, cr, nullptr, nullptr);

      lFreeElem(&cr);
   }

   test_normal_utilization(&id);
   test_extensive_utilization(&id);

   // --- concurrent accumulation ---
   printf("\n--- concurrent accumulation ---\n");
   {
      lListElem *cr = lCreateElem(RUE_Type);
      lSetString(cr, RUE_name, "slots");

      utilization_add(cr, 1500, 100, 4, 200, 1, PE_TAG, "pe_slots", "STARTING", false, false, nullptr, nullptr);
      utilization_add(cr, 1500, 100, 4, 201, 1, PE_TAG, "pe_slots", "STARTING", false, false, nullptr, nullptr);

      ocs::TopologyString binding_inuse;
      double uti = utilization_max(nullptr, nullptr, cr, 1500, 100, 0.0, 0.0, 0.0, false, binding_inuse);
      CHECK(id, "two concurrent reservations (4+4) accumulate to 8.0", uti == 8.0);
      id++;

      lFreeElem(&cr);
   }

   // the resource map tests came from V91_BRANCH with a counter of their own; fold it in
   s_fail += test_rsmap_utilization();
   s_fail += test_rsmap_below();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}

/**
 * @brief build a resource map instance list, as the granted resources carry it
 *
 * The arguments are identifier and amount pairs, ended by a nullptr identifier.
 */
static lList *
make_rsmap(const char *id, uint32_t amount, ...) {
   lList *ids = nullptr;
   va_list ap;

   va_start(ap, amount);
   while (id != nullptr) {
      lListElem *ep = lAddElemStr(&ids, RESL_value, id, RESL_Type);
      lSetUlong(ep, RESL_amount, amount);
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
 */
static uint32_t
rsmap_at(const lListElem *rde, const char *id) {
   const lListElem *ep = lGetSubStr(rde, RESL_value, id, RDE_resource_map_list);
   return (ep != nullptr) ? lGetUlong(ep, RESL_amount) : 0;
}

/**
 * @brief the entry of the diagram which is in force at a point in time
 */
static const lListElem *
rde_at_time(const lListElem *cr, uint64_t time) {
   const lListElem *hit = nullptr;
   const lListElem *ep;

   for_each_ep (ep, lGetList(cr, RUE_utilized)) {
      if (lGetUlong64(ep, RDE_time) > time) {
         break;
      }
      hit = ep;
   }

   return hit;
}

static int
check_rsmap(const lListElem *cr, uint64_t time, const char *id, uint32_t expected) {
   const lListElem *rde = rde_at_time(cr, time);
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

   lListElem *cr = lCreateElem(RUE_Type);
   lSetString(cr, RUE_name, "gpu");

   // a reservation takes two instances of gpu0 from 1000 for 200 seconds
   lList *two_of_gpu0 = make_rsmap("gpu0", 2, nullptr);
   utilization_add(cr, 1000, 200, 2, 100, 1, HOST_TAG, "node01", "RESERVING", false, false,
                   nullptr, two_of_gpu0);

   printf("a reservation of gpu0 twice, from 1000 for 200s\n");
   ret += check_rsmap(cr, 900, "gpu0", 0);
   ret += check_rsmap(cr, 1000, "gpu0", 2);
   ret += check_rsmap(cr, 1100, "gpu0", 2);
   ret += check_rsmap(cr, 1200, "gpu0", 0);

   // a job inside that window takes one of gpu1, which has to leave gpu0 alone
   lList *one_of_gpu1 = make_rsmap("gpu1", 1, nullptr);
   utilization_add(cr, 1050, 100, 1, 101, 1, HOST_TAG, "node01", "STARTING", false, false,
                   nullptr, one_of_gpu1);

   printf("a job taking gpu1 once, from 1050 for 100s\n");
   ret += check_rsmap(cr, 1000, "gpu1", 0);
   ret += check_rsmap(cr, 1050, "gpu1", 1);
   ret += check_rsmap(cr, 1100, "gpu1", 1);
   ret += check_rsmap(cr, 1150, "gpu1", 0);
   ret += check_rsmap(cr, 1100, "gpu0", 2);

   // a second booking of gpu0 inside the same window adds up
   lList *one_of_gpu0 = make_rsmap("gpu0", 1, nullptr);
   utilization_add(cr, 1100, 50, 1, 102, 1, HOST_TAG, "node01", "STARTING", false, false,
                   nullptr, one_of_gpu0);

   printf("a job taking gpu0 once more, from 1100 for 50s\n");
   ret += check_rsmap(cr, 1050, "gpu0", 2);
   ret += check_rsmap(cr, 1100, "gpu0", 3);
   ret += check_rsmap(cr, 1150, "gpu0", 2);
   ret += check_rsmap(cr, 1200, "gpu0", 0);

   // and withdrawing it leaves what the others hold
   utilization_add(cr, 1100, 50, -1, 102, 1, HOST_TAG, "node01", "STARTING", false, false,
                   nullptr, one_of_gpu0);

   printf("that job is withdrawn again\n");
   ret += check_rsmap(cr, 1100, "gpu0", 2);
   ret += check_rsmap(cr, 1150, "gpu0", 2);
   ret += check_rsmap(cr, 1200, "gpu0", 0);
   ret += check_rsmap(cr, 1100, "gpu1", 1);

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
      lList *taken = utilization_rsmap_max(cr, 1000, windows[i].start, windows[i].duration);
      const lListElem *ep = lGetElemStr(taken, RESL_value, windows[i].id);
      uint32_t got = (ep != nullptr) ? lGetUlong(ep, RESL_amount) : 0;

      if (got != windows[i].expected) {
         printf("   FAIL: %s: expected " sge_u32 " of %s over [" sge_u64 "," sge_u64 "),"
                " got " sge_u32 "\n",
                windows[i].what, windows[i].expected, windows[i].id, windows[i].start,
                windows[i].start + windows[i].duration, got);
         ret++;
      } else {
         printf("   ok: %s: " sge_u32 " of %s\n", windows[i].what, got, windows[i].id);
      }
      lFreeList(&taken);
   }

   // What a job holds right now is in the utilization of the moment as well as in the diagram,
   // so the two are combined by taking the larger and not by adding them up. gpu0 is held twice
   // over [1000,1200) in the diagram; saying the same in the now list must not make it four.
   lSetList(cr, RUE_utilized_now_resource_map_list, make_rsmap("gpu0", 2, "gpu2", 1, nullptr));

   printf("with the same instances also recorded as held right now\n");
   {
      lList *taken = utilization_rsmap_max(cr, 1000, 1000, 200);
      const lListElem *ep = lGetElemStr(taken, RESL_value, "gpu0");
      uint32_t got = (ep != nullptr) ? lGetUlong(ep, RESL_amount) : 0;
      if (got != 2) {
         printf("   FAIL: gpu0 is held twice, in both places, expected 2 got " sge_u32 "\n", got);
         ret++;
      } else {
         printf("   ok: gpu0 counts once, " sge_u32 "\n", got);
      }

      // an instance only the now list knows about still counts
      ep = lGetElemStr(taken, RESL_value, "gpu2");
      got = (ep != nullptr) ? lGetUlong(ep, RESL_amount) : 0;
      if (got != 1) {
         printf("   FAIL: gpu2 is held right now, expected 1 got " sge_u32 "\n", got);
         ret++;
      } else {
         printf("   ok: gpu2 is held right now, " sge_u32 "\n", got);
      }
      lFreeList(&taken);
   }

   printf("and at queue end\n");
   {
      // every job holding something now has ended by then, so the now list does not count;
      // what remains is what the diagram says at its end, which here is nothing
      lList *taken = utilization_rsmap_max(cr, 1000, DISPATCH_TIME_QUEUE_END, 100);
      if (lGetNumberOfElem(taken) != 0) {
         printf("   FAIL: something is taken at queue end, expected nothing\n");
         ret++;
      } else {
         printf("   ok: nothing is taken at queue end\n");
      }
      lFreeList(&taken);
   }

   printf("and for a window which begins after this moment\n");
   {
      // gpu2 is held now and nothing says it still is at 1200, so it does not count there
      lList *taken = utilization_rsmap_max(cr, 1000, 1200, 100);
      const lListElem *ep = lGetElemStr(taken, RESL_value, "gpu2");
      uint32_t got = (ep != nullptr) ? lGetUlong(ep, RESL_amount) : 0;
      if (got != 0) {
         printf("   FAIL: gpu2 is held now but the window is later, expected 0 got "
                sge_u32 "\n", got);
         ret++;
      } else {
         printf("   ok: what is held now does not count against a later window\n");
      }
      lFreeList(&taken);
   }

   // A map is configured on an exec host or on the global host, and a job takes it from
   // whichever has it, so both layers have to record the instances. A queue cannot carry a map
   // and must not record any.
   {
      lListElem *global_cr = lCreateElem(RUE_Type);
      lSetString(global_cr, RUE_name, "gpu");
      utilization_add(global_cr, 1000, 100, 1, 200, 1, GLOBAL_TAG, "global", "STARTING", false,
                      false, nullptr, one_of_gpu1);

      lList *taken = utilization_rsmap_max(global_cr, 1000, 1000, 100);
      const lListElem *ep = lGetElemStr(taken, RESL_value, "gpu1");
      if (ep == nullptr || lGetUlong(ep, RESL_amount) != 1) {
         printf("   FAIL: a map on the global host records nothing\n");
         ret++;
      } else {
         printf("   ok: a map on the global host records its instances\n");
      }
      lFreeList(&taken);
      lFreeElem(&global_cr);

      lListElem *queue_cr = lCreateElem(RUE_Type);
      lSetString(queue_cr, RUE_name, "gpu");
      utilization_add(queue_cr, 1000, 100, 1, 201, 1, QUEUE_TAG, "a.q", "STARTING", false,
                      false, nullptr, one_of_gpu1);

      taken = utilization_rsmap_max(queue_cr, 1000, 1000, 100);
      if (lGetNumberOfElem(taken) != 0) {
         printf("   FAIL: a queue recorded resource map instances\n");
         ret++;
      } else {
         printf("   ok: a queue records no instances\n");
      }
      lFreeList(&taken);
      lFreeElem(&queue_cr);
   }

   lFreeList(&two_of_gpu0);
   lFreeList(&one_of_gpu1);
   lFreeList(&one_of_gpu0);
   lFreeElem(&cr);

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
static lListElem *
make_two_cards() {
   lListElem *def = lCreateElem(CE_Type);
   lSetString(def, CE_name, "gpu");
   lSetUlong(def, CE_valtype, static_cast<uint32_t>(ocs::CEntry::Type::RSMAP));

   for (const char *id : {"gpu0", "gpu1"}) {
      lListElem *resl = lAddSubStr(def, RESL_value, id, CE_resource_map_list, RESL_Type);
      lSetUlong(resl, RESL_amount, 2);
      lListElem *prop = lAddSubStr(resl, CE_name, "rack", RESL_properties, CE_Type);
      lSetString(prop, CE_stringval, "r1");
   }

   return def;
}

static int
check_below(const lListElem *def, const lListElem *cr, const char *key_name, uint32_t amount,
            uint64_t expected, const char *what) {
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

   lListElem *def = make_two_cards();
   lListElem *cr = lCreateElem(RUE_Type);
   lSetString(cr, RUE_name, "gpu");

   // nothing booked at all: any group can serve the request straight away
   ret += check_below(def, cr, "id", 2, DISPATCH_TIME_NOW, "an empty diagram answers now");

   // one share of each card held to 1000 and 2000. Two shares in total are free the whole
   // time, but never two of one card until the first is given back.
   lList *one_of_gpu0 = make_rsmap("gpu0", 1, nullptr);
   lList *one_of_gpu1 = make_rsmap("gpu1", 1, nullptr);
   utilization_add(cr, 0, 1000, 1, 100, 1, HOST_TAG, "node01", "STARTING", false, false,
                   nullptr, one_of_gpu0);
   utilization_add(cr, 0, 2000, 1, 101, 1, HOST_TAG, "node01", "STARTING", false, false,
                   nullptr, one_of_gpu1);

   ret += check_below(def, cr, "id", 1, DISPATCH_TIME_NOW,
                      "one share is free on both cards now");
   ret += check_below(def, cr, "id", 2, 1000,
                      "a whole card comes free when the share on gpu0 is given back");

   // grouped by a characteristic both cards are one group, so the shares add up and two of
   // the group are free from the start
   ret += check_below(def, cr, "rack", 2, DISPATCH_TIME_NOW,
                      "grouped by rack the two free shares are in one group");
   ret += check_below(def, cr, "rack", 3, 1000,
                      "three of the group need the gpu0 share back");
   ret += check_below(def, cr, "rack", 4, 2000,
                      "the whole group needs both shares back");

   // A gap which closes again must not be offered. On a fresh diagram gpu0 is free from 1000,
   // taken again from 1500 to 2500 and free for good after that, while gpu1 is held to 3000.
   // The gap at 1000 fits a whole card but does not last, so the answer is gpu0 at 2500.
   lListElem *gap = lCreateElem(RUE_Type);
   lSetString(gap, RUE_name, "gpu");
   lList *two_of_gpu0 = make_rsmap("gpu0", 2, nullptr);
   utilization_add(gap, 0, 1000, 1, 102, 1, HOST_TAG, "node01", "STARTING", false, false,
                   nullptr, one_of_gpu0);
   utilization_add(gap, 1500, 1000, 2, 103, 1, HOST_TAG, "node01", "STARTING", false, false,
                   nullptr, two_of_gpu0);
   utilization_add(gap, 0, 3000, 1, 104, 1, HOST_TAG, "node01", "STARTING", false, false,
                   nullptr, one_of_gpu1);

   ret += check_below(def, gap, "id", 2, 2500,
                      "the gap at 1000 closes again, so gpu0 answers at 2500 instead");

   // a group still taken at the end of the diagram never comes free, but another group which
   // is free throughout still answers
   lListElem *full = lCreateElem(RUE_Type);
   lSetString(full, RUE_name, "gpu");
   lList *both_of_gpu0 = make_rsmap("gpu0", 2, nullptr);
   utilization_add(full, 0, std::numeric_limits<uint64_t>::max(), 2, 105, 1, HOST_TAG, "node01", "STARTING", false,
                   false, nullptr, both_of_gpu0);

   ret += check_below(def, full, "id", 2, DISPATCH_TIME_NOW,
                      "gpu0 never comes free, gpu1 is untouched and answers now");

   lList *both_cards = make_rsmap("gpu0", 2, "gpu1", 2, nullptr);
   lListElem *all_full = lCreateElem(RUE_Type);
   lSetString(all_full, RUE_name, "gpu");
   utilization_add(all_full, 0, std::numeric_limits<uint64_t>::max(), 4, 104, 1, HOST_TAG, "node01", "STARTING", false,
                   false, nullptr, both_cards);

   ret += check_below(def, all_full, "id", 2, std::numeric_limits<uint64_t>::max(),
                      "when no card ever comes free there is nothing to reserve");

   lFreeList(&one_of_gpu0);
   lFreeList(&one_of_gpu1);
   lFreeList(&two_of_gpu0);
   lFreeList(&both_of_gpu0);
   lFreeList(&both_cards);
   lFreeElem(&cr);
   lFreeElem(&gap);
   lFreeElem(&full);
   lFreeElem(&all_full);
   lFreeElem(&def);

   if (ret == 0) {
      printf("\n - when a group comes free: ok -\n");
   }
   return ret;
}
