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
 * @brief Unit tests for lookup by key in the core of the typed object model (libs/tom)
 *
 * Covers the lookups of the core (ocs::tom::find_first<V>() of the run-time
 * typed interface, lookup_first()/lookup_next() for all elements of a key,
 * lookup_first_str_like(), lookup_first_str_nocase()), the keyed removals
 * (remove_elem_ulong/ulong64/host/str) and the keyed adders (add_elem_*), also
 * on a sublist made with get_or_create_list(). These are the functions the old
 * lGetElem*, lDelElem*, lAddElem* and lAddSub/lGetSub/lDelSub functions were
 * wrappers of. The test type has no generated traits, so the typed find_all(),
 * erase<F>() and emplace_back<F>() do not apply.
 *
 * Every lookup scenario runs twice: once on a list with hash tables and once
 * on a list created without them (Hashing::Off), where the same functions
 * fall back to a linear search. Both must give the same answers; a change to
 * when or where hash tables are built must keep it so.
 */

#include <cstdint>
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

#include "tom/detail/ocs_Hash.h"
#include "tom/ocs_Types.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_Host.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_Raw.h"
#include "ocs_test_model.h"

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
   TEST_list   ///< a sublist of the same type, for the keyed add, find and remove on a sublist
};

LISTDEF(TEST_Type)
   SGE_ULONG  (TEST_ulong,   CULL_HASH)
   SGE_ULONG64(TEST_ulong64, CULL_HASH)
   SGE_HOST   (TEST_host,    CULL_HASH)
   SGE_STRING (TEST_string,  CULL_HASH)
   SGE_LIST   (TEST_list,    TEST_Type, CULL_DEFAULT)
LISTEND(TEST_Type)

NAMEDEF(TEST_Name)
   NAME("TEST_ulong")
   NAME("TEST_ulong64")
   NAME("TEST_host")
   NAME("TEST_string")
   NAME("TEST_list")
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

/** @brief Number of elements in the lists built by #make_list */
static constexpr int NELEM = 6;

/** @brief Number of distinct key values in the lists built by #make_list */
static constexpr int NKEYS = 3;

/** @brief The 64 bit key of group @p g, above 32 bit so truncation would show
 *
 * @param g the key group, 0 .. NKEYS-1
 * @return the TEST_ulong64 value of that group
 */
static std::uint64_t key64(const int g) {
   return 0x500000000ULL + g;
}

/** @brief Build a list of NELEM elements in NKEYS groups of equal keys
 *
 * Element i belongs to group i % NKEYS and carries TEST_ulong = group,
 * TEST_ulong64 = key64(group), TEST_host = `host<group>` and TEST_string =
 * `str<group>`, so every key value occurs NELEM / NKEYS times.
 *
 * @param hashed true for a list with hash tables, false for one without
 * @return the new list
 */
static ocs::tom::RawListPtr make_list(const bool hashed) {
   auto lp = ocs::tom::RawListPtr::create(TEST_Type, hashed ? ocs::tom::Hashing::On : ocs::tom::Hashing::Off,
                                          "lookup");
   for (int i = 0; i < NELEM; i++) {
      const int g = i % NKEYS;
      char host[16], str[16];
      snprintf(host, sizeof(host), "host%d", g);
      snprintf(str, sizeof(str), "str%d", g);
      auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
      ep->set(TEST_ulong, static_cast<std::uint32_t>(g));
      ep->set(TEST_ulong64, key64(g));
      ep->set(TEST_host, ocs::tom::Host(host));
      ep->set(TEST_string, static_cast<const char *>(str));
      lp->push_back(std::move(ep));
   }
   return lp;
}

/**
 * @brief Count the elements the core lookup yields for a key, from lookup_first() on through lookup_next()
 * @tparam T the key field's type
 * @tparam Holds the type of the predicate
 * @param lp the list to search
 * @param nm the key field
 * @param val the key
 * @param holds whether an element found holds the key
 * @return the number of elements found, or -1 if one carried a different value
 */
