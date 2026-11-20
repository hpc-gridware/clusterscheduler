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
 * @brief Unit tests for sorting in the core of the typed object model (libs/tom)
 *
 * Covers building a sort order (ocs::tom::make_sort_order), comparing two
 * elements by it (sort_compare), sorting a list (sort_list), inserting into a
 * sorted list (insert_sorted) and moving a changed element to its place
 * (resort_elem), and checks that hash tables stay usable after the list was
 * re-linked. The test type has no generated traits, so the lists are driven
 * through the run-time typed handles of tom/ocs_Raw.h.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/cull_hash.h"
#include "tom/cull_types.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_Host.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SortCore.h"
#include "ocs_test_model.h"

// ---------------------------------------------------------------------------
// Local CULL type used by all tests
// ---------------------------------------------------------------------------

/** @brief A synthetic CULL object type, defined only for these tests
 *
 * One attribute per sortable CULL data type, plus a unique hashed string key
 * and a non-unique hashed ulong so the tests can see whether sorting keeps the
 * hash tables consistent with the list.
 */
enum {
   TEST_int = 1,   ///< an int attribute
   TEST_host,   ///< a host name attribute
   TEST_string,   ///< a string attribute
   TEST_double,   ///< a double attribute
   TEST_long,   ///< a long attribute
   TEST_ulong,   ///< an unsigned long attribute
   TEST_bool,   ///< a bool attribute
   TEST_ulong64,   ///< a 64 bit unsigned attribute
   TEST_ulong_hashed,   ///< an unsigned long attribute with a non-unique hash
   TEST_string_ukey   ///< a string attribute that is a unique hashed key
};

LISTDEF(TEST_Type)
   SGE_INT    (TEST_int,    CULL_DEFAULT)
   SGE_HOST   (TEST_host,   CULL_DEFAULT)
   SGE_STRING (TEST_string, CULL_DEFAULT)
   SGE_DOUBLE (TEST_double, CULL_DEFAULT)
   SGE_LONG   (TEST_long,   CULL_DEFAULT)
   SGE_ULONG  (TEST_ulong,  CULL_DEFAULT)
   SGE_BOOL   (TEST_bool,   CULL_DEFAULT)
   SGE_ULONG64(TEST_ulong64, CULL_DEFAULT)
   SGE_ULONG  (TEST_ulong_hashed, CULL_HASH)
   SGE_STRING (TEST_string_ukey, CULL_UNIQUE | CULL_HASH)
LISTEND(TEST_Type)

NAMEDEF(TEST_Name)
   NAME("TEST_int")
   NAME("TEST_host")
   NAME("TEST_string")
   NAME("TEST_double")
   NAME("TEST_long")
   NAME("TEST_ulong")
   NAME("TEST_bool")
   NAME("TEST_ulong64")
   NAME("TEST_ulong_hashed")
   NAME("TEST_string_ukey")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)    ///< number of attributes of the synthetic type

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

/** @brief Number of elements of a list as int, for comparisons with int counts
 *
 * @param lp the list; nullptr counts as empty
 * @return the element count
 */
static int nelem(const ocs::tom::ConstRawList lp) {
   return static_cast<int>(lp.size());
}

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
 * @brief Build a sort order of the test type
 * @param keys the criteria, the first deciding first
 * @return the order, freed with ocs::tom::free_sort_order()
 */
static lSortOrder *make_order(const std::initializer_list<ocs::tom::SortKey> keys) {
   return ocs::tom::make_sort_order(TEST_Type, std::span<const ocs::tom::SortKey>(keys.begin(), keys.size()));
}

/**
 * @brief Create a test element with a string key and append it, as lAddElemStr() did
 * @param lp the list
 * @param nm the string field
 * @param key the key
 * @return the new element, owned by the list
 */
static ocs::tom::RawElement add_str(const ocs::tom::RawList lp, const int nm, const char *key) {
   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   ep->set(nm, key);
   return lp.push_back(std::move(ep));
}

/** @brief Build a list whose elements carry the given ulong values
 *
 * Every element also gets a unique key `k<value>` in TEST_string_ukey and the
 * value modulo 2 in TEST_ulong_hashed, so both hash tables are populated.
 *
 * @param values the TEST_ulong values, in list order
 * @param n the number of values
 * @return the new list
 */
