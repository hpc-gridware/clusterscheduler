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
 * @brief Module test of the installed object model of the typed object model (TOM-MODEL)
 *
 * The field names, the lookup of a field by name, the sub-types and the type
 * names come from the generated tables, installed with
 * ocs::tom::install_model(). They replace the field name space, which is
 * removed, so this test compares them with the field tables for every field
 * of every type, and with the generated table all_sub_types for the sub-types. It also checks the answers for unknown fields and
 * descriptors, the fallback text and its buffers, the behaviour without an
 * installed model, the visibility of the model in a thread that did not
 * install it, the rejection of an inconsistent model, one case per rule of the
 * check, and the static descriptors the list library finds through the model.
 */

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <thread>
#include <unordered_map>

#include "sgeobj/sge_object.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "tom/ocs_Descr.h"
#include "tom/ocs_Model.h"

#include "ocs_module_test.h"
#include "tom/ocs_Types.h"

/**
 * @brief The fallback text the model gives for an unknown field
 * @param nm the field number
 * @return "nm <n>"
 */
static std::string
fallback_name(const int nm) {
   return "nm " + std::to_string(nm);
}

/**
 * @brief The first field number above the fields of every type of the model
 * @return a field number that no type holds
 */
static int
beyond_all_fields() {
   int beyond = 0;
   for (const ocs::tom::TypeTable &table : ocs::tom::all_type_tables) {
      beyond = std::max(beyond, table.fields[0].nm + static_cast<int>(table.size));
   }
   return beyond;
}

/**
 * @brief The model of the Cluster Scheduler installs, and installing it again is accepted
 * @return nothing; CHECK records the results
 */
static void
test_install() {
   const bool first = ocs::tom::install_model(ocs::tom::gcs_model());
   const bool again = ocs::tom::install_model(ocs::tom::gcs_model());
   CHECK(1, "the model installs and installing it again succeeds", first && again && ocs::tom::has_model());
   CHECK(2, "the model views the generated tables",
         ocs::tom::gcs_model().types.data() == ocs::tom::all_type_tables &&
         ocs::tom::gcs_model().types.size() == std::size(ocs::tom::all_type_tables) &&
         ocs::tom::gcs_model().sub_types.data() == ocs::tom::all_sub_types &&
         ocs::tom::gcs_model().sub_types.size() == std::size(ocs::tom::all_sub_types));
}

/**
 * @brief Every field has the name of its field table, and its name leads back to it
 * @return nothing; CHECK records the results
 */
static void
test_names() {
   int name_mismatches = 0;
   int find_mismatches = 0;
   int info_mismatches = 0;
   std::size_t n_fields = 0;
   for (const ocs::tom::TypeTable &table : ocs::tom::all_type_tables) {
      for (std::size_t i = 0; i < table.size; i++, n_fields++) {
         const ocs::tom::FieldInfo &f = table.fields[i];
         const char *name = ocs::tom::field_name(f.nm);
         if (std::strcmp(name, f.name) != 0) {
            std::printf("%s: field %d is named %s, the table says %s\n", table.name, f.nm, name, f.name);
            name_mismatches++;
         }
         const int found = ocs::tom::find_field(f.name);
         if (found != f.nm) {
            std::printf("%s: %s is found as %d, expected %d\n", table.name, f.name, found, f.nm);
            find_mismatches++;
         }
         if (ocs::tom::field_info(f.nm) != &f) {
            std::printf("%s: the table entry of %s is not the one of the model\n", table.name, f.name);
            info_mismatches++;
         }
      }
   }
   std::printf("%zu types, %zu fields\n", std::size(ocs::tom::all_type_tables), n_fields);
   CHECK(3, "field_name() of every field equals the table's name", name_mismatches == 0);
   CHECK(4, "find_field() of every name gives its field number", find_mismatches == 0);
   CHECK(5, "field_info() of every field is its entry in the type's table", info_mismatches == 0);
}

