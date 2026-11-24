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
 * @brief Unit tests for list moves and the database functions of the list core in `libs/tom`
 *
 * Covers the functions that move elements between lists (append_list(),
 * split_from(), split_list()), the navigation helpers (prev(), find_last(),
 * find_next(), find_prev()) and the string list converters
 * (string_to_list(), string_to_list_none(), remove_common_str()). They
 * replace lAppendList(), lDechainList(), lSplit(), lSelectDestroy(), lPrev(),
 * lFindLastRW(), lFindNextRW(), lFindPrevRW(), lString2List(),
 * lString2ListNone() and lDiffListStr().
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

/** @brief Make this translation unit the one that defines the descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/ocs_Types.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_ListOpsCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SelectCore.h"
#include "ocs_test_model.h"

// ---------------------------------------------------------------------------
// Local object types used by all tests
// ---------------------------------------------------------------------------

/** @brief The main synthetic object type, defined only for these tests
 *
 * A unique hashed string key, a non-unique hashed group number, a host for
 * string_to_list(), a non-unique hashed string and a sublist.
 */
enum {
   TEST_key = 1,   ///< a string attribute that is a unique hashed key
   TEST_group,   ///< an unsigned long attribute with a non-unique hash
   TEST_host,   ///< a host name attribute
   TEST_opt,   ///< a string attribute with a non-unique hash
   TEST_list   ///< a sublist of the same type
};

LISTDEF(TEST_Type)
   SGE_STRING (TEST_key,   MT_UNIQUE | MT_HASH)
   SGE_ULONG  (TEST_group, MT_HASH)
   SGE_HOST   (TEST_host,  MT_DEFAULT)
   SGE_STRING (TEST_opt,   MT_HASH)
   SGE_LIST   (TEST_list,  TEST_Type, MT_DEFAULT)
LISTEND(TEST_Type)

NAMEDEF(TEST_Name)
   NAME("TEST_key")
   NAME("TEST_group")
   NAME("TEST_host")
   NAME("TEST_opt")
   NAME("TEST_list")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)    ///< number of attributes of the main type

/** @brief A second type with a different layout, for the descriptor mismatch checks */
enum {
   ALIEN_key = 201   ///< a string attribute
};

LISTDEF(ALIEN_Type)
   SGE_STRING (ALIEN_key, MT_DEFAULT)
LISTEND(ALIEN_Type)

NAMEDEF(ALIEN_Name)
   NAME("ALIEN_key")
NAMEEND

#define ALIEN_Size sizeof(ALIEN_Name) / sizeof(char *)    ///< number of attributes of the second type

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
 * @brief Lends the list of an owner as a list reference to a core function that may replace or free it
 *
 * The core functions split_list(), string_to_list(), string_to_list_none() and
 * remove_common_str() take an `ocs::tom::ListData *&`; the owner gets back whatever the
 * reference holds when this object goes out of scope.
 */
class ListRef {
public:
   /**
    * @brief Take the list out of an owner
    * @param owner the owner, empty until this object is destroyed
    */
   explicit ListRef(ocs::tom::RawListPtr &owner) noexcept : owner_(owner), lp_(owner.release()) {}

   ListRef(const ListRef &) = delete;
   ListRef &operator=(const ListRef &) = delete;

   /** @brief Give the list the reference holds now back to the owner */
   ~ListRef() {
      owner_ = ocs::tom::RawListPtr(lp_);
   }

   /**
    * @brief The reference for the core function
    * @return the reference to the list, nullptr if there is none
    */
   ocs::tom::ListData *&ref() noexcept {
      return lp_;
   }

private:
   ocs::tom::RawListPtr &owner_;   ///< the owner the list goes back to
   ocs::tom::ListData *lp_;                     ///< the list lent out
};

/** @brief Build a list with keys `<prefix>0` .. `<prefix><n-1>`
 *
 * Element i carries TEST_group = i % 2.
 *
 * @param prefix the key prefix, distinct per list so keys never collide
 * @param n the number of elements
 * @return the owner of the new list
 */
