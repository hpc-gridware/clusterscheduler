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
 * @brief Unit tests for lookup by key in `libs/cull`
 *
 * Covers lGetElem{Ulong,Ulong64,Host,Str} with their First/Next iterators and
 * RW variants, lGetElemStrLike, lGetElemCaseStr, the lDelElem* removals, and
 * the lAddSub*, lGetSub* and lDelSub* functions working on a sublist.
 *
 * Every lookup scenario runs twice: once on a list with hash tables and once
 * on a list created without them (lCreateListHash(..., false)), where the
 * same functions fall back to a linear search. Both must give the same
 * answers; a change to when or where hash tables are built must keep it so.
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
#include "cull/cull_list.h"
#include "cull/cull_multitype.h"

// ---------------------------------------------------------------------------
// Local CULL type used by all tests
// ---------------------------------------------------------------------------

/** @brief A synthetic CULL object type, defined only for these tests
 *
 * Every key type carries a non-unique hash, so a list created with hash tables
 * answers lookups through the hash chains and one created without them through
 * a linear walk.
 */
enum {
   TEST_ulong = 1,   ///< an unsigned long key with a non-unique hash
   TEST_ulong64,   ///< a 64 bit unsigned key with a non-unique hash
   TEST_host,   ///< a host name key with a non-unique hash
   TEST_string,   ///< a string key with a non-unique hash
   TEST_list   ///< a sublist of the same type, for the lAddSub/lGetSub/lDelSub functions
};

