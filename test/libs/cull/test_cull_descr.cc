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
 * Since M3-3 the lists and elements of the library hold shared descriptors;
 * DESCR-05 to DESCR-08 check that through the list API.
 *
 * DESCR-03 is meant to be run under the thread and the address sanitizer as well.
 */

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

/** @brief Make this translation unit the one that defines the CULL field names */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"
#include "cull/cull_hash.h"
#include "cull/cull_listP.h"
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

/**
 * @brief A list of n elements of TD_Type with ids 0 .. n-1 and names "n<id>"
 * @param n the number of elements
 * @return the list
 */
static lList *make_list(const int n) {
   lList *lp = lCreateList("descr", TD_Type);
   for (int i = 0; i < n; i++) {
      lListElem *ep = lAddElemUlong(&lp, TD_id, i, TD_Type);
      lSetString(ep, TD_name, ("n" + std::to_string(i)).c_str());
      lSetUlong(ep, TD_value, 2 * i);
   }
   return lp;
}

/**
 * @brief Pack a list and unpack it again, as a receiver does
 * @param lp the list
 * @return the unpacked list, or nullptr on an error
 */
static lList *pack_unpack(const lList *lp) {
   lList *result = nullptr;
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 1024, false, false) != PACK_SUCCESS) {
      return nullptr;
   }
   if (cull_pack_list(&pb, lp) == PACK_SUCCESS) {
      char *buf = sge_malloc(pb.bytes_used);
      memcpy(buf, pb.head_ptr, pb.bytes_used);
      sge_pack_buffer in;
      if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
         cull_unpack_list(&in, &result);
      }
      clear_packbuffer(&in);
   }
   clear_packbuffer(&pb);
   return result;
}

/**
 * @brief Whether every element of a list borrows the descriptor of the list
 * @param lp the list
 * @return true if all elements point at the descriptor of the list
 */
static bool elements_borrow(const lList *lp) {
   for_each_ep_lv(ep, lp) {
      if (ep->descr != lp->descr) {
         return false;
      }
   }
   return true;
}

/** @brief DESCR-05: full descriptors are the static block */
static void test_full_static() {
   const size_t base = ocs_cull_descr_registry_size();
   lList *lp = make_list(4);
   lListElem *free_ep = lCreateElem(TD_Type);
   lDescr *heap = heap_copy(TD_Type);
   lList *from_copy = lCreateList("from copy", heap);
   sge_free(&heap);
   lList *copied = lCopyList("copied", lp);
   lList *unpacked = pack_unpack(lp);
   lListElem *dechained = lDechainElem(lp, lFirstRW(lp));

   CHECK(17, "created, copied, unpacked lists and free elements of a full type use its static block",
         lp->descr == TD_Type && free_ep->descr == TD_Type && from_copy->descr == TD_Type &&
         copied->descr == TD_Type && unpacked != nullptr && unpacked->descr == TD_Type &&
         dechained->descr == TD_Type);
   CHECK(18, "bound elements borrow the static block and nothing is registered",
         elements_borrow(lp) && elements_borrow(copied) && elements_borrow(unpacked) &&
         ocs_cull_descr_registry_size() == base);

   lFreeElem(&dechained);
   lFreeElem(&free_ep);
   lFreeList(&unpacked);
   lFreeList(&copied);
   lFreeList(&from_copy);
   lFreeList(&lp);
}

