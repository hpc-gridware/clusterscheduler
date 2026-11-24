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
 * @brief Module test of the statistics counters of the list library (CS-2993)
 *
 * Built only with `ENABLE_TOM_STATS`. Each scenario runs a list operation
 * whose cost is known and compares the counters before and after it. The test
 * type has no generated traits, so the operations go through the run-time
 * typed interface (tom/ocs_Raw.h) and the core of the typed object model.
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

/** @brief Make this translation unit the one that defines the descriptors */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_Stats.h"
#include "tom/ocs_Types.h"
#include "ocs_test_listdef.h"
#include "tom/ocs_Descr.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_PackBuffer.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_Raw.h"

#include "uti/sge_component.h"
#include "uti/sge_stdlib.h"

#include "tom/ocs_Model.h"

#include "ocs_module_test.h"

/** @brief The counted type */
enum {
   ST_name = 1, ///< unique hashed key
   ST_group,    ///< non unique hashed ulong
   ST_value,    ///< an ulong
   ST_note      ///< a string
};

LISTDEF(ST_Type)
   SGE_STRING(ST_name, MT_DEFAULT | MT_HASH | MT_UNIQUE)
   SGE_ULONG(ST_group, MT_DEFAULT | MT_HASH)
   SGE_ULONG(ST_value, MT_DEFAULT)
   SGE_STRING(ST_note, MT_DEFAULT)
LISTEND(ST_Type)

/** @brief The field table of ST_Type, for the object model that names the fields in the dump */
static const ocs::tom::FieldInfo st_fields[] = {
   {ST_name, "ST_name", ocs::tom::FieldType::String, ocs::tom::Flag::Hash | ocs::tom::Flag::Unique},
   {ST_group, "ST_group", ocs::tom::FieldType::Uint32, ocs::tom::Flag::Hash},
   {ST_value, "ST_value", ocs::tom::FieldType::Uint32, ocs::tom::Flag::None},
   {ST_note, "ST_note", ocs::tom::FieldType::String, ocs::tom::Flag::None},
};

/** @brief The object types of this test, installed at start: the dump takes its names from the model */
static const ocs::tom::TypeTable st_types[] = {{"ST", ST_Type, st_fields, std::size(st_fields)}};

/// number of elements of the test list
static constexpr int LIST_SIZE = 10;

using ocs::tom::stats::Counter;

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

/**
 * @brief The calling thread's value of a type counter of ST_Type
 * @param c the counter
 * @return its value
 */
static uint64_t type_count(const Counter c) {
   return ocs::tom::stats::get(true, ST_name, c);
}

/**
 * @brief The calling thread's value of a field counter
 * @param nm the field
 * @param c the counter
 * @return its value
 */
static uint64_t field_count(const int nm, const Counter c) {
   return ocs::tom::stats::get(false, nm, c);
}

/**
 * @brief Build the test list: LIST_SIZE elements, names e0..., groups 0 to 2
 * @return the list
 */
static ocs::tom::RawListPtr make_list() {
   auto lp = ocs::tom::RawListPtr::create(ST_Type, ocs::tom::Hashing::On, "stats");
   for (int i = 0; i < LIST_SIZE; i++) {
      auto ep = ocs::tom::RawElementPtr::create(ST_Type);
      ep->set(ST_name, ("e" + std::to_string(i)).c_str());
      ep->set(ST_group, static_cast<std::uint32_t>(i % 3));
      ep->set(ST_value, static_cast<std::uint32_t>(i));
      lp->push_back(std::move(ep));
   }
   return lp;
}

/**
 * @brief Write a list into a buffer, without throwing
 * @param pb the buffer
 * @param lp the list
 * @return true on success
 */
static bool pack_list_ok(sge_pack_buffer *pb, const ocs::tom::ListData *lp) {
   try {
      ocs::tom::pack_list(pb, lp, nullptr, 0);
      return true;
   } catch (const ocs::tom::PackException &) {
      return false;
   }
}

/**
 * @brief Read a list from a buffer, without throwing
 * @param pb the buffer
 * @param[out] lp receives the list
 * @return true on success
 */
static bool unpack_list_ok(sge_pack_buffer *pb, ocs::tom::RawListPtr &lp) {
   try {
      lp = ocs::tom::unpack_raw_list(pb);
      return true;
   } catch (const ocs::tom::PackException &) {
      return false;
   }
}

/**
 * @brief Pack a list into a fresh pack buffer
 * @param lp the list
 * @param just_count true to only count the size
 * @param pb receives the buffer, to be cleared by the caller
 * @return true on success
 */
