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

/* #define HASH_STATISTICS */
#define XMALLINFO

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <cmath>

#ifdef MALLINFO
#include <malloc.h>
#endif

#include <cinttypes>

#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"

#include "uti/sge_component.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include <sge_log.h>

#ifdef TEST_USE_JOBL
#include "sge_jobL.h"
int NM_ULONG  = JB_job_number;
int NM_STRING = JB_owner;
lDescr *DESCR = JB_Type;

lNameSpace my_nmv[] = {
   {1, JBS, JBN },
   {0, 0, nullptr}
};

#else
enum {
   TEST_ulong = 1,
   TEST_string
};

LISTDEF(TEST_Type)
                SGE_ULONG (TEST_ulong, CULL_DEFAULT)
                SGE_STRING (TEST_string, CULL_DEFAULT)
LISTEND

NAMEDEF(TEST_Name)
                NAME("TEST_ulong")
                NAME("TEST_string")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)

int NM_ULONG = TEST_ulong;
int NM_STRING = TEST_string;
lDescr *DESCR = TEST_Type;

lNameSpace my_nmv[] = {
        {1, TEST_Size, TEST_Name, TEST_Type},
        {0, 0, nullptr, nullptr}
};
#endif

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

struct BenchTimes {
   double create, copy, lookup, iter_str, mod_key, mod_str, del_key, del_str;
   int objs_del_key, objs_del_str;
};

static void usage(const char *argv0) {
   fprintf(stderr, "usage: %s [<num_objects> <num_names> <uh> <nuh>]\n", argv0);
   fprintf(stderr, "<num_objects> = number of objects to be created\n");
   fprintf(stderr, "<num_names>   = number of entries in non unique hash\n");
   fprintf(stderr, "<uh>          = create unique hash\n");
   fprintf(stderr, "<nuh>         = create non unique hash\n");
   exit(EXIT_FAILURE);
}

static const char *random_string(int length) {
   static char buf[1000];
   int i;

   for (i = 0; i < length; i++) {
      buf[i] = rand() % 26 + 64;
   }
   buf[i] = 0;

   return strdup(buf);
}

const char **names = nullptr;

const char *HEADER_FORMAT = "%s %s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s\n";
const char *DATA_FORMAT   = "%s %s %8.3lf %8.3lf %8.3lf %8.3lf %8.3lf %8.3lf %8.3lf (%6d) %8.3lf (%6d) %8ld\n";

static double elapsed(const struct timespec &a, const struct timespec &b) {
   return (b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) * 1e-9;
}

static const char *classify_exp(double e) {
   if (e <= 1.1) { return "O(n)"; }
   if (e <= 1.3) { return "~O(n log n)"; }
   if (e <= 1.7) { return "superlinear"; }
   return "O(n^2)+";
}

static void print_scaling_row(const char *op, double t0, double t1, double t2) {
   char e01[32], e12[32];

   if (t0 > 1e-6 && t1 > 1e-6) {
      double e = log10(t1 / t0);
      snprintf(e01, sizeof(e01), "%5.2f  %s", e, classify_exp(e));
   } else {
      snprintf(e01, sizeof(e01), "n/a");
   }

   if (t1 > 1e-6 && t2 > 1e-6) {
      double e = log10(t2 / t1);
      snprintf(e12, sizeof(e12), "%5.2f  %s", e, classify_exp(e));
   } else {
      snprintf(e12, sizeof(e12), "n/a");
   }

   printf("  %-9s  %9.4f  %9.4f  %9.4f    %-22s  %s\n", op, t0, t1, t2, e01, e12);
}

static void print_scaling(const BenchTimes *t) {
   printf("\n--- scaling analysis (10k -> 100k -> 1M, uh+nuh) ---\n");
   printf("  %-9s  %9s  %9s  %9s    %-22s  %s\n",
          "op", "10k (s)", "100k (s)", "1M (s)", "exp(10k->100k)", "exp(100k->1M)");
   print_scaling_row("create",   t[0].create,   t[1].create,   t[2].create);
   print_scaling_row("copy",     t[0].copy,     t[1].copy,     t[2].copy);
   print_scaling_row("lookup",   t[0].lookup,   t[1].lookup,   t[2].lookup);
   print_scaling_row("iter_str", t[0].iter_str, t[1].iter_str, t[2].iter_str);
   print_scaling_row("mod_key",  t[0].mod_key,  t[1].mod_key,  t[2].mod_key);
   print_scaling_row("mod_str",  t[0].mod_str,  t[1].mod_str,  t[2].mod_str);
   print_scaling_row("del_key",  t[0].del_key,  t[1].del_key,  t[2].del_key);
   print_scaling_row("del_str",  t[0].del_str,  t[1].del_str,  t[2].del_str);
}

