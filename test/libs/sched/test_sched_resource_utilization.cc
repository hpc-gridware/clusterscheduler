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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <limits>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_centry.h"
#include "sgeobj/cull/sge_all_listsL.h"

#include "sge_resource_utilization.h"
#include "sge_qeti.h"

typedef struct {
   uint64_t    start_time;
   uint64_t    duration;
   double      uti;
   const char *desc;
} test_array_t;

static int s_fail = 0;

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

   utilization_add(cr, 800,  200, 8, 100, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr);
   utilization_add(cr, 1000, 100, 4, 101, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr);
   utilization_add(cr, 1100, 100, 8, 102, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr);

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

      utilization_add(cr, 800,  200, 8, 100, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr);
      utilization_add(cr, 1000, 100, 4, 101, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr);
      utilization_add(cr, 1100, std::numeric_limits<uint64_t>::max(), 4, 102, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr);
      utilization_add(cr, 2000, std::numeric_limits<uint64_t>::max(), 4, 103, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr);

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

      utilization_add(cr, 1000, 100,                               -4, 101, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr);
      utilization_add(cr, 1100, std::numeric_limits<uint64_t>::max(), -4, 102, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr);

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

      utilization_add(cr, 800,  200,                               -8, 100, 1, PE_TAG, "pe_slots", "STARTING",  false, false, nullptr);
      utilization_add(cr, 2000, std::numeric_limits<uint64_t>::max(), -4, 103, 1, PE_TAG, "pe_slots", "RESERVING", false, false, nullptr);

      do_utilization_test(id, cr, test_array);
      do_qeti_test(id, cr, nullptr, nullptr);
   }

   lFreeElem(&cr);
}

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

      utilization_add(cr, 1500, 100, 4, 200, 1, PE_TAG, "pe_slots", "STARTING", false, false, nullptr);
      utilization_add(cr, 1500, 100, 4, 201, 1, PE_TAG, "pe_slots", "STARTING", false, false, nullptr);

      ocs::TopologyString binding_inuse;
      double uti = utilization_max(nullptr, nullptr, cr, 1500, 100, 0.0, 0.0, 0.0, false, binding_inuse);
      CHECK(id, "two concurrent reservations (4+4) accumulate to 8.0", uti == 8.0);
      id++;

      lFreeElem(&cr);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
