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

#include "cull/cull_test_api.h"
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
 * @brief The typed builders of selections and conditions: what<F...>(), what<T>(fields), field<F>() with its
 *        operators and named comparisons, &&, ||, !, any(), |=, &=, merge() and nest() (CS-3077, OPS-01, WHT-01)
 *
 * Each scenario applies what it built to a typed list and checks the fixed keys or contents it selects.
 */
namespace typed_select {

   using ocs::tom::ConstElement;
   using ocs::tom::ConstList;
   using ocs::tom::Element;
   using ocs::tom::ElementPtr;
   using ocs::tom::FieldException;
   using ocs::tom::Host;
   using ocs::tom::ListPtr;
   using ocs::tom::TEST;
   using ocs::tom::UE;
   using ocs::tom::US;
   using ocs::tom::What;
   using ocs::tom::Where;
   using ocs::tom::field;
   using ocs::tom::what;
   using ocs::tom::what_all;

   /**
    * @brief Whether a function throws a FieldException with a given reason
    * @param call the call
    * @param reason the expected reason
    * @return true if it threw with that reason
    */
   template <typename Call>
   bool throws(Call call, const FieldException::Reason reason) {
      try {
         call();
      } catch (const FieldException &e) {
         return e.reason() == reason;
      }
      return false;
   }

   /** @brief One row of the TEST elements the comparisons run on */
   struct Row {
      std::uint32_t key;     ///< TEST_uint32, the unique key
      std::uint64_t u64;     ///< TEST_uint64
      std::int32_t i;        ///< TEST_int
      std::int64_t l;        ///< TEST_long
      double d;              ///< TEST_double
      bool b;                ///< TEST_bool
      const char *s;         ///< TEST_string, may be nullptr
      const char *h;         ///< TEST_host, may be nullptr
   };

   /** @brief The rows: the fourth holds no string and no host */
   constexpr Row Rows[] = {{1, 10, -1, -10, 0.5, true, "alpha", "node1"},
                           {2, 20, 0, 0, 1.5, false, "beta", "node2"},
                           {3, 30, 1, 10, 2.5, true, "Gamma", "NODE3"},
                           {4, 40, 2, 20, 3.5, false, nullptr, nullptr}};

   /**
    * @brief The rows as a typed list
    * @return the owning handle
    */
   ListPtr<TEST> typed_rows() {
      ListPtr<TEST> rows = ListPtr<TEST>::create("rows");
      for (const Row &r : Rows) {
         const Element<TEST> row = rows->push_back(ElementPtr<TEST>::create());
         row.set<TEST_uint32>(r.key);
         row.set<TEST_uint64>(r.u64);
         row.set<TEST_int>(r.i);
         row.set<TEST_long>(r.l);
         row.set<TEST_double>(r.d);
         row.set<TEST_bool>(r.b);
         row.set<TEST_string>(r.s);
         row.set<TEST_host>(Host{r.h});
      }
      return rows;
   }

   /**
    * @brief The keys a condition selects from a list
    * @param rows the list
    * @param where the condition
    * @return the keys joined by commas, empty if nothing was selected
    */
   std::string keys_of(const ConstList<TEST> rows, const Where<TEST> &where) {
      std::string keys;
      for (ConstElement<TEST> row = rows.find_if(where); row; row = row.next_if(where)) {
         keys += (keys.empty() ? "" : ",") + std::to_string(row.get<TEST_uint32>());
      }
      return keys;
   }

   /**
    * @brief The user sets the selections run on: three sets, the last without entries
    * @return the owning handle
    */
   ListPtr<US> typed_sets() {
      ListPtr<US> sets = ListPtr<US>::create("sets");
      const char *const entries[][3] = {
         {"acl", "alice", "bob"}, {"dept", "carol", nullptr}, {"empty", nullptr, nullptr}};
      std::uint32_t type = 1;
      for (const auto &set : entries) {
         const Element<US> us = sets->emplace_back<US_name>(set[0]);
         us.set<US_type>(type++);
         us.set<US_fshare>(10 * type);
         for (int i = 1; i < 3 && set[i] != nullptr; i++) {
            us.get_or_create<US_entries>().emplace_back<UE_name>(set[i]);
         }
      }
      return sets;
   }