static ocs::tom::RawListPtr make_ulong_list(const std::uint32_t *values, const int n) {
   auto lp = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "sort");
   for (int i = 0; i < n; i++) {
      char key[32];
      snprintf(key, sizeof(key), "k%u", static_cast<unsigned>(values[i]));
      auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
      ep->set(TEST_ulong, values[i]);
      ep->set(TEST_ulong_hashed, values[i] % 2);
      ep->set(TEST_string_ukey, static_cast<const char *>(key));
      lp->push_back(std::move(ep));
   }
   return lp;
}

/** @brief Compare the TEST_ulong values of a list with an expected sequence
 *
 * @param lp the list to inspect
 * @param expected the values the list must hold, in order
 * @param n the number of expected values; the list must have exactly this many
 * @return true when the list holds exactly the expected values in that order
 */
static bool ulong_order_is(const ocs::tom::ConstRawList lp, const std::uint32_t *expected, const int n) {
   if (nelem(lp) != n) {
      return false;
   }
   int i = 0;
   for (const ocs::tom::ConstRawElement ep : lp) {
      if (ep.get<std::uint32_t>(TEST_ulong) != expected[i++]) {
         return false;
      }
   }
   return true;
}

/** @brief Check that the prev pointers mirror the next pointers
 *
 * Sorting re-links the list by hand, so a reversed walk must visit the same
 * elements as a forward walk, back to front.
 *
 * @param lp the list to inspect
 * @return true when walking backwards from the last element reaches every element once
 */
static bool back_links_consistent(const ocs::tom::ConstRawList lp) {
   int n = 0;
   const lListElem *expected_next = nullptr;
   for (ocs::tom::ConstRawElement ep = lp.back(); ep; ep = ep.prev()) {
      if (ep.next().raw() != expected_next) {
         return false;
      }
      expected_next = ep.raw();
      n++;
   }
   return n == nelem(lp) && expected_next == lp.front().raw();
}

/** @brief Check that every element is still found through both hash tables
 *
 * @param lp the list to inspect
 * @return true when each unique key resolves to its own element and the
 *         non-unique hash yields exactly the elements carrying each value
 */
static bool hashes_consistent(const ocs::tom::ConstRawList lp) {
   int expected[2] = {0, 0};
   for (const ocs::tom::ConstRawElement ep : lp) {
      if (lp.find_first(TEST_string_ukey, ep.get<const char *>(TEST_string_ukey)).raw() != ep.raw()) {
         return false;
      }
      expected[ep.get<std::uint32_t>(TEST_ulong_hashed)]++;
   }
   const int pos = ocs::tom::field_pos(TEST_Type, TEST_ulong_hashed);
   for (std::uint32_t v = 0; v < 2; v++) {
      int found = 0;
      const void *it = nullptr;
      for (const lListElem *h = ocs::tom::lookup_first<ocs::tom::FieldType::Uint32>(lp.raw(), pos, v, &it);
           h != nullptr; h = ocs::tom::lookup_next<ocs::tom::FieldType::Uint32>(lp.raw(), pos, v, &it)) {
         if (ocs::tom::get<std::uint32_t>(h, TEST_ulong_hashed) != v) {
            return false;
         }
         found++;
      }
      if (found != expected[v]) {
         return false;
      }
   }
   return true;
}

/**
 * @brief The reason of the FieldException a call throws
 * @param call the call
 * @param reason the reason expected
 * @return true if the call threw a FieldException with @p reason
 */
template <typename F>
static bool throws_reason(F &&call, const ocs::tom::FieldException::Reason reason) {
   try {
      call();
   } catch (const ocs::tom::FieldException &e) {
      return e.reason() == reason;
   }
   return false;
}

// ---------------------------------------------------------------------------
// Sort order construction  [T01–T05]
//
// make_sort_order() builds an order from fields and directions, as
// lParseSortOrderVarArg() did from "%I+"/"%I-" format strings and
// lCreateSortOrder() with lAddSortCriteria() by hand. It must reject a field
// that is not part of the descriptor.
// ---------------------------------------------------------------------------