/**
 * @brief Every field has the sub-type the generated table all_sub_types names for it, and only those fields have one
 *
 * The independent reference object_get_subtype() of sgeobj, the lookup the model replaced, is removed with the
 * old list library (CS-3103); sub_type() is now checked against the generated table itself.
 *
 * @return nothing; CHECK records the results
 */
static void
test_sub_types() {
   int mismatches = 0;
   std::size_t n_with_sub = 0;
   for (const ocs::tom::TypeTable &table : ocs::tom::all_type_tables) {
      for (std::size_t i = 0; i < table.size; i++) {
         const int nm = table.fields[i].nm;
         const lDescr *got = ocs::tom::sub_type(nm);
         const lDescr *expected = nullptr;
         for (const ocs::tom::SubTypeEntry &entry : ocs::tom::all_sub_types) {
            if (entry.nm == nm) {
               expected = entry.sub;
            }
         }
         if (got != expected) {
            std::printf("%s: %s has another sub-type than all_sub_types names\n", table.name,
                        table.fields[i].name);
            mismatches++;
         }
         n_with_sub += got != nullptr ? 1 : 0;
      }
   }
   CHECK(6, "sub_type() of every field equals its entry in all_sub_types, nullptr included", mismatches == 0);
   CHECK(7, "every entry of all_sub_types gives exactly one field a sub-type",
         n_with_sub == std::size(ocs::tom::all_sub_types));
}

/**
 * @brief The type of a full and of a reduced descriptor is named, a descriptor of no type is not
 * @return nothing; CHECK records the results
 */
static void
test_type_names() {
   int mismatches = 0;
   for (const ocs::tom::TypeTable &table : ocs::tom::all_type_tables) {
      if (ocs::tom::type_name(table.descr) != table.name) {
         std::printf("%s: the full descriptor is named %s\n", table.name,
                     ocs::tom::type_name(table.descr) != nullptr ? ocs::tom::type_name(table.descr) : "nothing");
         mismatches++;
      }
   }
   CHECK(8, "type_name() of the full descriptor of every type is the type's name", mismatches == 0);

   // a reduced descriptor without the first field of the type: the first entry is kept, marked absent
   const lDescr *reduced = ocs::tom::descr_reduce(ocs::tom::JB::descriptor.e, {JB_job_name, JB_owner});
   const char *reduced_name = ocs::tom::type_name(reduced);
   CHECK(9, "type_name() of a reduced descriptor without the type's first field is the type's name",
         reduced != nullptr && reduced_name != nullptr && std::strcmp(reduced_name, "JB") == 0);
   ocs::tom::release_descr(reduced);

   const lDescr empty[] = {{NoName, MT_END, 0}};
   CHECK(10, "type_name() of nullptr, of an empty descriptor and of the tail of a descriptor is nullptr",
         ocs::tom::type_name(nullptr) == nullptr && ocs::tom::type_name(empty) == nullptr &&
         ocs::tom::type_name(ocs::tom::JB::descriptor.e + 1) == nullptr);
}

/**
 * @brief An unknown field number and an unknown name are answered without a crash
 * @return nothing; CHECK records the results
 */
static void
test_unknown() {
   const int beyond = beyond_all_fields();
   CHECK(11, "field_name() of an unknown field number is \"nm <n>\"",
         fallback_name(beyond) == ocs::tom::field_name(beyond) &&
         fallback_name(NoName) == ocs::tom::field_name(NoName));
   CHECK(12, "field_info() and sub_type() of an unknown field number are nullptr",
         ocs::tom::field_info(beyond) == nullptr && ocs::tom::field_info(NoName) == nullptr &&
         ocs::tom::sub_type(beyond) == nullptr && ocs::tom::sub_type(NoName) == nullptr);
   CHECK(13, "find_field() of an unknown name, an empty one, a short form and a prefix is NoName",
         ocs::tom::find_field("JB_no_such_field") == NoName && ocs::tom::find_field("") == NoName &&
         ocs::tom::find_field("job_number") == NoName && ocs::tom::find_field("JB_job_numbe") == NoName &&
         ocs::tom::find_field("JB_job_number_") == NoName);
   CHECK(14, "find_field() takes a view that is not terminated after the name",
         ocs::tom::find_field(std::string_view{"JB_job_number_and_more", 13}) == JB_job_number);
}

