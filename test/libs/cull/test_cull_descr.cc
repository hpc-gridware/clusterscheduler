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
 * DESCR-05 to DESCR-08 check that through the raw interface of TOM and the
 * list core.
 *
 * DESCR-03 is meant to be run under the thread and the address sanitizer as well.
 */

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <utility>
#include <vector>

/** @brief Make this translation unit the one that defines the CULL field names */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/detail/ocs_MultitypeP.h"
#include "tom/ocs_Descr.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Pack.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SelectCore.h"

#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "ocs_module_test.h"
#include "ocs_test_model.h"

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

/** @brief A type the installed object model does not hold */
enum {
   UR_id = 301, ///< ulong
   UR_text      ///< string
};

LISTDEF(UR_Type)
   SGE_ULONG(UR_id, CULL_DEFAULT)
   SGE_STRING(UR_text, CULL_DEFAULT)
LISTEND(UR_Type)

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
 *
 * The elements are added with add_elem_ulong() of the core, as lAddElemUlong() added them.
 *
 * @param n the number of elements
 * @return the owner of the list
 */
static ocs::tom::RawListPtr make_list(const int n) {
   auto owner = ocs::tom::RawListPtr::create(TD_Type, ocs::tom::Hashing::On, "descr");
   lList *lp = owner.get();
   for (int i = 0; i < n; i++) {
      const ocs::tom::RawElement ep = ocs::tom::add_elem_ulong(lp, TD_id, static_cast<lUlong>(i), TD_Type);
      ep.set(TD_name, ("n" + std::to_string(i)).c_str());
      ep.set(TD_value, static_cast<std::uint32_t>(2 * i));
   }
   return owner;
}

/**
 * @brief The selection of some fields of a type
 * @param descr the descriptor of the type
 * @param fields the fields, in any order
 * @return the owner of the selection
 */
static ocs::tom::RawWhatPtr what_of(const lDescr *descr, const std::initializer_list<int> fields) {
   return ocs::tom::what_of(descr, std::span<const int>(fields.begin(), fields.size()));
}

/**
 * @brief Select all elements of a list reduced to a selection, as lSelect() did
 * @param name the name of the result
 * @param lp the list
 * @param what the selection
 * @return the owner of the result, empty for no element or an error
 */
static ocs::tom::RawListPtr select_old(const char *name, const lList *lp, const lEnumeration *what) {
   return ocs::tom::select_as(name, lp, nullptr, what, ocs::tom::Hashing::On);
}

/**
 * @brief The descriptor of a list
 * @param lp the list, may be empty
 * @return the descriptor, nullptr for no list
 */
static const lDescr *descr_of(const ocs::tom::ConstRawList lp) {
   return lp ? lp.descr() : nullptr;
}

/**
 * @brief A buffer to read the bytes of another one from, as a receiver gets them
 * @param pb the buffer packed into
 * @return the buffer to read from
 * @throw ocs::tom::PackException if the header cannot be read
 */
static ocs::tom::PackBuffer reader_of(ocs::tom::PackBuffer &pb) {
   return ocs::tom::PackBuffer::reading(pb.data(), pb.size(), false);
}

/**
 * @brief Unpack a list from the bytes of a buffer and free it again
 * @param pb the buffer packed into
 * @return PACK_SUCCESS, or the code of the PackException the unpack threw
 */
