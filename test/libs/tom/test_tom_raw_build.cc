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
 * @brief Module test of the run-time builders of conditions and selections (RAW-BUILD)
 *
 * where_compare(), what_of() and what_all() build from fields and values known
 * only at run time what lWhere(), lIntVector2What() and lWhatAll() build from a
 * format. Each is compared with its old builder by applying both to the same
 * list with select_as() (same elements, same fields, written byte for byte),
 * by the written form of the condition or selection itself, and by the error
 * number and the empty result where the old builder returned nullptr.
 * reduced_descr() of a selection of what_of() must give the layout lReduceDescr()
 * gives for the selection of lIntVector2What().
 */

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cull/cull.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_cull_descr.h"

#include "ocs_module_test.h"

namespace {

   /**
    * @brief A list of TEST elements with a value in every plain field the builders compare
    *
    * For element i: TEST_uint32 i, TEST_uint64 i % 3, TEST_int i - 5, TEST_long i * 1000 - 3, TEST_double
    * i * 0.5, TEST_bool i % 2, TEST_string "s<i % 4>", TEST_host "h<i % 2>".
    *
    * @param n the number of elements
    * @return the owner of the list, hashed
    */
   ocs::tom::RawListPtr make_test_list(const int n) {
      auto list = ocs::tom::create_raw_list(TEST_Type, ocs::tom::Hashing::On, "test");
      for (int i = 0; i < n; i++) {
         auto ep = ocs::tom::create_raw_elem(TEST_Type);
         ocs::tom::set(ep.get().raw(), TEST_uint32, static_cast<std::uint32_t>(i));
         ocs::tom::set(ep.get().raw(), TEST_uint64, static_cast<std::uint64_t>(i % 3));
         ocs::tom::set(ep.get().raw(), TEST_int, static_cast<std::int32_t>(i - 5));
         ocs::tom::set(ep.get().raw(), TEST_long, static_cast<std::int64_t>(i) * 1000 - 3);
         ocs::tom::set(ep.get().raw(), TEST_double, i * 0.5);
         ocs::tom::set(ep.get().raw(), TEST_bool, i % 2 == 1);
         const std::string s = "s" + std::to_string(i % 4);
         ocs::tom::set(ep.get().raw(), TEST_string, s.c_str());
         const std::string h = "h" + std::to_string(i % 2);
         ocs::tom::set(ep.get().raw(), TEST_host, ocs::tom::Host(h.c_str()));
         ocs::tom::append(list.get().raw(), std::move(ep));
      }
      return list;
   }

   /**
    * @brief The bytes a write function puts into a stream
    * @tparam Write the type of the write function
    * @param write writes into the stream it is given
    * @return the bytes written, std::nullopt if no temporary file could be opened
    */
   template <typename Write>
   std::optional<std::string> written(Write &&write) {
      std::string out;
      FILE *fp = std::tmpfile();
      if (fp == nullptr) {
         std::printf("no temporary file\n");
         return std::nullopt;
      }
      write(fp);
      std::fflush(fp);
      std::rewind(fp);
      char chunk[4096];
      std::size_t n;
      while ((n = std::fread(chunk, 1, sizeof(chunk), fp)) > 0) {
         out.append(chunk, n);
      }
      std::fclose(fp);
      return out;
   }

   /**
    * @brief The written form of a list, a condition or a selection, as write_to() writes it
    * @tparam V the type of the object
    * @param value the object
    * @return the bytes, std::nullopt if no temporary file could be opened
    */
   template <typename V>
   std::optional<std::string> text_of(const V value) {
      return written([&](FILE *fp) { ocs::tom::write_to(fp, value); });
   }

   /**
    * @brief The error number of the list library a call leaves, starting from 0
    * @tparam Call the type of the call
    * @param call the call
    * @return the error number after the call
    */
   template <typename Call>
   int lerrno_after(Call &&call) {
      cull_state_set_lerrno(0);
      call();
      return cull_state_get_lerrno();
   }