   /** @brief What a reduced user set holds, as plain text: "name:entry,entry," and " type" if US_type is present */
   using Contents = std::vector<std::string>;

   /**
    * @brief The contents of a reduced list of user sets
    * @param sets the list
    * @return one line per set
    */
   Contents contents_of(const ConstList<US> sets) {
      Contents lines;
      for (const ConstElement<US> us : sets) {
         std::string line = std::string(us.get<US_name>()) + ":";
         if (us.has<US_entries>()) {
            for (const ConstElement<UE> entry : us.get<US_entries>()) {
               line += std::string(entry.get<UE_name>()) + ",";
            }
         }
         if (us.has<US_type>()) {
            line += " type";
         }
         lines.push_back(line);
      }
      return lines;
   }

   /**
    * @brief Print a result and the expected one, entry by entry, when they differ
    * @param label what the results are
    * @param seen the result
    * @param expected the expected result
    */
   void report_difference(const char *label, const std::vector<std::string> &seen,
                          const std::vector<std::string> &expected) {
      if (seen == expected) {
         return;
      }
      std::printf("%s differ:\n", label);
      for (std::size_t i = 0; i < expected.size(); i++) {
         std::printf("   %2zu: expected %-10s seen %s\n", i, expected[i].c_str(),
                     i < seen.size() ? seen[i].c_str() : "(missing)");
      }
   }

   /**
    * @brief SELECT-01: select fields, with a nested selection on a list field
    * @return nothing; CHECK records the results
    */
   void test_what() {
      const ListPtr<US> sets = typed_sets();
      const Contents nested = contents_of(*sets->select(what<US_name>().with<US_entries>(what<UE_name>())));
      const int fields[] = {US_entries, US_name};
      const Contents flat = contents_of(*sets->select(what<US>(fields)));
      const Contents expected{"acl:alice,bob,", "dept:carol,", "empty:"};
      CHECK(9, "what<F...>() with a nested selection and what<T>(fields) select the names and the entries' names",
            nested == expected && flat == expected);
   }

   /**
    * @brief SELECT-02: compare a field of every value type with a value
    * @return nothing; CHECK records the results
    */
   void test_compare() {
      std::vector<std::string> seen;
      const ListPtr<TEST> rows = typed_rows();
      seen.push_back(keys_of(*rows, field<TEST_uint32>() == 2));
      seen.push_back(keys_of(*rows, field<TEST_uint32>() != 2));
      seen.push_back(keys_of(*rows, field<TEST_uint32>() < 2));
      seen.push_back(keys_of(*rows, field<TEST_uint32>() <= 2));
      seen.push_back(keys_of(*rows, field<TEST_uint32>() > 2));
      seen.push_back(keys_of(*rows, field<TEST_uint32>() >= 2));
      seen.push_back(keys_of(*rows, field<TEST_uint64>() > 15));
      seen.push_back(keys_of(*rows, field<TEST_int>() < 0));
      seen.push_back(keys_of(*rows, field<TEST_long>() >= 10));
      seen.push_back(keys_of(*rows, field<TEST_double>() > 1.0));
      seen.push_back(keys_of(*rows, field<TEST_bool>() == true));
      seen.push_back(keys_of(*rows, field<TEST_string>() == "beta"));
      seen.push_back(keys_of(*rows, field<TEST_string>() != "beta"));
      seen.push_back(keys_of(*rows, field<TEST_string>() < "beta"));
      seen.push_back(keys_of(*rows, field<TEST_host>() == "node2"));
      // a null string is neither equal nor unequal; strcmp() orders upper case first, so "Gamma" < "beta"
      const std::vector<std::string> expected{"2", "1,3,4", "1", "1,2", "3,4", "2,3,4", "2,3,4", "1",
                                              "3,4", "2,3,4", "1,3", "2", "1,3", "1,3", "2"};
      report_difference("comparisons", seen, expected);
      CHECK(10, "field<F>() compared with ==, !=, <, <=, > and >= selects the expected keys for every value type",
            seen == expected);
   }