static int unpack_and_drop_list(ocs::tom::PackBuffer &pb) {
   try {
      ocs::tom::PackBuffer in = reader_of(pb);
      static_cast<void>(ocs::tom::unpack_raw_list(&in.raw(), 0));
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Pack a list and unpack it again, as a receiver does
 * @param lp the list
 * @return the owner of the unpacked list, empty on an error
 */
static ocs::tom::RawListPtr pack_unpack(const lList *lp) {
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::pack_list(&pb.raw(), lp, nullptr, 0);
      ocs::tom::PackBuffer in = reader_of(pb);
      return ocs::tom::unpack_raw_list(&in.raw(), 0);
   } catch (const ocs::tom::PackException &) {
      return {};
   }
}

/**
 * @brief Whether every element of a list borrows the descriptor of the list
 * @param lp the list
 * @return true if all elements point at the descriptor of the list
 */
static bool elements_borrow(const ocs::tom::ConstRawList lp) {
   for (const ocs::tom::ConstRawElement ep : lp) {
      if (ep.descr() != lp.descr()) {
         return false;
      }
   }
   return true;
}

/** @brief DESCR-05: full descriptors are the static block */
static void test_full_static() {
   const size_t base = ocs_cull_descr_registry_size();
   ocs::tom::RawListPtr lp = make_list(4);
   auto free_ep = ocs::tom::RawElementPtr::create(TD_Type);
   lDescr *heap = heap_copy(TD_Type);
   auto from_copy = ocs::tom::RawListPtr::create(heap, ocs::tom::Hashing::On, "from copy");
   sge_free(&heap);
   ocs::tom::RawListPtr copied = lp->clone();
   ocs::tom::RawListPtr unpacked = pack_unpack(lp.get());
   ocs::tom::RawElementPtr dechained = lp->take(lp->front());

   CHECK(17, "created, copied, unpacked lists and free elements of a full type use its static block",
         lp->descr() == TD_Type && free_ep->descr() == TD_Type && from_copy->descr() == TD_Type &&
         copied->descr() == TD_Type && unpacked && unpacked->descr() == TD_Type &&
         dechained->descr() == TD_Type);
   CHECK(18, "bound elements borrow the static block and nothing is registered",
         elements_borrow(lp.get()) && elements_borrow(copied.get()) && elements_borrow(unpacked.get()) &&
         ocs_cull_descr_registry_size() == base);
}

/** @brief DESCR-06: equal reductions share one descriptor across builders */
static void test_reduced_shared() {
   const size_t base = ocs_cull_descr_registry_size();
   ocs::tom::RawListPtr lp = make_list(8);
   ocs::tom::RawWhatPtr what1 = what_of(TD_Type, {TD_id, TD_name});
   ocs::tom::RawWhatPtr what2 = what_of(TD_Type, {TD_name, TD_id});
   ocs::tom::RawListPtr sel1 = select_old("sel1", lp.get(), what1.get());
   ocs::tom::RawListPtr sel2 = select_old("sel2", lp.get(), what2.get());
   ocs::tom::RawListPtr unpacked = pack_unpack(sel1.get());
   auto ep = ocs::tom::RawElementPtr::create(descr_of(sel1.get()));
   ocs::tom::RawWhatPtr what3 = what_of(TD_Type, {TD_id, TD_value});
   ocs::tom::RawListPtr other = select_old("other", lp.get(), what3.get());
   // a selection caches its reduction with a reference (DESCR-09); free them to count only lists and elements
   what3.reset();
   what2.reset();
   what1.reset();

   CHECK(19, "selected lists of one reduction, in either field order, share one counted descriptor",
         sel1 && sel2 && sel1->descr() == sel2->descr() && ocs_cull_descr_is_counted(sel1->descr()) &&
         elements_borrow(sel1.get()) && elements_borrow(sel2.get()));
   CHECK(20, "the unpacked list and a free element of that reduction share it as well",
         unpacked && unpacked->descr() == sel1->descr() && ep->descr() == sel1->descr() &&
         ocs_cull_descr_refs(sel1->descr()) == 4);
   CHECK(21, "a different reduction gets a descriptor of its own",
         other && other->descr() != sel1->descr() && ocs_cull_descr_registry_size() == base + 2);

   other.reset();
   ep.reset();
   unpacked.reset();
   sel2.reset();
   sel1.reset();
   lp.reset();
   CHECK(22, "freeing the lists and elements of the reductions empties the registry",
         ocs_cull_descr_registry_size() == base);
}

/** @brief DESCR-07: users of a reduced descriptor in all orders */
static void test_users_release() {
   const size_t base = ocs_cull_descr_registry_size();
   ocs::tom::RawListPtr lp = make_list(6);
   ocs::tom::RawWhatPtr what = what_of(TD_Type, {TD_id, TD_value});
   ocs::tom::RawListPtr sel = select_old("sel", lp.get(), what.get());
   // the selection caches its reduction with a reference (DESCR-09); free it to count only lists and elements
   what.reset();
   const lDescr *dp = descr_of(sel.get());

   // a taken element takes a reference, an appended one gives it up
   ocs::tom::RawElementPtr ep = sel->take(sel->front());
   const bool after_dechain = ocs_cull_descr_refs(dp) == 2 && ep->descr() == dp;
   auto second = ocs::tom::RawListPtr::create(dp, ocs::tom::Hashing::On, "second");
   const ocs::tom::RawElement bound = second->push_back(std::move(ep));
   const bool after_append = ocs_cull_descr_refs(dp) == 2 && bound.descr() == second->descr();
   ep = second->take(second->front());
   sel.reset();
   const bool after_list_freed = ocs_cull_descr_refs(dp) == 2 && ocs_cull_descr_registry_size() == base + 1;
   second.reset();
   const bool after_second_freed = ocs_cull_descr_refs(dp) == 1;
   CHECK(23, "lists hold one reference each, free elements one, bound elements none",
         after_dechain && after_append && after_list_freed && after_second_freed);
   ep.reset();
   CHECK(24, "the last user frees the descriptor", ocs_cull_descr_registry_size() == base);
}

/** @brief DESCR-08: a hash added to one list does not change other lists */
static void test_hash_new() {
   const size_t base = ocs_cull_descr_registry_size();
   ocs::tom::RawListPtr a_owner = make_list(4 * OCS_CULL_HASH_MIN_ELEMS);
   ocs::tom::RawListPtr b_owner = make_list(4);
   lList *a = a_owner.get();
   const lList *b = b_owner.get();
   const int static_mt = TD_Type[1].mt;
   const int ok = cull_hash_new(a, TD_name, false);

   CHECK(25, "the list gets a private counted copy with the hash flag, its elements follow it",
         ok == 1 && a->descr != TD_Type && ocs_cull_descr_is_counted(a->descr) && mt_do_hashing(a->descr[1].mt) &&
         elements_borrow(a) && ocs_cull_descr_registry_size() == base);
   CHECK(26, "the static block and the other list are unchanged",
         TD_Type[1].mt == static_mt && !mt_do_hashing(TD_Type[1].mt) && b->descr == TD_Type);
   const ocs::tom::ConstRawElement found = a_owner->find_first(TD_name, static_cast<const char *>("n7"));
   ocs::tom::RawElementPtr ep = a_owner->take(a_owner->front());
   CHECK(27, "the added hash answers lookups, and a taken element keeps the copy",
         found && found.get<std::uint32_t>(TD_id) == 7 && list_hash_table(a, 1) != nullptr &&
         ep->descr() == a->descr && ocs_cull_descr_refs(a->descr) == 2);

   ep.reset();
   b_owner.reset();
   a_owner.reset();

   // a hash on the first field changes the entry that carries the reference count mark of the copy
   auto g_owner = ocs::tom::RawListPtr::create(GR_Type, ocs::tom::Hashing::On, "first");
   lList *g = g_owner.get();
   const int first_ok = cull_hash_new(g, GR_id, true);
   CHECK(46, "a hash added on the first field leaves the copy counted, so that freeing the list frees it",
         first_ok == 1 && g->descr != GR_Type && ocs_cull_descr_is_counted(g->descr) &&
         mt_is_unique(g->descr[0].mt) && ocs_cull_descr_refs(g->descr) == 1);
}

/** @brief DESCR-09: builders and the selection cache hand out shared descriptors */
static void test_builders() {
   const size_t base = ocs_cull_descr_registry_size();
   ocs::tom::RawListPtr lp = make_list(8);
   ocs::tom::RawWhatPtr what = what_of(TD_Type, {TD_value, TD_id});

   ocs::tom::RawListPtr sel1 = select_old("sel1", lp.get(), what.get());
   const lDescr *dp = descr_of(sel1.get());
   const uint32_t refs_one = ocs_cull_descr_refs(dp);
   ocs::tom::RawListPtr sel2 = select_old("sel2", lp.get(), what.get());
   CHECK(28, "a selection caches its reduction: a second select adds only the reference of its list",
         refs_one == 2 && descr_of(sel2.get()) == dp && ocs_cull_descr_refs(dp) == 3 &&
         ocs_cull_descr_registry_size() == base + 1);

   // lReduceDescr() and lGetReducedDescr() both gave the shared reduction; reduced_descr() is their core
   const lDescr *reduced = ocs::tom::reduced_descr(TD_Type, what.get());
   const lDescr *got = ocs::tom::reduced_descr(TD_Type, what.get());
   CHECK(29, "reduced_descr() returns the shared reduction with a reference each",
         reduced == dp && got == dp && ocs_cull_descr_refs(dp) == 5);
   ocs_cull_descr_release(&reduced);
   ocs_cull_descr_release(&got);

   what.reset();
   CHECK(30, "freeing the selection gives the cached reference up", ocs_cull_descr_refs(dp) == 2);

   sel2.reset();
   sel1.reset();
   lp.reset();
   CHECK(31, "after the last list the registry is empty again", ocs_cull_descr_registry_size() == base);
}

/**
 * @brief The first word of a packed descriptor
 * @param dp the descriptor
 * @param flags the flags to pack with
 * @return the first word, or 0 on an error
 */
static uint32_t first_word_of_packed(const lDescr *dp, const int flags) {
   try {
      ocs::tom::PackBuffer pb(false, 256);
      ocs::tom::pack_descr(&pb.raw(), dp, flags);
      ocs::tom::PackBuffer in = reader_of(pb);
      return ocs::tom::get_uint32(&in.raw());
   } catch (const ocs::tom::PackException &) {
      return 0;
   }
}

/**
 * @brief Unpack a list whose descriptor is a hand-written type reference
 * @param first_nm the first field number written into the reference
 * @param count the field count written into the reference
 * @return the result of unpack_raw_list(): PACK_SUCCESS or the code it threw
 */
static int unpack_type_ref(const uint32_t first_nm, const uint32_t count) {
   try {
      ocs::tom::PackBuffer pb(false, 256);
      ocs::tom::put_uint32(&pb.raw(), 1);
      ocs::tom::put_str(&pb.raw(), "forged");
      ocs::tom::put_uint32(&pb.raw(), CULL_PACK_DESCR_TYPE_REF);
      ocs::tom::put_uint32(&pb.raw(), first_nm);
      ocs::tom::put_uint32(&pb.raw(), count);
      ocs::tom::put_uint32(&pb.raw(), 0);
      return unpack_and_drop_list(pb);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
}

/** @brief DESCR-10: a full descriptor travels as a reference to its type */
static void test_type_ref() {
   const size_t base = ocs_cull_descr_registry_size();
   lDescr *heap = heap_copy(TD_Type);
   const lDescr *reduced = ocs_cull_descr_reduce(TD_Type, {TD_id, TD_name});
   CHECK(32, "a full descriptor is packed as a type reference, a reduced one enters the table, a flag-filtered one "
         "goes field by field",
         first_word_of_packed(TD_Type, 0) == CULL_PACK_DESCR_TYPE_REF &&
         first_word_of_packed(heap, 0) == CULL_PACK_DESCR_TYPE_REF &&
         first_word_of_packed(TD_Type, CULL_SPOOL) == 4 &&
         first_word_of_packed(reduced, 0) == CULL_PACK_DESCR_TABLE_DEF);
   ocs_cull_descr_release(&reduced);
   sge_free(&heap);

   ocs::tom::RawListPtr lp = make_list(3);
   ocs::tom::RawListPtr unpacked = pack_unpack(lp.get());
   CHECK(33, "the receiver takes the static block of the type and registers nothing",
         unpacked && unpacked->descr() == TD_Type && unpacked->size() == 3 &&
         ocs_cull_descr_registry_size() == base);
   CHECK(34, "a type reference to an unknown type, with no field or more fields than the type is a format error",
         unpack_type_ref(9999, 4) == PACK_FORMAT && unpack_type_ref(TD_id, 5) == PACK_FORMAT &&
         unpack_type_ref(TD_id, 0) == PACK_FORMAT && unpack_type_ref(TD_id, 4) == PACK_SUCCESS &&
         unpack_type_ref(TD_id, 3) == PACK_SUCCESS);
}

/**
 * @brief Pack lists into one buffer and unpack them from it again
 * @param lists the lists, in the order to pack them
 * @param out the owners of the unpacked lists, in the same order
 * @param sent_entries set to the size of the sender's descriptor table
 * @param read_entries set to the size of the receiver's descriptor table
 * @param counted set to whether counting mode measures the same size
 * @return true if every list was packed and unpacked
 */
static bool pack_unpack_many(const std::vector<const lList *> &lists, std::vector<ocs::tom::RawListPtr> &out,
                             size_t *sent_entries, size_t *read_entries, bool *counted) {
   try {
      ocs::tom::PackBuffer pb(false, 1024);
      ocs::tom::PackBuffer cnt = ocs::tom::PackBuffer::counting();
      const size_t header = pb.size();
      for (const lList *lp : lists) {
         ocs::tom::pack_list(&pb.raw(), lp, nullptr, 0);
         ocs::tom::pack_list(&cnt.raw(), lp, nullptr, 0);
      }
      *sent_entries = ocs::tom::descr_table_size(&pb.raw());
      *counted = cnt.size() == pb.size() - header && ocs::tom::descr_table_size(&cnt.raw()) == *sent_entries;
      ocs::tom::PackBuffer in = reader_of(pb);
      for (size_t i = 0; i < lists.size(); i++) {
         out.push_back(ocs::tom::unpack_raw_list(&in.raw(), 0));
      }
      *read_entries = ocs::tom::descr_table_size(&in.raw());
   } catch (const ocs::tom::PackException &) {
      return false;
   }
   return true;
}

/** @brief DESCR-11: reduced descriptors go through the descriptor table of the buffer */
static void test_descr_table() {
   const size_t base = ocs_cull_descr_registry_size();
   ocs::tom::RawListPtr lp = make_list(4);
   ocs::tom::RawWhatPtr what_a = what_of(TD_Type, {TD_id, TD_name});
   ocs::tom::RawWhatPtr what_b = what_of(TD_Type, {TD_id, TD_value});
   ocs::tom::RawListPtr a1 = select_old("a1", lp.get(), what_a.get());
   ocs::tom::RawListPtr a2 = select_old("a2", lp.get(), what_a.get());
   ocs::tom::RawListPtr b1 = select_old("b1", lp.get(), what_b.get());
   what_b.reset();
   what_a.reset();

   std::vector<ocs::tom::RawListPtr> out;
   size_t sent = 0;
   size_t read = 0;
   bool counted = false;
   const bool ok = pack_unpack_many({a1.get(), lp.get(), a2.get(), b1.get(), a2.get()}, out, &sent, &read, &counted);
   CHECK(35, "each distinct reduction enters the table of the buffer once, on both sides, also when counting",
         ok && sent == 2 && read == 2 && counted);
   CHECK(36, "the unpacked lists share the descriptors of the sender's reductions and the full type keeps its block",
         ok && out[0]->descr() == a1->descr() && out[2]->descr() == a1->descr() && out[4]->descr() == a1->descr() &&
         out[3]->descr() == b1->descr() && out[1]->descr() == TD_Type && out[2]->size() == 4 &&
         out[3]->front().get<std::uint32_t>(TD_value) == 0 && out[0]->front().get<const char *>(TD_name) != nullptr);
   out.clear();
   CHECK(37, "the buffers gave their table references up", ocs_cull_descr_refs(a1->descr()) == 2);

   // an index the buffer has not defined yet is a format error
   int forged = PACK_SUCCESS;
   try {
      ocs::tom::PackBuffer pb(false, 256);
      ocs::tom::put_uint32(&pb.raw(), 1);
      ocs::tom::put_str(&pb.raw(), "forged");
      ocs::tom::put_uint32(&pb.raw(), CULL_PACK_DESCR_TABLE_REF);
      ocs::tom::put_uint32(&pb.raw(), 0);
      ocs::tom::put_uint32(&pb.raw(), 0);
      forged = unpack_and_drop_list(pb);
   } catch (const ocs::tom::PackException &e) {
      forged = e.code();
   }
   CHECK(38, "an index the buffer has not defined is a format error", forged == PACK_FORMAT);

   b1.reset();
   a2.reset();
   a1.reset();
   lp.reset();
   CHECK(39, "after the lists the registry is empty again", ocs_cull_descr_registry_size() == base);
}

/**
 * @brief Unpack a free element written by hand as an older or newer release would have packed it
 *
 * The element is followed by a trailer word, which must be the next thing read.
 *
 * @param fields the fields the sender's type has, each with a ulong, string or bool value
 * @param type_ref true for a type reference without flags, false for flag-filtered data packed field by field
 * @param expected the type passed to unpack_raw_elem()
 * @param[out] out receives the owner of the element, empty on an error
 * @param[out] trailer_ok set to whether the trailer followed the element
 * @return the result of unpack_raw_elem(): PACK_SUCCESS or the code it threw
 */
static int unpack_forged_elem(const std::vector<lDescr> &fields, const bool type_ref, const lDescr *expected,
                              ocs::tom::RawElementPtr &out, bool *trailer_ok) {
   constexpr uint32_t trailer = 0xABCD;
   out.reset();
   *trailer_ok = false;
   try {
      ocs::tom::PackBuffer pb(false, 256);
      sge_pack_buffer *raw = &pb.raw();
      ocs::tom::put_uint32(raw, elem_kind_to_wire(FREE_ELEM));
      if (type_ref) {
         ocs::tom::put_uint32(raw, CULL_PACK_DESCR_TYPE_REF);
         ocs::tom::put_uint32(raw, fields[0].nm);
         ocs::tom::put_uint32(raw, static_cast<uint32_t>(fields.size()));
      } else {
         ocs::tom::put_uint32(raw, static_cast<uint32_t>(fields.size()));
         for (const lDescr &f : fields) {
            ocs::tom::put_uint32(raw, f.nm);
            ocs::tom::put_uint32(raw, f.mt);
         }
      }
      for (size_t i = 0; i < fields.size(); i++) {
         switch (mt_get_type(fields[i].mt)) {
            case lStringT:
               ocs::tom::put_str(raw, "s");
               break;
            case lBoolT:
               ocs::tom::put_uint32(raw, 1);
               break;
            default:
               ocs::tom::put_uint32(raw, static_cast<uint32_t>(7 + i));
               break;
         }
      }
      ocs::tom::put_uint32(raw, trailer);
      ocs::tom::PackBuffer in = reader_of(pb);
      out = ocs::tom::unpack_raw_elem(&in.raw(), expected, type_ref ? 0 : CULL_SPOOL);
      *trailer_ok = ocs::tom::get_uint32(&in.raw()) == trailer;
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
   return PACK_SUCCESS;
}

/**
 * @brief Whether an element is a full GR_Type with the leading values of unpack_forged_elem() and initial ones after
 * @param ep the element, may be empty
 * @param sent how many leading fields the sender had
 * @return true if the element looks as expected
 */
static bool grown_elem_ok(const ocs::tom::ConstRawElement ep, const int sent) {
   return ep && ep.descr() == GR_Type && ep.get<std::uint32_t>(GR_id) == 7 &&
          sge_strnullcmp(ep.get<const char *>(GR_name), "s") == 0 &&
          ep.get<std::uint32_t>(GR_value) == (sent > 2 ? 9u : 0u) && ep.get<bool>(GR_flag) == (sent > 3);
}

/** @brief DESCR-12: types grown at the end stay readable (N-W-6) */
static void test_grown_types() {
   const size_t base = ocs_cull_descr_registry_size();
   const std::vector<lDescr> full(GR_Type, GR_Type + 4);
   const std::vector<lDescr> shorter(GR_Type, GR_Type + 2);
   std::vector<lDescr> longer = full;
   longer.push_back({GR_flag + 1, lUlongT | CULL_SPOOL});
   const std::vector<lDescr> gap = {GR_Type[0], GR_Type[2]};

   ocs::tom::RawElementPtr ep;
   bool trailer_a = false;
   bool trailer_b = false;
   const int ref_ret = unpack_forged_elem(shorter, true, nullptr, ep, &trailer_a);
   bool ref_ok = ref_ret == PACK_SUCCESS && grown_elem_ok(ep.get(), 2) && trailer_a;
   const int spool_ret = unpack_forged_elem(shorter, false, GR_Type, ep, &trailer_b);
   const bool spool_ok = spool_ret == PACK_SUCCESS && grown_elem_ok(ep.get(), 2) && trailer_b;
   ep.reset();
   CHECK(40, "a shorter type reference and a shorter spooled type unpack into the full type with initial values "
         "after the sent fields, and the data behind them is read next",
         ref_ok && spool_ok && ocs_cull_descr_registry_size() == base);

   unpack_forged_elem(full, false, GR_Type, ep, &trailer_a);
   ref_ok = grown_elem_ok(ep.get(), 4) && trailer_a;
   CHECK(41, "the full type packed field by field is the static block", ref_ok);

   const int longer_ref = unpack_forged_elem(longer, true, nullptr, ep, &trailer_a);
   const int longer_spool = unpack_forged_elem(longer, false, nullptr, ep, &trailer_a);
   CHECK(42, "a type longer than the local one is a format error", longer_ref == PACK_FORMAT &&
         longer_spool == PACK_FORMAT);

   const int gap_expected = unpack_forged_elem(gap, false, GR_Type, ep, &trailer_a);
   const int other_expected = unpack_forged_elem(shorter, false, TD_Type, ep, &trailer_a);
   const int gap_free = unpack_forged_elem(gap, false, nullptr, ep, &trailer_b);
   const bool gap_reduced = gap_free == PACK_SUCCESS && ep && (ep->descr()[0].mt & CULL_IS_REDUCED) != 0;
   ep.reset();
   CHECK(43, "on the spool path a descriptor that is not a version of the expected type is a format error, "
         "without an expected type it stays a reduced element",
         gap_expected == PACK_FORMAT && other_expected == PACK_FORMAT && gap_reduced);

   // a selection of the leading fields is marked reduced by the sender and never taken for a shorter type
   auto src = ocs::tom::RawElementPtr::create(GR_Type);
   src->set(GR_id, std::uint32_t{3});
   src->set(GR_name, static_cast<const char *>("n"));
   ocs::tom::RawWhatPtr what = what_of(GR_Type, {GR_id, GR_name});
   ocs::tom::RawElementPtr copy;
   try {
      ocs::tom::PackBuffer pb(false, 256);
      ocs::tom::pack_elem(&pb.raw(), src.get(), what.get(), CULL_SPOOL);
      ocs::tom::PackBuffer in = reader_of(pb);
      copy = ocs::tom::unpack_raw_elem(&in.raw(), nullptr, CULL_SPOOL);
   } catch (const ocs::tom::PackException &) {
      copy.reset();
   }
   CHECK(44, "a selection of the leading fields packed with flags stays a reduced element",
         copy && lCountPresent(copy->descr()) == 2 && ocs::tom::descr_length(copy->descr()) == 4 &&
         (copy->descr()[0].mt & CULL_IS_REDUCED) != 0 &&
         copy->get<std::uint32_t>(GR_id) == 3);
   copy.reset();
   what.reset();
   src.reset();
   CHECK(45, "after the elements the registry is empty again", ocs_cull_descr_registry_size() == base);
}

/**
 * @brief TD_Type at full length with TD_name and TD_flag present (CS-439, N-B-1)
 *
 * TD_name is held in slot 0 and TD_flag in slot 1; TD_id and TD_value are absent.
 *
 * @param out room for five entries
 * @param flag_first true to put TD_flag into slot 0 and TD_name into slot 1
 */
static void full_length_name_flag(lDescr out[5], const bool flag_first) {
   out[0] = {TD_id, lUlongT | CULL_HASH | CULL_UNIQUE | CULL_IS_REDUCED, OCS_CULL_ABSENT};
   out[1] = {TD_name, lStringT | CULL_IS_REDUCED, flag_first ? 0 : -1};
   out[2] = {TD_value, lUlongT | CULL_IS_REDUCED, OCS_CULL_ABSENT};
   out[3] = {TD_flag, lBoolT | CULL_IS_REDUCED, flag_first ? -3 : -2};
   out[4] = {NoName, lEndT | CULL_IS_REDUCED, 0};
}

/** @brief DESCR-13: slots of a full-length descriptor with absent fields */
static void test_slots() {
   const size_t base = ocs_cull_descr_registry_size();
   CHECK(47, "a static block has a value for every field", TD_Type_blk.h.np == 4 && lCountPresent(TD_Type) == 4);

   lDescr built[5];
   full_length_name_flag(built, false);
   CHECK(48, "a plain full-length array counts its present fields",
         lCountPresent(built) == 2 && ocs::tom::descr_length(built) == 4);

   const lDescr *dp = ocs_cull_descr_intern(built);
   const cull_descr_header *h = dp != nullptr ? ocs_cull_descr_header_of(dp) : nullptr;
   bool range_ok = h != nullptr && h->n == 4 && h->np == 2 &&
                   lCountPresent(dp) == 2 && ocs::tom::descr_length(dp) == 4;
   std::vector<std::pair<int, int>> seen;
   if (dp != nullptr) {
      for (const auto f : ocs_cull_present_fields(dp)) {
         seen.emplace_back(f.pos, f.slot);
      }
   }
   range_ok = range_ok && seen == std::vector<std::pair<int, int>>{{1, 0}, {3, 1}};
   CHECK(49, "the counted copy holds np and the reverse map, and the range walks its slots", range_ok);

   lDescr swapped[5];
   full_length_name_flag(swapped, true);
   const lDescr *dp2 = ocs_cull_descr_intern(swapped);
   seen.clear();
   if (dp2 != nullptr) {
      for (const auto f : ocs_cull_present_fields(dp2)) {
         seen.emplace_back(f.pos, f.slot);
      }
   }
   CHECK(50, "descriptors that differ only in their slot offsets are different entries",
         dp2 != nullptr && dp2 != dp && ocs_cull_descr_registry_size() == base + 2 &&
         seen == (std::vector<std::pair<int, int>>{{3, 0}, {1, 1}}));

   ocs::tom::RawElementPtr ep = dp != nullptr ? ocs::tom::RawElementPtr::create(dp) : ocs::tom::RawElementPtr();
   bool values_ok = ep && ep->descr() == dp;
   if (values_ok) {
      ep->set(TD_name, static_cast<const char *>("slot"));
      ep->set(TD_flag, true);
      const lListElem *raw = ep.get();
      values_ok = strcmp(raw->cont[0].str, "slot") == 0 && raw->cont[1].b &&
                  strcmp(ep->get<const char *>(TD_name), "slot") == 0 && ep->get<bool>(TD_flag);
      ocs::tom::RawElementPtr copy = ep.clone();
      values_ok = values_ok && copy && copy->descr() == dp && strcmp(copy->get<const char *>(TD_name), "slot") == 0 &&
                  copy->get<bool>(TD_flag);
   }
   CHECK(51, "an element of it holds two values in their slots, read, written and copied by field name", values_ok);
   ep.reset();

   ocs_cull_descr_release(&dp);
   ocs_cull_descr_release(&dp2);
   CHECK(52, "after the release the registry is empty again", ocs_cull_descr_registry_size() == base);
}

/** @brief DESCR-14: the builder of reduced descriptors from a field list (N-B-5, N-B-9) */
static void test_reduce() {
   const size_t base = ocs_cull_descr_registry_size();

   const lDescr *a = ocs_cull_descr_reduce(TD_Type, {TD_flag, TD_name, TD_flag});
   const lDescr *b = ocs_cull_descr_reduce(TD_Type, {TD_name, TD_flag});
   std::vector<std::pair<int, int>> seen;
   if (a != nullptr) {
      for (const auto f : ocs_cull_present_fields(a)) {
         seen.emplace_back(f.pos, f.slot);
      }
   }
   bool order_ok = a != nullptr && a == b && lCountPresent(a) == 2 && ocs::tom::descr_length(a) == 4 &&
                   seen == (std::vector<std::pair<int, int>>{{1, 0}, {3, 1}}) && a[0].off == OCS_CULL_ABSENT &&
                   mt_get_type(a[1].mt) == lStringT && (a[0].mt & CULL_IS_REDUCED) != 0 &&
                   (a[4].mt & CULL_IS_REDUCED) != 0 && ocs_cull_descr_refs(a) == 2 &&
                   ocs_cull_descr_registry_size() == base + 1;
   CHECK(53, "a field list in any order and with duplicates yields one descriptor in type order", order_ok);

   ocs::tom::RawWhatPtr what = what_of(TD_Type, {TD_flag, TD_name});
   const lDescr *c = ocs::tom::reduced_descr(TD_Type, what.get());
   CHECK(54, "the builder and a selection of the same fields share the descriptor", c == a);
   ocs_cull_descr_release(&c);
   what.reset();

   const lDescr *all = ocs_cull_descr_reduce(TD_Type, {TD_id, TD_name, TD_value, TD_flag});
   CHECK(55, "all fields of the type yield the static block", all == TD_Type);

   CHECK(56, "a field of another type or no field yields nullptr",
         ocs_cull_descr_reduce(TD_Type, {TD_id, GR_name}) == nullptr && ocs_cull_descr_reduce(TD_Type, {}) == nullptr &&
         ocs_cull_descr_registry_size() == base + 1);

   ocs_cull_descr_release(&a);
   ocs_cull_descr_release(&b);
   CHECK(57, "after the release the registry is empty again", ocs_cull_descr_registry_size() == base);
}

/**
 * @brief Unpack a list whose descriptor table entry lists field indices as given
 * @param indices the indices of TD_Type fields the sender lists
 * @return the result of unpack_raw_list(): PACK_SUCCESS or the code it threw
 */
static int unpack_forged_by_index(const std::vector<uint32_t> &indices) {
   try {
      ocs::tom::PackBuffer pb(false, 256);
      sge_pack_buffer *raw = &pb.raw();
      ocs::tom::put_uint32(raw, 1);
      ocs::tom::put_str(raw, "forged");
      ocs::tom::put_uint32(raw, CULL_PACK_DESCR_TABLE_DEF);
      ocs::tom::put_uint32(raw, CULL_PACK_DESCR_BY_INDEX);
      ocs::tom::put_uint32(raw, TD_id);
      ocs::tom::put_uint32(raw, static_cast<uint32_t>(indices.size()));
      for (const uint32_t idx : indices) {
         ocs::tom::put_uint32(raw, idx);
      }
      ocs::tom::put_uint32(raw, 0);
      return unpack_and_drop_list(pb);
   } catch (const ocs::tom::PackException &e) {
      return e.code();
   }
}

/** @brief DESCR-15: the wire lists fields in type order (N-B-8) */
static void test_wire_order() {
   const size_t base = ocs_cull_descr_registry_size();

   CHECK(58, "a table entry listing field indices out of order or twice is a format error",
         unpack_forged_by_index({2, 1}) == PACK_FORMAT && unpack_forged_by_index({1, 1}) == PACK_FORMAT);

   ocs::tom::RawElementPtr ep;
   bool trailer = false;
   const int ret = unpack_forged_elem({{TD_value, lUlongT | CULL_SPOOL}, {TD_id, lUlongT | CULL_SPOOL}}, false,
                                      nullptr, ep, &trailer);
   ep.reset();
   CHECK(59, "a descriptor packed field by field out of type order is a format error", ret == PACK_FORMAT);

   ocs::tom::RawListPtr lp = make_list(3);
   ocs::tom::RawWhatPtr what = what_of(TD_Type, {TD_name, TD_id, TD_name});
   ocs::tom::RawListPtr sel = select_old("dup", lp.get(), what.get());
   ocs::tom::RawListPtr back = sel ? pack_unpack(sel.get()) : ocs::tom::RawListPtr();
   const ocs::tom::ConstRawElement first = back ? back->front() : ocs::tom::ConstRawElement();
   CHECK(61, "a field selected twice is selected once, and the reduction goes over the wire",
         ocs::tom::count_what(what.get(), TD_Type) == 2 && sel && lCountPresent(sel->descr()) == 2 &&
         back && back->size() == 3 && first && first.get<std::uint32_t>(TD_id) == 0 &&
         strcmp(first.get<const char *>(TD_name), "n0") == 0);
   back.reset();
   sel.reset();
   what.reset();
   lp.reset();

   CHECK(60, "after the buffers the registry is empty again", ocs_cull_descr_registry_size() == base);
}

/**
 * @brief Whether reading a string field throws a FieldException with a given reason
 * @param ep the element
 * @param nm the field, of type string
 * @param reason the reason the exception must carry
 * @return true if the read throws with @p reason
 */
static bool read_throws(const ocs::tom::ConstRawElement ep, const int nm,
                        const ocs::tom::FieldException::Reason reason) {
   try {
      static_cast<void>(ep.get<const char *>(nm));
   } catch (const ocs::tom::FieldException &e) {
      return e.reason() == reason;
   }
   return false;
}

/**
 * @brief Whether reading a string field of an element throws nothing at all
 * @param ep the element
 * @param nm the field
 * @return true if the read throws no exception
 */
static bool reads_cleanly(const ocs::tom::ConstRawElement ep, const int nm) {
   try {
      static_cast<void>(ep.get<const char *>(nm));
   } catch (...) {
      return false;
   }
   return true;
}

/** @brief DESCR-16: positions, absent and foreign fields of full-length descriptors (N-B-2, N-B-3, N-B-4) */
static void test_absent_fields() {
   const size_t base = ocs_cull_descr_registry_size();
   ocs::tom::RawListPtr lp = make_list(2);
   ocs::tom::RawWhatPtr what = what_of(TD_Type, {TD_name, TD_value});
   ocs::tom::RawListPtr red = select_old("red", lp.get(), what.get());
   const ocs::tom::ConstRawElement full = lp->front();
   const ocs::tom::ConstRawElement ep = red ? red->front() : ocs::tom::ConstRawElement();
   const lDescr *dp = ep ? ep.descr() : nullptr;

   CHECK(62, "a reduced element has the length of its type and keeps the positions of the type",
         dp != nullptr && ocs::tom::descr_length(dp) == 4 && lCountPresent(dp) == 2 &&
         ocs::tom::present_pos(dp, TD_name) == 1 && ocs::tom::present_pos(dp, TD_value) == 2 &&
         ocs::tom::present_pos(full.descr(), TD_value) == 2);

   CHECK(63, "a field the reduction does not hold is not found and has no type",
         dp != nullptr && ocs::tom::present_pos(dp, TD_id) == -1 && !ep.has(TD_id) &&
         ocs::tom::present_pos(dp, TD_flag) == -1 && ocs::tom::field_type(dp, TD_id) == lEndT &&
         ocs::tom::field_type(dp, TD_name) == lStringT);

   CHECK(64, "a field of another type is not found in a full or a reduced descriptor",
         dp != nullptr && ocs::tom::present_pos(TD_Type, GR_name) == -1 && ocs::tom::present_pos(dp, GR_name) == -1 &&
         ocs::tom::present_pos(TD_Type, TD_id + 4) == -1 && !full.has(GR_id));

   CHECK(65, "the values of the held fields are those of the source",
         ep && ep.get<std::uint32_t>(TD_value) == full.get<std::uint32_t>(TD_value) &&
         strcmp(ep.get<const char *>(TD_name), full.get<const char *>(TD_name)) == 0);

   CHECK(66, "reading a field the reduction holds does not throw, one it does not hold throws AbsentField",
         ep && reads_cleanly(ep, TD_name) &&
         read_throws(ep, TD_id, ocs::tom::FieldException::Reason::AbsentField));

   red.reset();
   what.reset();
   lp.reset();
   CHECK(67, "after the lists the registry is empty again", ocs_cull_descr_registry_size() == base);
}

/** @brief DESCR-17: an element of a type the installed object model does not hold (CS-439) */
static void test_unregistered_type() {
   const size_t base = ocs_cull_descr_registry_size();
   auto ep = ocs::tom::RawElementPtr::create(UR_Type);
   ep->set(UR_id, std::uint32_t{7});
   ep->set(UR_text, static_cast<const char *>("packed"));
   ocs::tom::RawElementPtr back;
   int pack_ret = PACK_FORMAT;
   int ret = PACK_SUCCESS;
   try {
      ocs::tom::PackBuffer pb(false, 256);
      ocs::tom::pack_elem(&pb.raw(), ep.get(), nullptr, 0);
      pack_ret = PACK_SUCCESS;
      ocs::tom::PackBuffer in = reader_of(pb);
      back = ocs::tom::unpack_raw_elem(&in.raw(), nullptr, 0);
   } catch (const ocs::tom::PackException &e) {
      ret = e.code();
   }
   CHECK(68, "an element of an unregistered type is packed, and unpacking it is a format error",
         pack_ret == PACK_SUCCESS && ret == PACK_FORMAT && !back);
   back.reset();
   ep.reset();
   CHECK(69, "after the element the registry is empty again", ocs_cull_descr_registry_size() == base);
}

/**
 * @brief Run the scenarios of this test
 * @param argc the number of arguments
 * @param argv the arguments, handed to the module test driver
 * @return the exit status of the module test driver
 */
int main(int argc, char *argv[]) {
   ocs::module_test::install_test_model({{"TD", TD_Type, TD_Name, TD_Size}, {"GR", GR_Type, GR_Name, GR_Size}});

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
       {"select twice with one selection", "reduce with reduced_descr() twice",
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
      {"DESCR-13", "slots of descriptors with absent fields",
       "a full-length descriptor with absent fields holds its present fields in slots, found through its header",
       {"read the header of a static block", "count a plain array with absent entries",
        "intern it and a copy that differs only in the slot offsets", "create, write, read and copy an element of it"},
       "the header holds np, the range walks the slots through the reverse map, the offsets "
       "keep the two apart, and the values sit in their slots",
       {{"FLD-01", "partly"}}, {47, 48, 49, 50, 51, 52}, test_slots},
      {"DESCR-14", "builder from a field list",
       "descriptors written down in the code are built from a list of fields of their type",
       {"reduce a type to a field list given in two orders and with a duplicate", "select the same fields",
        "reduce to all fields", "reduce to a foreign field and to no field"},
       "both lists and the selection share one descriptor in type order, all fields are the static block, and "
       "a foreign or empty list is rejected",
       {{"FLD-04", "unit level"}}, {53, 54, 55, 56, 57}, test_reduce},
      {"DESCR-15", "fields on the wire in type order",
       "a receiver puts the values into their slots in the order the descriptor lists the fields",
       {"unpack a table entry listing indices out of order and twice",
        "unpack an element whose descriptor is packed field by field out of order",
        "select with a field listed twice and send the result"},
       "both are format errors, a field selected twice is packed once, and nothing stays in the registry",
       {{"FLD-03", "partly"}}, {58, 59, 61, 60}, test_wire_order},
      {"DESCR-16", "absent and foreign fields",
       "reduced elements keep the positions of their type, and fields they do not hold behave as before",
       {"select two fields of a list", "look up held, absent and foreign fields by position and type",
        "read held and absent fields"},
       "positions equal those of the type, absent and foreign fields are not found and have no type, held values "
       "are those of the source, and reading an absent field throws AbsentField",
       {{"FLD-01", "partly"}, {"FLD-02", "unit level"}}, {62, 63, 64, 65, 66, 67}, test_absent_fields},
      {"DESCR-17", "types the object model does not hold",
       "data of a type outside the object model is rejected on the wire",
       {"pack an element of an unregistered type and unpack it"},
       "unpacking is a format error and leaves no element and no descriptor behind",
       {{"FLD-03", "partly"}}, {68, 69}, test_unregistered_type},
   });
}