/**
 * @brief The fallback texts of a thread are kept in four buffers, and no other thread writes into them
 * @return nothing; CHECK records the results
 */
static void
test_fallback_buffers() {
   const int beyond = beyond_all_fields();
   const char *texts[4];
   for (int i = 0; i < 4; i++) {
      texts[i] = ocs::tom::field_name(beyond + i);
   }
   bool all_kept = true;
   for (int i = 0; i < 4; i++) {
      all_kept = all_kept && fallback_name(beyond + i) == texts[i];
   }
   CHECK(15, "four fallback texts in a row are all kept", all_kept);

   const char *own = ocs::tom::field_name(beyond + 100);
   std::thread other([beyond] {
      for (int i = 0; i < 16; i++) {
         static_cast<void>(ocs::tom::field_name(beyond + 1000 + i));
      }
   });
   other.join();
   CHECK(16, "another thread's fallback texts do not overwrite the own one", fallback_name(beyond + 100) == own);
   CHECK(17, "a known name is the model's constant, not a buffer",
         ocs::tom::field_name(JB_job_number) == ocs::tom::field_info(JB_job_number)->name);
}

/**
 * @brief Without an installed model nothing is found and nothing crashes; installing it again restores the lookups
 * @return nothing; CHECK records the results
 */
static void
test_without_model() {
   ocs::tom::uninstall_model();
   const bool none = !ocs::tom::has_model();
   const bool name = fallback_name(JB_job_number) == ocs::tom::field_name(JB_job_number);
   const bool rest = ocs::tom::find_field("JB_job_number") == NoName &&
                     ocs::tom::field_info(JB_job_number) == nullptr && ocs::tom::sub_type(JB_ja_tasks) == nullptr &&
                     ocs::tom::type_name(ocs::tom::JB::descriptor.e) == nullptr;
   const bool again = ocs::tom::install_model(ocs::tom::gcs_model());
   CHECK(18, "without a model: no model is reported and the name of a field is the fallback text", none && name);
   CHECK(19, "without a model: no field is found by name, no entry, no sub-type, no type name", rest);
   CHECK(20, "installed again: the lookups answer as before",
         again && ocs::tom::find_field("JB_job_number") == JB_job_number &&
         ocs::tom::sub_type(JB_ja_tasks) == ocs::tom::JAT::descriptor.e &&
         std::strcmp(ocs::tom::type_name(ocs::tom::JB::descriptor.e), "JB") == 0);
}

/**
 * @brief An inconsistent model is rejected and leaves the installed one in place
 * @return nothing; CHECK records the results
 */
static void
test_inconsistent() {
   const ocs::tom::Model empty{};
   const ocs::tom::TypeTable twice[] = {ocs::tom::all_type_tables[0], ocs::tom::all_type_tables[0]};
   const ocs::tom::Model overlapping{twice, {}};
   const ocs::tom::SubTypeEntry stray[] = {{beyond_all_fields(), ocs::tom::JB::descriptor.e}};
   const ocs::tom::Model stray_sub{ocs::tom::gcs_model().types, stray};
   CHECK(21, "a model without types, with a type twice or with a sub-type of no field is rejected",
         !ocs::tom::install_model(empty) && !ocs::tom::install_model(overlapping) &&
         !ocs::tom::install_model(stray_sub));
   CHECK(22, "after a rejected model the installed one still answers",
         ocs::tom::has_model() && ocs::tom::find_field("JB_job_number") == JB_job_number);
}

/**
 * @brief The model installed by the main thread answers in a thread that did not install it
 *
 * The model is process-wide, unlike the field name space it replaces, which
 * was bound per thread, so another thread finds the static descriptors too.
 *
 * @return nothing; CHECK records the results
 */
