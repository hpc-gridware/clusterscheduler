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
 * @brief Module test of the CULL statistics counters (CS-2993)
 *
 * Built only with `ENABLE_CULL_STATS`. Each scenario runs a list operation
 * whose cost is known and compares the counters before and after it.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

#include <unistd.h>

#include <rapidjson/document.h>

/** @brief Make this translation unit the one that defines the CULL descriptors */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "cull/cull.h"
#include "cull/cull_listP.h"
#include "cull/cull_pack.h"
#include "cull/cull_stats.h"
#include "cull/ocs_cull_descr.h"

#include "uti/sge_component.h"
#include "uti/sge_stdlib.h"

#include "ocs_module_test.h"

/** @brief The counted type */
enum {
   ST_name = 1, ///< unique hashed key
   ST_group,    ///< non unique hashed ulong
   ST_value,    ///< an ulong
   ST_note      ///< a string
};

LISTDEF(ST_Type)
   SGE_STRING(ST_name, CULL_DEFAULT | CULL_HASH | CULL_UNIQUE)
   SGE_ULONG(ST_group, CULL_DEFAULT | CULL_HASH)
   SGE_ULONG(ST_value, CULL_DEFAULT)
   SGE_STRING(ST_note, CULL_DEFAULT)
LISTEND(ST_Type)

NAMEDEF(ST_Name)
   NAME("ST_name")
   NAME("ST_group")
   NAME("ST_value")
   NAME("ST_note")
NAMEEND

#define ST_Size sizeof(ST_Name) / sizeof(char *) ///< number of fields of ST_Type

/** @brief The name space registering the type with CULL */
lNameSpace nmv[] = {
   {1, ST_Size, ST_Name, ST_Type},
   {0, 0, nullptr, nullptr}
};

/// number of elements of the test list
static constexpr int LIST_SIZE = 10;

using ocs::cull_stats::Counter;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

/**
 * @brief The calling thread's value of a type counter of ST_Type
 * @param c the counter
 * @return its value
 */
static uint64_t type_count(const Counter c) {
   return ocs::cull_stats::get(true, ST_name, c);
}

/**
 * @brief The calling thread's value of a field counter
 * @param nm the field
 * @param c the counter
 * @return its value
 */
static uint64_t field_count(const int nm, const Counter c) {
   return ocs::cull_stats::get(false, nm, c);
}

/**
 * @brief Build the test list: LIST_SIZE elements, names e0..., groups 0 to 2
 * @return the list, owned by the caller
 */
static lList *make_list() {
   lList *lp = lCreateList("stats", ST_Type);
   for (int i = 0; i < LIST_SIZE; i++) {
      lListElem *ep = lAddElemStr(&lp, ST_name, ("e" + std::to_string(i)).c_str(), ST_Type);
      lSetUlong(ep, ST_group, i % 3);
      lSetUlong(ep, ST_value, i);
   }
   return lp;
}

/**
 * @brief Pack a list into a fresh pack buffer
 * @param lp the list
 * @param just_count true to only count the size
 * @param pb receives the buffer, to be cleared by the caller
 * @return true on success
 */
static bool pack_list(const lList *lp, const bool just_count, sge_pack_buffer *pb) {
   return init_packbuffer(pb, 1024, just_count, false) == PACK_SUCCESS && cull_pack_list(pb, lp) == PACK_SUCCESS;
}

/**
 * @brief Unpack a list from what a pack buffer holds, as the receiver does
 * @param pb the packed buffer
 * @param lpp receives the unpacked list
 * @return true on success
 */
static bool unpack_list_from(const sge_pack_buffer *pb, lList **lpp) {
   char *buf = sge_malloc(pb->bytes_used);
   memcpy(buf, pb->head_ptr, pb->bytes_used);
   sge_pack_buffer in;
   if (init_packbuffer_from_buffer(&in, buf, pb->bytes_used, false) != PACK_SUCCESS) {
      sge_free(&buf);
      return false;
   }
   const bool ok = cull_unpack_list(&in, lpp) == PACK_SUCCESS;
   clear_packbuffer(&in);
   return ok;
}