   /**
    * @brief SELECT-03: the named comparisons: case-insensitive, pattern, host name, bit mask
    * @return nothing; CHECK records the results
    */
   void test_named() {
      std::vector<std::string> seen;
      const ListPtr<TEST> rows = typed_rows();
      seen.push_back(keys_of(*rows, field<TEST_string>().equals_nocase("GAMMA")));
      seen.push_back(keys_of(*rows, field<TEST_string>().like("*a")));
      seen.push_back(keys_of(*rows, field<TEST_string>().like("*")));
      seen.push_back(keys_of(*rows, field<TEST_host>().same_host("node3")));
      seen.push_back(keys_of(*rows, field<TEST_uint32>().has_all_bits(3)));
      // a pattern matches a null string as the empty one
      const std::vector<std::string> expected{"3", "1,2,3", "1,2,3,4", "3", "3"};
      report_difference("named comparisons", seen, expected);
      CHECK(11, "equals_nocase, like, same_host and has_all_bits select the expected keys", seen == expected);
   }

   /**
    * @brief SELECT-04: combine conditions, and a condition on a sub-list
    * @return nothing; CHECK records the results
    */
   void test_combine() {
      std::vector<std::string> seen;
      const ListPtr<TEST> rows = typed_rows();
      seen.push_back(keys_of(*rows, field<TEST_uint32>() < 3 && field<TEST_bool>() == true));
      seen.push_back(keys_of(*rows, field<TEST_uint32>() == 1 || field<TEST_uint32>() == 4));
      seen.push_back(keys_of(*rows, !(field<TEST_uint32>() == 1 || field<TEST_uint32>() == 4)));
      Where<TEST> low = field<TEST_uint32>() < 2;
      seen.push_back(keys_of(*rows, std::move(low) || field<TEST_uint32>() > 3));
      const ListPtr<US> sets = typed_sets();
      const ConstElement<US> found = sets->find_if(field<US_entries>().any(field<UE_name>() == "carol"));
      seen.emplace_back(found ? found.get<US_name>() : "-");
      const std::vector<std::string> expected{"1", "1,4", "2,3", "1,4", "dept"};
      report_difference("combinations", seen, expected);
      CHECK(12, "&&, || and ! combine conditions, any() looks into a sub-list", seen == expected);
   }

   /**
    * @brief SELECT-05: the errors of the builders
    * @return nothing; CHECK records the results
    */
   void test_errors() {
      const bool empty_operand = throws([] { static_cast<void>(Where<TEST>() || field<TEST_uint32>() == 1); },
                                        FieldException::Reason::NullElement);
      const bool with_on_all = throws([] { static_cast<void>(what_all<US>().with<US_entries>(what<UE_name>())); },
                                      FieldException::Reason::ForeignField);
      // an empty selection that a member is called on: EmptyHandle, as on an empty element or list handle
      const bool on_empty = throws([] { static_cast<void>(What<US>().with<US_entries>(what<UE_name>())); },
                                   FieldException::Reason::EmptyHandle) &&
                            throws([] { static_cast<void>(What<US>().nest<US_entries>(what<UE_name>())); },
                                   FieldException::Reason::EmptyHandle);
      CHECK(13, "an empty operand of || throws NullElement; with() on the selection of every field throws "
                "ForeignField",
            empty_operand && with_on_all);
      CHECK(14, "with() and nest() on an empty selection throw EmptyHandle", on_empty);
   }

