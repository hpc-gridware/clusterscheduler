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
 * @brief Module test: CULL hash tables are built by the first lookup (CS-2996, LZY-01 to LZY-06)
 *
 * A list that may be hashed starts with empty hash slots. The first lookup on a
 * field of a list with at least #OCS_CULL_HASH_MIN_ELEMS elements (the default
 * threshold of ocs::tom::detail::hash_set_min_elems(), which also holds in threads that never call it) builds the
 * table of that field; shorter lists are searched linearly. The test checks
 * that no table is built before a lookup, that the built table answers like a
 * linear search and is maintained afterwards, that concurrent first lookups
 * publish one table, that the threshold holds, that a duplicate unique key is
 * counted at the build, and that sorting drops the non unique tables.
 *
 * LAZY-03 is meant to be run under the thread and the address sanitizer as well.
 *
 * The test type has no generated traits, so the lists are driven through the
 * run-time typed handles of tom/ocs_Raw.h and the lookups of the core.
 */

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_ListP.h"
#include "tom/ocs_Types.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_Pack.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SelectCore.h"
#include "tom/ocs_SortCore.h"

#include "uti/sge_stdlib.h"

#include "ocs_module_test.h"
#include "ocs_test_model.h"

/** @brief The element type: a unique and a non unique hashed field */
enum {
   LZ_name = 1, ///< unique hashed string
   LZ_group,    ///< non unique hashed ulong
   LZ_value     ///< not hashed
};

LISTDEF(LZ_Type)
   SGE_STRING(LZ_name, CULL_DEFAULT | CULL_HASH | CULL_UNIQUE)
   SGE_ULONG(LZ_group, CULL_DEFAULT | CULL_HASH)
   SGE_ULONG(LZ_value, CULL_DEFAULT)
LISTEND(LZ_Type)

NAMEDEF(LZ_Name)
   NAME("LZ_name")
   NAME("LZ_group")
   NAME("LZ_value")
NAMEEND

#define LZ_Size sizeof(LZ_Name) / sizeof(char *) ///< number of fields of LZ_Type

/// position of LZ_name in LZ_Type
static const int POS_NAME = 0;
/// position of LZ_group in LZ_Type
static const int POS_GROUP = 1;

/**
 * @brief The name of the i-th element
 * @param i the index
 * @return "e<i>"
 */
static std::string name_of(const int i) {
   return "e" + std::to_string(i);
}

/**
 * @brief Create a test element with a name and append it, as lAddElemStr() did
 * @param lp the list
 * @param name the name
 * @return the new element, owned by the list
 */
static ocs::tom::RawElement add_named(const ocs::tom::RawList lp, const char *name) {
   auto ep = ocs::tom::RawElementPtr::create(LZ_Type);
   ep->set(LZ_name, name);
   return lp.push_back(std::move(ep));
}

/**
 * @brief Create a hashable list of n elements e0 .. e<n-1> in groups of four
 * @param n the number of elements
 * @return the list
 */
static ocs::tom::RawListPtr make_list(const int n) {
   auto lp = ocs::tom::RawListPtr::create(LZ_Type, ocs::tom::Hashing::On, "lazy");
   for (int i = 0; i < n; i++) {
      const ocs::tom::RawElement ep = add_named(lp.get(), name_of(i).c_str());
      ep.set(LZ_group, static_cast<std::uint32_t>(i % 4));
      ep.set(LZ_value, static_cast<std::uint32_t>(i));
   }
   return lp;
}

/**
 * @brief Whether a list may be hashed on both hashed fields and has no table yet
 *
 * A hashable list that has not built a table has not allocated its slot array
 * either, so no memory is spent on a list that is never searched.
 *
 * @param lp the list
 * @return true if the list counts both hashed fields and has no slot array
 */
