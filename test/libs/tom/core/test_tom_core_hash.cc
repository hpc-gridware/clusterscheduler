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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Unit tests and benchmark for the hash tables of the core of the typed object model (libs/tom)
 *
 * The lists are driven through the run-time typed interface (tom/ocs_Raw.h)
 * and the core (ocs::tom::detail::hash_new(), add_elem_ulong(), lookup_first()/lookup_next()),
 * the functions the old lAddElemUlong(), lGetElemUlongRW(),
 * lGetElemStrFirstRW()/NextRW(), lSetUlong(), lSetString(), lRemoveElem() and
 * lCopyList() ran on, so the benchmark measures the same core operations.
 */

/* #define HASH_STATISTICS */

/** @brief Report the process's memory use alongside the timings
 *
 * A hash index trades memory for speed, so a run that only reported time would
 * hide half of what the change costs.
 */
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

/** @brief Make this translation unit the one that defines the descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/detail/ocs_Hash.h"
#include "tom/ocs_Types.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Raw.h"

#include "uti/sge_component.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include <sge_log.h>

#include "ocs_test_model.h"

#ifdef TEST_USE_JOBL
#include "sge_jobL.h"
int NM_ULONG  = JB_job_number;   ///< the numeric attribute the benchmark hashes on
int NM_STRING = JB_owner;        ///< the string attribute it hashes on
const ocs::tom::FieldDescr *DESCR = ocs::tom::JB::descriptor.e;   ///< the object type the benchmark builds lists of

#else
enum {
   TEST_ulong = 1,   ///< an unsigned long attribute
   TEST_string   ///< a string attribute
};

LISTDEF(TEST_Type)
                SGE_ULONG (TEST_ulong, MT_DEFAULT)
                SGE_STRING (TEST_string, MT_DEFAULT)
LISTEND(TEST_Type)

NAMEDEF(TEST_Name)
                NAME("TEST_ulong")
                NAME("TEST_string")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)   ///< number of attributes of the synthetic type

int NM_ULONG = TEST_ulong;     ///< the numeric attribute the benchmark hashes on
int NM_STRING = TEST_string;   ///< the string attribute it hashes on
const ocs::tom::FieldDescr *DESCR = TEST_Type;     ///< the object type the benchmark builds lists of
#endif

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

/** @brief What one benchmark run measured, in seconds per operation
 *
 * The hash test is as much a benchmark as a correctness test: it reports the
 * cost of each operation with and without a hash index, so a change that makes
 * lookups faster but insertion far slower is visible rather than hidden.
 */
