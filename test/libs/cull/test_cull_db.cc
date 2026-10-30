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
 * @brief Unit tests for list moves and the database functions in `libs/cull`
 *
 * Covers the functions that move elements between lists (lAppendList,
 * lDechainList, lAddSubList, lOverrideStrList, lSplit, lSelectDestroy), the
 * navigation helpers (lPrev, lGetElemIndex, lFindLastRW, lFindNextRW,
 * lFindPrevRW, lCreateElemList), the string list converters (lString2List,
 * lString2ListNone, lDiffListStr) and lJoin.
 *
 * A moved element takes its descriptor from the target list, and with it the
 * target's hash tables. After every move the tests check that each element is
 * found by its key in the list it now belongs to, and no longer in the one it
 * left.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/stat.h>
#include <unistd.h>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"
#include "cull/cull_db.h"
#include "cull/cull_list.h"
#include "cull/cull_multitype.h"
#include "cull/cull_what.h"
#include "cull/cull_where.h"

// ---------------------------------------------------------------------------
// Local CULL types used by all tests
// ---------------------------------------------------------------------------

/** @brief The main synthetic CULL object type, defined only for these tests
 *
 * A unique hashed string key, a non-unique hashed group number, a host for
 * lString2List, a non-unique hashed string for lOverrideStrList and a sublist
 * for lAddSubList.
 */
enum {
   TEST_key = 1,   ///< a string attribute that is a unique hashed key
   TEST_group,   ///< an unsigned long attribute with a non-unique hash
   TEST_host,   ///< a host name attribute
   TEST_opt,   ///< a string attribute with a non-unique hash, the field lOverrideStrList works on
   TEST_list   ///< a sublist of the same type
};

LISTDEF(TEST_Type)
   SGE_STRING (TEST_key,   CULL_UNIQUE | CULL_HASH)
   SGE_ULONG  (TEST_group, CULL_HASH)
   SGE_HOST   (TEST_host,  CULL_DEFAULT)
   SGE_STRING (TEST_opt,   CULL_HASH)
   SGE_LIST   (TEST_list,  TEST_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(TEST_Name)
   NAME("TEST_key")
   NAME("TEST_group")
   NAME("TEST_host")
   NAME("TEST_opt")
   NAME("TEST_list")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)    ///< number of attributes of the main type

/** @brief A second synthetic type, the right-hand side of lJoin
 *
 * Its names start at 101 so a joined descriptor can hold fields of both types.
 */
enum {
   OTHER_group = 101,   ///< the join field, matched against TEST_group
   OTHER_label   ///< a string carried into the join result
};

LISTDEF(OTHER_Type)
   SGE_ULONG  (OTHER_group, CULL_DEFAULT)
   SGE_STRING (OTHER_label, CULL_DEFAULT)
LISTEND

NAMEDEF(OTHER_Name)
   NAME("OTHER_group")
   NAME("OTHER_label")
NAMEEND

#define OTHER_Size sizeof(OTHER_Name) / sizeof(char *)    ///< number of attributes of the second type

/** @brief A third type with a different layout, for the descriptor mismatch checks */
enum {
   ALIEN_key = 201   ///< a string attribute
};

LISTDEF(ALIEN_Type)
   SGE_STRING (ALIEN_key, CULL_DEFAULT)
LISTEND

NAMEDEF(ALIEN_Name)
   NAME("ALIEN_key")
NAMEEND

#define ALIEN_Size sizeof(ALIEN_Name) / sizeof(char *)    ///< number of attributes of the third type

