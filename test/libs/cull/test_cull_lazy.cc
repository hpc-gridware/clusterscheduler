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
 * threshold of lInit(), which also holds in threads that never call it) builds the
 * table of that field; shorter lists are searched linearly. The test checks
 * that no table is built before a lookup, that the built table answers like a
 * linear search and is maintained afterwards, that concurrent first lookups
 * publish one table, that the threshold holds, that a duplicate unique key is
 * counted at the build, and that sorting drops the non unique tables.
 *
 * LAZY-03 is meant to be run under the thread and the address sanitizer as well.
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

#include "cull/cull.h"
#include "cull/cull_listP.h"

#include "uti/sge_stdlib.h"

#include "ocs_module_test.h"

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
LISTEND

NAMEDEF(LZ_Name)
   NAME("LZ_name")
   NAME("LZ_group")
   NAME("LZ_value")
NAMEEND

#define LZ_Size sizeof(LZ_Name) / sizeof(char *) ///< number of fields of LZ_Type

/** @brief The name space registering the type with CULL */
lNameSpace nmv[] = {
   {1, LZ_Size, LZ_Name, LZ_Type},
   {0, 0, nullptr, nullptr}
};

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
 * @brief Create a hashable list of n elements e0 .. e<n-1> in groups of four
 * @param n the number of elements
 * @return the list
 */