static void
test_other_thread() {
   bool static_found = false;
   bool answers = false;
   std::thread other([&static_found, &answers] {
      static_found = ocs::tom::descr_static_for(JB_job_number) == ocs::tom::JB::descriptor.e &&
                     ocs::tom::descr_is_static(ocs::tom::JB::descriptor.e);
      const char *type = ocs::tom::type_name(ocs::tom::JB::descriptor.e);
      answers = ocs::tom::has_model() && std::strcmp(ocs::tom::field_name(JB_job_number), "JB_job_number") == 0 &&
                ocs::tom::find_field("JB_job_number") == JB_job_number &&
                ocs::tom::sub_type(JB_ja_tasks) == ocs::tom::JAT::descriptor.e && type != nullptr &&
                std::strcmp(type, "JB") == 0;
   });
   other.join();
   CHECK(23, "a thread that did not install the model finds the static descriptor of a type", static_found);
   CHECK(24, "in that thread the model gives names, numbers, sub-types and type names", answers);
}

namespace {

   // A small model of two types, valid as a whole; each invalid model below breaks one rule of it. The tables
   // are static: an installed model must outlive the program, and a built index is kept for a re-install.

   /** @brief The fields of type TA: a number and a list of TB */
   const ocs::tom::FieldInfo ta_fields[] = {
      {1, "TA_one", ocs::tom::FieldType::Uint32, ocs::tom::Flag::None},
      {2, "TA_list", ocs::tom::FieldType::List, ocs::tom::Flag::None},
   };
   /** @brief The descriptor of type TA */
   const lDescr ta_descr[] = {{1, MT_UINT32, 0}, {2, MT_LIST, 0}, {NoName, MT_END, 0}};
   /** @brief The fields of type TB: a string */
   const ocs::tom::FieldInfo tb_fields[] = {
      {10, "TB_one", ocs::tom::FieldType::String, ocs::tom::Flag::None},
   };
   /** @brief The descriptor of type TB */
   const lDescr tb_descr[] = {{10, MT_STRING, 0}, {NoName, MT_END, 0}};

   /** @brief The valid model: TA and TB, TA_list holds TB */
   const ocs::tom::TypeTable valid_types[] = {{"TA", ta_descr, ta_fields, 2}, {"TB", tb_descr, tb_fields, 1}};
   const ocs::tom::SubTypeEntry valid_subs[] = {{2, tb_descr}};

   /** @brief A field without a name */
   const ocs::tom::FieldInfo noname_fields[] = {
      {1, "TA_one", ocs::tom::FieldType::Uint32, ocs::tom::Flag::None},
      {2, nullptr, ocs::tom::FieldType::List, ocs::tom::Flag::None},
   };
   const ocs::tom::TypeTable noname_types[] = {{"TA", ta_descr, noname_fields, 2}};

   /** @brief A type without a name */
   const ocs::tom::TypeTable unnamed_types[] = {{nullptr, ta_descr, ta_fields, 2}};

   /** @brief A type without fields */
   const ocs::tom::TypeTable empty_types[] = {{"TA", ta_descr, ta_fields, 0}};

   /** @brief Field numbers 1 and 3: not consecutive, though the descriptor agrees */
   const ocs::tom::FieldInfo gap_fields[] = {
      {1, "TA_one", ocs::tom::FieldType::Uint32, ocs::tom::Flag::None},
      {3, "TA_list", ocs::tom::FieldType::List, ocs::tom::Flag::None},
   };
   const lDescr gap_descr[] = {{1, MT_UINT32, 0}, {3, MT_LIST, 0}, {NoName, MT_END, 0}};
   const ocs::tom::TypeTable gap_types[] = {{"TA", gap_descr, gap_fields, 2}};

   /** @brief A descriptor whose first entry is another field than the table's first */
   const lDescr first_descr[] = {{5, MT_UINT32, 0}, {2, MT_LIST, 0}, {NoName, MT_END, 0}};
   const ocs::tom::TypeTable first_types[] = {{"TA", first_descr, ta_fields, 2}};

   /** @brief A descriptor that agrees in entry 0, but declares entry 1 a string */
   const lDescr later_descr[] = {{1, MT_UINT32, 0}, {2, MT_STRING, 0}, {NoName, MT_END, 0}};
   const ocs::tom::TypeTable later_types[] = {{"TA", later_descr, ta_fields, 2}};