static bool pack_into(const ocs::tom::ListData *lp, const bool just_count, sge_pack_buffer *pb) {
   return ocs::tom::try_init_buffer(pb, 1024, just_count, false) == PACK_SUCCESS && pack_list_ok(pb, lp);
}

/**
 * @brief Prepare a reading buffer on a copy of what a pack buffer holds, as the receiver gets it
 * @param pb the packed buffer
 * @param in receives the reading buffer, to be cleared by the caller also on an error
 * @return true on success
 */
static bool reading_copy(const sge_pack_buffer *pb, sge_pack_buffer *in) {
   const std::size_t used = ocs::tom::buffer_used(pb);
   char *buf = sge_malloc(used);
   memcpy(buf, pb->head_ptr, used);
   return ocs::tom::try_init_buffer_from(in, buf, static_cast<std::uint32_t>(used), false) == PACK_SUCCESS;
}

/**
 * @brief Unpack a list from what a pack buffer holds, as the receiver does
 * @param pb the packed buffer
 * @param lp receives the unpacked list
 * @return true on success
 */
static bool unpack_list_from(const sge_pack_buffer *pb, ocs::tom::RawListPtr &lp) {
   sge_pack_buffer in;
   const bool ok = reading_copy(pb, &in) && unpack_list_ok(&in, lp);
   ocs::tom::clear_buffer(&in);
   return ok;
}

/**
 * @brief The selection of ST_name and ST_value, as lWhat("%T(%I %I)") built it
 * @return the selection
 */
static ocs::tom::RawWhatPtr name_value_what() {
   const int fields[] = {ST_name, ST_value};
   return ocs::tom::what_of(ST_Type, fields);
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
   const uint64_t create = field_count(ST_name, ocs::tom::stats::HASH_CREATE);
   const uint64_t create_group = field_count(ST_group, ocs::tom::stats::HASH_CREATE);
   const uint64_t lazy = field_count(ST_name, ocs::tom::stats::HASH_BUILD_LAZY);
   const uint64_t lazy_group = field_count(ST_group, ocs::tom::stats::HASH_BUILD_LAZY);
   const uint64_t insert = field_count(ST_name, ocs::tom::stats::HASH_INSERT);
   const uint64_t lookup = field_count(ST_name, ocs::tom::stats::HASH_LOOKUP);
   const uint64_t lookup_group = field_count(ST_group, ocs::tom::stats::HASH_LOOKUP);
   const uint64_t used = field_count(ST_name, ocs::tom::stats::HASH_FREE_USED);
   const uint64_t unused = field_count(ST_name, ocs::tom::stats::HASH_FREE_UNUSED);

   auto lp = make_list();
   CHECK(1, "creating and filling a list builds no table",
         field_count(ST_name, ocs::tom::stats::HASH_CREATE) == create &&
         field_count(ST_group, ocs::tom::stats::HASH_CREATE) == create_group &&
         field_count(ST_name, ocs::tom::stats::HASH_INSERT) == insert);

   static_cast<void>(lp->find_first(ST_name, "e3"));
   static_cast<void>(lp->find_first(ST_name, "e7"));
   const void *iterator = nullptr;
   const int pos_group = ocs::tom::field_pos(ST_Type, ST_group);
   static_cast<void>(ocs::tom::lookup_first<ocs::tom::FieldType::Uint32>(lp.get(), pos_group, 1, &iterator));
   CHECK(2, "the first lookup on a field builds its table with one insert per element",
         field_count(ST_name, ocs::tom::stats::HASH_CREATE) == create + 1 &&
         field_count(ST_group, ocs::tom::stats::HASH_CREATE) == create_group + 1 &&
         field_count(ST_value, ocs::tom::stats::HASH_CREATE) == 0 &&
         field_count(ST_name, ocs::tom::stats::HASH_BUILD_LAZY) == lazy + 1 &&
         field_count(ST_group, ocs::tom::stats::HASH_BUILD_LAZY) == lazy_group + 1 &&
         field_count(ST_name, ocs::tom::stats::HASH_INSERT) == insert + LIST_SIZE);
   CHECK(3, "every hashed search is one lookup on its field",
         field_count(ST_name, ocs::tom::stats::HASH_LOOKUP) == lookup + 2 &&
         field_count(ST_group, ocs::tom::stats::HASH_LOOKUP) == lookup_group + 1);

   lp.reset();
   lp = make_list();
   lp.reset();
   CHECK(4, "a freed table counts as used after a lookup; a list never searched frees no table",
         field_count(ST_name, ocs::tom::stats::HASH_FREE_USED) == used + 1 &&
         field_count(ST_name, ocs::tom::stats::HASH_FREE_UNUSED) == unused &&
         field_count(ST_name, ocs::tom::stats::HASH_CREATE) == create + 1);
}