   /**
    * @brief Whether an old and a new condition are written alike and select the same elements of a list
    * @param label the case, for the message
    * @param lp the list
    * @param old the condition of lWhere()
    * @param built the condition of the run-time builder
    * @return true if both exist, are written alike and give the same selection (both no list for no match)
    */
   bool same_where(const char *label, const lList *lp, const lCondition *old, const lCondition *built) {
      if (old == nullptr || built == nullptr) {
         std::printf("%s: a condition is missing\n", label);
         return false;
      }
      if (text_of(old) != text_of(built)) {
         std::printf("%s: the conditions are written differently\n", label);
         return false;
      }
      const auto all = ocs::tom::what_all();
      const auto old_list = ocs::tom::select_as("selected", lp, old, all.get(), ocs::tom::Hashing::On);
      const auto new_list = ocs::tom::select_as("selected", lp, built, all.get(), ocs::tom::Hashing::On);
      if (!old_list != !new_list ||
          (old_list && text_of(ocs::tom::ConstRawList(old_list.get().raw())) !=
                                      text_of(ocs::tom::ConstRawList(new_list.get().raw())))) {
         std::printf("%s: the conditions select different elements\n", label);
         return false;
      }
      return true;
   }

   /**
    * @brief Whether an old and a new selection are written alike and give the same fields of a list
    * @param label the case, for the message
    * @param lp the list
    * @param old the selection of the old builder
    * @param built the selection of the run-time builder
    * @return true if both exist, are written alike and select lists of the same layout and content
    */
   bool same_what(const char *label, const lList *lp, const lEnumeration *old, const lEnumeration *built) {
      if (old == nullptr || built == nullptr) {
         std::printf("%s: a selection is missing\n", label);
         return false;
      }
      if (text_of(old) != text_of(built)) {
         std::printf("%s: the selections are written differently\n", label);
         return false;
      }
      const auto old_list = ocs::tom::select_as("selected", lp, nullptr, old, ocs::tom::Hashing::On);
      const auto new_list = ocs::tom::select_as("selected", lp, nullptr, built, ocs::tom::Hashing::On);
      if (!old_list || !new_list ||
          !ocs::tom::same_layout(old_list.get().descr(), new_list.get().descr()) ||
          text_of(ocs::tom::ConstRawList(old_list.get().raw())) !=
              text_of(ocs::tom::ConstRawList(new_list.get().raw()))) {
         std::printf("%s: the selections give different lists\n", label);
         return false;
      }
      return true;
   }

} // namespace

/**
 * @brief where_compare() builds for every value type and operator the condition lWhere() builds
 * @return nothing; CHECK records the results
 */
