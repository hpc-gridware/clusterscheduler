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
#include "cull/cull_pack.h"
#include "cull/ocs_cull_descr.h"

#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

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

/** @brief A spooled object type whose shorter and longer versions are read (N-W-6) */
enum {
   GR_id = 101, ///< ulong
   GR_name,     ///< string
   GR_value,    ///< ulong
   GR_flag      ///< bool
};

LISTDEF(GR_Type)
   SGE_ULONG(GR_id, CULL_SPOOL)
   SGE_STRING(GR_name, CULL_SPOOL)
   SGE_ULONG(GR_value, CULL_SPOOL)
   SGE_BOOL(GR_flag, CULL_SPOOL)
LISTEND(GR_Type)

NAMEDEF(GR_Name)
   NAME("GR_id")
   NAME("GR_name")
   NAME("GR_value")
   NAME("GR_flag")
NAMEEND

#define GR_Size sizeof(GR_Name) / sizeof(char *) ///< number of fields of GR_Type

/** @brief The name space registering the types with CULL */
lNameSpace nmv[] = {
   {1, TD_Size, TD_Name, TD_Type},
   {101, GR_Size, GR_Name, GR_Type},
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

   // a hash on the first field changes the entry that carries the reference count mark of the copy
   lList *g = lCreateList("first", GR_Type);
   const int first_ok = cull_hash_new(g, GR_id, true);
   CHECK(46, "a hash added on the first field leaves the copy counted, so that freeing the list frees it",
         first_ok == 1 && g->descr != GR_Type && ocs_cull_descr_is_counted(g->descr) &&
         mt_is_unique(g->descr[0].mt) && ocs_cull_descr_refs(g->descr) == 1);
   lFreeList(&g);
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

/**
 * @brief The first word of a packed descriptor
 * @param dp the descriptor
 * @param flags the flags to pack with
 * @return the first word, or 0 on an error
 */
static uint32_t first_word_of_packed(const lDescr *dp, const int flags) {
   uint32_t word = 0;
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 256, false, false) != PACK_SUCCESS) {
      return 0;
   }
   if (cull_pack_descr(&pb, dp, flags) == PACK_SUCCESS) {
      sge_pack_buffer in;
      char *buf = sge_malloc(pb.bytes_used);
      memcpy(buf, pb.head_ptr, pb.bytes_used);
      if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
         unpackint(&in, &word);
      }
      clear_packbuffer(&in);
   }
   clear_packbuffer(&pb);
   return word;
}

/**
 * @brief Unpack a list whose descriptor is a hand-written type reference
 * @param first_nm the first field number written into the reference
 * @param count the field count written into the reference
 * @return the result of cull_unpack_list()
 */
static int unpack_type_ref(const uint32_t first_nm, const uint32_t count) {
   int ret = PACK_FORMAT;
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 256, false, false) != PACK_SUCCESS) {
      return ret;
   }
   packint(&pb, 1);
   packstr(&pb, "forged");
   packint(&pb, CULL_PACK_DESCR_TYPE_REF);
   packint(&pb, first_nm);
   packint(&pb, count);
   packint(&pb, 0);
   sge_pack_buffer in;
   char *buf = sge_malloc(pb.bytes_used);
   memcpy(buf, pb.head_ptr, pb.bytes_used);
   if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
      lList *lp = nullptr;
      ret = cull_unpack_list(&in, &lp);
      lFreeList(&lp);
   }
   clear_packbuffer(&in);
   clear_packbuffer(&pb);
   return ret;
}