/**
 * @brief Look up a path of object members in a JSON document
 * @param doc the document
 * @param path the member names, outermost first
 * @return the value, nullptr if a member is missing or not inside an object
 */
static const rapidjson::Value *json_at(const rapidjson::Value &doc, std::initializer_list<const char *> path) {
   const rapidjson::Value *value = &doc;
   for (const char *name : path) {
      if (!value->IsObject()) {
         return nullptr;
      }
      const auto it = value->FindMember(name);
      if (it == value->MemberEnd()) {
         return nullptr;
      }
      value = &it->value;
   }
   return value;
}

/**
 * @brief Read an unsigned number at a path of a JSON document
 * @param doc the document
 * @param path the member names, outermost first
 * @return the number, 0 if it is missing or no unsigned number
 */
static uint64_t json_uint(const rapidjson::Value &doc, std::initializer_list<const char *> path) {
   const rapidjson::Value *value = json_at(doc, path);
   return value != nullptr && value->IsUint64() ? value->GetUint64() : 0;
}

/**
 * @brief Read a whole file
 * @param path the file
 * @return its content, empty if it cannot be read
 */
static std::string read_file(const std::string &path) {
   std::ifstream in(path);
   return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// ---------------------------------------------------------------------------
// scenarios
// ---------------------------------------------------------------------------

/**
 * @brief STATS-01: hash tables are counted from their build on the first lookup to their release
 * @return nothing; CHECK records the results
 */
static void test_hash_tables() {
   const uint64_t create = field_count(ST_name, ocs::cull_stats::HASH_CREATE);
   const uint64_t create_group = field_count(ST_group, ocs::cull_stats::HASH_CREATE);
   const uint64_t lazy = field_count(ST_name, ocs::cull_stats::HASH_BUILD_LAZY);
   const uint64_t lazy_group = field_count(ST_group, ocs::cull_stats::HASH_BUILD_LAZY);
   const uint64_t insert = field_count(ST_name, ocs::cull_stats::HASH_INSERT);
   const uint64_t lookup = field_count(ST_name, ocs::cull_stats::HASH_LOOKUP);
   const uint64_t lookup_group = field_count(ST_group, ocs::cull_stats::HASH_LOOKUP);
   const uint64_t used = field_count(ST_name, ocs::cull_stats::HASH_FREE_USED);
   const uint64_t unused = field_count(ST_name, ocs::cull_stats::HASH_FREE_UNUSED);

   lList *lp = make_list();
   CHECK(1, "creating and filling a list builds no table",
         field_count(ST_name, ocs::cull_stats::HASH_CREATE) == create &&
         field_count(ST_group, ocs::cull_stats::HASH_CREATE) == create_group &&
         field_count(ST_name, ocs::cull_stats::HASH_INSERT) == insert);

   lGetElemStr(lp, ST_name, "e3");
   lGetElemStr(lp, ST_name, "e7");
   const void *iterator = nullptr;
   lGetElemUlongFirst(lp, ST_group, 1, &iterator);
   CHECK(2, "the first lookup on a field builds its table with one insert per element",
         field_count(ST_name, ocs::cull_stats::HASH_CREATE) == create + 1 &&
         field_count(ST_group, ocs::cull_stats::HASH_CREATE) == create_group + 1 &&
         field_count(ST_value, ocs::cull_stats::HASH_CREATE) == 0 &&
         field_count(ST_name, ocs::cull_stats::HASH_BUILD_LAZY) == lazy + 1 &&
         field_count(ST_group, ocs::cull_stats::HASH_BUILD_LAZY) == lazy_group + 1 &&
         field_count(ST_name, ocs::cull_stats::HASH_INSERT) == insert + LIST_SIZE);
   CHECK(3, "every hashed search is one lookup on its field",
         field_count(ST_name, ocs::cull_stats::HASH_LOOKUP) == lookup + 2 &&
         field_count(ST_group, ocs::cull_stats::HASH_LOOKUP) == lookup_group + 1);

   lFreeList(&lp);
   lp = make_list();
   lFreeList(&lp);
   CHECK(4, "a freed table counts as used after a lookup; a list never searched frees no table",
         field_count(ST_name, ocs::cull_stats::HASH_FREE_USED) == used + 1 &&
         field_count(ST_name, ocs::cull_stats::HASH_FREE_UNUSED) == unused &&
         field_count(ST_name, ocs::cull_stats::HASH_CREATE) == create + 1);
}

/**
 * @brief STATS-05: lookups below the threshold are counted with the list length
 * @return nothing; CHECK records the results
 */
static void test_threshold() {
   const uint64_t create = field_count(ST_name, ocs::cull_stats::HASH_CREATE);
   const uint64_t linear = field_count(ST_name, ocs::cull_stats::LOOKUP_LINEAR);
   const uint64_t len = field_count(ST_name, ocs::cull_stats::LOOKUP_LEN_LT16);

   ocs_cull_hash_set_min_elems(LIST_SIZE + 1);
   lList *lp = make_list();
   const lListElem *found = lGetElemStr(lp, ST_name, "e3");
   CHECK(17, "a lookup on a list below the threshold searches linearly and builds no table",
         found != nullptr && field_count(ST_name, ocs::cull_stats::LOOKUP_LINEAR) == linear + 1 &&
         field_count(ST_name, ocs::cull_stats::HASH_CREATE) == create);
   CHECK(18, "the lookup is counted in the bucket of the list length",
         field_count(ST_name, ocs::cull_stats::LOOKUP_LEN_LT16) == len + 1);
   lFreeList(&lp);
   ocs_cull_hash_set_min_elems(0);
}

/**
 * @brief STATS-02: descriptor copies and positional lookups
 * @return nothing; CHECK records the results
 */
static void test_descriptors() {
   const uint64_t elem = type_count(ocs::cull_stats::DCOPY_ELEM);
   const uint64_t intern_new = type_count(ocs::cull_stats::DESCR_INTERN_NEW);
   const uint64_t intern_hit = type_count(ocs::cull_stats::DESCR_INTERN_HIT);
   const uint64_t priv = type_count(ocs::cull_stats::DESCR_PRIVATE);
   lListElem *ep = lCreateElem(ST_Type);
   CHECK(5, "lCreateElem of a full type copies and registers nothing",
         type_count(ocs::cull_stats::DCOPY_ELEM) == elem &&
         type_count(ocs::cull_stats::DESCR_INTERN_NEW) == intern_new &&
         type_count(ocs::cull_stats::DESCR_PRIVATE) == priv);
   lFreeElem(&ep);

   lList *lp = make_list();
   CHECK(6, "creating a list of a full type copies and registers nothing",
         type_count(ocs::cull_stats::DCOPY_ELEM) == elem &&
         type_count(ocs::cull_stats::DESCR_INTERN_NEW) == intern_new &&
         type_count(ocs::cull_stats::DESCR_INTERN_HIT) == intern_hit);

   lEnumeration *what = lWhat("%T(%I %I)", ST_Type, ST_name, ST_value);
   lList *reduced = lSelect("reduced", lp, nullptr, what);
   const uint64_t new_after_first = type_count(ocs::cull_stats::DESCR_INTERN_NEW);
   const uint64_t new_b = type_count(ocs::cull_stats::DESCR_INTERN_NEW_B);
   lEnumeration *what2 = lWhat("%T(%I %I)", ST_Type, ST_name, ST_value);
   lList *reduced2 = lSelect("reduced", lp, nullptr, what2);
   CHECK(19, "a reduction is registered once with its bytes, an equal one from another selection is a hit, and "
         "the registry peak covers it",
         new_after_first == intern_new + 1 && new_b > 0 &&
         type_count(ocs::cull_stats::DESCR_INTERN_NEW) == intern_new + 1 &&
         type_count(ocs::cull_stats::DESCR_INTERN_HIT) == intern_hit + 1 &&
         type_count(ocs::cull_stats::DESCR_PRIVATE) == priv && lFirst(reduced2)->descr == lFirst(reduced)->descr &&
         ocs_cull_descr_registry_peak() >= ocs_cull_descr_registry_size() && ocs_cull_descr_registry_peak() >= 1);
   lFreeList(&reduced2);
   lFreeWhat(&what2);
   const lListElem *first = lFirst(reduced);
   const uint64_t calls = type_count(ocs::cull_stats::POS_CALLS);
   const uint64_t on_reduced = type_count(ocs::cull_stats::POS_REDUCED);
   const uint64_t iters = type_count(ocs::cull_stats::POS_ITERS);
   const lUlong value = lGetUlong(first, ST_value);
   // a reduced descriptor has the length of its type, so the lookup is constant time (CS-439, PRF-04)
   CHECK(7, "a field read on a reduced element is one lookup without a search over the entries",
         value == 0 &&
         type_count(ocs::cull_stats::POS_CALLS) == calls + 1 &&
         type_count(ocs::cull_stats::POS_REDUCED) == on_reduced &&
         type_count(ocs::cull_stats::POS_ITERS) == iters);

   const uint64_t reduced_before = type_count(ocs::cull_stats::POS_REDUCED);
   lGetUlong(lFirst(lp), ST_value);
   CHECK(8, "a field read on a full element is no reduced lookup",
         type_count(ocs::cull_stats::POS_REDUCED) == reduced_before);

   lFreeList(&reduced);
   lFreeWhat(&what);
   lFreeList(&lp);
}

/**
 * @brief STATS-03: packing and unpacking
 * @return nothing; CHECK records the results
 */
static void test_packing() {
   lList *lp = make_list();

   const uint64_t descr = type_count(ocs::cull_stats::PACK_DESCR);
   const uint64_t descr_b = type_count(ocs::cull_stats::PACK_DESCR_B);
   const uint64_t elems = type_count(ocs::cull_stats::PACK_ELEMS);
   const uint64_t value_b = field_count(ST_value, ocs::cull_stats::PACK_VALUE_B);
   const uint64_t type_ref = type_count(ocs::cull_stats::DESCR_PACK_TYPE_REF);

   sge_pack_buffer counted;
   const bool counted_ok = pack_list(lp, true, &counted);
   clear_packbuffer(&counted);
   CHECK(9, "the counting mode counts nothing",
         counted_ok &&
         type_count(ocs::cull_stats::PACK_DESCR) == descr &&
         type_count(ocs::cull_stats::PACK_ELEMS) == elems &&
         field_count(ST_value, ocs::cull_stats::PACK_VALUE_B) == value_b &&
         type_count(ocs::cull_stats::DESCR_PACK_TYPE_REF) == type_ref);

   sge_pack_buffer pb;
   const bool packed_ok = pack_list(lp, false, &pb);
   CHECK(10, "packing counts the descriptor as a type reference with its bytes, the elements and the value bytes "
         "per field",
         packed_ok &&
         type_count(ocs::cull_stats::PACK_DESCR) == descr + 1 &&
         type_count(ocs::cull_stats::PACK_DESCR_B) == descr_b + INTSIZE * 3 &&
         type_count(ocs::cull_stats::DESCR_PACK_TYPE_REF) == type_ref + 1 &&
         type_count(ocs::cull_stats::PACK_ELEMS) == elems + LIST_SIZE &&
         field_count(ST_value, ocs::cull_stats::PACK_VALUE_B) == value_b + INTSIZE * LIST_SIZE);

   const uint64_t lists = type_count(ocs::cull_stats::UNPACK_LISTS);
   const uint64_t unpacked = type_count(ocs::cull_stats::UNPACK_ELEMS);
   const uint64_t dcopy = type_count(ocs::cull_stats::DCOPY_UNPACK);
   const uint64_t dcopy_b = type_count(ocs::cull_stats::DCOPY_UNPACK_B);
   const uint64_t ns = type_count(ocs::cull_stats::UNPACK_NS);
   const uint64_t create = field_count(ST_name, ocs::cull_stats::HASH_CREATE);
   lList *copy = nullptr;
   const bool unpacked_ok = unpack_list_from(&pb, &copy);
   CHECK(11, "unpacking counts the list and its elements and builds no descriptor for a type reference",
         unpacked_ok && lGetNumberOfElem(copy) == LIST_SIZE &&
         type_count(ocs::cull_stats::UNPACK_LISTS) == lists + 1 &&
         type_count(ocs::cull_stats::UNPACK_ELEMS) == unpacked + LIST_SIZE &&
         type_count(ocs::cull_stats::DCOPY_UNPACK) == dcopy &&
         type_count(ocs::cull_stats::DCOPY_UNPACK_B) == dcopy_b);
   CHECK(12, "unpacking counts its time and builds no hash table",
         type_count(ocs::cull_stats::UNPACK_NS) > ns &&
         field_count(ST_name, ocs::cull_stats::HASH_CREATE) == create);

   clear_packbuffer(&pb);
   lFreeList(&copy);

   // two lists of one reduction in one buffer: the table entry is defined once and referenced once
   lEnumeration *what = lWhat("%T(%I %I)", ST_Type, ST_name, ST_value);
   lList *r1 = lSelect("r1", lp, nullptr, what);
   lList *r2 = lSelect("r2", lp, nullptr, what);
   const uint64_t def = type_count(ocs::cull_stats::DESCR_PACK_TABLE_DEF);
   const uint64_t ref = type_count(ocs::cull_stats::DESCR_PACK_TABLE_REF);
   const uint64_t tmp = type_count(ocs::cull_stats::DCOPY_UNPACK);
   bool both_ok = false;
   bool unpack_ok = false;
   sge_pack_buffer two;
   if (init_packbuffer(&two, 1024, false, false) == PACK_SUCCESS) {
      both_ok = cull_pack_list(&two, r1) == PACK_SUCCESS && cull_pack_list(&two, r2) == PACK_SUCCESS;
      char *buf = sge_malloc(two.bytes_used);
      memcpy(buf, two.head_ptr, two.bytes_used);
      sge_pack_buffer in;
      if (init_packbuffer_from_buffer(&in, buf, two.bytes_used, false) == PACK_SUCCESS) {
         lList *u1 = nullptr;
         lList *u2 = nullptr;
         unpack_ok = cull_unpack_list(&in, &u1) == PACK_SUCCESS && cull_unpack_list(&in, &u2) == PACK_SUCCESS;
         lFreeList(&u2);
         lFreeList(&u1);
      }
      clear_packbuffer(&in);
      clear_packbuffer(&two);
   }
   CHECK(20, "a reduction is entered into the table of a buffer once and referenced afterwards; the receiver "
         "builds one temporary descriptor for the entry",
         both_ok && unpack_ok &&
         type_count(ocs::cull_stats::DESCR_PACK_TABLE_DEF) == def + 1 &&
         type_count(ocs::cull_stats::DESCR_PACK_TABLE_REF) == ref + 1 &&
         type_count(ocs::cull_stats::DCOPY_UNPACK) == tmp + 1);
   lFreeList(&r2);
   lFreeList(&r1);
   lFreeWhat(&what);
   lFreeList(&lp);
}

/**
 * @brief STATS-04: threads are merged by class and written out
 * @return nothing; CHECK records the results
 */
static void test_merge_and_write() {
   // two threads of one class, the trailing number is dropped
   for (const char *name : {"probe1", "probe22"}) {
      std::thread worker([name] {
         component_set_thread_name(name);
         // a new thread has the default threshold; the lookup builds the table on the short list
         lInit(nmv, 0);
         lList *lp = make_list();
         lGetElemStr(lp, ST_name, "e3");
         lFreeList(&lp);
      });
      worker.join();
   }

   char dir_template[] = "/tmp/test_cull_stats.XXXXXX";
   const char *dir = mkdtemp(dir_template);
   const std::string path = dir != nullptr ? ocs::cull_stats::write(dir) : std::string{};
   const std::string content = path.empty() ? std::string{} : read_file(path);
   rapidjson::Document doc;
   doc.Parse(content.c_str());
   const rapidjson::Value *program = doc.HasParseError() ? nullptr : json_at(doc, {"program"});
   const rapidjson::Value *threads = doc.HasParseError() ? nullptr : json_at(doc, {"threads"});
   CHECK(13, "the dump is one JSON document in the directory, naming its layout, program and process",
         !path.empty() && path.size() > 5 && path.compare(path.size() - 5, 5, ".json") == 0 &&
         !doc.HasParseError() && json_uint(doc, {"format"}) == 2 &&
         json_at(doc, {"descr_registry_peak"}) != nullptr &&
         program != nullptr && program->IsString() && strcmp(program->GetString(), "test_cull_stats") == 0 &&
         json_uint(doc, {"pid"}) == static_cast<uint64_t>(getpid()) &&
         json_at(doc, {"written"}) != nullptr && threads != nullptr && threads->IsObject());
   CHECK(14, "both threads add up in their class, the field named through the name space",
         !doc.HasParseError() &&
         json_uint(doc, {"threads", "probe", "fields", "ST_Type", "ST_name", "hash_create"}) == 2 &&
         json_uint(doc, {"threads", "probe", "fields", "ST_Type", "ST_name", "hash_insert"}) == 2 * LIST_SIZE);
   CHECK(15, "type counters are written per type, counters that are 0 are left out",
         !doc.HasParseError() &&
         json_uint(doc, {"threads", "probe", "types", "ST_Type", "pos_calls"}) > 0 &&
         json_at(doc, {"threads", "probe", "types", "ST_Type"}) != nullptr &&
         json_at(doc, {"threads", "probe", "types", "ST_Type", "pos_reduced"}) == nullptr);

   const std::string second = dir != nullptr ? ocs::cull_stats::write(dir) : std::string{};
   CHECK(16, "a second dump of the same process goes into a new file",
         !second.empty() && second != path);

   if (dir != nullptr) {
      std::filesystem::remove_all(dir);
   }
}

int main(int argc, char *argv[]) {
   // the lists here are short; with the threshold at 0 their first lookup builds the table
   lInit(nmv, 0);

   return ocs::module_test::run(argc, argv, {
      {"STATS-01", "hash tables are counted from their build to their release",
       "the statistics build counts the tables the first lookups build, the inserts and lookups on them, and "
       "whether a table was ever looked up when it is freed",
       {"create a list with two hashed fields and append elements", "search it by both fields",
        "free it, then create and free a second list without searching it"},
       "no table before the first lookup; then one table per searched field built lazily, one insert per "
       "element, one lookup per search; the first list frees its tables as used, the second frees none",
       {{"MSR-03", "covers"}, {"LZY-01", "covers"}}, {1, 2, 3, 4}, test_hash_tables},
      {"STATS-02", "descriptor copies, the registry and positional lookups",
       "the statistics build counts descriptor copies, registry misses and hits, and the field reads that "
       "need the linear search of a reduced descriptor",
       {"create an element and a list of a full type", "select a reduction twice with two equal selections",
        "read a field of a reduced element", "read the same field of a full element"},
       "the full type copies and registers nothing; the reduction is one miss with its bytes and one hit, both "
       "selections share it, and the peak covers it; neither the reduced nor the full read searches the entries",
       {{"MSR-03", "covers"}, {"PRF-03", "prepares"}, {"PRF-04", "unit level"}}, {5, 6, 19, 7, 8}, test_descriptors},
      {"STATS-03", "packing and unpacking",
       "the statistics build counts packed descriptors, elements and value bytes, and unpacked lists with "
       "their time, but nothing in counting mode",
       {"count the size of a list", "pack it", "unpack it", "pack two lists of one reduction into one buffer and "
        "unpack them"},
       "counting adds nothing; packing adds one type reference of three words, the elements and the value "
       "bytes of each field; unpacking adds the list, its elements and its time and builds no descriptor; the "
       "reduction is defined once and referenced once in the buffer",
       {{"MSR-03", "covers"}, {"PRF-03", "prepares"}}, {9, 10, 11, 12, 20}, test_packing},
      {"STATS-05", "lookups below the threshold",
       "a lookup on a list shorter than the threshold is counted as a linear search, with the list length",
       {"raise the threshold above the list length", "look an element up"},
       "the element is found, one linear search and one lookup in the length bucket are counted, no table "
       "is built",
       {{"LZY-04", "covers"}}, {17, 18}, test_threshold},
      {"STATS-04", "threads are merged by class and written out",
       "the counters of ended threads add up per thread class and are written as one JSON document per dump",
       {"create and search the same list in two threads of one class", "write the counters into a new directory",
        "write them a second time"},
       "one JSON document with the counters per class, type and field, both threads in it and no counter of 0; "
       "a second dump does not overwrite the first",
       {{"MSR-03", "covers"}}, {13, 14, 15, 16}, test_merge_and_write},
   });
}