/** @brief The name space registering the synthetic types with CULL */
lNameSpace nmv[] = {
   {1, TEST_Size, TEST_Name, TEST_Type},
   {101, OTHER_Size, OTHER_Name, OTHER_Type},
   {201, ALIEN_Size, ALIEN_Name, ALIEN_Type},
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

/** @brief Build a list with keys `<prefix>0` .. `<prefix><n-1>`
 *
 * Element i carries TEST_group = i % 2.
 *
 * @param prefix the key prefix, distinct per list so keys never collide
 * @param n the number of elements
 * @return the new list, owned by the caller
 */
static lList *make_list(const char *prefix, int n) {
   lList *lp = lCreateList(prefix, TEST_Type);
   for (int i = 0; i < n; i++) {
      char key[32];
      snprintf(key, sizeof(key), "%s%d", prefix, i);
      lListElem *ep = lCreateElem(TEST_Type);
      lSetString(ep, TEST_key, key);
      lSetUlong(ep, TEST_group, i % 2);
      lAppendElem(lp, ep);
   }
   return lp;
}

/** @brief Check that a list's hash tables match its elements
 *
 * Every element must be found through its unique key, and for each group the
 * non-unique hash must yield exactly the elements of that group.
 *
 * @param lp the list to inspect; nullptr counts as consistent
 * @return true when both hash tables agree with the list
 */
static bool hashes_consistent(const lList *lp) {
   if (lp == nullptr) {
      return true;
   }
   const lListElem *ep;
   int expected[2] = {0, 0};
   for_each_ep(ep, lp) {
      if (lGetElemStr(lp, TEST_key, lGetString(ep, TEST_key)) != ep) {
         return false;
      }
      expected[lGetUlong(ep, TEST_group) % 2]++;
   }
   for (lUlong g = 0; g < 2; g++) {
      int found = 0;
      const void *it = nullptr;
      for (const lListElem *h = lGetElemUlongFirst(lp, TEST_group, g, &it); h != nullptr;
           h = lGetElemUlongNext(lp, TEST_group, g, &it)) {
         if (lGetUlong(h, TEST_group) != g) {
            return false;
         }
         found++;
      }
      if (found != expected[g]) {
         return false;
      }
   }
   return true;
}

/** @brief Check that the prev pointers mirror the next pointers
 *
 * @param lp the list to inspect; nullptr counts as consistent
 * @return true when walking backwards from lLast reaches every element once
 */
static bool back_links_consistent(const lList *lp) {
   if (lp == nullptr) {
      return true;
   }
   int n = 0;
   const lListElem *expected_next = nullptr;
   for (const lListElem *ep = lLast(lp); ep != nullptr; ep = lPrev(ep)) {
      if (lNext(ep) != expected_next) {
         return false;
      }
      expected_next = ep;
      n++;
   }
   return n == nelem(lp) && expected_next == lFirst(lp);
}

// ---------------------------------------------------------------------------
// Navigation  [T01–T09]
// ---------------------------------------------------------------------------

static void test_navigation() {
   printf("\n--- navigation ---\n");

   lList *lp = lCreateElemList("blank", TEST_Type, 3);
   CHECK(1, "lCreateElemList creates the requested number of elements", nelem(lp) == 3);
   lFreeList(&lp);

   lp = make_list("n", 5);   // groups 0 1 0 1 0
   const lListElem *third = lNext(lNext(lFirst(lp)));
   CHECK(2, "lGetElemIndex counts from 0", lGetElemIndex(third, lp) == 2);
   lListElem *loose = lCreateElem(TEST_Type);
   CHECK(3, "lGetElemIndex of an element outside any list is -1", lGetElemIndex(loose, lp) == -1);
   lFreeElem(&loose);

   CHECK(4, "lPrev/lPrevRW step back", lPrev(third) == lNext(lFirst(lp)) && lPrevRW(lFirst(lp)) == nullptr);

   lCondition *odd = lWhere("%T(%I==%u)", TEST_Type, TEST_group, 1);
   lListElem *last_odd = lFindLastRW(lp, odd);
   CHECK(5, "lFindLastRW finds the last match", last_odd != nullptr &&
         strcmp(lGetString(last_odd, TEST_key), "n3") == 0);
   lListElem *prev_odd = lFindPrevRW(last_odd, odd);
   CHECK(6, "lFindPrevRW finds the previous match", prev_odd != nullptr &&
         strcmp(lGetString(prev_odd, TEST_key), "n1") == 0);
   CHECK(7, "lFindPrevRW before the first match is nullptr", lFindPrevRW(prev_odd, odd) == nullptr);
   CHECK(8, "lFindNextRW finds the next match", lFindNextRW(prev_odd, odd) == last_odd);
   CHECK(9, "lFindNextRW after the last match is nullptr", lFindNextRW(last_odd, odd) == nullptr);
   lFreeWhere(&odd);

   lFreeList(&lp);
}

// ---------------------------------------------------------------------------
// Moving elements between lists  [T10–T27]
// ---------------------------------------------------------------------------

static void test_moves() {
   printf("\n--- moving elements between lists ---\n");

   // T10-T13: lAppendList moves all elements and leaves the source empty
   lList *a = make_list("a", 3);
   lList *b = make_list("b", 4);
   CHECK(10, "lAppendList returns 0", lAppendList(a, b) == 0);
   CHECK(11, "lAppendList moves every element", nelem(a) == 7 && nelem(b) == 0);
   CHECK(12, "lAppendList: moved elements are found in the target",
         hashes_consistent(a) && back_links_consistent(a) && lGetElemStr(a, TEST_key, "b3") != nullptr);
   CHECK(13, "lAppendList: moved elements are gone from the source", lGetElemStr(b, TEST_key, "b0") == nullptr);
   lFreeList(&b);

   // T14: different descriptors are refused
   lList *alien = lCreateList("alien", ALIEN_Type);
   CHECK(14, "lAppendList with a different descriptor fails", lAppendList(a, alien) == -1);
   lFreeList(&alien);

   // T15-T18: lDechainList moves the tail into a new list
   lListElem *at = lGetElemStrRW(a, TEST_key, "b0");   // fourth element
   lList *tail = nullptr;
   lDechainList(a, &tail, at);
   CHECK(15, "lDechainList splits at the element", nelem(a) == 3 && nelem(tail) == 4 &&
         lFirst(tail) == at);
   CHECK(16, "lDechainList: both halves have consistent links", back_links_consistent(a) && back_links_consistent(tail));
   CHECK(17, "lDechainList: both halves have consistent hashes", hashes_consistent(a) && hashes_consistent(tail));
   CHECK(18, "lDechainList: moved keys left the source",
         lGetElemStr(a, TEST_key, "b0") == nullptr && lGetElemStr(tail, TEST_key, "a0") == nullptr);

   // T19-T20: lDechainList into an existing list appends
   lListElem *a1 = lGetElemStrRW(a, TEST_key, "a1");
   lDechainList(a, &tail, a1);
   CHECK(19, "lDechainList appends to an existing target",
         nelem(a) == 1 && nelem(tail) == 6 && lLast(tail) == lGetElemStr(tail, TEST_key, "a2"));
   CHECK(20, "lDechainList: target still consistent", back_links_consistent(tail) && hashes_consistent(tail) &&
         hashes_consistent(a));
   lFreeList(&a);
   lFreeList(&tail);

   // T21-T24: lAddSubList creates, then extends the sublist
   lListElem *holder = lCreateElem(TEST_Type);
   lList *first = make_list("s", 2);
   lAddSubList(holder, TEST_list, first);
   CHECK(21, "lAddSubList into an empty field stores the list", lGetList(holder, TEST_list) == first);
   lList *more = make_list("t", 2);
   lAddSubList(holder, TEST_list, more);
   const lList *sub = lGetList(holder, TEST_list);
   CHECK(22, "lAddSubList into a filled field appends", nelem(sub) == 4);
   CHECK(23, "lAddSubList: appended keys are found", hashes_consistent(sub) && lGetElemStr(sub, TEST_key, "t1") != nullptr);
   lList *empty = lCreateList("empty", TEST_Type);
   lAddSubList(holder, TEST_list, empty);
   CHECK(24, "lAddSubList with an empty list leaves the field unchanged", nelem(lGetList(holder, TEST_list)) == 4);
   lFreeList(&empty);   // an empty list is not consumed
   lFreeElem(&holder);

   // T25-T27: lOverrideStrList replaces every occurrence of the override string
   // in the target by the entries of the update list
   lList *opts = lCreateList("opts", TEST_Type);
   const char *old_opts[] = {"-q", "-l", "-q"};
   for (int i = 0; i < 3; i++) {
      char key[32];
      snprintf(key, sizeof(key), "o%d", i);
      lListElem *ep = lCreateElem(TEST_Type);
      lSetString(ep, TEST_key, key);
      lSetString(ep, TEST_opt, old_opts[i]);
      lAppendElem(opts, ep);
   }
   lList *update = lCreateList("update", TEST_Type);
   lListElem *nq = lCreateElem(TEST_Type);
   lSetString(nq, TEST_key, "u0");
   lSetString(nq, TEST_opt, "-q");
   lAppendElem(update, nq);
   CHECK(25, "lOverrideStrList returns 0", lOverrideStrList(opts, update, TEST_opt, "-q") == 0);
   // lOverrideStrList frees the update list itself
   const lListElem *q = lGetElemStr(opts, TEST_opt, "-q");
   CHECK(26, "lOverrideStrList replaced both old -q entries by the new one",
         q != nullptr && strcmp(lGetString(q, TEST_key), "u0") == 0 && nelem(opts) == 2);
   CHECK(27, "lOverrideStrList kept the other options",
         lGetElemStr(opts, TEST_opt, "-l") != nullptr && hashes_consistent(opts));
   lFreeList(&opts);
}

// ---------------------------------------------------------------------------
// Selecting by condition  [T28–T35]
// ---------------------------------------------------------------------------

static void test_select() {
   printf("\n--- selecting by condition ---\n");

   lCondition *even = lWhere("%T(%I==%u)", TEST_Type, TEST_group, 0);

   // T28-T30: lSplit moves the non-matching elements into a new list
   lList *src = make_list("x", 5);   // groups 0 1 0 1 0
   lList *rest = nullptr;
   CHECK(28, "lSplit returns 0", lSplit(&src, &rest, "rest", even) == 0);
   CHECK(29, "lSplit keeps the matches and moves the others",
         nelem(src) == 3 && nelem(rest) == 2 && lGetElemStr(rest, TEST_key, "x1") != nullptr);
   CHECK(30, "lSplit: both lists consistent", hashes_consistent(src) && hashes_consistent(rest) &&
         back_links_consistent(src) && back_links_consistent(rest));

   // T31: lSplit without a target list frees the non-matching elements
   lFreeList(&src);
   src = make_list("y", 4);
   lSplit(&src, nullptr, nullptr, even);
   CHECK(31, "lSplit without a target drops the non-matching elements",
         nelem(src) == 2 && hashes_consistent(src));

   // T32: when nothing matches, the source list is freed
   lCondition *none = lWhere("%T(%I==%u)", TEST_Type, TEST_group, 5);
   lSplit(&src, nullptr, nullptr, none);
   CHECK(32, "lSplit leaving nothing frees the source", src == nullptr);

   // T33: when everything matches, no target list is created
   lList *all = make_list("z", 3);
   lList *nothing = nullptr;
   lCondition *any = lWhere("%T(%I<%u)", TEST_Type, TEST_group, 5);
   lSplit(&all, &nothing, "nothing", any);
   CHECK(33, "lSplit with everything matching creates no target", nothing == nullptr && nelem(all) == 3);
   lFreeList(&all);

   // T34-T35: lSelectDestroy keeps the matching elements of the same list
   lList *sel = make_list("w", 6);
   sel = lSelectDestroy(sel, even);
   CHECK(34, "lSelectDestroy keeps the matches", nelem(sel) == 3 && hashes_consistent(sel));
   sel = lSelectDestroy(sel, none);
   CHECK(35, "lSelectDestroy without a match returns nullptr", sel == nullptr);

   lFreeWhere(&even);
   lFreeWhere(&none);
   lFreeWhere(&any);
}

// ---------------------------------------------------------------------------
// String lists  [T36–T44]
// ---------------------------------------------------------------------------

static void test_string_lists() {
   printf("\n--- string lists ---\n");

   // T36-T37: lString2List splits, skips duplicates
   lList *lp = nullptr;
   CHECK(36, "lString2List returns 0", lString2List("a,b a,c", &lp, TEST_Type, TEST_key, ", ") == 0);
   CHECK(37, "lString2List drops duplicates", nelem(lp) == 3 && hashes_consistent(lp));
   lFreeList(&lp);

   // T38: lString2List on a host field
   lString2List("h1 h2 h1", &lp, TEST_Type, TEST_host, " ");
   CHECK(38, "lString2List on a host field", nelem(lp) == 2 && lGetElemHost(lp, TEST_host, "h2") != nullptr);
   lFreeList(&lp);

   // T39: lString2List of nullptr is an error
   CHECK(39, "lString2List of nullptr returns 1", lString2List(nullptr, &lp, TEST_Type, TEST_key, " ") == 1);

   // T40-T42: lString2ListNone treats "none" as empty
   CHECK(40, "lString2ListNone(\"none\") gives no list",
         lString2ListNone("NONE", &lp, TEST_Type, TEST_key, " ") == 0 && lp == nullptr);
   CHECK(41, "lString2ListNone with \"none\" among others is an error",
         lString2ListNone("a none", &lp, TEST_Type, TEST_key, " ") == 1 && lp == nullptr);
   CHECK(42, "lString2ListNone without \"none\" is lString2List",
         lString2ListNone("a b", &lp, TEST_Type, TEST_key, " ") == 0 && nelem(lp) == 2);
   lFreeList(&lp);

   // T43-T44: lDiffListStr removes the common keys from both lists
   lList *l1 = nullptr;
   lList *l2 = nullptr;
   lString2List("a b c", &l1, TEST_Type, TEST_key, " ");
   lString2List("b c d", &l2, TEST_Type, TEST_key, " ");
   lDiffListStr(TEST_key, &l1, &l2);
   CHECK(43, "lDiffListStr leaves only a in the first list",
         nelem(l1) == 1 && lGetElemStr(l1, TEST_key, "a") != nullptr && hashes_consistent(l1));
   CHECK(44, "lDiffListStr leaves only d in the second list",
         nelem(l2) == 1 && lGetElemStr(l2, TEST_key, "d") != nullptr && hashes_consistent(l2));
   lFreeList(&l1);
   lFreeList(&l2);
}

// ---------------------------------------------------------------------------
// lJoin  [T45–T47]
//
// lJoin builds the cross product of two lists restricted to equal join
// fields, with a descriptor made of the selected fields of both types.
// ---------------------------------------------------------------------------

static void test_join() {
   printf("\n--- lJoin ---\n");

   lList *left = make_list("j", 4);   // groups 0 1 0 1
   lList *right = lCreateList("right", OTHER_Type);
   for (lUlong g = 0; g < 2; g++) {
      lListElem *ep = lCreateElem(OTHER_Type);
      lSetUlong(ep, OTHER_group, g);
      lSetString(ep, OTHER_label, g == 0 ? "even" : "odd");
      lAppendElem(right, ep);
   }
   lEnumeration *what_left = lWhat("%T(%I %I)", TEST_Type, TEST_key, TEST_group);
   lEnumeration *what_right = lWhat("%T(%I)", OTHER_Type, OTHER_label);

   lList *joined = lJoin("joined", TEST_group, left, nullptr, what_left, OTHER_group, right, nullptr, what_right);
   CHECK(45, "lJoin pairs every left element with its one partner", joined != nullptr && nelem(joined) == 4);

   bool labels_ok = joined != nullptr;
   const lListElem *ep;
   for_each_ep(ep, joined) {
      const char *expected = lGetUlong(ep, TEST_group) == 0 ? "even" : "odd";
      if (strcmp(lGetString(ep, OTHER_label), expected) != 0) {
         labels_ok = false;
      }
   }
   CHECK(46, "lJoin carries fields of both types", labels_ok);
   CHECK(47, "lJoin result has the fields of both sides only",
         joined != nullptr && lCountDescr(lGetListDescr(joined)) == 3);

   lFreeList(&joined);
   lFreeWhat(&what_left);
   lFreeWhat(&what_right);
   lFreeList(&left);
   lFreeList(&right);
}

// ---------------------------------------------------------------------------
// Throwaway bootstrap for the host compare
// ---------------------------------------------------------------------------

/** @brief The temporary SGE_ROOT created by #setup_bootstrap */
static char s_sge_root[128];

/** @brief Create a throwaway SGE_ROOT holding a bootstrap file
 *
 * Host lookups compare names with sge_hostcmp(), which consults ocs::Bootstrap
 * for ignore_fqdn and default_domain. Bootstrap loads
 * $SGE_ROOT/$SGE_CELL/common/bootstrap on first access and exits if it cannot,
 * so a minimal one is created here, the same way test_sgeobj_hgroup does.
 *
 * @param name a name for the directory, unique per test program
 * @return true when the directory and file were created and the environment set
 */
static bool setup_bootstrap(const char *name) {
   char path[256];

   snprintf(s_sge_root, sizeof(s_sge_root), "/tmp/%s_%ld", name, (long) getpid());
   if (mkdir(s_sge_root, 0755) != 0) {
      return false;
   }
   snprintf(path, sizeof(path), "%s/default", s_sge_root);
   if (mkdir(path, 0755) != 0) {
      return false;
   }
   snprintf(path, sizeof(path), "%s/default/common", s_sge_root);
   if (mkdir(path, 0755) != 0) {
      return false;
   }
   snprintf(path, sizeof(path), "%s/default/common/bootstrap", s_sge_root);

   FILE *fp = fopen(path, "w");
   if (fp == nullptr) {
      return false;
   }
   fprintf(fp,
           "admin_user        none\n"
           "default_domain    none\n"
           "ignore_fqdn       true\n"
           "spooling_method   classic\n"
           "spooling_lib      none\n"
           "spooling_params   none\n"
           "binary_path       none\n"
           "qmaster_spool_dir none\n"
           "security_mode     none\n");
   fclose(fp);

   setenv("SGE_ROOT", s_sge_root, 1);
   setenv("SGE_CELL", "default", 1);
   return true;
}

/** @brief Remove the directory tree created by #setup_bootstrap */
static void teardown_bootstrap() {
   char path[256];

   snprintf(path, sizeof(path), "%s/default/common/bootstrap", s_sge_root);
   unlink(path);
   snprintf(path, sizeof(path), "%s/default/common", s_sge_root);
   rmdir(path);
   snprintf(path, sizeof(path), "%s/default", s_sge_root);
   rmdir(path);
   rmdir(s_sge_root);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);
   if (!setup_bootstrap("test_cull_db")) {
      printf("FAIL  cannot create the temporary bootstrap file\n");
      return 1;
   }

   test_navigation();
   test_moves();
   test_select();
   test_string_lists();
   test_join();

   teardown_bootstrap();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