// Correctness checks T01-T10; separated from benchmark timing so output is not mixed.
static void check_correctness(int num_objects, int num_names) {
   lList *lp = lCreateList("check list", DESCR);
   cull_hash_new(lp, NM_ULONG, true);
   cull_hash_new(lp, NM_STRING, false);
   lListElem *ep;
   int i, total_iterated, objs_dru;

   for (i = 0; i < num_objects; i++) {
      ep = lAddElemUlong(&lp, NM_ULONG, i, DESCR);
      lSetString(ep, NM_STRING, names[rand() % num_names]);
   }
   CHECK(1, "list contains num_objects elements after build", (int)lGetNumberOfElem(lp) == num_objects);

   lList *copy = lCopyList("copy", lp);
   CHECK(2, "copied list has same element count", (int)lGetNumberOfElem(copy) == num_objects);
   CHECK(3, "unique hash functional in copied list", lGetElemUlongRW(copy, NM_ULONG, 0) != nullptr);
   lFreeList(&copy);

   for (i = 0; i < num_objects; i++) {
      ep = lGetElemUlongRW(lp, NM_ULONG, rand() % num_objects);
   }
   CHECK(4, "element 0 reachable after random-access loop", lGetElemUlongRW(lp, NM_ULONG, 0) != nullptr);

   total_iterated = 0;
   for (i = 0; i < num_names; i++) {
      const void *iterator = nullptr;
      lListElem *next_ep = lGetElemStrFirstRW(lp, NM_STRING, names[i], &iterator);
      while ((ep = next_ep) != nullptr) {
         next_ep = lGetElemStrNextRW(lp, NM_STRING, names[i], &iterator);
         total_iterated++;
      }
   }
   CHECK(5, "non-unique iterator visits all elements", total_iterated == num_objects);

   for (i = 0; i < num_objects; i++) {
      int unique = rand() % num_objects;
      int newval = unique + num_objects;
      ep = lGetElemUlongRW(lp, NM_ULONG, unique);
      lSetUlong(ep, NM_ULONG, newval);
      lSetUlong(ep, NM_ULONG, unique);
   }
   CHECK(6, "element 0 reachable after unique-attr change/restore loop", lGetElemUlongRW(lp, NM_ULONG, 0) != nullptr);

   for (i = 0; i < num_objects; i++) {
      ep = lGetElemUlongRW(lp, NM_ULONG, rand() % num_objects);
      lSetString(ep, NM_STRING, names[rand() % num_names]);
   }
   CHECK(7, "unique hash intact after non-unique attr changes", lGetElemUlongRW(lp, NM_ULONG, 0) != nullptr);

   objs_dru = 0;
   for (i = 0; i < num_objects / 2; i++) {
      ep = lGetElemUlongRW(lp, NM_ULONG, rand() % num_objects);
      if (ep != nullptr) {
         lRemoveElem(lp, &ep);
         objs_dru++;
      }
   }
   CHECK(8, "list size correct after delete-random phase", (int)lGetNumberOfElem(lp) == num_objects - objs_dru);

   for (i = 0; i < num_names; i++) {
      const void *iterator = nullptr;
      lListElem *next_ep = lGetElemStrFirstRW(lp, NM_STRING, names[i], &iterator);
      while ((ep = next_ep) != nullptr) {
         next_ep = lGetElemStrNextRW(lp, NM_STRING, names[i], &iterator);
         lRemoveElem(lp, &ep);
      }
   }
   CHECK(9, "list empty after delete-by-iterate phase", (int)lGetNumberOfElem(lp) == 0);

   lFreeList(&lp);
   CHECK(10, "lFreeList nulls the list pointer", lp == nullptr);
}