/** @brief DESCR-10: a full descriptor travels as a reference to its type */
static void test_type_ref() {
   const size_t base = ocs_cull_descr_registry_size();
   lDescr *heap = heap_copy(TD_Type);
   CHECK(32, "a full descriptor is packed as a type reference, a reduced one enters the table, a flag-filtered one "
         "goes field by field",
         first_word_of_packed(TD_Type, 0) == CULL_PACK_DESCR_TYPE_REF &&
         first_word_of_packed(heap, 0) == CULL_PACK_DESCR_TYPE_REF &&
         first_word_of_packed(TD_Type, CULL_SPOOL) == 4 && first_word_of_packed(RED_A, 0) == CULL_PACK_DESCR_TABLE_DEF);
   sge_free(&heap);

   lList *lp = make_list(3);
   lList *unpacked = pack_unpack(lp);
   CHECK(33, "the receiver takes the static block of the type and registers nothing",
         unpacked != nullptr && unpacked->descr == TD_Type && lGetNumberOfElem(unpacked) == 3 &&
         ocs_cull_descr_registry_size() == base);
   CHECK(34, "a type reference to an unknown type, with no field or more fields than the type is a format error",
         unpack_type_ref(9999, 4) == PACK_FORMAT && unpack_type_ref(TD_id, 5) == PACK_FORMAT &&
         unpack_type_ref(TD_id, 0) == PACK_FORMAT && unpack_type_ref(TD_id, 4) == PACK_SUCCESS &&
         unpack_type_ref(TD_id, 3) == PACK_SUCCESS);
   lFreeList(&unpacked);
   lFreeList(&lp);
}

/**
 * @brief Pack lists into one buffer and unpack them from it again
 * @param lists the lists, in the order to pack them
 * @param out the unpacked lists, in the same order
 * @param sent_entries set to the size of the sender's descriptor table
 * @param read_entries set to the size of the receiver's descriptor table
 * @param counted set to whether counting mode measures the same size
 * @return true if every list was packed and unpacked
 */
static bool pack_unpack_many(const std::vector<const lList *> &lists, std::vector<lList *> &out,
                             size_t *sent_entries, size_t *read_entries, bool *counted) {
   bool ok = true;
   sge_pack_buffer pb;
   sge_pack_buffer cnt;
   if (init_packbuffer(&pb, 1024, false, false) != PACK_SUCCESS || init_packbuffer(&cnt, 0, true, false) != PACK_SUCCESS) {
      return false;
   }
   const size_t header = pb.bytes_used;
   for (const lList *lp : lists) {
      ok = ok && cull_pack_list(&pb, lp) == PACK_SUCCESS && cull_pack_list(&cnt, lp) == PACK_SUCCESS;
   }
   *sent_entries = ocs_cull_pack_table_size(&pb);
   *counted = cnt.bytes_used == pb.bytes_used - header && ocs_cull_pack_table_size(&cnt) == *sent_entries;
   char *buf = sge_malloc(pb.bytes_used);
   memcpy(buf, pb.head_ptr, pb.bytes_used);
   sge_pack_buffer in;
   if (ok && init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
      for (size_t i = 0; i < lists.size(); i++) {
         lList *lp = nullptr;
         ok = ok && cull_unpack_list(&in, &lp) == PACK_SUCCESS;
         out.push_back(lp);
      }
      *read_entries = ocs_cull_pack_table_size(&in);
   } else {
      ok = false;
   }
   clear_packbuffer(&in);
   clear_packbuffer(&cnt);
   clear_packbuffer(&pb);
   return ok;
}