/**
 * @brief STATS-05: lookups below the threshold are counted with the list length
 * @return nothing; CHECK records the results
 */
static void test_threshold() {
   const uint64_t create = field_count(ST_name, ocs::tom::stats::HASH_CREATE);
   const uint64_t linear = field_count(ST_name, ocs::tom::stats::LOOKUP_LINEAR);
   const uint64_t len = field_count(ST_name, ocs::tom::stats::LOOKUP_LEN_LT16);

   ocs::tom::detail::hash_set_min_elems(LIST_SIZE + 1);
   const auto lp = make_list();
   const ocs::tom::ConstRawElement found = lp->find_first(ST_name, "e3");
   CHECK(17, "a lookup on a list below the threshold searches linearly and builds no table",
         found && field_count(ST_name, ocs::tom::stats::LOOKUP_LINEAR) == linear + 1 &&
         field_count(ST_name, ocs::tom::stats::HASH_CREATE) == create);
   CHECK(18, "the lookup is counted in the bucket of the list length",
         field_count(ST_name, ocs::tom::stats::LOOKUP_LEN_LT16) == len + 1);
   ocs::tom::detail::hash_set_min_elems(0);
}

/**
 * @brief STATS-02: descriptor copies and positional lookups
 * @return nothing; CHECK records the results
 */
static void test_descriptors() {
   const uint64_t elem = type_count(ocs::tom::stats::DCOPY_ELEM);
   const uint64_t intern_new = type_count(ocs::tom::stats::DESCR_INTERN_NEW);
   const uint64_t intern_hit = type_count(ocs::tom::stats::DESCR_INTERN_HIT);
   const uint64_t priv = type_count(ocs::tom::stats::DESCR_PRIVATE);
   {
      const auto ep = ocs::tom::RawElementPtr::create(ST_Type);
      CHECK(5, "creating an element of a full type copies and registers nothing",
            type_count(ocs::tom::stats::DCOPY_ELEM) == elem &&
            type_count(ocs::tom::stats::DESCR_INTERN_NEW) == intern_new &&
            type_count(ocs::tom::stats::DESCR_PRIVATE) == priv);
   }

   const auto lp = make_list();
   CHECK(6, "creating a list of a full type copies and registers nothing",
         type_count(ocs::tom::stats::DCOPY_ELEM) == elem &&
         type_count(ocs::tom::stats::DESCR_INTERN_NEW) == intern_new &&
         type_count(ocs::tom::stats::DESCR_INTERN_HIT) == intern_hit);

   const ocs::tom::RawWhatPtr what = name_value_what();
   const auto reduced = ocs::tom::select_as("reduced", lp.get(), nullptr, what.get(), ocs::tom::Hashing::On);
   const uint64_t new_after_first = type_count(ocs::tom::stats::DESCR_INTERN_NEW);
   const uint64_t new_b = type_count(ocs::tom::stats::DESCR_INTERN_NEW_B);
   {
      const ocs::tom::RawWhatPtr what2 = name_value_what();
      const auto reduced2 = ocs::tom::select_as("reduced", lp.get(), nullptr, what2.get(), ocs::tom::Hashing::On);
      CHECK(19, "a reduction is registered once with its bytes, an equal one from another selection is a hit, and "
            "the registry peak covers it",
            new_after_first == intern_new + 1 && new_b > 0 &&
            type_count(ocs::tom::stats::DESCR_INTERN_NEW) == intern_new + 1 &&
            type_count(ocs::tom::stats::DESCR_INTERN_HIT) == intern_hit + 1 &&
            type_count(ocs::tom::stats::DESCR_PRIVATE) == priv &&
            reduced2->front().descr() == reduced->front().descr() &&
            ocs::tom::descr_registry_peak() >= ocs::tom::descr_registry_size() && ocs::tom::descr_registry_peak() >= 1);
   }
   // CHECK 7 and CHECK 8 removed: the position counters POS_CALLS, POS_REDUCED and POS_ITERS were kept by
   // the old lGetPosInDescr(), which has no TOM counterpart; the typed object model reads a reduced
   // element by position in constant time and counts nothing (CS-3103)
}