// Pure benchmark: no correctness check output, returns timing data for scaling analysis.
static BenchTimes do_test(bool unique_hash, bool non_unique_hash,
                          int num_objects, int num_names) {
   BenchTimes bt{};
   lList *lp = nullptr;
   lList *copy;
   lListElem *ep;
   struct timespec ts0, ts1;
   int i, total_iterated;
#ifdef MALLINFO
   struct mallinfo meminfo;
#endif
#ifdef HASH_STATISTICS
   cull_htable cht;
   dstring stat_dstring = DSTRING_INIT;
#endif

   lp = lCreateList("test list", DESCR);
   if (unique_hash) {
      cull_hash_new(lp, NM_ULONG, true);
   }
   if (non_unique_hash) {
      cull_hash_new(lp, NM_STRING, false);
   }
#ifdef HASH_STATISTICS
   cht = lp->descr[1].ht;
   printf("%s\n", cull_hash_statistics(cht, &stat_dstring));
#endif

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   for (i = 0; i < num_objects; i++) {
      ep = lAddElemUlong(&lp, NM_ULONG, i, DESCR);
      lSetString(ep, NM_STRING, names[rand() % num_names]);
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.create = elapsed(ts0, ts1);

#ifdef MALLINFO
   meminfo = mallinfo();
#endif

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   copy = lCopyList("copy", lp);
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.copy = elapsed(ts0, ts1);
   lFreeList(&copy);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   for (i = 0; i < num_objects; i++) {
      ep = lGetElemUlongRW(lp, NM_ULONG, rand() % num_objects);
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.lookup = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   total_iterated = 0;
   for (i = 0; i < num_names; i++) {
      const void *iterator = nullptr;
      lListElem *next_ep;
      next_ep = lGetElemStrFirstRW(lp, NM_STRING, names[i], &iterator);
      while ((ep = next_ep) != nullptr) {
         next_ep = lGetElemStrNextRW(lp, NM_STRING, names[i], &iterator);
         total_iterated++;
      }
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.iter_str = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   for (i = 0; i < num_objects; i++) {
      int unique = rand() % num_objects;
      int newval = unique + num_objects;
      ep = lGetElemUlongRW(lp, NM_ULONG, unique);
      lSetUlong(ep, NM_ULONG, newval);
      lSetUlong(ep, NM_ULONG, unique);
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.mod_key = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   for (i = 0; i < num_objects; i++) {
      ep = lGetElemUlongRW(lp, NM_ULONG, rand() % num_objects);
      lSetString(ep, NM_STRING, names[rand() % num_names]);
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.mod_str = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   bt.objs_del_key = 0;
   for (i = 0; i < num_objects / 2; i++) {
      ep = lGetElemUlongRW(lp, NM_ULONG, rand() % num_objects);
      if (ep != nullptr) {
         lRemoveElem(lp, &ep);
         bt.objs_del_key++;
      }
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.del_key = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   bt.objs_del_str = 0;
   for (i = 0; i < num_names; i++) {
      const void *iterator = nullptr;
      lListElem *next_ep;
      next_ep = lGetElemStrFirstRW(lp, NM_STRING, names[i], &iterator);
      while ((ep = next_ep) != nullptr) {
         next_ep = lGetElemStrNextRW(lp, NM_STRING, names[i], &iterator);
         lRemoveElem(lp, &ep);
         bt.objs_del_str++;
      }
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.del_str = elapsed(ts0, ts1);

   printf(DATA_FORMAT,
          unique_hash ? " * " : "   ",
          non_unique_hash ? " * " : "   ",
          bt.create, bt.copy,
          bt.lookup, bt.iter_str,
          bt.mod_key, bt.mod_str,
          bt.del_key, bt.objs_del_key, bt.del_str, bt.objs_del_str,
#ifdef MALLINFO
          (meminfo.usmblks + meminfo.uordblks) / 1024
#else
          0L
#endif
   );

#ifdef HASH_STATISTICS
   printf("%s\n", cull_hash_statistics(cht, &stat_dstring));
   sge_dstring_free(&stat_dstring);
#endif
   lFreeList(&lp);

   return bt;
}

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_cull_hash");
   int i;

   if (argc != 1 && argc < 5) {
      usage(argv[0]);
   }

   component_set_daemonized(true);
   lInit(my_nmv);
   srand(time(0));

   if (argc == 1) {
      // multi-scale auto mode: correctness checks first, then benchmark at 10k/100k/1M
      static const int scales[3] = {10000, 100000, 1000000};
      static const int num_names = 10;
      BenchTimes results[3];

      names = (const char **)sge_malloc(num_names * sizeof(const char *));
      SGE_ASSERT(names != nullptr);
      for (i = 0; i < num_names; i++) {
         names[i] = random_string(10);
      }

      printf("\n--- correctness checks (1000 objects) ---\n");
      check_correctness(1000, num_names);

      printf("\n--- benchmark (10k, 100k, 1M objects, uh+nuh) ---\n");
      printf(HEADER_FORMAT, "uh ", "nuh",
             "create", "copy", "lookup", "iter_str", "mod_key", "mod_str",
             "del_key", "(objs)", "del_str", "(objs)", "mem(kB)");

      for (int s = 0; s < 3; s++) {
         results[s] = do_test(true, true, scales[s], num_names);
      }

      for (i = 0; i < num_names; i++) {
         sge_free(&(names[i]));
      }

      print_scaling(results);

   } else {
      // single-scale CLI mode: correctness checks then single benchmark run
      int num_objects = atoi(argv[1]);
      int num_names   = atoi(argv[2]);
      bool uh         = atoi(argv[3]) != 0;
      bool nuh        = atoi(argv[4]) != 0;

      names = (const char **)sge_malloc(num_names * sizeof(const char *));
      SGE_ASSERT(names != nullptr);
      for (i = 0; i < num_names; i++) {
         names[i] = random_string(10);
      }

      printf("\n--- correctness checks (%d objects) ---\n", num_objects);
      check_correctness(num_objects, num_names);

      printf("\n--- benchmark ---\n");
      printf(HEADER_FORMAT, "uh ", "nuh",
             "create", "copy", "lookup", "iter_str", "mod_key", "mod_str",
             "del_key", "(objs)", "del_str", "(objs)", "mem(kB)");

      do_test(uh, nuh, num_objects, num_names);

      for (i = 0; i < num_names; i++) {
         sge_free(&(names[i]));
      }
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