/** @brief DESCR-11: reduced descriptors go through the descriptor table of the buffer */
static void test_descr_table() {
   const size_t base = ocs_cull_descr_registry_size();
   lList *lp = make_list(4);
   lEnumeration *what_a = lWhat("%T(%I %I)", TD_Type, TD_id, TD_name);
   lEnumeration *what_b = lWhat("%T(%I %I)", TD_Type, TD_id, TD_value);
   lList *a1 = lSelect("a1", lp, nullptr, what_a);
   lList *a2 = lSelect("a2", lp, nullptr, what_a);
   lList *b1 = lSelect("b1", lp, nullptr, what_b);
   lFreeWhat(&what_b);
   lFreeWhat(&what_a);

   std::vector<lList *> out;
   size_t sent = 0;
   size_t read = 0;
   bool counted = false;
   const bool ok = pack_unpack_many({a1, lp, a2, b1, a2}, out, &sent, &read, &counted);
   CHECK(35, "each distinct reduction enters the table of the buffer once, on both sides, also when counting",
         ok && sent == 2 && read == 2 && counted);
   CHECK(36, "the unpacked lists share the descriptors of the sender's reductions and the full type keeps its block",
         ok && out[0]->descr == a1->descr && out[2]->descr == a1->descr && out[4]->descr == a1->descr &&
         out[3]->descr == b1->descr && out[1]->descr == TD_Type && lGetNumberOfElem(out[2]) == 4 &&
         lGetUlong(lFirst(out[3]), TD_value) == 0 && lGetString(lFirst(out[0]), TD_name) != nullptr);
   for (lList *l : out) {
      lFreeList(&l);
   }
   CHECK(37, "the buffers gave their table references up", ocs_cull_descr_refs(a1->descr) == 2);

   // an index the buffer has not defined yet is a format error
   sge_pack_buffer pb;
   int forged = PACK_SUCCESS;
   if (init_packbuffer(&pb, 256, false, false) == PACK_SUCCESS) {
      packint(&pb, 1);
      packstr(&pb, "forged");
      packint(&pb, CULL_PACK_DESCR_TABLE_REF);
      packint(&pb, 0);
      packint(&pb, 0);
      char *buf = sge_malloc(pb.bytes_used);
      memcpy(buf, pb.head_ptr, pb.bytes_used);
      sge_pack_buffer in;
      if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
         lList *l = nullptr;
         forged = cull_unpack_list(&in, &l);
         lFreeList(&l);
      }
      clear_packbuffer(&in);
      clear_packbuffer(&pb);
   }
   CHECK(38, "an index the buffer has not defined is a format error", forged == PACK_FORMAT);

   lFreeList(&b1);
   lFreeList(&a2);
   lFreeList(&a1);
   lFreeList(&lp);
   CHECK(39, "after the lists the registry is empty again", ocs_cull_descr_registry_size() == base);
}

/**
 * @brief Unpack a free element written by hand as an older or newer release would have packed it
 *
 * The element is followed by a trailer word, which must be the next thing read.
 *
 * @param fields the fields the sender's type has, each with a ulong, string or bool value
 * @param type_ref true for a type reference without flags, false for flag-filtered data packed field by field
 * @param expected the type passed to cull_unpack_elem_partial()
 * @param[out] out receives the element, owned by the caller
 * @param[out] trailer_ok set to whether the trailer followed the element
 * @return the result of cull_unpack_elem_partial()
 */
static int unpack_forged_elem(const std::vector<lDescr> &fields, const bool type_ref, const lDescr *expected,
                              lListElem **out, bool *trailer_ok) {
   constexpr uint32_t trailer = 0xABCD;
   int ret = PACK_FORMAT;
   *out = nullptr;
   *trailer_ok = false;
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 256, false, false) != PACK_SUCCESS) {
      return ret;
   }
   packint(&pb, elem_kind_to_wire(FREE_ELEM));
   if (type_ref) {
      packint(&pb, CULL_PACK_DESCR_TYPE_REF);
      packint(&pb, fields[0].nm);
      packint(&pb, fields.size());
   } else {
      packint(&pb, fields.size());
      for (const lDescr &f : fields) {
         packint(&pb, f.nm);
         packint(&pb, f.mt);
      }
   }
   for (size_t i = 0; i < fields.size(); i++) {
      switch (mt_get_type(fields[i].mt)) {
         case lStringT:
            packstr(&pb, "s");
            break;
         case lBoolT:
            packint(&pb, 1);
            break;
         default:
            packint(&pb, 7 + i);
            break;
      }
   }
   packint(&pb, trailer);
   sge_pack_buffer in;
   char *buf = sge_malloc(pb.bytes_used);
   memcpy(buf, pb.head_ptr, pb.bytes_used);
   if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
      ret = cull_unpack_elem_partial(&in, out, expected, type_ref ? 0 : CULL_SPOOL);
      uint32_t word = 0;
      *trailer_ok = ret == PACK_SUCCESS && unpackint(&in, &word) == PACK_SUCCESS && word == trailer;
   }
   clear_packbuffer(&in);
   clear_packbuffer(&pb);
   return ret;
}