/**
 * @brief STATS-03: packing and unpacking
 * @return nothing; CHECK records the results
 */
static void test_packing() {
   const auto lp = make_list();

   const uint64_t descr = type_count(ocs::tom::stats::PACK_DESCR);
   const uint64_t descr_b = type_count(ocs::tom::stats::PACK_DESCR_B);
   const uint64_t elems = type_count(ocs::tom::stats::PACK_ELEMS);
   const uint64_t value_b = field_count(ST_value, ocs::tom::stats::PACK_VALUE_B);
   const uint64_t type_ref = type_count(ocs::tom::stats::DESCR_PACK_TYPE_REF);

   sge_pack_buffer counted;
   const bool counted_ok = pack_into(lp.get(), true, &counted);
   ocs::tom::clear_buffer(&counted);
   CHECK(9, "the counting mode counts nothing",
         counted_ok &&
         type_count(ocs::tom::stats::PACK_DESCR) == descr &&
         type_count(ocs::tom::stats::PACK_ELEMS) == elems &&
         field_count(ST_value, ocs::tom::stats::PACK_VALUE_B) == value_b &&
         type_count(ocs::tom::stats::DESCR_PACK_TYPE_REF) == type_ref);

   sge_pack_buffer pb;
   const bool packed_ok = pack_into(lp.get(), false, &pb);
   CHECK(10, "packing counts the descriptor as a type reference with its bytes, the elements and the value bytes "
         "per field",
         packed_ok &&
         type_count(ocs::tom::stats::PACK_DESCR) == descr + 1 &&
         type_count(ocs::tom::stats::PACK_DESCR_B) == descr_b + INTSIZE * 3 &&
         type_count(ocs::tom::stats::DESCR_PACK_TYPE_REF) == type_ref + 1 &&
         type_count(ocs::tom::stats::PACK_ELEMS) == elems + LIST_SIZE &&
         field_count(ST_value, ocs::tom::stats::PACK_VALUE_B) == value_b + INTSIZE * LIST_SIZE);

   const uint64_t lists = type_count(ocs::tom::stats::UNPACK_LISTS);
   const uint64_t unpacked = type_count(ocs::tom::stats::UNPACK_ELEMS);
   const uint64_t dcopy = type_count(ocs::tom::stats::DCOPY_UNPACK);
   const uint64_t dcopy_b = type_count(ocs::tom::stats::DCOPY_UNPACK_B);
   const uint64_t ns = type_count(ocs::tom::stats::UNPACK_NS);
   const uint64_t create = field_count(ST_name, ocs::tom::stats::HASH_CREATE);
   {
      ocs::tom::RawListPtr copy;
      const bool unpacked_ok = unpack_list_from(&pb, copy);
      CHECK(11, "unpacking counts the list and its elements and builds no descriptor for a type reference",
            unpacked_ok && copy && copy->size() == LIST_SIZE &&
            type_count(ocs::tom::stats::UNPACK_LISTS) == lists + 1 &&
            type_count(ocs::tom::stats::UNPACK_ELEMS) == unpacked + LIST_SIZE &&
            type_count(ocs::tom::stats::DCOPY_UNPACK) == dcopy &&
            type_count(ocs::tom::stats::DCOPY_UNPACK_B) == dcopy_b);
      CHECK(12, "unpacking counts its time and builds no hash table",
            type_count(ocs::tom::stats::UNPACK_NS) > ns &&
            field_count(ST_name, ocs::tom::stats::HASH_CREATE) == create);
   }
   ocs::tom::clear_buffer(&pb);

   // two lists of one reduction in one buffer: the table entry is defined once and referenced once
   const ocs::tom::RawWhatPtr what = name_value_what();
   const auto r1 = ocs::tom::select_as("r1", lp.get(), nullptr, what.get(), ocs::tom::Hashing::On);
   const auto r2 = ocs::tom::select_as("r2", lp.get(), nullptr, what.get(), ocs::tom::Hashing::On);
   const uint64_t def = type_count(ocs::tom::stats::DESCR_PACK_TABLE_DEF);
   const uint64_t ref = type_count(ocs::tom::stats::DESCR_PACK_TABLE_REF);
   const uint64_t tmp = type_count(ocs::tom::stats::DCOPY_UNPACK);
   bool both_ok = false;
   bool unpack_ok = false;
   sge_pack_buffer two;
   if (ocs::tom::try_init_buffer(&two, 1024, false, false) == PACK_SUCCESS) {
      both_ok = pack_list_ok(&two, r1.get()) && pack_list_ok(&two, r2.get());
      sge_pack_buffer in;
      if (reading_copy(&two, &in)) {
         ocs::tom::RawListPtr u1;
         ocs::tom::RawListPtr u2;
         unpack_ok = unpack_list_ok(&in, u1) && unpack_list_ok(&in, u2);
      }
      ocs::tom::clear_buffer(&in);
      ocs::tom::clear_buffer(&two);
   }
   CHECK(20, "a reduction is entered into the table of a buffer once and referenced afterwards; the receiver "
         "builds one temporary descriptor for the entry",
         both_ok && unpack_ok &&
         type_count(ocs::tom::stats::DESCR_PACK_TABLE_DEF) == def + 1 &&
         type_count(ocs::tom::stats::DESCR_PACK_TABLE_REF) == ref + 1 &&
         type_count(ocs::tom::stats::DCOPY_UNPACK) == tmp + 1);
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
         ocs::tom::detail::hash_set_min_elems(0);
         const auto lp = make_list();
         static_cast<void>(lp->find_first(ST_name, "e3"));
         // a type counter for the dump: the typed object model counts no positional lookups (pos_calls)
         sge_pack_buffer pb;
         static_cast<void>(pack_into(lp.get(), false, &pb));
         ocs::tom::clear_buffer(&pb);
      });
      worker.join();
   }

   char dir_template[] = "/tmp/test_tom_core_stats.XXXXXX";
   const char *dir = mkdtemp(dir_template);
   const std::string path = dir != nullptr ? ocs::tom::stats::write(dir) : std::string{};
   const std::string content = path.empty() ? std::string{} : read_file(path);
   rapidjson::Document doc;
   doc.Parse(content.c_str());
   const rapidjson::Value *program = doc.HasParseError() ? nullptr : json_at(doc, {"program"});
   const rapidjson::Value *threads = doc.HasParseError() ? nullptr : json_at(doc, {"threads"});
   CHECK(13, "the dump is one JSON document in the directory, naming its layout, program and process",
         !path.empty() && path.size() > 5 && path.compare(path.size() - 5, 5, ".json") == 0 &&
         !doc.HasParseError() && json_uint(doc, {"format"}) == 2 &&
         json_at(doc, {"descr_registry_peak"}) != nullptr &&
         program != nullptr && program->IsString() && strcmp(program->GetString(), "test_tom_core_stats") == 0 &&
         json_uint(doc, {"pid"}) == static_cast<uint64_t>(getpid()) &&
         json_at(doc, {"written"}) != nullptr && threads != nullptr && threads->IsObject());
   CHECK(14, "both threads add up in their class, the field named through the installed object model",
         !doc.HasParseError() &&
         json_uint(doc, {"threads", "probe", "fields", "ST_Type", "ST_name", "hash_create"}) == 2 &&
         json_uint(doc, {"threads", "probe", "fields", "ST_Type", "ST_name", "hash_insert"}) == 2 * LIST_SIZE);
   CHECK(15, "type counters are written per type, counters that are 0 are left out",
         !doc.HasParseError() &&
         json_uint(doc, {"threads", "probe", "types", "ST_Type", "pack_elems"}) == 2 * LIST_SIZE &&
         json_at(doc, {"threads", "probe", "types", "ST_Type"}) != nullptr &&
         json_at(doc, {"threads", "probe", "types", "ST_Type", "pos_reduced"}) == nullptr);

   const std::string second = dir != nullptr ? ocs::tom::stats::write(dir) : std::string{};
   CHECK(16, "a second dump of the same process goes into a new file",
         !second.empty() && second != path);

   if (dir != nullptr) {
      std::filesystem::remove_all(dir);
   }
}

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
int main(int argc, char *argv[]) {
   // the lists here are short; with the threshold at 0 their first lookup builds the table
   ocs::tom::detail::hash_set_min_elems(0);
   ocs::tom::install_model({st_types, {}});

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
       {{"MSR-03", "covers"}, {"PRF-03", "prepares"}, {"PRF-04", "unit level"}}, {5, 6, 19}, test_descriptors},
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
       {"create, search and pack the same list in two threads of one class", "write the counters into a new directory",
        "write them a second time"},
       "one JSON document with the counters per class, type and field, both threads in it and no counter of 0; "
       "a second dump does not overwrite the first",
       {{"MSR-03", "covers"}}, {13, 14, 15, 16}, test_merge_and_write},
   });
}