static bool slots_empty(const lList *lp) {
   return lp != nullptr && lp->nslots == 2 && list_slots(lp) == nullptr &&
          list_hash_table(lp, POS_NAME) == nullptr && list_hash_table(lp, POS_GROUP) == nullptr;
}

/**
 * @brief Find an element by name by walking the list, the reference for a lookup
 * @param lp the list
 * @param name the name
 * @return the first element with that name, or nullptr
 */
static const lListElem *linear_find(const ocs::tom::ConstRawList lp, const char *name) {
   for (const ocs::tom::ConstRawElement ep : lp) {
      const char *s = ep.get<const char *>(LZ_name);
      if (s != nullptr && strcmp(s, name) == 0) {
         return ep.raw();
      }
   }
   return nullptr;
}

/**
 * @brief Look an element up by name, as lGetElemStr() did
 * @param lp the list
 * @param name the name
 * @return the element, or nullptr
 */
static const lListElem *find_name(const ocs::tom::ConstRawList lp, const char *name) {
   return lp.find_first(LZ_name, name).raw();
}

/**
 * @brief Whether every element is found by its name as by the linear search
 * @param lp the list
 * @return true if every lookup returns the element the linear search returns
 */
static bool all_found(const ocs::tom::ConstRawList lp) {
   for (const ocs::tom::ConstRawElement ep : lp) {
      const char *name = ep.get<const char *>(LZ_name);
      if (find_name(lp, name) != linear_find(lp, name)) {
         return false;
      }
   }
   return true;
}

/**
 * @brief The values of LZ_value of the elements of a group, in the order the lookup yields them
 * @param lp the list
 * @param group the group
 * @return the values
 */
static std::vector<uint32_t> group_values(const ocs::tom::ConstRawList lp, const uint32_t group) {
   std::vector<uint32_t> values;
   const void *iterator = nullptr;
   for (const lListElem *ep = ocs::tom::lookup_first<ocs::tom::FieldType::Uint32>(lp.raw(), POS_GROUP, group,
                                                                                    &iterator);
        ep != nullptr; ep = ocs::tom::lookup_next<ocs::tom::FieldType::Uint32>(lp.raw(), POS_GROUP, group, &iterator)) {
      values.push_back(ocs::tom::get<std::uint32_t>(ep, LZ_value));
   }
   return values;
}

/**
 * @brief The values of LZ_value of the elements of a group, in list order
 * @param lp the list
 * @param group the group
 * @return the values
 */
static std::vector<uint32_t> group_values_in_list_order(const ocs::tom::ConstRawList lp, const uint32_t group) {
   std::vector<uint32_t> values;
   for (const ocs::tom::ConstRawElement ep : lp) {
      if (ep.get<std::uint32_t>(LZ_group) == group) {
         values.push_back(ep.get<std::uint32_t>(LZ_value));
      }
   }
   return values;
}

/**
 * @brief Pack a list and unpack it again, as a receiver does
 * @param lp the list
 * @return the unpacked list, empty on an error
 */
static ocs::tom::RawListPtr pack_unpack(const lList *lp) {
   try {
      ocs::tom::PackBuffer out(false, 1024);
      ocs::tom::pack_list(&out.raw(), lp, nullptr, 0);
      ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(out.data(), out.size(), false);
      return ocs::tom::unpack_raw_list(&in.raw());
   } catch (const ocs::tom::PackException &) {
      return ocs::tom::RawListPtr();
   }
}

/**
 * @brief Sort a list by one field of the test type
 * @param lp the list
 * @param nm the field
 * @param ascending true for ascending, false for descending
 */
static void sort_by(lList *lp, const int nm, const bool ascending) {
   const ocs::tom::SortKey keys[] = {{nm, ascending}};
   lSortOrder *so = ocs::tom::make_sort_order(LZ_Type, keys);
   ocs::tom::sort_list(lp, so);
   ocs::tom::free_sort_order(so);
}

