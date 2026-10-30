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
 * @brief Unit tests for sorting in `libs/cull`
 *
 * Covers the sort order builders (lParseSortOrderVarArg, lCreateSortOrder,
 * lAddSortCriteria), lSortCompare, lSortList, lInsertSorted and lResortElem,
 * and checks that hash tables stay usable after the list was re-linked.
 */

#include <cstdio>
#include <cstring>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"
#include "cull/cull_list.h"
#include "cull/cull_multitype.h"
#include "cull/cull_sort.h"

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
LISTEND

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

/** @brief The name space registering the synthetic types with CULL */
lNameSpace nmv[] = {
   {1, TEST_Size, TEST_Name, TEST_Type},
   {0, 0, nullptr, nullptr}
};

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

/** @brief Number of elements of a list as int, for comparisons with int counts
 *
 * @param lp the list; nullptr counts as empty
 * @return the element count
 */
static int nelem(const lList *lp) {
   return static_cast<int>(lGetNumberOfElem(lp));
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

/** @brief Build a list whose elements carry the given ulong values
 *
 * Every element also gets a unique key `k<value>` in TEST_string_ukey and the
 * value modulo 2 in TEST_ulong_hashed, so both hash tables are populated.
 *
 * @param values the TEST_ulong values, in list order
 * @param n the number of values
 * @return the new list, owned by the caller
 */
static lList *make_ulong_list(const lUlong *values, int n) {
   lList *lp = lCreateList("sort", TEST_Type);
   for (int i = 0; i < n; i++) {
      char key[32];
      snprintf(key, sizeof(key), "k%u", (unsigned) values[i]);
      lListElem *ep = lCreateElem(TEST_Type);
      lSetUlong(ep, TEST_ulong, values[i]);
      lSetUlong(ep, TEST_ulong_hashed, values[i] % 2);
      lSetString(ep, TEST_string_ukey, key);
      lAppendElem(lp, ep);
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
static bool ulong_order_is(const lList *lp, const lUlong *expected, int n) {
   if (nelem(lp) != n) {
      return false;
   }
   int i = 0;
   const lListElem *ep;
   for_each_ep(ep, lp) {
      if (lGetUlong(ep, TEST_ulong) != expected[i++]) {
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
 * @return true when walking backwards from lLast reaches every element once
 */
static bool back_links_consistent(const lList *lp) {
   int n = 0;
   const lListElem *ep;
   const lListElem *expected_next = nullptr;
   for (ep = lLast(lp); ep != nullptr; ep = lPrev(ep)) {
      if (lNext(ep) != expected_next) {
         return false;
      }
      expected_next = ep;
      n++;
   }
   return n == nelem(lp) && expected_next == lFirst(lp);
}

/** @brief Check that every element is still found through both hash tables
 *
 * @param lp the list to inspect
 * @return true when each unique key resolves to its own element and the
 *         non-unique hash yields exactly the elements carrying each value
 */
static bool hashes_consistent(const lList *lp) {
   const lListElem *ep;
   int expected[2] = {0, 0};
   for_each_ep(ep, lp) {
      if (lGetElemStr(lp, TEST_string_ukey, lGetString(ep, TEST_string_ukey)) != ep) {
         return false;
      }
      expected[lGetUlong(ep, TEST_ulong_hashed)]++;
   }
   for (lUlong v = 0; v < 2; v++) {
      int found = 0;
      const void *it = nullptr;
      for (const lListElem *h = lGetElemUlongFirst(lp, TEST_ulong_hashed, v, &it);
           h != nullptr; h = lGetElemUlongNext(lp, TEST_ulong_hashed, v, &it)) {
         if (lGetUlong(h, TEST_ulong_hashed) != v) {
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

// ---------------------------------------------------------------------------
// Sort order construction  [T01–T05]
//
// lParseSortOrderVarArg() parses "%I+"/"%I-" format strings; lCreateSortOrder()
// with lAddSortCriteria() builds the same order by hand. Both must reject a
// field that is not part of the descriptor.
// ---------------------------------------------------------------------------

static void test_sort_order() {
   printf("\n--- sort order construction ---\n");

   // T01: a valid single-field order parses
   lSortOrder *so = lParseSortOrderVarArg(TEST_Type, "%I+", TEST_ulong);
   CHECK(1, "lParseSortOrderVarArg with a known field", so != nullptr);
   lFreeSortOrder(&so);
   CHECK(2, "lFreeSortOrder resets the pointer", so == nullptr);

   // T03: a field that is not in the descriptor is rejected
   so = lParseSortOrderVarArg(TEST_Type, "%I+", 4711);
   CHECK(3, "lParseSortOrderVarArg with an unknown field returns nullptr", so == nullptr);
   lFreeSortOrder(&so);

   // T04: lAddSortCriteria rejects an unknown field
   so = lCreateSortOrder(1);
   CHECK(4, "lAddSortCriteria with an unknown field returns -1",
         so != nullptr && lAddSortCriteria(TEST_Type, so, 4711, 1) == -1);
   lFreeSortOrder(&so);

   // T05: a hand-built order sorts like the parsed one
   const lUlong in[] = {3, 1, 2};
   const lUlong desc[] = {3, 2, 1};
   lList *lp = make_ulong_list(in, 3);
   so = lCreateSortOrder(1);
   lAddSortCriteria(TEST_Type, so, TEST_ulong, -1);
   lSortList(lp, so);
   CHECK(5, "lCreateSortOrder/lAddSortCriteria descending order", ulong_order_is(lp, desc, 3));
   lFreeSortOrder(&so);
   lFreeList(&lp);
}

// ---------------------------------------------------------------------------
// lSortCompare  [T06–T09]
//
// Returns <0, 0, >0 like strcmp, inverted for a descending criterion. A second
// criterion decides only when the first is equal.
// ---------------------------------------------------------------------------

static void test_sort_compare() {
   printf("\n--- lSortCompare ---\n");

   lListElem *a = lCreateElem(TEST_Type);
   lListElem *b = lCreateElem(TEST_Type);
   lSetUlong(a, TEST_ulong, 1);
   lSetUlong(b, TEST_ulong, 2);
   lSetString(a, TEST_string, "y");
   lSetString(b, TEST_string, "x");

   lSortOrder *up = lParseSortOrderVarArg(TEST_Type, "%I+", TEST_ulong);
   lSortOrder *down = lParseSortOrderVarArg(TEST_Type, "%I-", TEST_ulong);

   // T06-T07: sign follows the direction of the criterion
   CHECK(6, "ascending: smaller value compares less", lSortCompare(a, b, up) < 0);
   CHECK(7, "descending: smaller value compares greater", lSortCompare(a, b, down) > 0);

   // T08: equal values compare equal
   lSetUlong(b, TEST_ulong, 1);
   CHECK(8, "equal values compare equal", lSortCompare(a, b, up) == 0);

   // T09: the second criterion breaks the tie
   lSortOrder *two = lParseSortOrderVarArg(TEST_Type, "%I+%I+", TEST_ulong, TEST_string);
   CHECK(9, "second criterion decides a tie", lSortCompare(a, b, two) > 0);

   lFreeSortOrder(&up);
   lFreeSortOrder(&down);
   lFreeSortOrder(&two);
   lFreeElem(&a);
   lFreeElem(&b);
}

// ---------------------------------------------------------------------------
// lSortList  [T10–T20]
//
// Ascending, descending and two-key orders on ulong values, one sort per
// remaining data type, nullptr strings first, and the trivial lists. After
// every sort the back links must mirror the forward links.
// ---------------------------------------------------------------------------

static void test_sort_list() {
   printf("\n--- lSortList ---\n");

   const lUlong in[] = {5, 1, 4, 2, 3};
   const lUlong asc[] = {1, 2, 3, 4, 5};
   const lUlong desc[] = {5, 4, 3, 2, 1};

   // T10-T11: ascending
   lList *lp = make_ulong_list(in, 5);
   lPSortList(lp, "%I+", TEST_ulong);
   CHECK(10, "ulong ascending", ulong_order_is(lp, asc, 5));
   CHECK(11, "ulong ascending: back links consistent", back_links_consistent(lp));

   // T12: descending
   lSortOrder *so = lParseSortOrderVarArg(TEST_Type, "%I-", TEST_ulong);
   lSortList(lp, so);
   CHECK(12, "ulong descending", ulong_order_is(lp, desc, 5));
   lFreeSortOrder(&so);
   lFreeList(&lp);

   // T13: two keys, the first ascending, the second descending
   lp = lCreateList("two", TEST_Type);
   const char *grp[] = {"b", "a", "b", "a"};
   const lUlong val[] = {1, 1, 2, 2};
   for (int i = 0; i < 4; i++) {
      lListElem *ep = lAddElemStr(&lp, TEST_string, grp[i], TEST_Type);
      lSetUlong(ep, TEST_ulong, val[i]);
   }
   so = lParseSortOrderVarArg(TEST_Type, "%I+%I-", TEST_string, TEST_ulong);
   lSortList(lp, so);
   const lUlong two_expected[] = {2, 1, 2, 1};
   const lListElem *first = lFirst(lp);
   const lListElem *third = lNext(lNext(first));
   CHECK(13, "string ascending, then ulong descending",
         ulong_order_is(lp, two_expected, 4) &&
         strcmp(lGetString(first, TEST_string), "a") == 0 &&
         strcmp(lGetString(third, TEST_string), "b") == 0);
   lFreeSortOrder(&so);
   lFreeList(&lp);

   // T14-T19: one ascending sort per remaining data type. Each element gets the
   // same rank in every field, so after sorting by any one field TEST_ulong
   // (holding the rank) must read 0, 1, 2.
   const lUlong ranks[] = {0, 1, 2};
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
      lp = lCreateList("types", TEST_Type);
      const int order[] = {2, 0, 1};
      for (int i = 0; i < 3; i++) {
         int r = order[i];
         char name[16];
         snprintf(name, sizeof(name), "h%d", r);
         lListElem *ep = lCreateElem(TEST_Type);
         lSetUlong(ep, TEST_ulong, r);
         lSetInt(ep, TEST_int, r - 1);
         lSetLong(ep, TEST_long, -100 + r);
         lSetDouble(ep, TEST_double, 0.5 * r);
         lSetUlong64(ep, TEST_ulong64, 0x100000000ULL * r);
         lSetHost(ep, TEST_host, name);
         lSetString(ep, TEST_string, name);
         lAppendElem(lp, ep);
      }
      lPSortList(lp, "%I+", fields[f].nm);
      CHECK(14 + f, fields[f].label, ulong_order_is(lp, ranks, 3));
      lFreeList(&lp);
   }

   // T20: bool sorts false before true
   lp = lCreateList("bool", TEST_Type);
   for (int i = 0; i < 3; i++) {
      lListElem *ep = lCreateElem(TEST_Type);
      lSetBool(ep, TEST_bool, i != 1);
      lSetUlong(ep, TEST_ulong, i == 1 ? 0 : 1);
      lAppendElem(lp, ep);
   }
   lPSortList(lp, "%I+", TEST_bool);
   CHECK(20, "bool ascending: false first", lGetBool(lFirst(lp), TEST_bool) == false);
   lFreeList(&lp);

   // T21: a nullptr string sorts before any other string
   lp = lCreateList("null", TEST_Type);
   lAddElemStr(&lp, TEST_string, "a", TEST_Type);
   lAppendElem(lp, lCreateElem(TEST_Type));
   lPSortList(lp, "%I+", TEST_string);
   CHECK(21, "nullptr string sorts first", lGetString(lFirst(lp), TEST_string) == nullptr);
   lFreeList(&lp);

   // T22-T23: trivial lists are accepted as sorted
   so = lParseSortOrderVarArg(TEST_Type, "%I+", TEST_ulong);
   CHECK(22, "lSortList on a nullptr list returns 0", lSortList(nullptr, so) == 0);
   const lUlong one[] = {7};
   lp = make_ulong_list(one, 1);
   CHECK(23, "lSortList on a single element returns 0 and keeps it",
         lSortList(lp, so) == 0 && ulong_order_is(lp, one, 1) && back_links_consistent(lp));
   lFreeList(&lp);
   lFreeSortOrder(&so);
}

// ---------------------------------------------------------------------------
// Hash tables after sorting  [T24–T26]
//
// lSortList() re-links the elements and then rebuilds the non-unique hash
// chains (cull_hash_recreate_after_sort). Every key must still resolve, and the
// non-unique hash must still yield each element exactly once.
// ---------------------------------------------------------------------------

static void test_sort_keeps_hash() {
   printf("\n--- hash tables after sorting ---\n");

   const lUlong in[] = {9, 3, 7, 1, 5, 8, 2, 6, 4};
   lList *lp = make_ulong_list(in, 9);
   CHECK(24, "hashes consistent before sorting", hashes_consistent(lp));

   lPSortList(lp, "%I+", TEST_ulong);
   CHECK(25, "hashes consistent after ascending sort", hashes_consistent(lp));

   lPSortList(lp, "%I-", TEST_ulong);
   CHECK(26, "hashes consistent after descending sort", hashes_consistent(lp));
   lFreeList(&lp);
}

// ---------------------------------------------------------------------------
// lInsertSorted / lResortElem  [T27–T34]
//
// lInsertSorted() puts an element before the first element that does not sort
// before it, so a sorted list stays sorted. lResortElem() moves an element
// whose key changed back to its place. Both go through the hash-aware list
// operations, so the new or moved element must be findable by its key.
// ---------------------------------------------------------------------------

static void test_insert_and_resort() {
   printf("\n--- lInsertSorted / lResortElem ---\n");

   const lUlong in[] = {2, 4, 6};
   lList *lp = make_ulong_list(in, 3);
   lSortOrder *so = lParseSortOrderVarArg(TEST_Type, "%I+", TEST_ulong);

   auto insert = [&](lUlong v) {
      char key[32];
      snprintf(key, sizeof(key), "k%u", (unsigned) v);
      lListElem *ep = lCreateElem(TEST_Type);
      lSetUlong(ep, TEST_ulong, v);
      lSetUlong(ep, TEST_ulong_hashed, v % 2);
      lSetString(ep, TEST_string_ukey, key);
      return lInsertSorted(so, ep, lp);
   };

   // T27-T29: at the front, in the middle, at the end
   insert(1);
   const lUlong e1[] = {1, 2, 4, 6};
   CHECK(27, "lInsertSorted at the front", ulong_order_is(lp, e1, 4));
   insert(5);
   const lUlong e2[] = {1, 2, 4, 5, 6};
   CHECK(28, "lInsertSorted in the middle", ulong_order_is(lp, e2, 5));
   insert(9);
   const lUlong e3[] = {1, 2, 4, 5, 6, 9};
   CHECK(29, "lInsertSorted at the end", ulong_order_is(lp, e3, 6) && back_links_consistent(lp));

   // T30: an equal key goes before the existing one
   lListElem *dup = lCreateElem(TEST_Type);
   lSetUlong(dup, TEST_ulong, 4);
   lSetString(dup, TEST_string_ukey, "dup4");
   lInsertSorted(so, dup, lp);
   CHECK(30, "lInsertSorted puts an equal key first", lNext(lFirst(lp)) != dup &&
         lNext(lNext(lFirst(lp))) == dup);

   // T31: inserted elements are findable through the hashes
   CHECK(31, "inserted elements are in the hash tables", hashes_consistent(lp));

   // T32: nullptr arguments are rejected
   CHECK(32, "lInsertSorted rejects nullptr arguments", lInsertSorted(so, nullptr, lp) == -1);

   // T33-T34: move the first element to the end after raising its key
   lListElem *head = lFirstRW(lp);
   lSetUlong(head, TEST_ulong, 100);
   lResortElem(so, head, lp);
   CHECK(33, "lResortElem moves a raised key to the end", lLast(lp) == head && back_links_consistent(lp));
   CHECK(34, "lResortElem keeps the hash tables consistent", hashes_consistent(lp));

   lFreeSortOrder(&so);
   lFreeList(&lp);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_sort_order();
   test_sort_compare();
   test_sort_list();
   test_sort_keeps_hash();
   test_insert_and_resort();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