static lList *make_list(const int n) {
   lList *lp = lCreateList("lazy", LZ_Type);
   for (int i = 0; i < n; i++) {
      lListElem *ep = lAddElemStr(&lp, LZ_name, name_of(i).c_str(), LZ_Type);
      lSetUlong(ep, LZ_group, i % 4);
      lSetUlong(ep, LZ_value, i);
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
static const lListElem *linear_find(const lList *lp, const char *name) {
   for_each_ep_lv(ep, lp) {
      const char *s = lGetString(ep, LZ_name);
      if (s != nullptr && strcmp(s, name) == 0) {
         return ep;
      }
   }
   return nullptr;
}

/**
 * @brief Whether every element is found by its name as by the linear search
 * @param lp the list
 * @return true if every lookup returns the element the linear search returns
 */
static bool all_found(const lList *lp) {
   for_each_ep_lv(ep, lp) {
      const char *name = lGetString(ep, LZ_name);
      if (lGetElemStr(lp, LZ_name, name) != linear_find(lp, name)) {
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
static std::vector<uint32_t> group_values(const lList *lp, const uint32_t group) {
   std::vector<uint32_t> values;
   const void *iterator = nullptr;
   for (const lListElem *ep = lGetElemUlongFirst(lp, LZ_group, group, &iterator); ep != nullptr;
        ep = lGetElemUlongNext(lp, LZ_group, group, &iterator)) {
      values.push_back(lGetUlong(ep, LZ_value));
   }
   return values;
}

/**
 * @brief The values of LZ_value of the elements of a group, in list order
 * @param lp the list
 * @param group the group
 * @return the values
 */
static std::vector<uint32_t> group_values_in_list_order(const lList *lp, const uint32_t group) {
   std::vector<uint32_t> values;
   for_each_ep_lv(ep, lp) {
      if (lGetUlong(ep, LZ_group) == group) {
         values.push_back(lGetUlong(ep, LZ_value));
      }
   }
   return values;
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
 * @brief LAZY-01: no table without a lookup
 * @return nothing; CHECK records the results
 */
static void test_no_table() {
   const int n = 2 * OCS_CULL_HASH_MIN_ELEMS;
   lList *lp = make_list(n);
   CHECK(1, "a created and filled list is hashable without a slot array or a table", slots_empty(lp));

   lList *unpacked = pack_unpack(lp);
   lList *copy = lCopyList("copy", lp);
   lCondition *where = lWhere("%T(%I>=%u)", LZ_Type, LZ_group, 0);
   lEnumeration *what = lWhat("%T(ALL)", LZ_Type);
   lList *selected = lSelect("selected", lp, where, what);
   CHECK(2, "unpacked, copied and selected lists are hashable without a slot array or a table",
         lGetNumberOfElem(unpacked) == (uint32_t) n && slots_empty(unpacked) &&
         lGetNumberOfElem(copy) == (uint32_t) n && slots_empty(copy) &&
         lGetNumberOfElem(selected) == (uint32_t) n && slots_empty(selected));

   lList *plain = lCreateListHash("plain", LZ_Type, false);
   for (int i = 0; i < n; i++) {
      lAddElemStr(&plain, LZ_name, name_of(i).c_str(), LZ_Type);
   }
   CHECK(3, "a list created without hashing has no slots and stays linear after a lookup",
         plain->nslots == 0 && lGetElemStr(plain, LZ_name, "e5") == linear_find(plain, "e5") &&
         list_slots(plain) == nullptr);

   CHECK(4, "the source list still has no table after being packed, copied and selected", slots_empty(lp));

   lFreeWhere(&where);
   lFreeWhat(&what);
   lFreeList(&plain);
   lFreeList(&selected);
   lFreeList(&copy);
   lFreeList(&unpacked);
   lFreeList(&lp);
}

/**
 * @brief LAZY-02: the first lookup builds the table, which is maintained afterwards
 * @return nothing; CHECK records the results
 */
static void test_first_lookup() {
   lList *lp = make_list(2 * OCS_CULL_HASH_MIN_ELEMS);
   const lListElem *e3 = lGetElemStr(lp, LZ_name, "e3");
   CHECK(5, "the first lookup builds the table of its field only and answers like the linear search",
         e3 != nullptr && e3 == linear_find(lp, "e3") && list_hash_table(lp, POS_NAME) != nullptr &&
         list_hash_table(lp, POS_GROUP) == nullptr && all_found(lp));

   lSetString(lFirstRW(lp), LZ_name, "renamed");
   lAddElemStr(&lp, LZ_name, "appended", LZ_Type);
   lListElem *e7 = lGetElemStrRW(lp, LZ_name, "e7");
   lRemoveElem(lp, &e7);
   CHECK(6, "after a rename, an append and a removal every lookup answers like the linear search",
         all_found(lp) && lGetElemStr(lp, LZ_name, "renamed") == lFirst(lp) &&
         lGetElemStr(lp, LZ_name, "e0") == nullptr && lGetElemStr(lp, LZ_name, "appended") == lLast(lp) &&
         lGetElemStr(lp, LZ_name, "e7") == nullptr);

   CHECK(7, "a non unique lookup builds its table and yields the group in list order",
         group_values(lp, 1) == group_values_in_list_order(lp, 1) && list_hash_table(lp, POS_GROUP) != nullptr);
   lFreeList(&lp);
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
      lList *lp = make_list(n);
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
               const lListElem *ep = lGetElemStr(lp, LZ_name, name_of(k).c_str());
               if (ep == nullptr || lGetUlong(ep, LZ_value) != (uint32_t) k) {
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
      lFreeList(&lp);
   }
   CHECK(8, "all concurrent first lookups answer correctly", wrong.load() == 0);
   CHECK(9, "after every round a table is published on each field", published);
}

/**
 * @brief LAZY-04: short lists stay linear, a table survives shrinking
 * @return nothing; CHECK records the results
 */
static void test_threshold() {
   lList *lp = make_list(OCS_CULL_HASH_MIN_ELEMS / 2);
   CHECK(10, "a lookup on a list below the threshold answers without building a table",
         lGetElemStr(lp, LZ_name, "e2") == linear_find(lp, "e2") && slots_empty(lp));

   for (uint32_t i = OCS_CULL_HASH_MIN_ELEMS / 2; i < OCS_CULL_HASH_MIN_ELEMS; i++) {
      lAddElemStr(&lp, LZ_name, name_of(i).c_str(), LZ_Type);
   }
   CHECK(11, "a list grown to the threshold gets the table on its next lookup",
         lGetElemStr(lp, LZ_name, "e2") == linear_find(lp, "e2") && list_hash_table(lp, POS_NAME) != nullptr);

   while (lGetNumberOfElem(lp) > 4) {
      lListElem *last = lLastRW(lp);
      lRemoveElem(lp, &last);
   }
   CHECK(12, "a list shrunk below the threshold keeps its table, which still answers",
         list_hash_table(lp, POS_NAME) != nullptr && all_found(lp) && lGetElemStr(lp, LZ_name, "e10") == nullptr);
   lFreeList(&lp);
}

/**
 * @brief LAZY-05: a duplicate in a unique field is counted when the table is built
 * @return nothing; CHECK records the results
 */
static void test_duplicate() {
   lList *lp = make_list(OCS_CULL_HASH_MIN_ELEMS);
   lListElem *first = lAddElemStr(&lp, LZ_name, "dup", LZ_Type);
   lListElem *second = lAddElemStr(&lp, LZ_name, "dup", LZ_Type);
   CHECK(13, "the lookup that builds the table returns the duplicate stored last",
         slots_empty(lp) && lGetElemStr(lp, LZ_name, "dup") == second);

   // the build counted the duplicate, so the removal repairs the table
   lRemoveElem(lp, &second);
   CHECK(14, "after removing that element the other one is found again",
         lGetElemStr(lp, LZ_name, "dup") == first);
   lFreeList(&lp);
}

/**
 * @brief LAZY-06: sorting drops the non unique tables
 * @return nothing; CHECK records the results
 */
static void test_sort() {
   lList *lp = make_list(2 * OCS_CULL_HASH_MIN_ELEMS);
   const std::vector<uint32_t> before = group_values(lp, 1);
   lGetElemStr(lp, LZ_name, "e1");
   const cull_htable unique_table = list_hash_table(lp, POS_NAME);

   lPSortList(lp, "%I-", LZ_value);
   CHECK(15, "the sort drops the non unique table and keeps the unique one",
         list_hash_table(lp, POS_GROUP) == nullptr && list_hash_table(lp, POS_NAME) == unique_table &&
         unique_table != nullptr);
   const std::vector<uint32_t> after = group_values(lp, 1);
   CHECK(16, "the next lookup yields the group in the new list order",
         after == group_values_in_list_order(lp, 1) && after != before && list_hash_table(lp, POS_GROUP) != nullptr);
   CHECK(17, "the unique table still finds every element", all_found(lp));
   lFreeList(&lp);
}

int main(int argc, char *argv[]) {
   lInit(nmv);

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
      {"LAZY-05", "unique duplicate counted at the build",
       "a duplicate in a unique field is counted when the table is built",
       {"append two elements with the same unique key without a lookup", "look the key up",
        "remove the element found and look the key up again"},
       "the lookup returns the element stored last; after its removal the other one is found",
       {{"LZY-05", "covers"}}, {13, 14}, test_duplicate},
      {"LAZY-06", "sorting drops non unique tables",
       "after a sort the hits of a non unique key come in the new list order",
       {"look a non unique and a unique key up", "sort the list", "look them up again"},
       "the non unique table is dropped and rebuilt in the new order, the unique table is unchanged",
       {{"LZY-06", "covers"}}, {15, 16, 17}, test_sort},
   });
}