   /**
    * @brief SELECT-06: collect conditions in a loop with |= and &=, and narrow a merged selection with nest()
    * @return nothing; CHECK records the results
    */
   void test_collect_and_nest() {
      std::vector<std::string> seen;
      const ListPtr<TEST> rows = typed_rows();
      Where<TEST> where;
      for (const std::uint32_t key : {1U, 3U, 4U}) {
         where |= field<TEST_uint32>() == key;
      }
      where &= field<TEST_uint32>() < 4U;
      seen.push_back(keys_of(*rows, where));
      Where<TEST> none;
      none &= field<TEST_uint32>().has_all_bits(1U);
      seen.push_back(keys_of(*rows, none));
      Where<TEST> empty;
      empty |= Where<TEST>();
      empty &= Where<TEST>();
      const bool empty_collect = !empty;
      // a non-empty side keeps its condition when the other side is empty
      Where<TEST> kept_or = field<TEST_uint32>() == 1U;
      kept_or |= Where<TEST>();
      Where<TEST> kept_and = field<TEST_uint32>() == 1U;
      kept_and &= Where<TEST>();
      const bool kept_collect = keys_of(*rows, kept_or) == "1" && keys_of(*rows, kept_and) == "1";

      const ListPtr<US> sets = typed_sets();
      const What<US> what_sets = what<US_name>().merge(what<US_entries>()).nest<US_entries>(what<UE_name>());
      const Contents nested = contents_of(*sets->select(what_sets, "selected"));
      // nest() on a field that has a sub-selection already replaces it (the old one is freed)
      const What<US> renested = what<US_name>().merge(what<US_entries>()).nest<US_entries>(what_all<UE>())
                                   .nest<US_entries>(what<UE_name>());
      const bool renested_same = contents_of(*sets->select(renested, "selected")) == nested;
      const bool nest_refused = throws([] { static_cast<void>(what<US_name>().nest<US_entries>(what<UE_name>())); },
                                       FieldException::Reason::ForeignField) &&
                                throws([] { static_cast<void>(what_all<US>().nest<US_entries>(what<UE_name>())); },
                                       FieldException::Reason::ForeignField);
      // the keys 1, 3, 4 below 4 are 1 and 3; the odd keys are 1 and 3
      const std::vector<std::string> expected{"1,3", "1,3"};
      report_difference("collected conditions", seen, expected);
      CHECK(15, "|= and &= collect conditions in a loop, starting from none; nest() narrows a field of a merged "
                "selection",
            seen == expected && empty_collect && kept_collect &&
               nested == Contents({"acl:alice,bob,", "dept:carol,", "empty:"}) && renested_same && nest_refused);
   }

} // namespace typed_select

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
int main(int argc, char *argv[]) {
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
      {"RAW-BUILD-05", "typed what", "what<F...>() and what<T>(fields) select fields, also nested",
       {"select the names of user sets and the names of their entries, nested and flat"}, "the names and entries",
       {{"OPS-01", "covers"}, {"WHT-01", "covers"}}, {9}, typed_select::test_what},
      {"RAW-BUILD-06", "typed comparisons", "field<F>() compares a field of every value type",
       {"select from four elements with ==, !=, <, <=, > and >= on 32 and 64 bit, int, long, double, bool, "
        "string and host fields"}, "the expected keys", {{"OPS-01", "covers"}, {"WHT-01", "covers"}}, {10},
       typed_select::test_compare},
      {"RAW-BUILD-07", "named comparisons", "equals_nocase, like, same_host and has_all_bits",
       {"select with each named comparison"}, "the expected keys", {{"OPS-01", "covers"}, {"WHT-01", "covers"}},
       {11}, typed_select::test_named},
      {"RAW-BUILD-08", "combinations", "&&, || and ! combine and any() looks into a sub-list",
       {"select with &&, || and !, with a combination of a stored condition, and find by an entry of a sub-list"},
       "the expected keys and user set", {{"OPS-01", "covers"}, {"WHT-01", "covers"}}, {12},
       typed_select::test_combine},
      {"RAW-BUILD-09", "builder errors", "the builders throw on an empty operand, on with() on ALL and on an empty "
       "selection",
       {"combine an empty condition; extend the selection of every field; extend and narrow an empty selection"},
       "NullElement, ForeignField and EmptyHandle thrown", {{"OPS-01", "covers"}}, {13, 14},
       typed_select::test_errors},
      {"RAW-BUILD-10", "collect and nest", "|= and &= collect conditions, nest() narrows a field selected before",
       {"or three key conditions together in a loop and and a bound onto them, starting from no condition",
        "merge two selections of user sets and narrow their entries"},
       "the expected keys and contents; ForeignField for nest() on a field not selected and on ALL",
       {{"OPS-01", "covers"}, {"WHT-01", "covers"}}, {15}, typed_select::test_collect_and_nest},
   });
}