template <ocs::tom::FieldType T, typename Holds>
static int count_key(const ocs::tom::ConstRawList lp, const int nm, const ocs::tom::KeyArgOf<T> val,
                     const Holds &holds) {
   const int pos = ocs::tom::field_pos(lp.descr(), nm);
   int n = 0;
   const void *it = nullptr;
   for (const lListElem *ep = ocs::tom::lookup_first<T>(lp.raw(), pos, val, &it); ep != nullptr;
        ep = ocs::tom::lookup_next<T>(lp.raw(), pos, val, &it)) {
      if (!holds(ocs::tom::ConstRawElement(ep))) {
         return -1;
      }
      n++;
   }
   return n;
}

/** @brief Count the elements the 32 bit lookup yields for a value
 *
 * @param lp the list to search
 * @param val the TEST_ulong value to look for
 * @return the number of elements found, or -1 if one carried a different value
 */
static int count_ulong(const ocs::tom::ConstRawList lp, const std::uint32_t val) {
   return count_key<ocs::tom::FieldType::Uint32>(lp, TEST_ulong, val, [val](const ocs::tom::ConstRawElement ep) {
      return ep.get<std::uint32_t>(TEST_ulong) == val;
   });
}

/** @brief Count the elements the 64 bit lookup yields for a value
 *
 * @param lp the list to search
 * @param val the TEST_ulong64 value to look for
 * @return the number of elements found, or -1 if one carried a different value
 */
static int count_ulong64(const ocs::tom::ConstRawList lp, const std::uint64_t val) {
   return count_key<ocs::tom::FieldType::Uint64>(lp, TEST_ulong64, val, [val](const ocs::tom::ConstRawElement ep) {
      return ep.get<std::uint64_t>(TEST_ulong64) == val;
   });
}

/** @brief Count the elements the host lookup yields for a name
 *
 * @param lp the list to search
 * @param host the TEST_host value to look for
 * @return the number of elements found, or -1 if one carried a different value
 */
static int count_host(const ocs::tom::ConstRawList lp, const char *host) {
   return count_key<ocs::tom::FieldType::Host>(lp, TEST_host, host, [host](const ocs::tom::ConstRawElement ep) {
      return strcmp(ep.get<ocs::tom::Host>(TEST_host).c_str(), host) == 0;
   });
}

/** @brief Count the elements the string lookup yields for a value
 *
 * @param lp the list to search
 * @param str the TEST_string value to look for
 * @return the number of elements found, or -1 if one carried a different value
 */
static int count_str(const ocs::tom::ConstRawList lp, const char *str) {
   return count_key<ocs::tom::FieldType::String>(lp, TEST_string, str, [str](const ocs::tom::ConstRawElement ep) {
      return strcmp(ep.get<const char *>(TEST_string), str) == 0;
   });
}

/** @brief Check that every key value yields every element carrying it, in all four key types
 *
 * @param lp the list to search
 * @return true when for every group the four iterators each yield exactly the
 *         elements of that group
 */