   /** @brief A descriptor with one entry more than the table has fields */
   const lDescr long_descr[] = {{1, MT_UINT32, 0}, {2, MT_LIST, 0}, {3, MT_UINT32, 0}, {NoName, MT_END, 0}};
   const ocs::tom::TypeTable long_types[] = {{"TA", long_descr, ta_fields, 2}};

   /** @brief Two types of the name TA, with field ranges and names of their own */
   const ocs::tom::TypeTable same_name_types[] = {{"TA", ta_descr, ta_fields, 2}, {"TA", tb_descr, tb_fields, 1}};

   /** @brief A second type that reuses the field name TA_one */
   const ocs::tom::FieldInfo dup_name_fields[] = {
      {10, "TA_one", ocs::tom::FieldType::String, ocs::tom::Flag::None},
   };
   const ocs::tom::TypeTable dup_name_types[] = {{"TA", ta_descr, ta_fields, 2}, {"TB", tb_descr, dup_name_fields, 1}};

   /** @brief A type TC whose only field number 2 lies in the range of TA */
   const ocs::tom::FieldInfo overlap_fields[] = {
      {2, "TC_one", ocs::tom::FieldType::Uint32, ocs::tom::Flag::None},
   };
   const lDescr overlap_descr[] = {{2, MT_UINT32, 0}, {NoName, MT_END, 0}};
   const ocs::tom::TypeTable overlap_types[] = {
      {"TA", ta_descr, ta_fields, 2},
      {"TC", overlap_descr, overlap_fields, 1},
   };

   /** @brief A sub-type on the scalar field TA_one */
   const ocs::tom::SubTypeEntry scalar_subs[] = {{1, tb_descr}};

   /** @brief Two sub-type entries for TA_list */
   const ocs::tom::SubTypeEntry twice_subs[] = {{2, tb_descr}, {2, tb_descr}};

} // namespace

/**
 * @brief Whether a model is rejected, and for the rule expected
 *
 * The rule is recognised by a part of the reason install_model() reports, so
 * that a model rejected for another rule than the one it breaks fails the check.
 *
 * @param model the model to install
 * @param rule a text the reason must contain
 * @return true if the install failed and the reason contains @p rule
 */
static bool
rejected_for(const ocs::tom::Model &model, const char *rule) {
   std::string reason;
   if (ocs::tom::install_model(model, &reason)) {
      std::printf("the model was installed, expected a rejection for \"%s\"\n", rule);
      return false;
   }
   if (reason.find(rule) == std::string::npos) {
      std::printf("the model was rejected for \"%s\", expected \"%s\"\n", reason.c_str(), rule);
      return false;
   }
   return true;
}

/**
 * @brief Every rule of the consistency check rejects a model that breaks only that rule
 *
 * A small valid model is installed first, to show that each invalid model
 * fails for the one broken rule only, which the reason of the rejection
 * names; the model of the Cluster Scheduler is installed again at the end.
 *
 * @return nothing; CHECK records the results
 */