static ocs::tom::RawListPtr make_list(const char *prefix, const int n) {
   auto lp = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::On, prefix);
   for (int i = 0; i < n; i++) {
      char key[32];
      snprintf(key, sizeof(key), "%s%d", prefix, i);
      auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
      ep->set(TEST_key, static_cast<const char *>(key));
      ep->set(TEST_group, static_cast<std::uint32_t>(i % 2));
      lp->push_back(std::move(ep));
   }
   return lp;
}

/** @brief The element of a list with a string key
 *
 * @param lp the list, may be empty
 * @param nm the string field
 * @param key the key
 * @return the element, nullptr if none holds the key
 */
static ocs::tom::ElementData *find_str(const ocs::tom::RawList lp, const int nm, const char *key) {
   return lp.find_first(nm, key);
}

/** @brief Check that a list's hash tables match its elements
 *
 * Every element must be found through its unique key, and for each group the
 * non-unique hash must yield exactly the elements of that group.
 *
 * @param lp the list to inspect; nullptr counts as consistent
 * @return true when both hash tables agree with the list
 */
static bool hashes_consistent(const ocs::tom::ConstRawList lp) {
   if (!lp) {
      return true;
   }
   int expected[2] = {0, 0};
   for (const ocs::tom::ConstRawElement ep : lp) {
      if (lp.find_first(TEST_key, ep.get<const char *>(TEST_key)).raw() != ep.raw()) {
         return false;
      }
      expected[ep.get<std::uint32_t>(TEST_group) % 2]++;
   }
   const int pos = ocs::tom::field_pos(lp.descr(), TEST_group);
   for (std::uint32_t g = 0; g < 2; g++) {
      int found = 0;
      const void *it = nullptr;
      for (const ocs::tom::ElementData *h = ocs::tom::lookup_first<ocs::tom::FieldType::Uint32>(lp, pos, g,
                                                                                                &it); h != nullptr;
           h = ocs::tom::lookup_next<ocs::tom::FieldType::Uint32>(lp, pos, g, &it)) {
         if (ocs::tom::get<std::uint32_t>(h, TEST_group) != g) {
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
 * @return true when walking backwards from back() reaches every element once
 */
static bool back_links_consistent(const ocs::tom::ConstRawList lp) {
   if (!lp) {
      return true;
   }
   int n = 0;
   const ocs::tom::ElementData *expected_next = nullptr;
   for (ocs::tom::ConstRawElement ep = lp.back(); ep; ep = ep.prev()) {
      if (ep.next().raw() != expected_next) {
         return false;
      }
      expected_next = ep.raw();
      n++;
   }
   return n == nelem(lp) && expected_next == lp.front().raw();
}

/** @brief The condition TEST_group == value
 *
 * @param op the comparison
 * @param value the value
 * @return the owner of the condition
 */
static ocs::tom::RawWherePtr group_is(const ocs::tom::CompareOp op, const std::uint32_t value) {
   return ocs::tom::where_compare(TEST_Type, TEST_group, op, value);
}

// ---------------------------------------------------------------------------
// Navigation  [T01–T09]
// ---------------------------------------------------------------------------

/** @brief prev(), find_last(), find_prev() and find_next() */
static void test_navigation() {
   printf("\n--- navigation ---\n");

   // CHECK 1 removed: lCreateElemList() has no TOM counterpart (CS-3103)

   ocs::tom::RawListPtr lp = make_list("n", 5);   // groups 0 1 0 1 0
   const ocs::tom::RawElement third = lp->front().next().next();
   // CHECK 2 removed: lGetElemIndex() has no TOM counterpart (CS-3103)
   // CHECK 3 removed: lGetElemIndex() has no TOM counterpart (CS-3103)

   CHECK(4, "prev() steps back", third.prev().raw() == lp->front().next().raw() && !lp->front().prev());

   ocs::tom::RawWherePtr odd = group_is(ocs::tom::CompareOp::Equal, 1);
   const ocs::tom::ElementData *last_odd = ocs::tom::find_last(lp.get(), odd.get());
   CHECK(5, "find_last() finds the last match", last_odd != nullptr &&
         strcmp(ocs::tom::get<const char *>(last_odd, TEST_key), "n3") == 0);
   const ocs::tom::ElementData *prev_odd = ocs::tom::find_prev(last_odd, odd.get());
   CHECK(6, "find_prev() finds the previous match", prev_odd != nullptr &&
         strcmp(ocs::tom::get<const char *>(prev_odd, TEST_key), "n1") == 0);
   CHECK(7, "find_prev() before the first match is nullptr", ocs::tom::find_prev(prev_odd, odd.get()) == nullptr);
   CHECK(8, "find_next() finds the next match", ocs::tom::find_next(prev_odd, odd.get()) == last_odd);
   CHECK(9, "find_next() after the last match is nullptr", ocs::tom::find_next(last_odd, odd.get()) == nullptr);
}

// ---------------------------------------------------------------------------
// Moving elements between lists  [T10–T27]
// ---------------------------------------------------------------------------

/** @brief append_list() and split_from() */
static void test_moves() {
   printf("\n--- moving elements between lists ---\n");

   // T10-T13: append_list() moves all elements and leaves the source empty
   ocs::tom::RawListPtr a = make_list("a", 3);
   ocs::tom::RawListPtr b = make_list("b", 4);
   // CHECK 10 removed: the return value 0 of lAppendList() has no TOM counterpart (CS-3103)
   ocs::tom::append_list(a.get(), b.get());
   CHECK(11, "append_list() moves every element", nelem(a.get()) == 7 && nelem(b.get()) == 0);
   CHECK(12, "append_list(): moved elements are found in the target",
         hashes_consistent(a.get()) && back_links_consistent(a.get()) && find_str(a.get(), TEST_key, "b3") != nullptr);
   CHECK(13, "append_list(): moved elements are gone from the source", find_str(b.get(), TEST_key, "b0") == nullptr);
   b.reset();

   // T14: different descriptors are refused
   auto alien = ocs::tom::RawListPtr::create(ALIEN_Type, ocs::tom::Hashing::On, "alien");
   auto alien_elem = ocs::tom::RawElementPtr::create(ALIEN_Type);
   alien_elem->set(ALIEN_key, static_cast<const char *>("x"));
   alien->push_back(std::move(alien_elem));
   bool refused = false;
   try {
      ocs::tom::append_list(a.get(), alien.get());
   } catch (const ocs::tom::FieldException &e) {
      refused = e.reason() == ocs::tom::FieldException::Reason::WrongType;
   }
   CHECK(14, "append_list() with a different descriptor throws WrongType and moves nothing",
         refused && nelem(a.get()) == 7 && nelem(alien.get()) == 1);
   alien.reset();

   // T15-T18: split_from() moves the tail into a new list
   ocs::tom::ElementData *at = find_str(a.get(), TEST_key, "b0");   // fourth element
   ocs::tom::RawListPtr tail(ocs::tom::split_from(a.get(), at, "a"));
   CHECK(15, "split_from() splits at the element", nelem(a.get()) == 3 && nelem(tail.get()) == 4 &&
         tail->front().raw() == at);
   CHECK(16, "split_from(): both halves have consistent links",
         back_links_consistent(a.get()) && back_links_consistent(tail.get()));
   CHECK(17, "split_from(): both halves have consistent hashes",
         hashes_consistent(a.get()) && hashes_consistent(tail.get()));
   CHECK(18, "split_from(): moved keys left the source",
         find_str(a.get(), TEST_key, "b0") == nullptr && find_str(tail.get(), TEST_key, "a0") == nullptr);

   // T19-T20: lDechainList() into an existing list appended; that is split_from() and append_list()
   ocs::tom::ElementData *a1 = find_str(a.get(), TEST_key, "a1");
   ocs::tom::RawListPtr part(ocs::tom::split_from(a.get(), a1, "a"));
   ocs::tom::append_list(tail.get(), part.get());
   part.reset();
   CHECK(19, "split_from() and append_list() append to an existing target",
         nelem(a.get()) == 1 && nelem(tail.get()) == 6 &&
         tail->back().raw() == find_str(tail.get(), TEST_key, "a2"));
   CHECK(20, "split_from() and append_list(): target still consistent",
         back_links_consistent(tail.get()) && hashes_consistent(tail.get()) && hashes_consistent(a.get()));

   // CHECK 21 removed: lAddSubList() has no TOM counterpart (CS-3103)
   // CHECK 22 removed: lAddSubList() has no TOM counterpart (CS-3103)
   // CHECK 23 removed: lAddSubList() has no TOM counterpart (CS-3103)
   // CHECK 24 removed: lAddSubList() has no TOM counterpart (CS-3103)
   // CHECK 25 removed: lOverrideStrList() has no TOM counterpart (CS-3103)
   // CHECK 26 removed: lOverrideStrList() has no TOM counterpart (CS-3103)
   // CHECK 27 removed: lOverrideStrList() has no TOM counterpart (CS-3103)
}

// ---------------------------------------------------------------------------
// Selecting by condition  [T28–T35]
// ---------------------------------------------------------------------------

/** @brief split_list(), with and without a list for the elements that do not match */
static void test_select() {
   printf("\n--- selecting by condition ---\n");

   ocs::tom::RawWherePtr even = group_is(ocs::tom::CompareOp::Equal, 0);

   // T28-T30: split_list() moves the non-matching elements into a new list
   ocs::tom::RawListPtr src = make_list("x", 5);   // groups 0 1 0 1 0
   ocs::tom::RawListPtr rest;
   // CHECK 28 removed: the return value 0 of lSplit() has no TOM counterpart (CS-3103)
   {
      ListRef src_ref(src);
      ListRef rest_ref(rest);
      ocs::tom::split_list(src_ref.ref(), &rest_ref.ref(), "rest", even.get());
   }
   CHECK(29, "split_list() keeps the matches and moves the others",
         nelem(src.get()) == 3 && nelem(rest.get()) == 2 && find_str(rest.get(), TEST_key, "x1") != nullptr);
   CHECK(30, "split_list(): both lists consistent", hashes_consistent(src.get()) && hashes_consistent(rest.get()) &&
         back_links_consistent(src.get()) && back_links_consistent(rest.get()));
   rest.reset();

   // T31: split_list() without a target list frees the non-matching elements
   src = make_list("y", 4);
   {
      ListRef src_ref(src);
      ocs::tom::split_list(src_ref.ref(), nullptr, nullptr, even.get());
   }
   CHECK(31, "split_list() without a target drops the non-matching elements",
         nelem(src.get()) == 2 && hashes_consistent(src.get()));

   // T32: when nothing matches, the source list is freed
   ocs::tom::RawWherePtr none = group_is(ocs::tom::CompareOp::Equal, 5);
   {
      ListRef src_ref(src);
      ocs::tom::split_list(src_ref.ref(), nullptr, nullptr, none.get());
   }
   CHECK(32, "split_list() leaving nothing frees the source", !src);

   // T33: when everything matches, no target list is created
   ocs::tom::RawListPtr all = make_list("z", 3);
   ocs::tom::RawListPtr nothing;
   ocs::tom::RawWherePtr any = group_is(ocs::tom::CompareOp::Lower, 5);
   {
      ListRef all_ref(all);
      ListRef nothing_ref(nothing);
      ocs::tom::split_list(all_ref.ref(), &nothing_ref.ref(), "nothing", any.get());
   }
   CHECK(33, "split_list() with everything matching creates no target", !nothing && nelem(all.get()) == 3);

   // T34-T35: lSelectDestroy() was split_list() without a target list, keeping the matching elements
   ocs::tom::RawListPtr sel = make_list("w", 6);
   {
      ListRef sel_ref(sel);
      ocs::tom::split_list(sel_ref.ref(), nullptr, nullptr, even.get());
   }
   CHECK(34, "split_list() without a target keeps the matches", nelem(sel.get()) == 3 && hashes_consistent(sel.get()));
   {
      ListRef sel_ref(sel);
      ocs::tom::split_list(sel_ref.ref(), nullptr, nullptr, none.get());
   }
   CHECK(35, "split_list() without a target and without a match leaves no list", !sel);
}

// ---------------------------------------------------------------------------
// String lists  [T36–T44]
// ---------------------------------------------------------------------------

/** @brief string_to_list(), string_to_list_none() and remove_common_str() */
static void test_string_lists() {
   printf("\n--- string lists ---\n");

   // T36-T37: string_to_list() splits, skips duplicates
   ocs::tom::RawListPtr lp;
   // CHECK 36 removed: the return value 0 of lString2List() has no TOM counterpart (CS-3103)
   {
      ListRef lp_ref(lp);
      ocs::tom::string_to_list("a,b a,c", lp_ref.ref(), TEST_Type, TEST_key, ", ");
   }
   CHECK(37, "string_to_list() drops duplicates", nelem(lp.get()) == 3 && hashes_consistent(lp.get()));
   lp.reset();

   // T38: string_to_list() on a host field
   {
      ListRef lp_ref(lp);
      ocs::tom::string_to_list("h1 h2 h1", lp_ref.ref(), TEST_Type, TEST_host, " ");
   }
   CHECK(38, "string_to_list() on a host field",
         nelem(lp.get()) == 2 && lp->find_first(TEST_host, ocs::tom::Host("h2")));
   lp.reset();

   // CHECK 39 removed: the return value 1 of lString2List(nullptr) has no TOM counterpart (CS-3103)

   // T40-T42: string_to_list_none() treats "none" as empty
   bool thrown = false;
   try {
      ListRef lp_ref(lp);
      ocs::tom::string_to_list_none("NONE", lp_ref.ref(), TEST_Type, TEST_key, " ");
   } catch (const ocs::tom::ListCoreError &) {
      thrown = true;
   }
   CHECK(40, "string_to_list_none(\"none\") gives no list", !thrown && !lp);
   thrown = false;
   try {
      ListRef lp_ref(lp);
      ocs::tom::string_to_list_none("a none", lp_ref.ref(), TEST_Type, TEST_key, " ");
   } catch (const ocs::tom::ListCoreError &) {
      thrown = true;
   }
   CHECK(41, "string_to_list_none() with \"none\" among others throws ListCoreError and leaves no list",
         thrown && !lp);
   thrown = false;
   try {
      ListRef lp_ref(lp);
      ocs::tom::string_to_list_none("a b", lp_ref.ref(), TEST_Type, TEST_key, " ");
   } catch (const ocs::tom::ListCoreError &) {
      thrown = true;
   }
   CHECK(42, "string_to_list_none() without \"none\" is string_to_list()", !thrown && nelem(lp.get()) == 2);
   lp.reset();

   // T43-T44: remove_common_str() removes the common keys from both lists
   ocs::tom::RawListPtr l1;
   ocs::tom::RawListPtr l2;
   {
      ListRef l1_ref(l1);
      ListRef l2_ref(l2);
      ocs::tom::string_to_list("a b c", l1_ref.ref(), TEST_Type, TEST_key, " ");
      ocs::tom::string_to_list("b c d", l2_ref.ref(), TEST_Type, TEST_key, " ");
      ocs::tom::remove_common_str(l1_ref.ref(), l2_ref.ref(), TEST_key, true);
   }
   CHECK(43, "remove_common_str() leaves only a in the first list",
         nelem(l1.get()) == 1 && find_str(l1.get(), TEST_key, "a") != nullptr && hashes_consistent(l1.get()));
   CHECK(44, "remove_common_str() leaves only d in the second list",
         nelem(l2.get()) == 1 && find_str(l2.get(), TEST_key, "d") != nullptr && hashes_consistent(l2.get()));
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
   ocs::module_test::install_test_model({{"TEST", TEST_Type, TEST_Name, TEST_Size},
                                         {"ALIEN", ALIEN_Type, ALIEN_Name, ALIEN_Size}});
   if (!setup_bootstrap("test_tom_core_db")) {
      printf("FAIL  cannot create the temporary bootstrap file\n");
      return 1;
   }

   test_navigation();
   test_moves();
   test_select();
   test_string_lists();

   teardown_bootstrap();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