/** @brief T01-T05: building and freeing a sort order */
static void test_sort_order() {
   printf("\n--- sort order construction ---\n");

   // T01: a valid single-field order is built
   lSortOrder *so = make_order({{TEST_ulong, true}});
   CHECK(1, "make_sort_order with a known field", so != nullptr);
   ocs::tom::free_sort_order(so);
   CHECK(2, "free_sort_order resets the pointer", so == nullptr);

   // T03: a field that is not in the descriptor is rejected
   CHECK(3, "make_sort_order with an unknown field throws ForeignField",
         throws_reason([] { lSortOrder *bad = make_order({{4711, true}}); ocs::tom::free_sort_order(bad); },
                       ocs::tom::FieldException::Reason::ForeignField));

   // CHECK 4 removed: lAddSortCriteria has no TOM counterpart (CS-3103)

   // T05: an order built from a descending criterion sorts descending
   const std::uint32_t in[] = {3, 1, 2};
   const std::uint32_t desc[] = {3, 2, 1};
   auto lp = make_ulong_list(in, 3);
   so = make_order({{TEST_ulong, false}});
   ocs::tom::sort_list(lp.get(), so);
   CHECK(5, "make_sort_order descending order", ulong_order_is(lp.get(), desc, 3));
   ocs::tom::free_sort_order(so);
}

// ---------------------------------------------------------------------------
// sort_compare  [T06–T09]
//
// Returns <0, 0, >0 like strcmp, inverted for a descending criterion. A second
// criterion decides only when the first is equal.
// ---------------------------------------------------------------------------

/** @brief T06-T09: comparing two elements by a sort order */
static void test_sort_compare() {
   printf("\n--- sort_compare ---\n");

   auto a = ocs::tom::RawElementPtr::create(TEST_Type);
   auto b = ocs::tom::RawElementPtr::create(TEST_Type);
   a->set(TEST_ulong, std::uint32_t{1});
   b->set(TEST_ulong, std::uint32_t{2});
   a->set(TEST_string, "y");
   b->set(TEST_string, "x");

   lSortOrder *up = make_order({{TEST_ulong, true}});
   lSortOrder *down = make_order({{TEST_ulong, false}});

   // T06-T07: sign follows the direction of the criterion
   CHECK(6, "ascending: smaller value compares less", ocs::tom::sort_compare(a.get(), b.get(), up) < 0);
   CHECK(7, "descending: smaller value compares greater", ocs::tom::sort_compare(a.get(), b.get(), down) > 0);

   // T08: equal values compare equal
   b->set(TEST_ulong, std::uint32_t{1});
   CHECK(8, "equal values compare equal", ocs::tom::sort_compare(a.get(), b.get(), up) == 0);

   // T09: the second criterion breaks the tie
   lSortOrder *two = make_order({{TEST_ulong, true}, {TEST_string, true}});
   CHECK(9, "second criterion decides a tie", ocs::tom::sort_compare(a.get(), b.get(), two) > 0);

   ocs::tom::free_sort_order(up);
   ocs::tom::free_sort_order(down);
   ocs::tom::free_sort_order(two);
}

// ---------------------------------------------------------------------------
// sort_list  [T10–T23]
//
// Ascending, descending and two-key orders on ulong values, one sort per
// remaining data type, nullptr strings first, and the trivial lists. After
// every sort the back links must mirror the forward links.
// ---------------------------------------------------------------------------