/** @brief DESCR-06: equal reductions share one descriptor across builders */
static void test_reduced_shared() {
   const size_t base = ocs_cull_descr_registry_size();
   lList *lp = make_list(8);
   lEnumeration *what1 = lWhat("%T(%I %I)", TD_Type, TD_id, TD_name);
   lEnumeration *what2 = lWhat("%T(%I %I)", TD_Type, TD_name, TD_id);
   lList *sel1 = lSelect("sel1", lp, nullptr, what1);
   lList *sel2 = lSelect("sel2", lp, nullptr, what2);
   lList *unpacked = pack_unpack(sel1);
   lListElem *ep = lCreateElem(lGetListDescr(sel1));
   lEnumeration *what3 = lWhat("%T(%I %I)", TD_Type, TD_id, TD_value);
   lList *other = lSelect("other", lp, nullptr, what3);
   // a selection caches its reduction with a reference (DESCR-09); free them to count only lists and elements
   lFreeWhat(&what3);
   lFreeWhat(&what2);
   lFreeWhat(&what1);

   CHECK(19, "selected lists of one reduction, in either field order, share one counted descriptor",
         sel1 != nullptr && sel2 != nullptr && sel1->descr == sel2->descr && ocs_cull_descr_is_counted(sel1->descr) &&
         elements_borrow(sel1) && elements_borrow(sel2));
   CHECK(20, "the unpacked list and a free element of that reduction share it as well",
         unpacked != nullptr && unpacked->descr == sel1->descr && ep->descr == sel1->descr &&
         ocs_cull_descr_refs(sel1->descr) == 4);
   CHECK(21, "a different reduction gets a descriptor of its own",
         other != nullptr && other->descr != sel1->descr && ocs_cull_descr_registry_size() == base + 2);

   lFreeList(&other);
   lFreeElem(&ep);
   lFreeList(&unpacked);
   lFreeList(&sel2);
   lFreeList(&sel1);
   lFreeList(&lp);
   CHECK(22, "freeing the lists and elements of the reductions empties the registry",
         ocs_cull_descr_registry_size() == base);
}

/** @brief DESCR-07: users of a reduced descriptor in all orders */
static void test_users_release() {
   const size_t base = ocs_cull_descr_registry_size();
   lList *lp = make_list(6);
   lEnumeration *what = lWhat("%T(%I %I)", TD_Type, TD_id, TD_value);
   lList *sel = lSelect("sel", lp, nullptr, what);
   // the selection caches its reduction with a reference (DESCR-09); free it to count only lists and elements
   lFreeWhat(&what);
   const lDescr *dp = lGetListDescr(sel);

   // a dechained element takes a reference, an appended one gives it up
   lListElem *ep = lDechainElem(sel, lFirstRW(sel));
   const bool after_dechain = ocs_cull_descr_refs(dp) == 2 && ep->descr == dp;
   lList *second = lCreateList("second", dp);
   lAppendElem(second, ep);
   const bool after_append = ocs_cull_descr_refs(dp) == 2 && ep->descr == second->descr;
   ep = lDechainElem(second, lFirstRW(second));
   lFreeList(&sel);
   const bool after_list_freed = ocs_cull_descr_refs(dp) == 2 && ocs_cull_descr_registry_size() == base + 1;
   lFreeList(&second);
   const bool after_second_freed = ocs_cull_descr_refs(dp) == 1;
   CHECK(23, "lists hold one reference each, free elements one, bound elements none",
         after_dechain && after_append && after_list_freed && after_second_freed);
   lFreeElem(&ep);
   CHECK(24, "the last user frees the descriptor", ocs_cull_descr_registry_size() == base);

   lFreeList(&lp);
}

/** @brief DESCR-08: a hash added to one list does not change other lists */
static void test_hash_new() {
   const size_t base = ocs_cull_descr_registry_size();
   lList *a = make_list(4 * OCS_CULL_HASH_MIN_ELEMS);
   lList *b = make_list(4);
   const int static_mt = TD_Type[1].mt;
   const int ok = cull_hash_new(a, TD_name, false);

   CHECK(25, "the list gets a private counted copy with the hash flag, its elements follow it",
         ok == 1 && a->descr != TD_Type && ocs_cull_descr_is_counted(a->descr) && mt_do_hashing(a->descr[1].mt) &&
         elements_borrow(a) && ocs_cull_descr_registry_size() == base);
   CHECK(26, "the static block and the other list are unchanged",
         TD_Type[1].mt == static_mt && !mt_do_hashing(TD_Type[1].mt) && b->descr == TD_Type);
   const lListElem *found = lGetElemStr(a, TD_name, "n7");
   lListElem *ep = lDechainElem(a, lFirstRW(a));
   CHECK(27, "the added hash answers lookups, and a dechained element keeps the copy",
         found != nullptr && lGetUlong(found, TD_id) == 7 && list_hash_table(a, 1) != nullptr &&
         ep->descr == a->descr && ocs_cull_descr_refs(a->descr) == 2);

   lFreeElem(&ep);
   lFreeList(&b);
   lFreeList(&a);
}