static void
test_where_compare() {
   const auto owner = make_test_list(10);
   const lList *lp = owner.get().raw();
   using Op = ocs::tom::CompareOp;
   bool same = true;

   // every operator of the bridge on a string and a host field, with %s
   const std::pair<const char *, Op> ops[] = {{"==", Op::Equal},  {"!=", Op::NotEqual},  {"<", Op::Lower},
                                              {"<=", Op::LowerEqual}, {">", Op::Greater}, {">=", Op::GreaterEqual}};
   for (const auto &[text, op] : ops) {
      const std::string fmt = std::string("%T(%I ") + text + " %s)";
      const ocs::tom::RawWherePtr old_string(lWhere(fmt.c_str(), TEST_Type, TEST_string, "s2"));
      same = same_where(fmt.c_str(), lp, old_string.get(),
                        ocs::tom::where_compare(TEST_Type, TEST_string, op, "s2").get()) && same;
      const ocs::tom::RawWherePtr old_host(lWhere(fmt.c_str(), TEST_Type, TEST_host, "h1"));
      same = same_where(fmt.c_str(), lp, old_host.get(),
                        ocs::tom::where_compare(TEST_Type, TEST_host, op, "h1").get()) && same;
   }
   CHECK(1, "where_compare() with a string on a string and a host field builds, for ==, !=, <, <=, >, >=, the "
            "condition lWhere(\"%T(%I <op> %s)\") builds",
         same);

   bool numbers = true;
   {
      const ocs::tom::RawWherePtr old(lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, 7U));
      numbers = same_where("uint32 ==", lp, old.get(),
                           ocs::tom::where_compare(TEST_Type, TEST_uint32, Op::Equal, std::uint32_t{7}).get()) &&
                numbers;
   }
   {
      const ocs::tom::RawWherePtr old(lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 4U));
      numbers = same_where("uint32 >", lp, old.get(),
                           ocs::tom::where_compare(TEST_Type, TEST_uint32, Op::Greater, std::uint32_t{4}).get()) &&
                numbers;
   }
   {
      const ocs::tom::RawWherePtr old(lWhere("%T(%I == %lu)", TEST_Type, TEST_uint64, static_cast<lUlong64>(2)));
      numbers = same_where("uint64 ==", lp, old.get(),
                           ocs::tom::where_compare(TEST_Type, TEST_uint64, Op::Equal, std::uint64_t{2}).get()) &&
                numbers;
   }
   {
      const ocs::tom::RawWherePtr old(lWhere("%T(%I < %d)", TEST_Type, TEST_int, -2));
      numbers = same_where("int32 <", lp, old.get(),
                           ocs::tom::where_compare(TEST_Type, TEST_int, Op::Lower, std::int32_t{-2}).get()) &&
                numbers;
   }
   {
      const ocs::tom::RawWherePtr old(lWhere("%T(%I >= %o)", TEST_Type, TEST_long, static_cast<lLong>(3997)));
      numbers = same_where("int64 >=", lp, old.get(),
                           ocs::tom::where_compare(TEST_Type, TEST_long, Op::GreaterEqual, std::int64_t{3997}).get()) &&
                numbers;
   }
   {
      const ocs::tom::RawWherePtr old(lWhere("%T(%I <= %g)", TEST_Type, TEST_double, 2.0));
      numbers = same_where("double <=", lp, old.get(),
                           ocs::tom::where_compare(TEST_Type, TEST_double, Op::LowerEqual, 2.0).get()) &&
                numbers;
   }
   {
      const ocs::tom::RawWherePtr old(lWhere("%T(%I == %b)", TEST_Type, TEST_bool, true));
      numbers = same_where("bool ==", lp, old.get(),
                           ocs::tom::where_compare(TEST_Type, TEST_bool, Op::Equal, true).get()) &&
                numbers;
   }
   {
      // no element matches: both give no list
      const ocs::tom::RawWherePtr old(lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, 100U));
      numbers = same_where("no match", lp, old.get(),
                           ocs::tom::where_compare(TEST_Type, TEST_uint32, Op::Equal, std::uint32_t{100}).get()) &&
                numbers;
   }
   CHECK(2, "where_compare() with a 32 and 64 bit unsigned, a 32 and 64 bit signed number, a double and a bool "
            "builds the condition lWhere() builds with %u, %lu, %d, %o, %g and %b; no match is no list in both",
         numbers);
}

/**
 * @brief where_compare() fails where lWhere() returned nullptr, with the same error number, and throws WrongType
 *        where lWhere() aborted
 * @return nothing; CHECK records the results
 */