/**
 * @brief LAZY-01: no table without a lookup
 *
 * Laziness is the release behaviour. A debug build checks every append for
 * a duplicate key and uses, and so builds, the table of a list at the
 * threshold (N-H-18); the checks are suspended here.
 *
 * @return nothing; CHECK records the results
 */
static void test_no_table() {
   const ocs::tom::UniqueCheckSuspension suspension;
   const int n = 2 * OCS_CULL_HASH_MIN_ELEMS;
   const auto lp = make_list(n);
   CHECK(1, "a created and filled list is hashable without a slot array or a table", slots_empty(lp.get()));

   const auto unpacked = pack_unpack(lp.get());
   const auto copy = ocs::tom::clone(lp.get(), ocs::tom::Hashing::On);
   const ocs::tom::RawWherePtr where =
           ocs::tom::where_compare(LZ_Type, LZ_group, ocs::tom::CompareOp::GreaterEqual, std::uint32_t{0});
   const ocs::tom::RawWhatPtr what = ocs::tom::what_all();
   const auto selected = ocs::tom::select_as("selected", lp.get(), where.get(), what.get(), ocs::tom::Hashing::On);
   CHECK(2, "unpacked, copied and selected lists are hashable without a slot array or a table",
         unpacked->size() == static_cast<std::size_t>(n) && slots_empty(unpacked.get()) &&
         copy->size() == static_cast<std::size_t>(n) && slots_empty(copy.get()) &&
         selected->size() == static_cast<std::size_t>(n) && slots_empty(selected.get()));

   const auto plain = ocs::tom::RawListPtr::create(LZ_Type, ocs::tom::Hashing::Off, "plain");
   for (int i = 0; i < n; i++) {
      add_named(plain.get(), name_of(i).c_str());
   }
   CHECK(3, "a list created without hashing has no slots and stays linear after a lookup",
         plain.get().raw()->nslots == 0 && find_name(plain.get(), "e5") == linear_find(plain.get(), "e5") &&
         list_slots(plain.get()) == nullptr);

   CHECK(4, "the source list still has no table after being packed, copied and selected", slots_empty(lp.get()));
}

/**
 * @brief LAZY-02: the first lookup builds the table, which is maintained afterwards
 * @return nothing; CHECK records the results
 */
static void test_first_lookup() {
   const auto owner = make_list(2 * OCS_CULL_HASH_MIN_ELEMS);
   const ocs::tom::RawList lp = owner.get();
   const lListElem *e3 = find_name(lp, "e3");
   CHECK(5, "the first lookup builds the table of its field only and answers like the linear search",
         e3 != nullptr && e3 == linear_find(lp, "e3") && list_hash_table(lp, POS_NAME) != nullptr &&
         list_hash_table(lp, POS_GROUP) == nullptr && all_found(lp));

   lp.front().set(LZ_name, "renamed");
   add_named(lp, "appended");
   lp.erase(lp.find_first(LZ_name, "e7"));
   CHECK(6, "after a rename, an append and a removal every lookup answers like the linear search",
         all_found(lp) && find_name(lp, "renamed") == lp.front().raw() &&
         find_name(lp, "e0") == nullptr && find_name(lp, "appended") == lp.back().raw() &&
         find_name(lp, "e7") == nullptr);

   CHECK(7, "a non unique lookup builds its table and yields the group in list order",
         group_values(lp, 1) == group_values_in_list_order(lp, 1) && list_hash_table(lp, POS_GROUP) != nullptr);
}

/**
 * @brief LAZY-03: concurrent first lookups publish one table per field
 * @return nothing; CHECK records the results
 */