static void
test_rules() {
   const bool valid = ocs::tom::install_model({valid_types, valid_subs}) &&
                      std::strcmp(ocs::tom::field_name(2), "TA_list") == 0 && ocs::tom::sub_type(2) == tb_descr;
   CHECK(25, "the small valid model installs and answers", valid);
   CHECK(26, "a field without a name is rejected",
         rejected_for({noname_types, {}}, "type TA, field 1 (number 2) has no name"));
   CHECK(27, "a type without a name is rejected", rejected_for({unnamed_types, {}}, "a type has no name"));
   CHECK(28, "a type without fields is rejected",
         rejected_for({empty_types, {}}, "type TA has no descriptor or no fields"));
   CHECK(29, "field numbers that are not consecutive are rejected",
         rejected_for({gap_types, {}}, "the field numbers are not consecutive"));
   CHECK(30, "a descriptor whose first entry is not the table's first field is rejected",
         rejected_for({first_types, {}}, "field 0 (TA_one): the descriptor holds field 5 of type MT_UINT32"));
   CHECK(31, "a descriptor that differs from the table in a later entry is rejected, naming both types",
         rejected_for({later_types, {}},
                      "field 1 (TA_list): the descriptor holds field 2 of type MT_STRING, the table field 2 of type "
                      "MT_LIST"));
   CHECK(32, "a descriptor with more entries than the table has fields is rejected",
         rejected_for({long_types, {}}, "the descriptor has more entries than the 2 fields"));
   CHECK(33, "a type name that occurs twice is rejected",
         rejected_for({same_name_types, {}}, "the type name TA occurs twice"));
   CHECK(34, "a field name that occurs twice is rejected",
         rejected_for({dup_name_types, {}}, "the field name TA_one occurs twice"));
   CHECK(35, "a sub-type on a field that is neither a list nor an object field is rejected",
         rejected_for({valid_types, scalar_subs}, "TA_one, which is neither a list nor an object field"));
   CHECK(36, "two sub-type entries for the same field are rejected",
         rejected_for({valid_types, twice_subs}, "TA_list has more than one sub-type entry"));
   CHECK(37, "types with overlapping field numbers are rejected",
         rejected_for({overlap_types, {}}, "overlap those of type TC"));
   const bool kept = ocs::tom::sub_type(2) == tb_descr;
   const bool again = ocs::tom::install_model(ocs::tom::gcs_model()) &&
                      ocs::tom::find_field("JB_job_number") == JB_job_number &&
                      ocs::tom::find_field("TA_one") == NoName;
   CHECK(38, "the rejected models left the small one in place; the model of the Cluster Scheduler installs again",
         kept && again);
}

/**
 * @brief The list library finds the type of a field and the static descriptor of a type through the model
 *
 * This is the part of the removed field name space that the list library
 * itself needs: a full descriptor is held as the static block of its type,
 * and the unpacking of a type reference finds the block by the type's first
 * field.
 *
 * @return nothing; CHECK records the results
 */
