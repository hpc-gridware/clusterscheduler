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
 * @brief Module test of the object types of the typed object model (CS-3043, TRT-01)
 *
 * The field table of a type is the source of its descriptor, so the two cannot
 * disagree. This test checks what does not follow from that: that the object
 * types of the model hold disjoint ranges of field numbers in ascending order, that the translation into
 * the list library's tags keeps every field, that the sub-types the model names
 * are the ones `object_get_subtype()` hands out, and that the value type of host
 * fields compares like a host name.
 *
 * The host comparison reads the bootstrap file of an installed cluster
 * (sge_hostcmp()), so the test needs SGE_ROOT; module_test_cull provides it.
 */

#include <cstdint>
#include <cstdio>
#include <iterator>
#include <type_traits>

#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Model.h"

#include "ocs_module_test.h"
#include "tom/ocs_Types.h"

using ocs::tom::FieldTraits;
using ocs::tom::FieldType;
using ocs::tom::Host;
using ocs::tom::ValueOf;

// what follows from the object types at compile time, on a few known fields
static_assert(FieldTraits<JB_job_number>::pos == 0, "the first field of a type is at position 0");
static_assert(FieldTraits<JB_job_number>::base == JB_LOWERBOUND, "the base of a type is its lower bound");
static_assert(FieldTraits<JB_job_number>::type == FieldType::Uint32 && FieldTraits<JB_job_number>::hashed);
static_assert(std::is_same_v<FieldTraits<JB_job_number>::Type, ocs::tom::JB>, "a field number names its object type");
static_assert(std::is_same_v<FieldTraits<JB_ja_tasks>::Sub, ocs::tom::JAT>,
              "the sub-type of a list field is taken from the model");
static_assert(FieldTraits<JB_ja_tasks>::type == FieldType::List && FieldTraits<JB_ja_tasks>::has_sub);
static_assert(!FieldTraits<OR_joker>::has_sub, "a field of any sub-type names none");
static_assert(std::is_same_v<FieldTraits<JB_checkpoint_object>::Sub, ocs::tom::CK>,
              "an object field whose type is fixed names it in the model (CS-3102)");
static_assert(std::is_same_v<FieldTraits<CQ_seq_no>::Sub, ocs::tom::AULNG>,
              "a cluster queue attribute list names the type of its href table entry (CS-3102)");
static_assert(std::is_same_v<FieldTraits<STN_children>::Sub, ocs::tom::STN>, "a type can be its own sub-type");
static_assert(FieldTraits<ABOOL_href>::hashed && FieldTraits<ABOOL_href>::unique);
static_assert(std::is_same_v<ValueOf<FieldTraits<JB_job_name>::type>::type, const char *>);
static_assert(std::is_same_v<ValueOf<FieldTraits<JB_job_number>::type>::type, std::uint32_t>);
static_assert(std::is_same_v<ValueOf<FieldTraits<ABOOL_href>::type>::type, Host>);
static_assert(FieldTraits<JB_submission_time>::type == FieldType::Uint64, "a timestamp is an unsigned 64 bit number");

// the field enumeration of a type is named, so a parameter can take the field numbers of one type only
static_assert(std::is_same_v<decltype(JB_job_number), JB_Field> && std::is_same_v<decltype(JB_ja_tasks), JB_Field>);
static_assert(std::is_convertible_v<JB_Field, int>, "a field number still converts to the int of the old interface");
static_assert(!std::is_convertible_v<ABOOL_Field, JB_Field>, "a field of another type is not a job field");
static_assert(!std::is_convertible_v<int, JB_Field>, "a plain number is not a job field");

// a host name is a pointer, made from a C string only explicitly, and given back as one implicitly
static_assert(sizeof(Host) == sizeof(const char *) && std::is_trivially_copyable_v<Host>);
static_assert(!std::is_convertible_v<const char *, Host> && std::is_constructible_v<Host, const char *>);
static_assert(std::is_convertible_v<Host, const char *>);

/**
 * @brief The object types of the model hold disjoint ranges of field numbers, in ascending order
 *
 * The generator allocates the ranges in the order of `all_type_tables`, one
 * after the other, as the CULL name space listed them; the model that the
 * list library reads is these tables.
 *
 * @return nothing; CHECK records the results
 */
static void
test_every_type() {
   bool in_order = true;
   int next_free = 0;
   for (std::size_t i = 0; i < std::size(ocs::tom::all_type_tables); i++) {
      const ocs::tom::TypeTable &table = ocs::tom::all_type_tables[i];
      if (table.fields[0].nm < next_free || table.descr == nullptr || table.descr[0].nm != table.fields[0].nm) {
         std::printf("type %s at index %zu with lower bound %d does not follow the previous type\n", table.name, i,
                     table.fields[0].nm);
         in_order = false;
      }
      next_free = table.fields[0].nm + static_cast<int>(table.size);
   }
   CHECK(1, "the types of the model hold disjoint ranges of field numbers in ascending order, and are the model",
         in_order && ocs::tom::gcs_model().types.data() == ocs::tom::all_type_tables &&
         ocs::tom::type_table(JB_job_number) == &ocs::tom::all_type_tables[0]);
}