/** @brief T10-T23: sorting a list by every data type and the trivial lists */
static void test_sort_list() {
   printf("\n--- sort_list ---\n");

   const std::uint32_t in[] = {5, 1, 4, 2, 3};
   const std::uint32_t asc[] = {1, 2, 3, 4, 5};
   const std::uint32_t desc[] = {5, 4, 3, 2, 1};

   // T10-T11: ascending
   auto lp = make_ulong_list(in, 5);
   lSortOrder *so = make_order({{TEST_ulong, true}});
   ocs::tom::sort_list(lp.get(), so);
   ocs::tom::free_sort_order(so);
   CHECK(10, "ulong ascending", ulong_order_is(lp.get(), asc, 5));
   CHECK(11, "ulong ascending: back links consistent", back_links_consistent(lp.get()));

   // T12: descending
   so = make_order({{TEST_ulong, false}});
   ocs::tom::sort_list(lp.get(), so);
   CHECK(12, "ulong descending", ulong_order_is(lp.get(), desc, 5));
   ocs::tom::free_sort_order(so);

   // T13: two keys, the first ascending, the second descending
   lp = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "two");
   const char *grp[] = {"b", "a", "b", "a"};
   const std::uint32_t val[] = {1, 1, 2, 2};
   for (int i = 0; i < 4; i++) {
      add_str(lp.get(), TEST_string, grp[i]).set(TEST_ulong, val[i]);
   }
   so = make_order({{TEST_string, true}, {TEST_ulong, false}});
   ocs::tom::sort_list(lp.get(), so);
   const std::uint32_t two_expected[] = {2, 1, 2, 1};
   const ocs::tom::ConstRawElement first = lp->front();
   const ocs::tom::ConstRawElement third = first.next().next();
   CHECK(13, "string ascending, then ulong descending",
         ulong_order_is(lp.get(), two_expected, 4) &&
         strcmp(first.get<const char *>(TEST_string), "a") == 0 &&
         strcmp(third.get<const char *>(TEST_string), "b") == 0);
   ocs::tom::free_sort_order(so);

   // T14-T19: one ascending sort per remaining data type. Each element gets the
   // same rank in every field, so after sorting by any one field TEST_ulong
   // (holding the rank) must read 0, 1, 2.
   const std::uint32_t ranks[] = {0, 1, 2};
   struct {
      int nm;
      const char *label;
   } fields[] = {
      {TEST_int,     "int ascending"},
      {TEST_long,    "long ascending"},
      {TEST_double,  "double ascending"},
      {TEST_ulong64, "ulong64 ascending"},
      {TEST_host,    "host ascending"},
      {TEST_string,  "string ascending"},
   };
   for (int f = 0; f < 6; f++) {
      lp = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "types");
      const int order[] = {2, 0, 1};
      for (int i = 0; i < 3; i++) {
         const int r = order[i];
         char name[16];
         snprintf(name, sizeof(name), "h%d", r);
         auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
         ep->set(TEST_ulong, static_cast<std::uint32_t>(r));
         ep->set(TEST_int, static_cast<std::int32_t>(r - 1));
         ep->set(TEST_long, static_cast<std::int64_t>(-100 + r));
         ep->set(TEST_double, 0.5 * r);
         ep->set(TEST_ulong64, static_cast<std::uint64_t>(0x100000000ULL * r));
         ep->set(TEST_host, ocs::tom::Host(name));
         ep->set(TEST_string, static_cast<const char *>(name));
         lp->push_back(std::move(ep));
      }
      so = make_order({{fields[f].nm, true}});
      ocs::tom::sort_list(lp.get(), so);
      ocs::tom::free_sort_order(so);
      CHECK(14 + f, fields[f].label, ulong_order_is(lp.get(), ranks, 3));
   }

   // T20: bool sorts false before true
   lp = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "bool");
   for (int i = 0; i < 3; i++) {
      auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
      ep->set(TEST_bool, i != 1);
      ep->set(TEST_ulong, std::uint32_t{i == 1 ? 0u : 1u});
      lp->push_back(std::move(ep));
   }
   so = make_order({{TEST_bool, true}});
   ocs::tom::sort_list(lp.get(), so);
   ocs::tom::free_sort_order(so);
   CHECK(20, "bool ascending: false first", !lp->front().get<bool>(TEST_bool));

   // T21: a nullptr string sorts before any other string
   lp = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, "null");
   add_str(lp.get(), TEST_string, "a");
   lp->push_back(ocs::tom::RawElementPtr::create(TEST_Type));
   so = make_order({{TEST_string, true}});
   ocs::tom::sort_list(lp.get(), so);
   ocs::tom::free_sort_order(so);
   CHECK(21, "nullptr string sorts first", lp->front().get<const char *>(TEST_string) == nullptr);

   // T22-T23: trivial lists are accepted as sorted
   so = make_order({{TEST_ulong, true}});
   CHECK(22, "sort_list on a nullptr list accepts it",
         [so] {
            try {
               ocs::tom::sort_list(nullptr, so);
               return true;
            } catch (...) {
               return false;
            }
         }());
   const std::uint32_t one[] = {7};
   lp = make_ulong_list(one, 1);
   ocs::tom::sort_list(lp.get(), so);
   CHECK(23, "sort_list on a single element keeps it",
         ulong_order_is(lp.get(), one, 1) && back_links_consistent(lp.get()));
   ocs::tom::free_sort_order(so);
}

// ---------------------------------------------------------------------------
// Hash tables after sorting  [T24–T26]
//
// sort_list() re-links the elements and then drops the non-unique hash tables
// (cull_hash_recreate_after_sort), which the next lookup builds again in the new
// order. Every key must still resolve, and the non-unique hash must still yield
// each element exactly once.
// ---------------------------------------------------------------------------

