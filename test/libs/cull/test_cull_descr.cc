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
 * @brief Module test: the CULL descriptor registry on its own (CS-2997, SHD-02, SHD-03, SHD-06 at unit level)
 *
 * The registry hands out one shared, reference counted descriptor per distinct
 * sequence of entries, frees it with its last reference and falls back to
 * private copies above its soft cap. Nothing in the library uses it yet, so the
 * test drives it directly with hand-built descriptors. The static blocks built by
 * LISTDEF/LISTEND are checked as well: their header and that reference
 * operations leave them alone.
 *
 * DESCR-03 is meant to be run under the thread and the address sanitizer as well.
 */

#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

/** @brief Make this translation unit the one that defines the CULL field names */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"
#include "cull/ocs_cull_descr.h"

#include "uti/sge_stdlib.h"

#include "ocs_module_test.h"

/** @brief The object type the reductions are taken from */
enum {
   TD_id = 1, ///< unique hashed ulong
   TD_name,   ///< string
   TD_value,  ///< ulong
   TD_flag    ///< bool
};

LISTDEF(TD_Type)
   SGE_ULONG(TD_id, CULL_DEFAULT | CULL_HASH | CULL_UNIQUE)
   SGE_STRING(TD_name, CULL_DEFAULT)
   SGE_ULONG(TD_value, CULL_DEFAULT)
   SGE_BOOL(TD_flag, CULL_DEFAULT)
LISTEND(TD_Type)

NAMEDEF(TD_Name)
   NAME("TD_id")
   NAME("TD_name")
   NAME("TD_value")
   NAME("TD_flag")
NAMEEND

#define TD_Size sizeof(TD_Name) / sizeof(char *) ///< number of fields of TD_Type

/** @brief The name space registering the type with CULL */
lNameSpace nmv[] = {
   {1, TD_Size, TD_Name, TD_Type},
   {0, 0, nullptr, nullptr}
};

/// a reduction of TD_Type to id and name
static const lDescr RED_A[] = {{TD_id, lUlongT | CULL_HASH | CULL_UNIQUE | CULL_IS_REDUCED},
                               {TD_name, lStringT | CULL_IS_REDUCED},
                               {NoName, lEndT}};
/// a reduction of TD_Type to id and value
static const lDescr RED_B[] = {{TD_id, lUlongT | CULL_HASH | CULL_UNIQUE | CULL_IS_REDUCED},
                               {TD_value, lUlongT | CULL_IS_REDUCED},
                               {NoName, lEndT}};
/// RED_A in the other field order
static const lDescr RED_A_SWAPPED[] = {{TD_name, lStringT | CULL_IS_REDUCED},
                                       {TD_id, lUlongT | CULL_HASH | CULL_UNIQUE | CULL_IS_REDUCED},
                                       {NoName, lEndT}};

/**
 * @brief A plain heap copy of a descriptor, as a builder would produce it
 * @param dp the entries, terminated by lEndT
 * @return the copy, to be freed with sge_free()
 */
static lDescr *heap_copy(const lDescr *dp) {
   int n = 0;
   while (mt_get_type(dp[n].mt) != lEndT) {
      n++;
   }
   auto *copy = reinterpret_cast<lDescr *>(sge_malloc((n + 1) * sizeof(lDescr)));
   memcpy(copy, dp, (n + 1) * sizeof(lDescr));
   return copy;
}

/**
 * @brief The n-th of many distinct reductions: TD_id and TD_value with a varying type flag
 *
 * The descriptors differ in the CULL_HASH bit of entry 1 and in a
 * per-n field number, so each n yields a different key.
 *
 * @param n the index
 * @param out room for three entries
 */
static void distinct_reduction(const int n, lDescr out[3]) {
   out[0] = {TD_id, lUlongT | CULL_IS_REDUCED};
   out[1] = {TD_value + 100 * n, lUlongT | CULL_IS_REDUCED};
   out[2] = {NoName, lEndT};
}