/**
 * @brief Whether an element is a full GR_Type with the leading values of unpack_forged_elem() and initial ones after
 * @param ep the element
 * @param sent how many leading fields the sender had
 * @return true if the element looks as expected
 */
static bool grown_elem_ok(const lListElem *ep, const int sent) {
   return ep != nullptr && ep->descr == GR_Type && lGetUlong(ep, GR_id) == 7 &&
          sge_strnullcmp(lGetString(ep, GR_name), "s") == 0 &&
          lGetUlong(ep, GR_value) == (sent > 2 ? 9u : 0u) && lGetBool(ep, GR_flag) == (sent > 3);
}

/** @brief DESCR-12: types grown at the end stay readable (N-W-6) */
static void test_grown_types() {
   const size_t base = ocs_cull_descr_registry_size();
   const std::vector<lDescr> full(GR_Type, GR_Type + 4);
   const std::vector<lDescr> shorter(GR_Type, GR_Type + 2);
   std::vector<lDescr> longer = full;
   longer.push_back({GR_flag + 1, lUlongT | CULL_SPOOL});
   const std::vector<lDescr> gap = {GR_Type[0], GR_Type[2]};

   lListElem *ep = nullptr;
   bool trailer_a = false;
   bool trailer_b = false;
   const int ref_ret = unpack_forged_elem(shorter, true, nullptr, &ep, &trailer_a);
   bool ref_ok = ref_ret == PACK_SUCCESS && grown_elem_ok(ep, 2) && trailer_a;
   lFreeElem(&ep);
   const int spool_ret = unpack_forged_elem(shorter, false, GR_Type, &ep, &trailer_b);
   const bool spool_ok = spool_ret == PACK_SUCCESS && grown_elem_ok(ep, 2) && trailer_b;
   lFreeElem(&ep);
   CHECK(40, "a shorter type reference and a shorter spooled type unpack into the full type with initial values "
         "after the sent fields, and the data behind them is read next",
         ref_ok && spool_ok && ocs_cull_descr_registry_size() == base);

   unpack_forged_elem(full, false, GR_Type, &ep, &trailer_a);
   ref_ok = grown_elem_ok(ep, 4) && trailer_a;
   lFreeElem(&ep);
   CHECK(41, "the full type packed field by field is the static block", ref_ok);

   const int longer_ref = unpack_forged_elem(longer, true, nullptr, &ep, &trailer_a);
   lFreeElem(&ep);
   const int longer_spool = unpack_forged_elem(longer, false, nullptr, &ep, &trailer_a);
   lFreeElem(&ep);
   CHECK(42, "a type longer than the local one is a format error", longer_ref == PACK_FORMAT &&
         longer_spool == PACK_FORMAT);

   const int gap_expected = unpack_forged_elem(gap, false, GR_Type, &ep, &trailer_a);
   lFreeElem(&ep);
   const int other_expected = unpack_forged_elem(shorter, false, TD_Type, &ep, &trailer_a);
   lFreeElem(&ep);
   const int gap_free = unpack_forged_elem(gap, false, nullptr, &ep, &trailer_b);
   const bool gap_reduced = gap_free == PACK_SUCCESS && ep != nullptr && (ep->descr[0].mt & CULL_IS_REDUCED) != 0;
   lFreeElem(&ep);
   CHECK(43, "on the spool path a descriptor that is not a version of the expected type is a format error, "
         "without an expected type it stays a reduced element",
         gap_expected == PACK_FORMAT && other_expected == PACK_FORMAT && gap_reduced);

   // a selection of the leading fields is marked reduced by the sender and never taken for a shorter type
   lListElem *src = lCreateElem(GR_Type);
   lSetUlong(src, GR_id, 3);
   lSetString(src, GR_name, "n");
   const int sel[] = {GR_id, GR_name, NoName};
   lEnumeration *what = lIntVector2What(GR_Type, sel);
   lListElem *copy = nullptr;
   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 256, false, false) == PACK_SUCCESS) {
      if (cull_pack_elem_partial(&pb, src, what, CULL_SPOOL) == PACK_SUCCESS) {
         char *buf = sge_malloc(pb.bytes_used);
         memcpy(buf, pb.head_ptr, pb.bytes_used);
         sge_pack_buffer in;
         if (init_packbuffer_from_buffer(&in, buf, pb.bytes_used, false) == PACK_SUCCESS) {
            cull_unpack_elem_partial(&in, &copy, nullptr, CULL_SPOOL);
         }
         clear_packbuffer(&in);
      }
      clear_packbuffer(&pb);
   }
   CHECK(44, "a selection of the leading fields packed with flags stays a reduced element",
         copy != nullptr && lCountDescr(copy->descr) == 2 && (copy->descr[0].mt & CULL_IS_REDUCED) != 0 &&
         lGetUlong(copy, GR_id) == 3);
   lFreeElem(&copy);
   lFreeWhat(&what);
   lFreeElem(&src);
   CHECK(45, "after the elements the registry is empty again", ocs_cull_descr_registry_size() == base);
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
       {"add a hash on a field of one list", "look it up and dechain an element", "add a hash on the first field"},
       "the list and its elements hold the copy with the flag, the static block and other lists are unchanged, and "
       "a copy changed in its first field stays counted",
       {{"SHD-01", "partly"}}, {25, 26, 27, 46}, test_hash_new},
      {"DESCR-09", "builders and the selection cache",
       "a field selection builds its reduction once, and the builders return the shared descriptor",
       {"select twice with one selection", "reduce with lReduceDescr and lGetReducedDescr",
        "free the selection and the lists"},
       "the second select and the builders only take references, freeing the selection gives the cached one up, "
       "and the registry is empty at the end",
       {{"SHD-02", "partly"}}, {28, 29, 30, 31}, test_builders},
      {"DESCR-10", "type references on the wire",
       "a full descriptor is packed as a reference to its type and unpacked as the static block",
       {"pack a static, a heap copy, a flag-filtered and a reduced descriptor",
        "pack and unpack a list of a full type", "unpack forged type references"},
       "an unfiltered full descriptor becomes a type reference, the receiver gets the static block, and an "
       "unknown type, no field or more fields than the type is a format error",
       {{"SHD-01", "partly"}}, {32, 33, 34}, test_type_ref},
      {"DESCR-11", "descriptor table of a buffer",
       "reduced descriptors are written once per buffer and referenced by index afterwards",
       {"pack two lists of one reduction, a full list and a list of another reduction into one buffer, also counting",
        "unpack them", "unpack a forged index"},
       "both tables hold one entry per distinct reduction, counting measures the same size, the unpacked lists "
       "share the descriptors, and an undefined index is a format error",
       {{"SHD-04", "unit level"}}, {35, 36, 37, 38, 39}, test_descr_table},
      {"DESCR-12", "types grown at the end",
       "data packed by a release whose type had fewer fields at the end is read into the full local type",
       {"unpack a shorter type reference and shorter spooled data, each followed by more data",
        "unpack the full type packed field by field", "unpack a longer type both ways",
        "unpack spooled data that is not a version of the expected type, with and without it",
        "pack a selection of the leading fields with flags and unpack it"},
       "shorter data becomes full elements with initial values in the added fields and the data behind it is read "
       "next, longer data and spooled data of another shape are format errors, and a selection stays reduced",
       {{"SHD-09", "unit level"}}, {40, 41, 42, 43, 44, 45}, test_grown_types},
   });
}