LISTDEF(TEST_Type)
   SGE_ULONG  (TEST_ulong,   CULL_HASH)
   SGE_ULONG64(TEST_ulong64, CULL_HASH)
   SGE_HOST   (TEST_host,    CULL_HASH)
   SGE_STRING (TEST_string,  CULL_HASH)
   SGE_LIST   (TEST_list,    TEST_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(TEST_Name)
   NAME("TEST_ulong")
   NAME("TEST_ulong64")
   NAME("TEST_host")
   NAME("TEST_string")
   NAME("TEST_list")
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

/** @brief Number of elements in the lists built by #make_list */
static constexpr int NELEM = 6;

/** @brief Number of distinct key values in the lists built by #make_list */
static constexpr int NKEYS = 3;

/** @brief The 64 bit key of group @p g, above 32 bit so truncation would show
 *
 * @param g the key group, 0 .. NKEYS-1
 * @return the TEST_ulong64 value of that group
 */
static lUlong64 key64(int g) {
   return 0x500000000ULL + g;
}

/** @brief Build a list of NELEM elements in NKEYS groups of equal keys
 *
 * Element i belongs to group i % NKEYS and carries TEST_ulong = group,
 * TEST_ulong64 = key64(group), TEST_host = `host<group>` and TEST_string =
 * `str<group>`, so every key value occurs NELEM / NKEYS times.
 *
 * @param hashed true for a list with hash tables, false for one without
 * @return the new list, owned by the caller
 */
static lList *make_list(bool hashed) {
   lList *lp = lCreateListHash("lookup", TEST_Type, hashed);
   for (int i = 0; i < NELEM; i++) {
      int g = i % NKEYS;
      char host[16], str[16];
      snprintf(host, sizeof(host), "host%d", g);
      snprintf(str, sizeof(str), "str%d", g);
      lListElem *ep = lCreateElem(TEST_Type);
      lSetUlong(ep, TEST_ulong, g);
      lSetUlong64(ep, TEST_ulong64, key64(g));
      lSetHost(ep, TEST_host, host);
      lSetString(ep, TEST_string, str);
      lAppendElem(lp, ep);
   }
   return lp;
}

/** @brief Count the elements the ulong First/Next iterator yields for a value
 *
 * @param lp the list to search
 * @param val the TEST_ulong value to look for
 * @return the number of elements found, or -1 if one carried a different value
 */
static int count_ulong(const lList *lp, lUlong val) {
   int n = 0;
   const void *it = nullptr;
   for (const lListElem *ep = lGetElemUlongFirst(lp, TEST_ulong, val, &it); ep != nullptr;
        ep = lGetElemUlongNext(lp, TEST_ulong, val, &it)) {
      if (lGetUlong(ep, TEST_ulong) != val) {
         return -1;
      }
      n++;
   }
   return n;
}

/** @brief Count the elements the ulong64 First/Next iterator yields for a value
 *
 * @param lp the list to search
 * @param val the TEST_ulong64 value to look for
 * @return the number of elements found, or -1 if one carried a different value
 */
static int count_ulong64(const lList *lp, lUlong64 val) {
   int n = 0;
   const void *it = nullptr;
   for (const lListElem *ep = lGetElemUlong64First(lp, TEST_ulong64, val, &it); ep != nullptr;
        ep = lGetElemUlong64Next(lp, TEST_ulong64, val, &it)) {
      if (lGetUlong64(ep, TEST_ulong64) != val) {
         return -1;
      }
      n++;
   }
   return n;
}

/** @brief Count the elements the host First/Next iterator yields for a name
 *
 * @param lp the list to search
 * @param host the TEST_host value to look for
 * @return the number of elements found, or -1 if one carried a different value
 */
static int count_host(const lList *lp, const char *host) {
   int n = 0;
   const void *it = nullptr;
   for (const lListElem *ep = lGetElemHostFirst(lp, TEST_host, host, &it); ep != nullptr;
        ep = lGetElemHostNext(lp, TEST_host, host, &it)) {
      if (strcmp(lGetHost(ep, TEST_host), host) != 0) {
         return -1;
      }
      n++;
   }
   return n;
}

/** @brief Count the elements the string First/Next iterator yields for a value
 *
 * @param lp the list to search
 * @param str the TEST_string value to look for
 * @return the number of elements found, or -1 if one carried a different value
 */
static int count_str(const lList *lp, const char *str) {
   int n = 0;
   const void *it = nullptr;
   for (const lListElem *ep = lGetElemStrFirst(lp, TEST_string, str, &it); ep != nullptr;
        ep = lGetElemStrNext(lp, TEST_string, str, &it)) {
      if (strcmp(lGetString(ep, TEST_string), str) != 0) {
         return -1;
      }
      n++;
   }
   return n;
}

/** @brief Check that every key value yields every element carrying it, in all four key types
 *
 * @param lp the list to search
 * @return true when for every group the four iterators each yield exactly the
 *         elements of that group
 */
static bool all_iterators_agree(const lList *lp) {
   int per_group[NKEYS] = {0};
   const lListElem *ep;
   for_each_ep(ep, lp) {
      per_group[lGetUlong(ep, TEST_ulong)]++;
   }
   for (int g = 0; g < NKEYS; g++) {
      char host[16], str[16];
      snprintf(host, sizeof(host), "host%d", g);
      snprintf(str, sizeof(str), "str%d", g);
      if (count_ulong(lp, g) != per_group[g] || count_ulong64(lp, key64(g)) != per_group[g] ||
          count_host(lp, host) != per_group[g] || count_str(lp, str) != per_group[g]) {
         return false;
      }
   }
   return true;
}

// ---------------------------------------------------------------------------
// Single lookup and iteration  [base+0 – base+11]
//
// lGetElemXxx returns some element carrying the key, the First/Next iterators
// every element carrying it, the RW variants the same elements writable.
// A key that does not occur yields nullptr.
// ---------------------------------------------------------------------------

/** @brief Run the lookup scenario on one list flavour
 *
 * @param hashed true to run on a list with hash tables, false without
 * @param base the first test number of this run
 */
static void test_lookup(bool hashed, int base) {
   printf("\n--- lookup by key, %s ---\n", hashed ? "hashed list" : "unhashed list");

   lList *lp = make_list(hashed);

   const lListElem *ep = lGetElemUlong(lp, TEST_ulong, 1);
   CHECK(base + 0, "lGetElemUlong finds the key", ep != nullptr && lGetUlong(ep, TEST_ulong) == 1);
   CHECK(base + 1, "lGetElemUlong of a missing key is nullptr", lGetElemUlong(lp, TEST_ulong, 99) == nullptr);

   ep = lGetElemUlong64(lp, TEST_ulong64, key64(2));
   CHECK(base + 2, "lGetElemUlong64 finds a key above 32 bit",
         ep != nullptr && lGetUlong64(ep, TEST_ulong64) == key64(2));
   CHECK(base + 3, "lGetElemUlong64 does not match the low 32 bits alone",
         lGetElemUlong64(lp, TEST_ulong64, key64(2) & 0xffffffffULL) == nullptr);

   ep = lGetElemHost(lp, TEST_host, "host0");
   CHECK(base + 4, "lGetElemHost finds the key", ep != nullptr && strcmp(lGetHost(ep, TEST_host), "host0") == 0);

   ep = lGetElemStr(lp, TEST_string, "str2");
   CHECK(base + 5, "lGetElemStr finds the key", ep != nullptr && strcmp(lGetString(ep, TEST_string), "str2") == 0);

   CHECK(base + 6, "First/Next iterators yield every element of every key", all_iterators_agree(lp));

   // RW variants address the same elements
   const void *it = nullptr;
   CHECK(base + 7, "RW variants return the same elements as the read-only ones",
         lGetElemUlongRW(lp, TEST_ulong, 1) == lGetElemUlong(lp, TEST_ulong, 1) &&
         lGetElemUlong64RW(lp, TEST_ulong64, key64(1)) == lGetElemUlong64(lp, TEST_ulong64, key64(1)) &&
         lGetElemHostRW(lp, TEST_host, "host1") == lGetElemHost(lp, TEST_host, "host1") &&
         lGetElemStrRW(lp, TEST_string, "str1") == lGetElemStr(lp, TEST_string, "str1") &&
         lGetElemUlongFirstRW(lp, TEST_ulong, 1, &it) != nullptr &&
         lGetElemUlongNextRW(lp, TEST_ulong, 1, &it) != nullptr &&
         lGetElemUlongNextRW(lp, TEST_ulong, 1, &it) == nullptr);

   // lGetElemStrLike: exact match or a trailing '*' as prefix wildcard
   ep = lGetElemStrLike(lp, TEST_string, "str*");
   CHECK(base + 8, "lGetElemStrLike with a trailing '*' matches the prefix",
         ep != nullptr && strncmp(lGetString(ep, TEST_string), "str", 3) == 0);
   CHECK(base + 9, "lGetElemStrLike without a match returns nullptr",
         lGetElemStrLike(lp, TEST_string, "xyz*") == nullptr && lGetElemStrLike(lp, TEST_string, "str") == nullptr);

   // lGetElemCaseStr ignores case
   ep = lGetElemCaseStr(lp, TEST_string, "STR1");
   CHECK(base + 10, "lGetElemCaseStr ignores case", ep != nullptr && strcmp(lGetString(ep, TEST_string), "str1") == 0);

   // changing a key in place moves the element to the other key
   lListElem *mod = lGetElemStrRW(lp, TEST_string, "str0");
   lSetUlong(mod, TEST_ulong, 1);
   lSetUlong64(mod, TEST_ulong64, key64(1));
   lSetHost(mod, TEST_host, "host1");
   lSetString(mod, TEST_string, "str1");
   CHECK(base + 11, "after changing all keys of an element the iterators still agree", all_iterators_agree(lp) &&
         count_ulong(lp, 1) == NELEM / NKEYS + 1 && count_str(lp, "str0") == NELEM / NKEYS - 1);

   lFreeList(&lp);
}

// ---------------------------------------------------------------------------
// Removal by key  [base+0 – base+7]
//
// lDelElemXxx removes one element carrying the key and frees the list when it
// becomes empty. The remaining elements must still be found by their keys.
// ---------------------------------------------------------------------------

/** @brief Run the removal scenario on one list flavour
 *
 * @param hashed true to run on a list with hash tables, false without
 * @param base the first test number of this run
 */
static void test_delete(bool hashed, int base) {
   printf("\n--- removal by key, %s ---\n", hashed ? "hashed list" : "unhashed list");

   lList *lp = make_list(hashed);

   CHECK(base + 0, "lDelElemUlong removes one element of the key",
         lDelElemUlong(&lp, TEST_ulong, 1) == 1 && count_ulong(lp, 1) == NELEM / NKEYS - 1 &&
         nelem(lp) == NELEM - 1);
   CHECK(base + 1, "lDelElemUlong64 removes the other element of the key",
         lDelElemUlong64(&lp, TEST_ulong64, key64(1)) == 1 && count_ulong64(lp, key64(1)) == 0);
   CHECK(base + 2, "lDelElemHost removes one element of the key",
         lDelElemHost(&lp, TEST_host, "host2") == 1 && count_host(lp, "host2") == NELEM / NKEYS - 1);
   CHECK(base + 3, "lDelElemStr of a missing key removes nothing",
         lDelElemStr(&lp, TEST_string, "nope") == 0 && nelem(lp) == NELEM - 3);
   CHECK(base + 4, "lDelElemStr removes one element of the key",
         lDelElemStr(&lp, TEST_string, "str0") == 1 && count_str(lp, "str0") == NELEM / NKEYS - 1);
   CHECK(base + 5, "the remaining elements are still found by every key", all_iterators_agree(lp));

   // remove what is left; the last removal frees the list
   while (lp != nullptr && nelem(lp) > 1) {
      lDelElemStr(&lp, TEST_string, lGetString(lFirst(lp), TEST_string));
   }
   CHECK(base + 6, "one element left", lp != nullptr && nelem(lp) == 1);
   lDelElemStr(&lp, TEST_string, lGetString(lFirst(lp), TEST_string));
   CHECK(base + 7, "removing the last element frees the list", lp == nullptr);
}

// ---------------------------------------------------------------------------
// Sublist helpers  [T41–T54]
//
// lAddSubXxx creates the sublist on first use and appends an element with the
// key; lGetSubXxx finds it; lDelSubXxx removes it and frees the sublist when
// it becomes empty. lAddElemUlong64/lAddElemHost are the list-level adders.
// ---------------------------------------------------------------------------

static void test_sublist_helpers() {
   printf("\n--- sublist helpers ---\n");

   lListElem *ep = lCreateElem(TEST_Type);

   CHECK(41, "lAddSubUlong creates the sublist", lAddSubUlong(ep, TEST_ulong, 7, TEST_list, TEST_Type) != nullptr &&
         nelem(lGetList(ep, TEST_list)) == 1);
   lAddSubUlong64(ep, TEST_ulong64, key64(7), TEST_list, TEST_Type);
   lAddSubHost(ep, TEST_host, "subhost", TEST_list, TEST_Type);
   lAddSubStr(ep, TEST_string, "substr", TEST_list, TEST_Type);
   CHECK(42, "four lAddSub calls give four sub-elements", nelem(lGetList(ep, TEST_list)) == 4);

   CHECK(43, "lGetSubUlong finds the key", lGetSubUlong(ep, TEST_ulong, 7, TEST_list) != nullptr);
   CHECK(44, "lGetSubUlong64 finds the key", lGetSubUlong64(ep, TEST_ulong64, key64(7), TEST_list) != nullptr);
   CHECK(45, "lGetSubHost finds the key", lGetSubHost(ep, TEST_host, "subhost", TEST_list) != nullptr);
   CHECK(46, "lGetSubStr finds the key", lGetSubStr(ep, TEST_string, "substr", TEST_list) != nullptr);
   CHECK(47, "lGetSub*RW return the same elements",
         lGetSubUlongRW(ep, TEST_ulong, 7, TEST_list) == lGetSubUlong(ep, TEST_ulong, 7, TEST_list) &&
         lGetSubUlong64RW(ep, TEST_ulong64, key64(7), TEST_list) ==
         lGetSubUlong64(ep, TEST_ulong64, key64(7), TEST_list) &&
         lGetSubHostRW(ep, TEST_host, "subhost", TEST_list) == lGetSubHost(ep, TEST_host, "subhost", TEST_list) &&
         lGetSubStrRW(ep, TEST_string, "substr", TEST_list) == lGetSubStr(ep, TEST_string, "substr", TEST_list));
   CHECK(48, "lGetSubUlong of a missing key is nullptr", lGetSubUlong(ep, TEST_ulong, 8, TEST_list) == nullptr);

   CHECK(49, "lDelSubUlong removes the element", lDelSubUlong(ep, TEST_ulong, 7, TEST_list) == 1 &&
         lGetSubUlong(ep, TEST_ulong, 7, TEST_list) == nullptr);
   CHECK(50, "lDelSubUlong64 removes the element", lDelSubUlong64(ep, TEST_ulong64, key64(7), TEST_list) == 1 &&
         lGetSubUlong64(ep, TEST_ulong64, key64(7), TEST_list) == nullptr);
   CHECK(51, "lDelSubStr removes the element", lDelSubStr(ep, TEST_string, "substr", TEST_list) == 1 &&
         nelem(lGetList(ep, TEST_list)) == 1);

   // the last sub-element goes through the list-level remover, which frees the list
   lList *sub = nullptr;
   lXchgList(ep, TEST_list, &sub);
   lDelElemHost(&sub, TEST_host, "subhost");
   CHECK(52, "removing the last sub-element frees the sublist", sub == nullptr);

   // list-level adders create the list on first use
   lList *lp = nullptr;
   lAddElemUlong64(&lp, TEST_ulong64, key64(9), TEST_Type);
   lAddElemHost(&lp, TEST_host, "addhost", TEST_Type);
   CHECK(53, "lAddElemUlong64/lAddElemHost create the list and append",
         nelem(lp) == 2 && lGetElemUlong64(lp, TEST_ulong64, key64(9)) != nullptr &&
         lGetElemHost(lp, TEST_host, "addhost") != nullptr);
   lFreeList(&lp);

   // the value 0 is a key like any other (CS-2980)
   lAddElemUlong(&lp, TEST_ulong, 0, TEST_Type);
   lAddElemUlong64(&lp, TEST_ulong64, 0, TEST_Type);
   CHECK(54, "lDelElemUlong and lDelElemUlong64 remove the value 0",
         lDelElemUlong(&lp, TEST_ulong, 0) == 1 && nelem(lp) == 1 &&
         lDelElemUlong64(&lp, TEST_ulong64, 0) == 1 && lp == nullptr);
   lFreeList(&lp);

   lFreeElem(&ep);
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
   if (!setup_bootstrap("test_cull_lookup")) {
      printf("FAIL  cannot create the temporary bootstrap file\n");
      return 1;
   }

   test_lookup(true, 1);
   test_lookup(false, 13);
   test_delete(true, 25);
   test_delete(false, 33);
   test_sublist_helpers();

   teardown_bootstrap();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