/** @brief T24-T26: the hash tables stay consistent across sorts */
static void test_sort_keeps_hash() {
   printf("\n--- hash tables after sorting ---\n");

   const std::uint32_t in[] = {9, 3, 7, 1, 5, 8, 2, 6, 4};
   auto lp = make_ulong_list(in, 9);
   CHECK(24, "hashes consistent before sorting", hashes_consistent(lp.get()));

   lSortOrder *so = make_order({{TEST_ulong, true}});
   ocs::tom::sort_list(lp.get(), so);
   ocs::tom::free_sort_order(so);
   CHECK(25, "hashes consistent after ascending sort", hashes_consistent(lp.get()));

   so = make_order({{TEST_ulong, false}});
   ocs::tom::sort_list(lp.get(), so);
   ocs::tom::free_sort_order(so);
   CHECK(26, "hashes consistent after descending sort", hashes_consistent(lp.get()));
}

// ---------------------------------------------------------------------------
// insert_sorted / resort_elem  [T27–T34]
//
// insert_sorted() puts an element before the first element that does not sort
// before it, so a sorted list stays sorted. resort_elem() moves an element
// whose key changed back to its place. Both go through the hash-aware list
// operations, so the new or moved element must be findable by its key.
// ---------------------------------------------------------------------------

/** @brief T27-T34: inserting into a sorted list and moving a changed element */
static void test_insert_and_resort() {
   printf("\n--- insert_sorted / resort_elem ---\n");

   const std::uint32_t in[] = {2, 4, 6};
   auto lp = make_ulong_list(in, 3);
   lSortOrder *so = make_order({{TEST_ulong, true}});

   auto insert = [&](const std::uint32_t v) {
      char key[32];
      snprintf(key, sizeof(key), "k%u", static_cast<unsigned>(v));
      auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
      ep->set(TEST_ulong, v);
      ep->set(TEST_ulong_hashed, v % 2);
      ep->set(TEST_string_ukey, static_cast<const char *>(key));
      ocs::tom::insert_sorted(lp.get(), ep.get(), so);
      static_cast<void>(ep.release());   // the list owns it now
   };

   // T27-T29: at the front, in the middle, at the end
   insert(1);
   const std::uint32_t e1[] = {1, 2, 4, 6};
   CHECK(27, "insert_sorted at the front", ulong_order_is(lp.get(), e1, 4));
   insert(5);
   const std::uint32_t e2[] = {1, 2, 4, 5, 6};
   CHECK(28, "insert_sorted in the middle", ulong_order_is(lp.get(), e2, 5));
   insert(9);
   const std::uint32_t e3[] = {1, 2, 4, 5, 6, 9};
   CHECK(29, "insert_sorted at the end", ulong_order_is(lp.get(), e3, 6) && back_links_consistent(lp.get()));

   // T30: an equal key goes before the existing one
   auto dup_owner = ocs::tom::RawElementPtr::create(TEST_Type);
   dup_owner->set(TEST_ulong, std::uint32_t{4});
   dup_owner->set(TEST_string_ukey, "dup4");
   const lListElem *dup = dup_owner.get().raw();
   ocs::tom::insert_sorted(lp.get(), dup_owner.get(), so);
   static_cast<void>(dup_owner.release());   // the list owns it now
   CHECK(30, "insert_sorted puts an equal key first", lp->front().next().raw() != dup &&
         lp->front().next().next().raw() == dup);

   // T31: inserted elements are findable through the hashes
   CHECK(31, "inserted elements are in the hash tables", hashes_consistent(lp.get()));

   // T32: a null element is rejected
   CHECK(32, "insert_sorted rejects a null element with NullElement",
         throws_reason([&] { ocs::tom::insert_sorted(lp.get(), nullptr, so); },
                       ocs::tom::FieldException::Reason::NullElement));

   // T33-T34: move the first element to the end after raising its key
   const ocs::tom::RawElement head = lp->front();
   head.set(TEST_ulong, std::uint32_t{100});
   ocs::tom::resort_elem(lp.get(), head, so);
   CHECK(33, "resort_elem moves a raised key to the end",
         lp->back().raw() == head.raw() && back_links_consistent(lp.get()));
   CHECK(34, "resort_elem keeps the hash tables consistent", hashes_consistent(lp.get()));

   ocs::tom::free_sort_order(so);
}

// ---------------------------------------------------------------------------

/**
 * @brief Run all sorting tests
 * @return 0 if every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   // the lists here are short; with the threshold at 0 every lookup goes through the hash tables
   ocs::module_test::install_test_model({{"TEST", TEST_Type, TEST_Name, TEST_Size}});
   ocs_cull_hash_set_min_elems(0);

   test_sort_order();
   test_sort_compare();
   test_sort_list();
   test_sort_keeps_hash();
   test_insert_and_resort();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