static bool all_iterators_agree(const ocs::tom::ConstRawList lp) {
   int per_group[NKEYS] = {0};
   for (const ocs::tom::ConstRawElement ep : lp) {
      per_group[ep.get<std::uint32_t>(TEST_ulong)]++;
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

/**
 * @brief The string key of the first element of a list, copied, so that the removal by it does not read freed memory
 * @param lp the list, not empty
 * @param buf the buffer for the copy
 * @param size the size of @p buf
 * @return @p buf
 */
static const char *first_str(const ocs::tom::ConstRawList lp, char *buf, const std::size_t size) {
   snprintf(buf, size, "%s", lp.front().get<const char *>(TEST_string));
   return buf;
}

// ---------------------------------------------------------------------------
// Single lookup and iteration  [base+0 – base+11]
//
// find_first<V>() returns some element carrying the key, lookup_first() and
// lookup_next() every element carrying it, the form on a mutable list the
// same elements writable. A key that does not occur yields nullptr.
// ---------------------------------------------------------------------------

/** @brief Run the lookup scenario on one list flavour
 *
 * @param hashed true to run on a list with hash tables, false without
 * @param base the first test number of this run
 */
static void test_lookup(const bool hashed, const int base) {
   printf("\n--- lookup by key, %s ---\n", hashed ? "hashed list" : "unhashed list");

   const auto owner = make_list(hashed);
   const ocs::tom::RawList lp = owner.get();
   const ocs::tom::ConstRawList clp = lp;

   ocs::tom::ConstRawElement ep = clp.find_first(TEST_ulong, std::uint32_t{1});
   CHECK(base + 0, "find_first<std::uint32_t> finds the key", ep && ep.get<std::uint32_t>(TEST_ulong) == 1);
   CHECK(base + 1, "find_first<std::uint32_t> of a missing key is nullptr",
         !clp.find_first(TEST_ulong, std::uint32_t{99}));

   ep = clp.find_first(TEST_ulong64, key64(2));
   CHECK(base + 2, "find_first<std::uint64_t> finds a key above 32 bit",
         ep && ep.get<std::uint64_t>(TEST_ulong64) == key64(2));
   CHECK(base + 3, "find_first<std::uint64_t> does not match the low 32 bits alone",
         !clp.find_first(TEST_ulong64, std::uint64_t{key64(2) & 0xffffffffULL}));

   ep = clp.find_first(TEST_host, ocs::tom::Host("host0"));
   CHECK(base + 4, "find_first<Host> finds the key",
         ep && strcmp(ep.get<ocs::tom::Host>(TEST_host).c_str(), "host0") == 0);

   ep = clp.find_first(TEST_string, "str2");
   CHECK(base + 5, "find_first<const char *> finds the key",
         ep && strcmp(ep.get<const char *>(TEST_string), "str2") == 0);

   CHECK(base + 6, "lookup_first/lookup_next yield every element of every key", all_iterators_agree(clp));

   // the forms on a mutable list address the same elements
   const int pos_ulong = ocs::tom::field_pos(TEST_Type, TEST_ulong);
   const void *it = nullptr;
   const std::uint32_t one = 1;
   CHECK(base + 7, "the forms on a mutable list return the same elements as the read-only ones",
         lp.find_first(TEST_ulong, one).raw() == clp.find_first(TEST_ulong, one).raw() &&
         lp.find_first(TEST_ulong64, key64(1)).raw() == clp.find_first(TEST_ulong64, key64(1)).raw() &&
         lp.find_first(TEST_host, ocs::tom::Host("host1")).raw() ==
         clp.find_first(TEST_host, ocs::tom::Host("host1")).raw() &&
         lp.find_first(TEST_string, "str1").raw() == clp.find_first(TEST_string, "str1").raw() &&
         ocs::tom::lookup_first<ocs::tom::FieldType::Uint32>(lp.raw(), pos_ulong, one, &it) != nullptr &&
         ocs::tom::lookup_next<ocs::tom::FieldType::Uint32>(lp.raw(), pos_ulong, one, &it) != nullptr &&
         ocs::tom::lookup_next<ocs::tom::FieldType::Uint32>(lp.raw(), pos_ulong, one, &it) == nullptr);

   // lookup_first_str_like: exact match or a trailing '*' as prefix wildcard
   const lListElem *found = ocs::tom::lookup_first_str_like(clp.raw(), TEST_string, "str*");
   CHECK(base + 8, "lookup_first_str_like with a trailing '*' matches the prefix",
         found != nullptr && strncmp(ocs::tom::get<const char *>(found, TEST_string), "str", 3) == 0);
   CHECK(base + 9, "lookup_first_str_like without a match returns nullptr",
         ocs::tom::lookup_first_str_like(clp.raw(), TEST_string, "xyz*") == nullptr &&
         ocs::tom::lookup_first_str_like(clp.raw(), TEST_string, "str") == nullptr);

   // lookup_first_str_nocase ignores case
   found = ocs::tom::lookup_first_str_nocase(clp.raw(), TEST_string, "STR1");
   CHECK(base + 10, "lookup_first_str_nocase ignores case",
         found != nullptr && strcmp(ocs::tom::get<const char *>(found, TEST_string), "str1") == 0);

   // changing a key in place moves the element to the other key
   const ocs::tom::RawElement mod = lp.find_first(TEST_string, "str0");
   mod.set(TEST_ulong, std::uint32_t{1});
   mod.set(TEST_ulong64, key64(1));
   mod.set(TEST_host, ocs::tom::Host("host1"));
   mod.set(TEST_string, "str1");
   CHECK(base + 11, "after changing all keys of an element the iterators still agree", all_iterators_agree(clp) &&
         count_ulong(clp, 1) == NELEM / NKEYS + 1 && count_str(clp, "str0") == NELEM / NKEYS - 1);
}

// ---------------------------------------------------------------------------
// Removal by key  [base+0 – base+7]
//
// remove_elem_ulong/ulong64/host/str remove one element carrying the key. The
// remaining elements must still be found by their keys. Unlike the old
// lDelElem* functions, the core leaves an emptied list to its owner.
// ---------------------------------------------------------------------------

/** @brief Run the removal scenario on one list flavour
 *
 * @param hashed true to run on a list with hash tables, false without
 * @param base the first test number of this run
 */
static void test_delete(const bool hashed, const int base) {
   printf("\n--- removal by key, %s ---\n", hashed ? "hashed list" : "unhashed list");

   const auto owner = make_list(hashed);
   const ocs::tom::RawList lp = owner.get();

   CHECK(base + 0, "remove_elem_ulong removes one element of the key",
         ocs::tom::remove_elem_ulong(lp, TEST_ulong, 1) && count_ulong(lp, 1) == NELEM / NKEYS - 1 &&
         nelem(lp) == NELEM - 1);
   CHECK(base + 1, "remove_elem_ulong64 removes the other element of the key",
         ocs::tom::remove_elem_ulong64(lp, TEST_ulong64, key64(1)) && count_ulong64(lp, key64(1)) == 0);
   CHECK(base + 2, "remove_elem_host removes one element of the key",
         ocs::tom::remove_elem_host(lp, TEST_host, "host2") && count_host(lp, "host2") == NELEM / NKEYS - 1);
   CHECK(base + 3, "remove_elem_str of a missing key removes nothing",
         !ocs::tom::remove_elem_str(lp, TEST_string, "nope") && nelem(lp) == NELEM - 3);
   CHECK(base + 4, "remove_elem_str removes one element of the key",
         ocs::tom::remove_elem_str(lp, TEST_string, "str0") && count_str(lp, "str0") == NELEM / NKEYS - 1);
   CHECK(base + 5, "the remaining elements are still found by every key", all_iterators_agree(lp));

   // remove what is left
   char key[16];
   while (nelem(lp) > 1) {
      static_cast<void>(ocs::tom::remove_elem_str(lp, TEST_string, first_str(lp, key, sizeof(key))));
   }
   CHECK(base + 6, "one element left", nelem(lp) == 1);
   // CHECK base + 7 removed: lDelElemStr() freed the emptied list, which has no TOM counterpart (CS-3103)
}

// ---------------------------------------------------------------------------
// Sublist helpers  [T41–T54]
//
// get_or_create_list() creates the sublist on first use, add_elem_* append an
// element with the key; find_first<V>() on the sublist finds it; remove_elem_*
// remove it. add_elem_ulong64/add_elem_host also create a missing list.
// ---------------------------------------------------------------------------

/**
 * @brief The sublist of a test element, created on first use, as the old lAddSub* functions made it
 * @param ep the element
 * @return the sublist
 */
static lList *sub_list(const ocs::tom::RawElement ep) {
   return ocs::tom::get_or_create_list(ep, ocs::tom::field_pos(TEST_Type, TEST_list), nullptr, TEST_Type);
}

/** @brief T41-T54: keyed add, find and remove on a sublist and on a list created on first use */
static void test_sublist_helpers() {
   printf("\n--- sublist helpers ---\n");

   const auto owner = ocs::tom::RawElementPtr::create(TEST_Type);
   const ocs::tom::RawElement ep = owner.get();
   lList *sub = sub_list(ep);

   CHECK(41, "get_or_create_list and add_elem_ulong create the sublist",
         ocs::tom::add_elem_ulong(sub, TEST_ulong, 7, TEST_Type) != nullptr &&
         nelem(ep.get<ocs::tom::ConstRawList>(TEST_list)) == 1);
   static_cast<void>(ocs::tom::add_elem_ulong64(sub, TEST_ulong64, key64(7), TEST_Type));
   static_cast<void>(ocs::tom::add_elem_host(sub, TEST_host, "subhost", TEST_Type));
   static_cast<void>(ocs::tom::add_elem_str(sub, TEST_string, "substr", TEST_Type));
   CHECK(42, "four add_elem calls give four sub-elements", nelem(ep.get<ocs::tom::ConstRawList>(TEST_list)) == 4);

   const ocs::tom::ConstRawList csub = ep.get<ocs::tom::ConstRawList>(TEST_list);
   const ocs::tom::RawList rsub = ep.get<ocs::tom::RawList>(TEST_list);
   CHECK(43, "find_first<std::uint32_t> on the sublist finds the key", csub.find_first(TEST_ulong, std::uint32_t{7}));
   CHECK(44, "find_first<std::uint64_t> on the sublist finds the key", csub.find_first(TEST_ulong64, key64(7)));
   CHECK(45, "find_first<Host> on the sublist finds the key", csub.find_first(TEST_host, ocs::tom::Host("subhost")));
   CHECK(46, "find_first<const char *> on the sublist finds the key", csub.find_first(TEST_string, "substr"));
   CHECK(47, "the forms on the mutable sublist return the same elements",
         rsub.find_first(TEST_ulong, std::uint32_t{7}).raw() == csub.find_first(TEST_ulong, std::uint32_t{7}).raw() &&
         rsub.find_first(TEST_ulong64, key64(7)).raw() == csub.find_first(TEST_ulong64, key64(7)).raw() &&
         rsub.find_first(TEST_host, ocs::tom::Host("subhost")).raw() ==
         csub.find_first(TEST_host, ocs::tom::Host("subhost")).raw() &&
         rsub.find_first(TEST_string, "substr").raw() == csub.find_first(TEST_string, "substr").raw());
   CHECK(48, "find_first<std::uint32_t> of a missing key is nullptr", !csub.find_first(TEST_ulong, std::uint32_t{8}));

   CHECK(49, "remove_elem_ulong removes the element", ocs::tom::remove_elem_ulong(rsub, TEST_ulong, 7) &&
         !csub.find_first(TEST_ulong, std::uint32_t{7}));
   CHECK(50, "remove_elem_ulong64 removes the element", ocs::tom::remove_elem_ulong64(rsub, TEST_ulong64, key64(7)) &&
         !csub.find_first(TEST_ulong64, key64(7)));
   CHECK(51, "remove_elem_str removes the element", ocs::tom::remove_elem_str(rsub, TEST_string, "substr") &&
         nelem(ep.get<ocs::tom::ConstRawList>(TEST_list)) == 1);

   // CHECK 52 removed: lDelElemHost() freed the emptied sublist, which has no TOM counterpart (CS-3103)

   // the list-level adders create the list on first use
   lList *lp = nullptr;
   static_cast<void>(ocs::tom::add_elem_ulong64(lp, TEST_ulong64, key64(9), TEST_Type));
   static_cast<void>(ocs::tom::add_elem_host(lp, TEST_host, "addhost", TEST_Type));
   ocs::tom::RawListPtr list(lp);
   CHECK(53, "add_elem_ulong64/add_elem_host create the list and append",
         nelem(list.get()) == 2 && list->find_first(TEST_ulong64, key64(9)) &&
         list->find_first(TEST_host, ocs::tom::Host("addhost")));
   list.reset();

   // the value 0 is a key like any other (CS-2980)
   lp = nullptr;
   static_cast<void>(ocs::tom::add_elem_ulong(lp, TEST_ulong, 0, TEST_Type));
   static_cast<void>(ocs::tom::add_elem_ulong64(lp, TEST_ulong64, 0, TEST_Type));
   list = ocs::tom::RawListPtr(lp);
   // the old lDelElemUlong64() freed the emptied list; the core leaves it to its owner
   CHECK(54, "remove_elem_ulong and remove_elem_ulong64 remove the value 0",
         ocs::tom::remove_elem_ulong(list.get(), TEST_ulong, 0) && nelem(list.get()) == 1 &&
         ocs::tom::remove_elem_ulong64(list.get(), TEST_ulong64, 0) && nelem(list.get()) == 0);
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

/**
 * @brief Run all lookup tests
 * @return 0 if every check passed, 1 otherwise
 */
int main(int /*argc*/, char * /*argv*/[]) {
   // the lists here are short; with the threshold at 0 every lookup goes through the hash tables
   ocs::module_test::install_test_model({{"TEST", TEST_Type, TEST_Name, TEST_Size}});
   ocs::tom::detail::hash_set_min_elems(0);
   if (!setup_bootstrap("test_tom_core_lookup")) {
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