static void
test_static_descriptors() {
   const int beyond = beyond_all_fields();
   const ocs::tom::TypeTable *first = ocs::tom::type_table(JB_job_number);
   const ocs::tom::TypeTable *later = ocs::tom::type_table(JB_owner);
   CHECK(39, "type_table() of a first and a later field is the type's table, of an unknown field nullptr",
         first != nullptr && first == later && first->descr == ocs::tom::JB::descriptor.e &&
         std::strcmp(first->name, "JB") == 0 &&
         ocs::tom::type_table(beyond) == nullptr && ocs::tom::type_table(NoName) == nullptr);

   int pos = -1;
   const lDescr *containing = ocs::tom::descr_static_containing(JB_owner, &pos);
   int unknown_pos = -1;
   CHECK(40, "the static block lookups give JB and the field's position, nothing for a later or unknown field",
         ocs::tom::descr_static_for(JB_job_number) == ocs::tom::JB::descriptor.e &&
         ocs::tom::descr_static_for(JB_owner) == nullptr &&
         ocs::tom::descr_static_for(beyond) == nullptr && containing == ocs::tom::JB::descriptor.e &&
         pos == JB_owner - JB_job_number && ocs::tom::descr_static_containing(beyond, &unknown_pos) == nullptr &&
         unknown_pos == -1 && ocs::tom::descr_is_static(ocs::tom::JB::descriptor.e));

   ocs::tom::uninstall_model();
   int none_pos = -1;
   const bool none = ocs::tom::type_table(JB_job_number) == nullptr &&
                     ocs::tom::descr_static_for(JB_job_number) == nullptr &&
                     ocs::tom::descr_static_containing(JB_owner, &none_pos) == nullptr && none_pos == -1 &&
                     !ocs::tom::descr_is_static(ocs::tom::JB::descriptor.e);
   const bool again = ocs::tom::install_model(ocs::tom::gcs_model());
   CHECK(41, "without a model no type and no static block is found", none);
   CHECK(42, "installed again: JB is the static block of the job type again",
         again && ocs::tom::descr_static_for(JB_job_number) == ocs::tom::JB::descriptor.e &&
         ocs::tom::descr_is_static(ocs::tom::JB::descriptor.e));
}

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"TOM-MODEL-01", "install",
       "the model of the Cluster Scheduler installs, idempotently, and views the generated tables",
       {"install the model twice", "compare its views with all_type_tables and all_sub_types"},
       "both installs succeed; the views are the generated tables",
       {}, {1, 2}, test_install},
      {"TOM-MODEL-02", "names",
       "field names and the lookup by name agree with the field tables for every field of every type",
       {"for every field: compare field_name() with the table's name, find_field() with the number, "
        "field_info() with the table entry"},
       "no field differs",
       {}, {3, 4, 5}, test_names},
      {"TOM-MODEL-03", "sub-types",
       "the sub-type of every field is the one the generated table all_sub_types names",
       {"compare sub_type() of every field with all_sub_types",
        "count the fields with a sub-type against the entries of all_sub_types"},
       "no field differs; every entry of all_sub_types gives one field a sub-type",
       {}, {6, 7}, test_sub_types},
      {"TOM-MODEL-04", "type names",
       "the type of a full and of a reduced descriptor is named; a descriptor of no type is not",
       {"name the full descriptor of every type", "name a reduced job descriptor without JB_job_number",
        "name nullptr, an empty descriptor and the tail of the job descriptor"},
       "the type's name for full and reduced descriptors, nullptr otherwise",
       {}, {8, 9, 10}, test_type_names},
      {"TOM-MODEL-05", "unknown",
       "an unknown field number and an unknown name are answered without a crash",
       {"ask for the name, entry and sub-type of field numbers no type holds",
        "look up unknown names, the empty name, a short form, a prefix and an unterminated view"},
       "\"nm <n>\" as name, nullptr as entry and sub-type, NoName as number; the view is read to its length only",
       {}, {11, 12, 13, 14}, test_unknown},
      {"TOM-MODEL-06", "fallback buffers",
       "the fallback text of an unknown field is kept per thread in four buffers",
       {"ask for four unknown names and compare all four afterwards",
        "ask for unknown names in another thread and compare the own text afterwards"},
       "all texts are kept; a known name is the model's constant",
       {}, {15, 16, 17}, test_fallback_buffers},
      {"TOM-MODEL-07", "without model",
       "without an installed model nothing is found and nothing crashes",
       {"uninstall the model and ask every lookup", "install the model again and ask again"},
       "fallback name, NoName and nullptr without the model; the old answers after installing it again",
       {}, {18, 19, 20}, test_without_model},
      {"TOM-MODEL-08", "inconsistent",
       "an inconsistent model is rejected and leaves the installed one in place",
       {"install a model without types, one with a type twice, one with a sub-type of no field"},
       "every install fails; the installed model still answers",
       {}, {21, 22}, test_inconsistent},
      {"TOM-MODEL-09", "other thread",
       "the model installed by the main thread answers in a thread that did not install it",
       {"in a new thread: look up the static descriptor of a type, then ask every lookup"},
       "the static descriptor is found; the lookups answer as in the main thread",
       {}, {23, 24}, test_other_thread},
      {"TOM-MODEL-10", "rules",
       "every rule of the consistency check rejects a model that breaks only that rule, and names that rule",
       {"install a small valid model", "install one model per rule that breaks only that rule",
        "install types with overlapping field numbers", "install the model of the Cluster Scheduler again"},
       "the valid model installs; every other install fails for the rule it breaks and leaves the valid one in "
       "place; the model installs again",
       {}, {25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38}, test_rules},
      {"TOM-MODEL-11", "static descriptors",
       "the list library finds the type of a field and the static descriptor of a type through the model",
       {"ask type_table() for a first, a later and an unknown field", "ask the static block lookups of the list "
        "library for the same fields", "uninstall the model and ask again", "install the model again"},
       "the job type's table and JB with the field's position; nothing for an unknown field or without "
       "the model; the old answers after installing it again",
       {}, {39, 40, 41, 42}, test_static_descriptors},
   });
}