static void test_concurrent() {
   const int threads = 16;
   const int rounds = 500;
   const int n = 4 * OCS_CULL_HASH_MIN_ELEMS;
   std::atomic<int> wrong{0};
   bool published = true;

   for (int r = 0; r < rounds; r++) {
      const auto owner = make_list(n);
      const ocs::tom::ConstRawList lp = owner.get();
      std::atomic<int> ready{0};
      std::atomic<bool> go{false};
      std::vector<std::thread> workers;
      for (int t = 0; t < threads; t++) {
         workers.emplace_back([&, t] {
            ready++;
            while (!go.load(std::memory_order_acquire)) {
               std::this_thread::yield();
            }
            // every thread starts with a different key, on the unique and the non unique field
            for (int i = 0; i < n; i++) {
               const int k = (i + t) % n;
               const ocs::tom::ConstRawElement ep = lp.find_first(LZ_name, name_of(k).c_str());
               if (!ep || ep.get<std::uint32_t>(LZ_value) != static_cast<std::uint32_t>(k)) {
                  wrong++;
               }
            }
            if (group_values(lp, t % 4) != group_values_in_list_order(lp, t % 4)) {
               wrong++;
            }
         });
      }
      while (ready.load() < threads) {
         std::this_thread::yield();
      }
      go.store(true, std::memory_order_release);
      for (auto &w : workers) {
         w.join();
      }
      published = published && list_hash_table(lp, POS_NAME) != nullptr && list_hash_table(lp, POS_GROUP) != nullptr;
   }
   CHECK(8, "all concurrent first lookups answer correctly", wrong.load() == 0);
   CHECK(9, "after every round a table is published on each field", published);
}

/**
 * @brief LAZY-04: short lists stay linear, a table survives shrinking
 * @return nothing; CHECK records the results
 */
static void test_threshold() {
   const auto owner = make_list(OCS_CULL_HASH_MIN_ELEMS / 2);
   const ocs::tom::RawList lp = owner.get();
   CHECK(10, "a lookup on a list below the threshold answers without building a table",
         find_name(lp, "e2") == linear_find(lp, "e2") && slots_empty(lp));

   for (uint32_t i = OCS_CULL_HASH_MIN_ELEMS / 2; i < OCS_CULL_HASH_MIN_ELEMS; i++) {
      add_named(lp, name_of(i).c_str());
   }
   CHECK(11, "a list grown to the threshold gets the table on its next lookup",
         find_name(lp, "e2") == linear_find(lp, "e2") && list_hash_table(lp, POS_NAME) != nullptr);

   while (lp.size() > 4) {
      lp.erase(lp.back());
   }
   CHECK(12, "a list shrunk below the threshold keeps its table, which still answers",
         list_hash_table(lp, POS_NAME) != nullptr && all_found(lp) && find_name(lp, "e10") == nullptr);
}

/**
 * @brief LAZY-05: a duplicate in a unique field stops the build of its table
 *
 * The duplicate is created on purpose, which a debug build refuses at the
 * append (N-H-18); the checks are suspended here. The lookup that would build
 * the table is a logic error (CS-3052 stage 2): it throws, publishes no table,
 * and after the duplicate is removed the next lookup builds the table.
 *
 * @return nothing; CHECK records the results
 */
static void test_duplicate() {
   const ocs::tom::UniqueCheckSuspension suspension;
   const auto owner = make_list(OCS_CULL_HASH_MIN_ELEMS);
   const ocs::tom::RawList lp = owner.get();
   const ocs::tom::RawElement first = add_named(lp, "dup");
   const ocs::tom::RawElement second = add_named(lp, "dup");
   bool thrown = false;
   try {
      const void *iterator = nullptr;
      static_cast<void>(ocs::tom::lookup_first<ocs::tom::FieldType::String>(lp, POS_NAME, "dup", &iterator));
   } catch (const ocs::tom::FieldException &e) {
      thrown = e.reason() == ocs::tom::FieldException::Reason::DuplicateKey;
   }
   CHECK(13, "the lookup that would build the table throws and publishes no table",
         thrown && list_hash_table(lp, POS_NAME) == nullptr);

   lp.erase(second);
   CHECK(14, "after removing one of the two the lookup builds the table and finds the other one",
         find_name(lp, "dup") == first.raw() && list_hash_table(lp, POS_NAME) != nullptr);
}