struct BenchTimes {
   double create,/**< building the list */copy,/**< copying it */lookup,/**< finding elements by key */iter_str,/**< walking it in string-key order */mod_key,/**< changing the hashed key of elements */mod_str,/**< changing a non-key string */del_key,/**< deleting by key */del_str;/**< deleting by string */
   int objs_del_key,/**< how many elements the del_key pass removed */objs_del_str;/**< how many the del_str pass removed */
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

const char **names = nullptr;   ///< the attribute names of #DESCR, for the output

const char *HEADER_FORMAT = "%s %s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s %8s\n";   ///< column headings of the timing table
const char *DATA_FORMAT   = "%s %s %8.3lf %8.3lf %8.3lf %8.3lf %8.3lf %8.3lf %8.3lf (%6d) %8.3lf (%6d) %8ld\n";   ///< one row of it, matching #HEADER_FORMAT

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

/**
 * @brief Create a list of the benchmark type with the hash tables asked for
 * @param name the name of the list
 * @param unique_hash whether NM_ULONG gets a unique hash table
 * @param non_unique_hash whether NM_STRING gets a non unique hash table
 * @return the list
 */
static ocs::tom::RawListPtr make_hashed_list(const char *name, const bool unique_hash, const bool non_unique_hash) {
   auto lp = ocs::tom::RawListPtr::create(DESCR, ocs::tom::Hashing::On, name);
   if (unique_hash) {
      ocs::tom::detail::hash_new(lp.get(), NM_ULONG, true);
   }
   if (non_unique_hash) {
      ocs::tom::detail::hash_new(lp.get(), NM_STRING, false);
   }
   return lp;
}

/**
 * @brief Append num_objects elements with the keys 0 .. num_objects-1 and a random name each
 *
 * add_elem_ulong() is the core of the old lAddElemUlong(); the name is set
 * through the run-time typed setter, as lSetString() set it.
 *
 * @param lp the list, not empty
 * @param num_objects the number of elements
 * @param num_names the number of names to choose from
 */
static void fill_list(const ocs::tom::RawList lp, const int num_objects, const int num_names) {
   ocs::tom::ListData *raw = lp.raw();   // never nullptr, so add_elem_ulong() does not replace it
   for (int i = 0; i < num_objects; i++) {
      ocs::tom::ElementData *ep = ocs::tom::add_elem_ulong(raw, NM_ULONG, static_cast<uint32_t>(i), DESCR);
      ocs::tom::set(ep, NM_STRING, names[rand() % num_names]);
   }
}

/**
 * @brief Look an element up by its unique key, as lGetElemUlongRW() did
 * @param lp the list
 * @param key the key
 * @return the element, or nullptr
 */
static ocs::tom::ElementData *find_key(const ocs::tom::RawList lp, const int key) {
   return ocs::tom::find_first<std::uint32_t>(lp.raw(), NM_ULONG, static_cast<std::uint32_t>(key));
}

/**
 * @brief Correctness checks T01-T10, separated from the benchmark timing so that the output is not mixed
 * @param num_objects the number of elements to build
 * @param num_names the number of names in the non unique hash
 */
static void check_correctness(const int num_objects, const int num_names) {
   auto owner = make_hashed_list("check list", true, true);
   const ocs::tom::RawList lp = owner.get();
   const int pos_string = ocs::tom::field_pos(DESCR, NM_STRING);
   ocs::tom::ElementData *ep;
   int i, total_iterated, objs_dru;

   fill_list(lp, num_objects, num_names);
   CHECK(1, "list contains num_objects elements after build", static_cast<int>(lp.size()) == num_objects);

   {
      const auto copy = ocs::tom::clone(lp.raw(), ocs::tom::Hashing::On);
      CHECK(2, "copied list has same element count", static_cast<int>(copy->size()) == num_objects);
      CHECK(3, "unique hash functional in copied list", find_key(copy.get(), 0) != nullptr);
   }

   for (i = 0; i < num_objects; i++) {
      ep = find_key(lp, rand() % num_objects);
   }
   CHECK(4, "element 0 reachable after random-access loop", find_key(lp, 0) != nullptr);

   total_iterated = 0;
   for (i = 0; i < num_names; i++) {
      const void *iterator = nullptr;
      ocs::tom::ElementData *next_ep = ocs::tom::lookup_first<ocs::tom::FieldType::String>(lp, pos_string, names[i],
                                                                                           &iterator);
      while ((ep = next_ep) != nullptr) {
         next_ep = ocs::tom::lookup_next<ocs::tom::FieldType::String>(lp, pos_string, names[i], &iterator);
         total_iterated++;
      }
   }
   CHECK(5, "non-unique iterator visits all elements", total_iterated == num_objects);

   for (i = 0; i < num_objects; i++) {
      const int unique = rand() % num_objects;
      const int newval = unique + num_objects;
      ep = find_key(lp, unique);
      ocs::tom::set(ep, NM_ULONG, static_cast<std::uint32_t>(newval));
      ocs::tom::set(ep, NM_ULONG, static_cast<std::uint32_t>(unique));
   }
   CHECK(6, "element 0 reachable after unique-attr change/restore loop", find_key(lp, 0) != nullptr);

   for (i = 0; i < num_objects; i++) {
      ep = find_key(lp, rand() % num_objects);
      ocs::tom::set(ep, NM_STRING, names[rand() % num_names]);
   }
   CHECK(7, "unique hash intact after non-unique attr changes", find_key(lp, 0) != nullptr);

   objs_dru = 0;
   for (i = 0; i < num_objects / 2; i++) {
      ep = find_key(lp, rand() % num_objects);
      if (ep != nullptr) {
         lp.erase(ep);
         objs_dru++;
      }
   }
   CHECK(8, "list size correct after delete-random phase", static_cast<int>(lp.size()) == num_objects - objs_dru);

   for (i = 0; i < num_names; i++) {
      const void *iterator = nullptr;
      ocs::tom::ElementData *next_ep = ocs::tom::lookup_first<ocs::tom::FieldType::String>(lp, pos_string, names[i],
                                                                                           &iterator);
      while ((ep = next_ep) != nullptr) {
         next_ep = ocs::tom::lookup_next<ocs::tom::FieldType::String>(lp, pos_string, names[i], &iterator);
         lp.erase(ep);
      }
   }
   CHECK(9, "list empty after delete-by-iterate phase", lp.size() == 0);

   owner.reset();
   CHECK(10, "reset() frees the list and empties the owner", !owner);
}

/**
 * @brief The benchmark: no correctness check output, returns the timing data for the scaling analysis
 *
 * Every timed operation runs on the core path the old function ran on:
 * add_elem_ulong() and set() for lAddElemUlong()/lSetString(), clone() for
 * lCopyList(), find_first<std::uint32_t>() for lGetElemUlongRW(),
 * lookup_first()/lookup_next() for lGetElemStrFirstRW()/NextRW() (with the
 * position of the field resolved once per run instead of once per name),
 * set() for lSetUlong()/lSetString() and erase() for lRemoveElem().
 *
 * @param unique_hash whether NM_ULONG gets a unique hash table
 * @param non_unique_hash whether NM_STRING gets a non unique hash table
 * @param num_objects the number of elements to build
 * @param num_names the number of names in the non unique hash
 * @return the seconds per operation
 */
static BenchTimes do_test(const bool unique_hash, const bool non_unique_hash,
                          const int num_objects, const int num_names) {
   BenchTimes bt{};
   ocs::tom::ElementData *ep;
   struct timespec ts0, ts1;
   int i;
#ifdef MALLINFO
   struct mallinfo meminfo;
#endif
#ifdef HASH_STATISTICS
   tom_htable cht;
   dstring stat_dstring = DSTRING_INIT;
#endif

   auto owner = make_hashed_list("test list", unique_hash, non_unique_hash);
   const ocs::tom::RawList lp = owner.get();
   const int pos_string = ocs::tom::field_pos(DESCR, NM_STRING);
#ifdef HASH_STATISTICS
   cht = list_hash_table(lp, 1);
   printf("%s\n", ocs::tom::detail::hash_statistics(cht, &stat_dstring));
#endif

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   fill_list(lp, num_objects, num_names);
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.create = elapsed(ts0, ts1);

#ifdef MALLINFO
   meminfo = mallinfo();
#endif

   {
      clock_gettime(CLOCK_MONOTONIC, &ts0);
      const auto copy = ocs::tom::clone(lp.raw(), ocs::tom::Hashing::On);
      clock_gettime(CLOCK_MONOTONIC, &ts1);
      bt.copy = elapsed(ts0, ts1);
   }

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   for (i = 0; i < num_objects; i++) {
      ep = find_key(lp, rand() % num_objects);
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.lookup = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   for (i = 0; i < num_names; i++) {
      const void *iterator = nullptr;
      ocs::tom::ElementData *next_ep;
      next_ep = ocs::tom::lookup_first<ocs::tom::FieldType::String>(lp, pos_string, names[i], &iterator);
      while ((ep = next_ep) != nullptr) {
         next_ep = ocs::tom::lookup_next<ocs::tom::FieldType::String>(lp, pos_string, names[i], &iterator);
      }
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.iter_str = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   for (i = 0; i < num_objects; i++) {
      const int unique = rand() % num_objects;
      const int newval = unique + num_objects;
      ep = find_key(lp, unique);
      ocs::tom::set(ep, NM_ULONG, static_cast<std::uint32_t>(newval));
      ocs::tom::set(ep, NM_ULONG, static_cast<std::uint32_t>(unique));
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.mod_key = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   for (i = 0; i < num_objects; i++) {
      ep = find_key(lp, rand() % num_objects);
      ocs::tom::set(ep, NM_STRING, names[rand() % num_names]);
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.mod_str = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   bt.objs_del_key = 0;
   for (i = 0; i < num_objects / 2; i++) {
      ep = find_key(lp, rand() % num_objects);
      if (ep != nullptr) {
         lp.erase(ep);
         bt.objs_del_key++;
      }
   }
   clock_gettime(CLOCK_MONOTONIC, &ts1);
   bt.del_key = elapsed(ts0, ts1);

   clock_gettime(CLOCK_MONOTONIC, &ts0);
   bt.objs_del_str = 0;
   for (i = 0; i < num_names; i++) {
      const void *iterator = nullptr;
      ocs::tom::ElementData *next_ep;
      next_ep = ocs::tom::lookup_first<ocs::tom::FieldType::String>(lp, pos_string, names[i], &iterator);
      while ((ep = next_ep) != nullptr) {
         next_ep = ocs::tom::lookup_next<ocs::tom::FieldType::String>(lp, pos_string, names[i], &iterator);
         lp.erase(ep);
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
   printf("%s\n", ocs::tom::detail::hash_statistics(cht, &stat_dstring));
   sge_dstring_free(&stat_dstring);
#endif

   return bt;
}

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_tom_core_hash");
   int i;

   if (argc != 1 && argc < 5) {
      usage(argv[0]);
   }

   component_set_daemonized(true);
#ifndef TEST_USE_JOBL
   ocs::module_test::install_test_model({{"TEST", TEST_Type, TEST_Name, TEST_Size}});
#endif
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