static void
test_where_compare_errors() {
   using Op = ocs::tom::CompareOp;
   const auto reduced_what = ocs::tom::what_of(TEST_Type, std::vector<int>{TEST_uint32});
   const lDescr *reduced = ocs::tom::reduced_descr(TEST_Type, reduced_what.get());

   struct Case {
      const char *label;
      const lDescr *descr;
      int nm;
   };
   const Case cases[] = {{"null descriptor", nullptr, TEST_string},
                         {"foreign field", TEST_Type, JB_owner},
                         {"absent field", reduced, TEST_string}};
   bool refused = reduced != nullptr;
   for (const Case &c : cases) {
      lCondition *old = nullptr;
      const int old_errno = lerrno_after([&] { old = lWhere("%T(%I == %s)", c.descr, c.nm, "s1"); });
      ocs::tom::RawWherePtr built;
      const int new_errno =
          lerrno_after([&] { built = ocs::tom::where_compare(c.descr, c.nm, Op::Equal, "s1"); });
      if (old != nullptr || built != nullptr || old_errno != new_errno || new_errno != LEPARSECOND) {
         std::printf("%s: old %s (%d), new %s (%d)\n", c.label, old ? "condition" : "none", old_errno,
                     built ? "condition" : "none", new_errno);
         refused = false;
      }
      lFreeWhere(&old);
   }
   ocs_cull_descr_release(&reduced);
   CHECK(3, "where_compare() with a null descriptor, a foreign field and a field the reduced descriptor leaves out "
            "gives no condition and LEPARSECOND, as lWhere() does",
         refused);

   bool wrong_type = false;
   try {
      static_cast<void>(ocs::tom::where_compare(TEST_Type, TEST_string, Op::Equal, std::uint32_t{1}));
   } catch (const ocs::tom::FieldException &e) {
      wrong_type = e.reason() == ocs::tom::FieldException::Reason::WrongType;
   }
   bool host_string = true;
   try {
      host_string = ocs::tom::where_compare(TEST_Type, TEST_host, Op::Equal, "h0") != nullptr;
   } catch (const ocs::tom::FieldException &) {
      host_string = false;
   }
   CHECK(4, "where_compare() with a number on a string field throws WrongType (lWhere() aborted); a string on a "
            "host field is taken",
         wrong_type && host_string);
}

/**
 * @brief what_of() and what_all() build the selections lIntVector2What(), lWhatAll() and lWhat("%T(ALL)") build,
 *        and reduced_descr() of them gives the layout of lReduceDescr()
 * @return nothing; CHECK records the results
 */
static void
test_what() {
   const auto owner = make_test_list(6);
   const lList *lp = owner.get().raw();

   const std::vector<std::vector<int>> field_sets = {
       {TEST_uint32},
       {TEST_string, TEST_uint32},
       {TEST_host, TEST_uint32, TEST_host, TEST_double},
       {TEST_bool, TEST_uint32, TEST_uint64, TEST_int, TEST_long, TEST_double, TEST_host, TEST_string}};
   bool same = true;
   bool reduced_same = true;
   for (const auto &fields : field_sets) {
      std::vector<int> intv = fields;
      intv.push_back(NoName);
      const ocs::tom::RawWhatPtr old(lIntVector2What(TEST_Type, intv.data()));
      const auto built = ocs::tom::what_of(TEST_Type, fields);
      const std::string label = "what_of with " + std::to_string(fields.size()) + " fields";
      same = same_what(label.c_str(), lp, old.get(), built.get()) && same;

      const lDescr *old_reduced = nullptr;
      lReduceDescr(&old_reduced, TEST_Type, old.get());
      const lDescr *new_reduced = ocs::tom::reduced_descr(TEST_Type, built.get());
      if (old_reduced == nullptr || new_reduced == nullptr || !ocs::tom::same_layout(old_reduced, new_reduced)) {
         std::printf("%s: the reduced descriptors differ\n", label.c_str());
         reduced_same = false;
      }
      ocs_cull_descr_release(&old_reduced);
      ocs_cull_descr_release(&new_reduced);
   }
   CHECK(5, "what_of() selects the fields lIntVector2What() selects, in descriptor order and each once, and gives "
            "the same lists through select_as()",
         same);
   CHECK(6, "reduced_descr() of a selection of what_of() has the layout lReduceDescr() gives for lIntVector2What()",
         reduced_same);

   const ocs::tom::RawWhatPtr old_all(lWhatAll());
   const ocs::tom::RawWhatPtr old_format(lWhat("%T(ALL)", TEST_Type));
   const auto all = ocs::tom::what_all();
   CHECK(7, "what_all() selects every field, as lWhatAll() and lWhat(\"%T(ALL)\") do",
         same_what("lWhatAll", lp, old_all.get(), all.get()) &&
             same_what("lWhat ALL", lp, old_format.get(), all.get()));
}