/**
 * @brief LAZY-06: sorting drops the non unique tables
 * @return nothing; CHECK records the results
 */
static void test_sort() {
   const auto owner = make_list(2 * OCS_CULL_HASH_MIN_ELEMS);
   const ocs::tom::RawList lp = owner.get();
   const std::vector<uint32_t> before = group_values(lp, 1);
   static_cast<void>(find_name(lp, "e1"));
   const cull_htable unique_table = list_hash_table(lp, POS_NAME);

   sort_by(lp, LZ_value, false);
   CHECK(15, "the sort drops the non unique table and keeps the unique one",
         list_hash_table(lp, POS_GROUP) == nullptr && list_hash_table(lp, POS_NAME) == unique_table &&
         unique_table != nullptr);
   const std::vector<uint32_t> after = group_values(lp, 1);
   CHECK(16, "the next lookup yields the group in the new list order",
         after == group_values_in_list_order(lp, 1) && after != before && list_hash_table(lp, POS_GROUP) != nullptr);
   CHECK(17, "the unique table still finds every element", all_found(lp));
}

/**
 * @brief Run the scenarios of the lazy hash tables
 * @param argc the number of arguments
 * @param argv the arguments of the module test frame
 * @return the exit code of the module test frame
 */
int main(int argc, char *argv[]) {
   ocs::module_test::install_test_model({{"LZ", LZ_Type, LZ_Name, LZ_Size}});

   return ocs::module_test::run(argc, argv, {
      {"LAZY-01", "no table without a lookup",
       "lists that are created, unpacked, copied or selected get no hash table until a lookup",
       {"create and fill a list above the threshold", "unpack, copy and select it",
        "create a list without hashing and look it up"},
       "all slots are empty; a list created without hashing has no slots at all",
       {{"LZY-01", "covers"}}, {1, 2, 3, 4}, test_no_table},
      {"LAZY-02", "first lookup builds the table",
       "the first lookup builds the table of its field, which answers like the linear search and is maintained",
       {"look a key up", "rename, append and remove elements", "look every key up again",
        "look a non unique key up"},
       "the table exists after the first lookup on its field only, and every lookup returns the element the "
       "linear search returns",
       {{"LZY-02", "covers"}}, {5, 6, 7}, test_first_lookup},
      {"LAZY-03", "concurrent first lookups",
       "many threads look up keys on the same list at the same time, all of them first lookups",
       {"create a list above the threshold", "start 16 threads that look up every key on both fields",
        "repeat 500 times"},
       "all lookups answer correctly and a table is published on each field",
       {{"LZY-03", "covers"}}, {8, 9}, test_concurrent},
      {"LAZY-04", "short lists stay linear",
       "lists below the threshold get no table; a table that exists survives shrinking",
       {"look up a list below the threshold", "grow it to the threshold and look it up",
        "shrink it below the threshold and look it up"},
       "the short list has no table, the grown one has, the shrunk one keeps it",
       {{"LZY-04", "covers"}}, {10, 11, 12}, test_threshold},
      {"LAZY-05", "unique duplicate stops the build",
       "a duplicate in a unique field stops the build of the table",
       {"append two elements with the same unique key without a lookup", "look the key up",
        "remove one of them and look the key up again"},
       "the lookup throws and no table is published; after the removal the table is built and finds the other one",
       {{"LZY-05", "covers"}}, {13, 14}, test_duplicate},
      {"LAZY-06", "sorting drops non unique tables",
       "after a sort the hits of a non unique key come in the new list order",
       {"look a non unique and a unique key up", "sort the list", "look them up again"},
       "the non unique table is dropped and rebuilt in the new order, the unique table is unchanged",
       {{"LZY-06", "covers"}}, {15, 16, 17}, test_sort},
   });
}