/** @brief DESCR-01: the static blocks of LISTDEF/LISTEND */
static void test_static() {
   CHECK(1, "the static block counts the fields and hashes them like the registry",
         TD_Type_blk.h.n == 4 && TD_Type_blk.h.hash == ocs_cull_descr_hash(TD_Type, 4) &&
         TD_Type_blk.h.refs.load() == 0 && TD_Type == TD_Type_blk.e);
   const lDescr *dp = TD_Type;
   CHECK(2, "reference operations leave a static descriptor alone",
         !ocs_cull_descr_is_counted(dp) && ocs_cull_descr_ref(dp) == TD_Type && ocs_cull_descr_refs(dp) == 0);
   ocs_cull_descr_release(&dp);
   CHECK(3, "releasing a static descriptor only clears the caller's pointer",
         dp == nullptr && TD_Type_blk.h.refs.load() == 0 && TD_Type[0].nm == TD_id);
}

/** @brief DESCR-02: one descriptor per reduction */
static void test_intern() {
   const size_t base = ocs_cull_descr_registry_size();
   lDescr *built1 = heap_copy(RED_A);
   lDescr *built2 = heap_copy(RED_A);

   const lDescr *a1 = ocs_cull_descr_intern(built1);
   const lDescr *a2 = ocs_cull_descr_intern(built2);
   const lDescr *a3 = ocs_cull_descr_intern(a1);
   CHECK(4, "independently built equal reductions yield one counted descriptor",
         a1 != nullptr && a1 == a2 && a2 == a3 && a1 != built1 && ocs_cull_descr_is_counted(a1) &&
         ocs_cull_descr_refs(a1) == 3 && ocs_cull_descr_registry_size() == base + 1);
   CHECK(5, "the shared descriptor has the entries of its source",
         a1[0].nm == TD_id && (a1[0].mt & ~CULL_DESCR_INTERNED) == RED_A[0].mt && a1[1].nm == TD_name &&
         a1[1].mt == RED_A[1].mt && mt_get_type(a1[2].mt) == lEndT);

   const lDescr *b = ocs_cull_descr_intern(RED_B);
   const lDescr *s = ocs_cull_descr_intern(RED_A_SWAPPED);
   CHECK(6, "a different reduction and a different field order yield other descriptors",
         b != nullptr && s != nullptr && b != a1 && s != a1 && s != b && ocs_cull_descr_registry_size() == base + 3);

   const lDescr *none = ocs_cull_descr_intern(nullptr);
   CHECK(7, "interning nullptr yields nullptr", none == nullptr);

   ocs_cull_descr_release(&a1);
   ocs_cull_descr_release(&a2);
   ocs_cull_descr_release(&a3);
   ocs_cull_descr_release(&b);
   ocs_cull_descr_release(&s);
   sge_free(&built1);
   sge_free(&built2);
}

/** @brief DESCR-03: reference counts and release, also concurrently */
static void test_release() {
   const size_t base = ocs_cull_descr_registry_size();

   const lDescr *a = ocs_cull_descr_intern(RED_A);
   const lDescr *r = ocs_cull_descr_ref(a);
   ocs_cull_descr_release(&a);
   CHECK(8, "a descriptor stays registered while a reference is left",
         a == nullptr && r != nullptr && ocs_cull_descr_refs(r) == 1 && ocs_cull_descr_registry_size() == base + 1);
   ocs_cull_descr_release(&r);
   CHECK(9, "the last release removes the descriptor from the registry",
         r == nullptr && ocs_cull_descr_registry_size() == base);

   const lDescr *again = ocs_cull_descr_intern(RED_A);
   CHECK(10, "interning after the last release registers a new descriptor",
         again != nullptr && ocs_cull_descr_refs(again) == 1 && ocs_cull_descr_registry_size() == base + 1);

   // `again` keeps RED_A alive, so every thread must get exactly it; RED_B has
   // no keeper and dies and comes back many times while the threads race
   const int threads = 16;
   const int rounds = 20000;
   std::atomic<int> wrong{0};
   std::atomic<bool> go{false};
   std::vector<std::thread> workers;
   for (int t = 0; t < threads; t++) {
      workers.emplace_back([&, t] {
         while (!go.load(std::memory_order_acquire)) {
            std::this_thread::yield();
         }
         for (int i = 0; i < rounds; i++) {
            const lDescr *x = ocs_cull_descr_intern(RED_A);
            const lDescr *y = ocs_cull_descr_intern((i + t) % 2 == 0 ? RED_B : RED_A_SWAPPED);
            const lDescr *z = ocs_cull_descr_ref(y);
            if (x != again || y == nullptr || z != y || y[1].nm != ((i + t) % 2 == 0 ? TD_value : TD_id)) {
               wrong++;
            }
            ocs_cull_descr_release(&z);
            ocs_cull_descr_release(&y);
            ocs_cull_descr_release(&x);
         }
      });
   }
   go.store(true, std::memory_order_release);
   for (auto &w : workers) {
      w.join();
   }
   CHECK(11, "concurrent interning and release hand out the shared descriptor and the right entries",
         wrong.load() == 0);
   CHECK(12, "after the threads only the kept descriptor is left, with its one reference",
         ocs_cull_descr_refs(again) == 1 && ocs_cull_descr_registry_size() == base + 1);
   ocs_cull_descr_release(&again);
   CHECK(13, "the release of the kept descriptor empties the registry", ocs_cull_descr_registry_size() == base);
}