/**
 * @brief The descriptor derived from a field table holds every field as the table declares it
 * @return nothing; CHECK records the results
 */
static void
test_entries() {
   int mismatches = 0;
   std::size_t n_fields = 0;
   for (const ocs::tom::TypeTable &table : ocs::tom::all_type_tables) {
      const lDescr *dp = table.descr;
      for (std::size_t i = 0; i < table.size; i++, n_fields++) {
         const ocs::tom::FieldInfo &f = table.fields[i];
         const int mt = ocs::tom::cull_type_of(f.type) | ocs::tom::cull_flags_of(f.flags);
         if (dp[i].nm != f.nm || dp[i].mt != mt || dp[i].off != 0 ||
             (i > 0 && f.nm != table.fields[i - 1].nm + 1)) {
            std::printf("%s: entry %zu (%s) is not held as declared\n", table.name, i, ocs::tom::field_name(f.nm));
            mismatches++;
         }
      }
      if (dp[table.size].nm != NoName) {
         std::printf("%s: the descriptor has more fields than the table\n", table.name);
         mismatches++;
      }
   }
   std::printf("%zu types, %zu fields\n", std::size(ocs::tom::all_type_tables), n_fields);
   CHECK(2, "every field is held in its descriptor with its number, type and flags, numbers consecutive",
         mismatches == 0);
}

/**
 * @brief The sub-type of every list and object field is the one object_get_subtype() hands out
 * @return nothing; CHECK records the results
 */
static void
test_sub_types() {
   int mismatches = 0;
   int n_list_fields = 0;
   for (const ocs::tom::TypeTable &table : ocs::tom::all_type_tables) {
      for (std::size_t i = 0; i < table.size; i++) {
         const ocs::tom::FieldInfo &f = table.fields[i];
         if (f.type != FieldType::List && f.type != FieldType::Object) {
            continue;
         }
         n_list_fields++;
         const lDescr *traits = nullptr;
         for (const ocs::tom::SubTypeEntry &entry : ocs::tom::all_sub_types) {
            if (entry.nm == f.nm) {
               traits = entry.sub;
            }
         }
         if (traits != object_get_subtype(f.nm)) {
            std::printf("%s: %s names a different sub-type than object_get_subtype()\n", table.name,
                        ocs::tom::field_name(f.nm));
            mismatches++;
         }
      }
   }
   std::printf("%d list and object fields, %zu of them name a sub-type\n", n_list_fields,
               std::size(ocs::tom::all_sub_types));
   CHECK(3, "every list and object field names the sub-type of object_get_subtype()", mismatches == 0);
   CHECK(4, "the model names sub-types at all", std::size(ocs::tom::all_sub_types) > 0);
}

/**
 * @brief A host name compares like a host name, also against a C string
 * @return nothing; CHECK records the results
 */
static void
test_host() {
   const Host lower{"host1"};
   const Host upper{"HOST1"};
   const Host other{"host2"};
   const Host unset;
   CHECK(5, "host names differing in case are equal", lower == upper);
   CHECK(6, "different host names are not equal", lower != other);
   CHECK(7, "a host name equals a C string of the same host", lower == "Host1" && "HOST1" == lower);
   CHECK(8, "an unset host name equals nullptr and no set one", unset == nullptr && unset != lower && unset == Host{});
   const char *name = lower;
   CHECK(9, "a host name is handed on as the same C string", name == lower.c_str());
}

int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"TRAITS-01", "every type",
       "the object types of the model hold disjoint ranges of field numbers in ascending order",
       {"walk the list of tables and compare each type's first field number with the end of the previous type"},
       "every range starts after the previous one; the model views the tables",
       {{"TRT-01", "covers"}}, {1}, test_every_type},
      {"TRAITS-02", "entries",
       "the descriptor derived from a field table holds every field as declared",
       {"compare field number, type and flags of every entry with the descriptor entry at its position"},
       "no entry differs, numbers are consecutive, and no descriptor has more fields than its table",
       {{"TRT-01", "covers"}}, {2}, test_entries},
      {"TRAITS-03", "sub-types",
       "the sub-type of every list and object field comes from the model",
       {"compare the sub-type of every list and object field with object_get_subtype()"},
       "every sub-type is the same; fields of any sub-type name none",
       {{"TRT-01", "covers"}}, {3, 4}, test_sub_types},
      {"TRAITS-04", "host",
       "the value type of a host field compares like a host name",
       {"compare host names differing in case, different ones and unset ones, also against C strings"},
       "equal as sge_hostcmp() says, unset only equal to unset; the C string is handed on unchanged",
       {{"TRT-01", "covers"}}, {5, 6, 7, 8, 9}, test_host},
   });
}