/** @brief DESCR-09: builders and the selection cache hand out shared descriptors */
static void test_builders() {
   const size_t base = ocs_cull_descr_registry_size();
   lList *lp = make_list(8);
   lEnumeration *what = lWhat("%T(%I %I)", TD_Type, TD_value, TD_id);

   lList *sel1 = lSelect("sel1", lp, nullptr, what);
   const lDescr *dp = lGetListDescr(sel1);
   const uint32_t refs_one = ocs_cull_descr_refs(dp);
   lList *sel2 = lSelect("sel2", lp, nullptr, what);
   CHECK(28, "a selection caches its reduction: a second select adds only the reference of its list",
         refs_one == 2 && lGetListDescr(sel2) == dp && ocs_cull_descr_refs(dp) == 3 &&
         ocs_cull_descr_registry_size() == base + 1);

   const lDescr *reduced = nullptr;
   const int rc = lReduceDescr(&reduced, TD_Type, what);
   const lDescr *got = lGetReducedDescr(TD_Type, what);
   CHECK(29, "lReduceDescr and lGetReducedDescr return the shared reduction with a reference each",
         rc == 0 && reduced == dp && got == dp && ocs_cull_descr_refs(dp) == 5);
   ocs_cull_descr_release(&reduced);
   ocs_cull_descr_release(&got);

   lFreeWhat(&what);
   CHECK(30, "freeing the selection gives the cached reference up", ocs_cull_descr_refs(dp) == 2);

   lFreeList(&sel2);
   lFreeList(&sel1);
   lFreeList(&lp);
   CHECK(31, "after the last list the registry is empty again", ocs_cull_descr_registry_size() == base);
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
      {"DESCR-05", "full descriptors are the static block",
       "lists and elements of a full type use the static block of the type instead of a copy",
       {"create, copy, unpack and dechain lists and elements of a full type", "create a list from a heap copy of it"},
       "every descriptor is the static block, bound elements borrow it, nothing is registered",
       {{"SHD-01", "covers"}}, {17, 18}, test_full_static},
      {"DESCR-06", "one descriptor per reduction across builders",
       "selected, unpacked and created lists and elements of one reduction share one descriptor",
       {"select one reduction twice, with the fields in two orders", "unpack one of the results",
        "create a free element with its descriptor", "select a different reduction", "free everything"},
       "equal reductions share one counted descriptor with one reference per list and free element, a different one "
       "has its own, and the registry is empty again at the end",
       {{"SHD-02", "partly"}}, {19, 20, 21, 22}, test_reduced_shared},
      {"DESCR-07", "users of a descriptor in all orders",
       "lists, free and bound elements of a reduction hold and give up their references",
       {"dechain an element", "append it to a second list", "dechain it again",
        "free the lists before the element"},
       "lists and free elements hold one reference each, bound ones none, and the last user frees the descriptor",
       {{"SHD-03", "covers"}}, {23, 24}, test_users_release},
      {"DESCR-08", "added hash stays in its list",
       "cull_hash_new() gives the list a private copy of the descriptor instead of changing the shared one",
       {"add a hash on a field of one list", "look it up and dechain an element"},
       "the list and its elements hold the copy with the flag, the static block and other lists are unchanged",
       {{"SHD-01", "partly"}}, {25, 26, 27}, test_hash_new},
      {"DESCR-09", "builders and the selection cache",
       "a field selection builds its reduction once, and the builders return the shared descriptor",
       {"select twice with one selection", "reduce with lReduceDescr and lGetReducedDescr",
        "free the selection and the lists"},
       "the second select and the builders only take references, freeing the selection gives the cached one up, "
       "and the registry is empty at the end",
       {{"SHD-02", "partly"}}, {28, 29, 30, 31}, test_builders},
   });
}