/** @brief DESCR-04: the soft cap */
static void test_soft_cap() {
   const size_t base = ocs_cull_descr_registry_size();
   const size_t prev = ocs_cull_descr_set_soft_cap(base + 2);

   const int count = 5;
   std::vector<const lDescr *> held;
   for (int n = 0; n < count; n++) {
      lDescr dp[3];
      distinct_reduction(n, dp);
      held.push_back(ocs_cull_descr_intern(dp));
   }
   lDescr last[3];
   distinct_reduction(count - 1, last);
   const lDescr *other = ocs_cull_descr_intern(last);

   bool all_counted = true;
   for (const lDescr *dp : held) {
      all_counted = all_counted && ocs_cull_descr_is_counted(dp);
   }
   CHECK(14, "above the soft cap descriptors are private counted copies and the registry stops growing",
         all_counted && ocs_cull_descr_registry_size() == base + 2 && other != held[count - 1] &&
         ocs_cull_descr_refs(other) == 1 && other[1].nm == held[count - 1][1].nm);

   lDescr first[3];
   distinct_reduction(0, first);
   const lDescr *shared = ocs_cull_descr_intern(first);
   CHECK(15, "descriptors registered below the cap are still shared", shared == held[0]);

   ocs_cull_descr_release(&shared);
   ocs_cull_descr_release(&other);
   for (const lDescr *dp : held) {
      ocs_cull_descr_release(&dp);
   }
   CHECK(16, "releasing everything shrinks the registry back", ocs_cull_descr_registry_size() == base);
   ocs_cull_descr_set_soft_cap(prev);
}

int main(int argc, char *argv[]) {
   lInit(nmv);

   return ocs::module_test::run(argc, argv, {
      {"DESCR-01", "static descriptor blocks",
       "the blocks built by LISTDEF/LISTEND carry a header and are not reference counted",
       {"read the header of a static block", "take and release a reference on it"},
       "the header holds the field count and the hash, and reference operations change nothing",
       {{"SHD-01", "partly"}}, {1, 2, 3}, test_static},
      {"DESCR-02", "one descriptor per reduction",
       "equal sequences of entries share one counted descriptor, different ones do not",
       {"intern two independently built copies of one reduction and the result itself",
        "intern another reduction and the first one in a different field order", "intern nullptr"},
       "equal reductions yield one pointer with one reference per call; the others yield other pointers",
       {{"SHD-02", "unit level"}}, {4, 5, 6, 7}, test_intern},
      {"DESCR-03", "reference counts and release",
       "a descriptor lives as long as its references, also when threads intern and release it at the same time",
       {"release references in turn", "intern again after the last release",
        "start 16 threads that intern, reference and release two reductions 20000 times, one of them kept alive"},
       "the entry exists while a reference exists and is gone after the last one; the threads always get the "
       "kept descriptor and correct entries",
       {{"SHD-03", "unit level"}}, {8, 9, 10, 11, 12, 13}, test_release},
      {"DESCR-04", "soft cap",
       "above the soft cap interning returns private copies and the registry does not grow",
       {"lower the soft cap", "intern more distinct reductions than it allows", "release everything"},
       "the private copies are correct and unshared, registered ones stay shared, and the registry shrinks back",
       {{"SHD-06", "unit level"}}, {14, 15, 16}, test_soft_cap},
   });
}