/**
 * @brief what_of() fails where lIntVector2What() returned nullptr, with the same error number
 * @return nothing; CHECK records the results
 */
static void
test_what_errors() {
   const auto reduced_what = ocs::tom::what_of(TEST_Type, std::vector<int>{TEST_uint32});
   const lDescr *reduced = ocs::tom::reduced_descr(TEST_Type, reduced_what.get());

   struct Case {
      const char *label;
      const lDescr *descr;
      std::vector<int> fields;
   };
   const Case cases[] = {{"no field", TEST_Type, {}},
                         {"foreign field", TEST_Type, {TEST_uint32, JB_job_number}},
                         {"null descriptor", nullptr, {TEST_uint32}},
                         {"absent field", reduced, {TEST_string}}};
   bool refused = reduced != nullptr;
   for (const Case &c : cases) {
      std::vector<int> intv = c.fields;
      intv.push_back(NoName);
      lEnumeration *old = nullptr;
      const int old_errno = lerrno_after([&] { old = lIntVector2What(c.descr, intv.data()); });
      ocs::tom::RawWhatPtr built;
      const int new_errno = lerrno_after([&] { built = ocs::tom::what_of(c.descr, c.fields); });
      if (old != nullptr || built != nullptr || old_errno != new_errno || new_errno != LENAMENOT) {
         std::printf("%s: old %s (%d), new %s (%d)\n", c.label, old ? "selection" : "none", old_errno,
                     built ? "selection" : "none", new_errno);
         refused = false;
      }
      lFreeWhat(&old);
   }
   ocs_cull_descr_release(&reduced);
   CHECK(8, "what_of() with no field, a foreign field, a null descriptor and a field the reduced descriptor leaves "
            "out gives no selection and LENAMENOT, as lIntVector2What() does",
         refused);
}

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
int main(int argc, char *argv[]) {
   lInit(nmv);
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"RAW-BUILD-01", "where_compare",
       "where_compare() builds the single comparison lWhere() builds for every value type and operator",
       {"build each comparison with lWhere() and with where_compare() on a TEST list",
        "write both and select with both through select_as()"},
       "the same written condition and the same selected elements, no list in both for no match", {}, {1, 2},
       test_where_compare},
      {"RAW-BUILD-02", "where_compare errors",
       "where_compare() fails where lWhere() fails, without an exception except for a value of the wrong type",
       {"build with a null descriptor, a foreign field and an absent field through both",
        "build with a number on a string field and a string on a host field"},
       "no condition and LEPARSECOND in both; WrongType where lWhere() aborted", {}, {3, 4},
       test_where_compare_errors},
      {"RAW-BUILD-03", "what_of and what_all",
       "what_of(), what_all() and reduced_descr() give what lIntVector2What(), lWhatAll(), lWhat(\"%T(ALL)\") and "
       "lReduceDescr() give",
       {"build field selections through both, write them and select with them through select_as()",
        "reduce the descriptor through both", "build the selection of every field through all three"},
       "the same written selections, the same lists, the same layouts", {}, {5, 6, 7}, test_what},
      {"RAW-BUILD-04", "what_of errors", "what_of() fails where lIntVector2What() fails",
       {"build with no field, a foreign field, a null descriptor and an absent field through both"},
       "no selection and LENAMENOT in both", {}, {8}, test_what_errors},
   });
}
