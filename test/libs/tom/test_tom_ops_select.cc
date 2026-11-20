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
 * @brief Selections through the typed handles and through the old interface (CS-3077, OPS-01, WHT-01)
 *
 * This test is also an example of how fields are selected and elements chosen
 * by a condition with the typed object model. Every scenario does the same
 * thing twice: a block marked CULL uses only the old functions of the list
 * library, a block marked TOM only the typed handles. Each block builds its
 * own data and reduces what came out to plain values (names, keys, flags), and
 * the scenario compares those. Each step carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The old interface parses a format string at run time: lWhat("%T(%I%I)",
 * US_Type, US_name, US_type) and lWhere("%T(%I == %s)", US_Type, US_name,
 * "x"), with the fields and values passed through varargs unchecked. The typed
 * form names the fields as template arguments: what<US_name, US_type>() and
 * field<US_name>() == "x". A field of another type, a value of another type
 * than the field, or a nested selection or condition on a field that is not a
 * list does not compile (the compile_fail tests show it). Both build the same
 * lEnumeration and lCondition, which the typed handles own and free.
 *
 * In a Release build the last scenario times building a condition and
 * selecting with both forms and prints the ratios.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "cull/cull_lerrno.h"
#include "cull/cull_state.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

using ocs::tom::ConstElement;
using ocs::tom::ConstList;
using ocs::tom::Element;
using ocs::tom::ElementPtr;
using ocs::tom::FieldException;
using ocs::tom::Host;
using ocs::tom::JB;
using ocs::tom::List;
using ocs::tom::ListPtr;
using ocs::tom::TEST;
using ocs::tom::UE;
using ocs::tom::US;
using ocs::tom::What;
using ocs::tom::Where;
using ocs::tom::field;
using ocs::tom::what;
using ocs::tom::what_all;

namespace {

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
    * @brief The rows as a list of the old interface
    * @return the list, owned by the caller
    */
   lList *old_rows() {
      lList *lp = lCreateList("rows", TEST_Type);
      for (const Row &r : Rows) {
         lListElem *ep = lCreateElem(TEST_Type);
         lSetUlong(ep, TEST_uint32, r.key);
         lSetUlong64(ep, TEST_uint64, r.u64);
         lSetInt(ep, TEST_int, r.i);
         lSetLong(ep, TEST_long, r.l);
         lSetDouble(ep, TEST_double, r.d);
         lSetBool(ep, TEST_bool, r.b);
         lSetString(ep, TEST_string, r.s);
         lSetHost(ep, TEST_host, r.h);
         lAppendElem(lp, ep);
      }
      return lp;
   }

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
    * @brief The keys a condition selects from a list of the old interface; frees the condition
    * @param lp the list
    * @param cp the condition
    * @return the keys joined by commas, empty if nothing was selected
    */
   std::string old_keys(const lList *lp, lCondition *cp) {
      std::string keys;
      for (const lListElem *ep = lFindFirstRW(lp, cp); ep != nullptr; ep = lFindNextRW(ep, cp)) {
         keys += (keys.empty() ? "" : ",") + std::to_string(lGetUlong(ep, TEST_uint32));
      }
      lFreeWhere(&cp);
      return keys;
   }

   /**
    * @brief The keys a condition selects from a typed list
    * @param rows the list
    * @param where the condition
    * @return the keys joined by commas, empty if nothing was selected
    */
   std::string typed_keys(const ConstList<TEST> rows, const Where<TEST> &where) {
      std::string keys;
      for (ConstElement<TEST> row = rows.find_if(where); row; row = row.next_if(where)) {
         keys += (keys.empty() ? "" : ",") + std::to_string(row.get<TEST_uint32>());
      }
      return keys;
   }

   /**
    * @brief The user sets the selections run on: three sets, the last without entries
    * @return the list, owned by the caller
    */
   lList *old_sets() {
      lList *sets = lCreateList("sets", US_Type);
      const char *const entries[][3] = {{"acl", "alice", "bob"}, {"dept", "carol", nullptr}, {"empty", nullptr, nullptr}};
      std::uint32_t type = 1;
      for (const auto &set : entries) {
         lListElem *ep = lAddElemStr(&sets, US_name, set[0], US_Type);
         lSetUlong(ep, US_type, type++);
         lSetUlong(ep, US_fshare, 10 * type);
         for (int i = 1; i < 3 && set[i] != nullptr; i++) {
            lAddSubStr(ep, UE_name, set[i], US_entries, UE_Type);
         }
      }
      return sets;
   }

   /**
    * @brief The user sets of old_sets() as a typed list
    * @return the owning handle
    */
   ListPtr<US> typed_sets() {
      ListPtr<US> sets = ListPtr<US>::create("sets");
      const char *const entries[][3] = {{"acl", "alice", "bob"}, {"dept", "carol", nullptr}, {"empty", nullptr, nullptr}};
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

   /**
    * @brief What a reduced user set holds, as plain text: "name:entry,entry," and " type" if US_type is present
    */
   using Contents = std::vector<std::string>;

   /**
    * @brief The contents of a reduced list of user sets of the old interface
    * @param lp the list
    * @return one line per set
    */
   Contents old_contents(const lList *lp) {
      Contents lines;
      for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
         std::string line = std::string(lGetString(ep, US_name)) + ":";
         if (lGetPosViaElem(ep, US_entries, SGE_NO_ABORT) >= 0) {
            for (const lListElem *entry = lFirst(lGetList(ep, US_entries)); entry != nullptr; entry = lNext(entry)) {
               line += std::string(lGetString(entry, UE_name)) + ",";
            }
         }
         if (lGetPosViaElem(ep, US_type, SGE_NO_ABORT) >= 0) {
            line += " type";
         }
         lines.push_back(line);
      }
      return lines;
   }

   /**
    * @brief The contents of a reduced typed list of user sets
    * @param sets the list
    * @return one line per set
    */
   Contents typed_contents(const ConstList<US> sets) {
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
    * @brief Print two results that should be equal, entry by entry, when they are not
    * @param what what the results are
    * @param old_seen the result of the old interface
    * @param typed_seen the result of the typed handles
    * @param expected the expected result
    */
   template <typename V>
   void report_difference(const char *what, const std::vector<V> &old_seen, const std::vector<V> &typed_seen,
                          const std::vector<V> &expected) {
      if (old_seen == expected && typed_seen == expected) {
         return;
      }
      printf("%s differ:\n", what);
      for (std::size_t i = 0; i < expected.size(); i++) {
         const auto text = [i](const std::vector<V> &v) {
            if (i >= v.size()) {
               return std::string("(missing)");
            }
            if constexpr (std::is_same_v<V, std::string>) {
               return v[i];
            } else {
               return std::to_string(v[i]);
            }
         };
         printf("   %2zu: expected %-10s old %-10s typed %s\n", i, text(expected).c_str(), text(old_seen).c_str(),
                text(typed_seen).c_str());
      }
   }

} // namespace

// The scenarios have external linkage, so that a stack written by a failing check names them.

/**
 * @brief Select fields, with a nested selection on a list field (WHT-01 step 1)
 */
void test_what() {
   // old:     lWhat("%T(%I%I->%T(%I))", US_Type, US_name, US_entries, UE_Type, UE_name); lSelect(...)
   // typed:   what<US_name>().with<US_entries>(what<UE_name>()); sets.select(what)
   // old:     lIntVector2What(US_Type, fields) for fields known only at run time
   // typed:   what<US>(fields)
   // differs: the fields are template arguments; with<F>() takes only a list or object field and a selection
   //          of its sub-type. what<US>(fields) checks each field at run time. select() of nothing gives an
   //          empty list where lSelect() gives nullptr.

   // --- CULL
   Contents old_nested;
   Contents old_flat;
   {
      lList *sets = old_sets();
      lEnumeration *nested = lWhat("%T(%I%I->%T(%I))", US_Type, US_name, US_entries, UE_Type, UE_name);
      lList *selected = lSelect("selected", sets, nullptr, nested);
      old_nested = old_contents(selected);
      lFreeList(&selected);
      const int fields[] = {US_entries, US_name, NoName};
      lEnumeration *flat = lIntVector2What(US_Type, fields);
      selected = lSelect("selected", sets, nullptr, flat);
      old_flat = old_contents(selected);
      lFreeList(&selected);
      lFreeWhat(&flat);
      lFreeWhat(&nested);
      lFreeList(&sets);
   }

   // --- TOM
   Contents typed_nested;
   Contents typed_flat;
   {
      const ListPtr<US> sets = typed_sets();
      typed_nested = typed_contents(*sets->select(what<US_name>().with<US_entries>(what<UE_name>())));
      const int fields[] = {US_entries, US_name};
      typed_flat = typed_contents(*sets->select(what<US>(fields)));
   }

   const Contents expected{"acl:alice,bob,", "dept:carol,", "empty:"};
   CHECK(1, "what<F...>() with a nested selection and what<T>(fields) select what lWhat and lIntVector2What select",
         old_nested == expected && old_flat == expected && typed_nested == expected && typed_flat == expected);
}

/**
 * @brief A field number of another type, known only at run time (WHT-01 step 3)
 */
void test_what_foreign() {
   // old:     lIntVector2What(US_Type, {US_name, JB_job_number}) returns nullptr with LENAMENOT
   // typed:   what<US>({US_name, JB_job_number}) throws a logic error with reason ForeignField
   // differs: the typed form throws where the old one returned nullptr

   // --- CULL
   const int fields[] = {US_name, JB_job_number, NoName};
   cull_state_set_lerrno(0);
   const bool old_refused = lIntVector2What(US_Type, fields) == nullptr && cull_state_get_lerrno() == LENAMENOT;

   // --- TOM
   const int typed_fields[] = {US_name, JB_job_number};
   const bool typed_refused = throws([&] { static_cast<void>(what<US>(typed_fields)); },
                                     FieldException::Reason::ForeignField);

   CHECK(2, "a field of another type known only at run time throws ForeignField where lIntVector2What gives nullptr",
         old_refused && typed_refused);
}

/**
 * @brief Compare a field of every value type with a value (WHT-01 step 4)
 */
void test_compare() {
   // old:     lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, 2U) and the other operators and value tokens
   // typed:   field<TEST_uint32>() == 2
   // differs: the value takes the field's type, a string or host field a C string; there are no tokens to
   //          get wrong (%u, %lu, %d, %o, %g, %b, %s). A null string is neither equal nor unequal, as before.
   //          The order of strings holds in every build now; the old lCompare() took only -1 and 1 from strcmp()

   // --- CULL
   std::vector<std::string> old_seen;
   {
      lList *rows = old_rows();
      old_seen.push_back(old_keys(rows, lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, 2U)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I != %u)", TEST_Type, TEST_uint32, 2U)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I < %u)", TEST_Type, TEST_uint32, 2U)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I <= %u)", TEST_Type, TEST_uint32, 2U)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 2U)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I >= %u)", TEST_Type, TEST_uint32, 2U)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I > %lu)", TEST_Type, TEST_uint64, static_cast<lUlong64>(15))));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I < %d)", TEST_Type, TEST_int, 0)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I >= %o)", TEST_Type, TEST_long, static_cast<lLong>(10))));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I > %g)", TEST_Type, TEST_double, 1.0)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I == %b)", TEST_Type, TEST_bool, true)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I == %s)", TEST_Type, TEST_string, "beta")));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I != %s)", TEST_Type, TEST_string, "beta")));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I < %s)", TEST_Type, TEST_string, "beta")));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I == %s)", TEST_Type, TEST_host, "node2")));
      lFreeList(&rows);
   }

   // --- TOM
   std::vector<std::string> typed_seen;
   {
      const ListPtr<TEST> rows = typed_rows();
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint32>() == 2));
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint32>() != 2));
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint32>() < 2));
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint32>() <= 2));
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint32>() > 2));
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint32>() >= 2));
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint64>() > 15));
      typed_seen.push_back(typed_keys(*rows, field<TEST_int>() < 0));
      typed_seen.push_back(typed_keys(*rows, field<TEST_long>() >= 10));
      typed_seen.push_back(typed_keys(*rows, field<TEST_double>() > 1.0));
      typed_seen.push_back(typed_keys(*rows, field<TEST_bool>() == true));
      typed_seen.push_back(typed_keys(*rows, field<TEST_string>() == "beta"));
      typed_seen.push_back(typed_keys(*rows, field<TEST_string>() != "beta"));
      typed_seen.push_back(typed_keys(*rows, field<TEST_string>() < "beta"));
      typed_seen.push_back(typed_keys(*rows, field<TEST_host>() == "node2"));
   }

   const std::vector<std::string> expected{"2", "1,3,4", "1", "1,2", "3,4", "2,3,4", "2,3,4", "1",
                                           "3,4", "2,3,4", "1,3", "2", "1,3", "1,3", "2"};
   report_difference("comparisons", old_seen, typed_seen, expected);
   CHECK(3, "field<F>() compared with ==, !=, <, <=, > and >= selects what lWhere selects, for every value type",
         old_seen == expected && typed_seen == expected);
}

/**
 * @brief The named comparisons: case-insensitive, pattern, host name, bit mask (WHT-01 step 4)
 */
void test_named() {
   // old:     lWhere("%T(%I c= %s)", ...), "p=", "h=", "m="
   // typed:   field<F>().equals_nocase(s), .like(pattern), .same_host(h), .has_all_bits(mask)
   // differs: has_all_bits() takes only a 32 bit field, where "m=" on a 64 bit field compiles and selects nothing;
   //          the string comparisons take only a string or host field

   // --- CULL
   std::vector<std::string> old_seen;
   {
      lList *rows = old_rows();
      old_seen.push_back(old_keys(rows, lWhere("%T(%I c= %s)", TEST_Type, TEST_string, "GAMMA")));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I p= %s)", TEST_Type, TEST_string, "*a")));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I p= %s)", TEST_Type, TEST_string, "*")));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I h= %s)", TEST_Type, TEST_host, "node3")));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I m= %u)", TEST_Type, TEST_uint32, 3U)));
      lFreeList(&rows);
   }

   // --- TOM
   std::vector<std::string> typed_seen;
   {
      const ListPtr<TEST> rows = typed_rows();
      typed_seen.push_back(typed_keys(*rows, field<TEST_string>().equals_nocase("GAMMA")));
      typed_seen.push_back(typed_keys(*rows, field<TEST_string>().like("*a")));
      typed_seen.push_back(typed_keys(*rows, field<TEST_string>().like("*")));
      typed_seen.push_back(typed_keys(*rows, field<TEST_host>().same_host("node3")));
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint32>().has_all_bits(3)));
   }

   // a pattern matches a null string as the empty one
   const std::vector<std::string> expected{"3", "1,2,3", "1,2,3,4", "3", "3"};
   report_difference("named comparisons", old_seen, typed_seen, expected);
   CHECK(4, "equals_nocase, like, same_host and has_all_bits select what c=, p=, h= and m= select",
         old_seen == expected && typed_seen == expected);
}

/**
 * @brief Combine conditions, and a condition on a sub-list (WHT-01 step 4)
 */
void test_combine() {
   // old:     lWhere("%T(%I < %u && %I == %b)", ...), "||", "!", lAndWhere(), lOrWhere()
   // typed:   (field<A>() < 3) && (field<B>() == true), ||, !
   // old:     lWhere("%T(%I -> %T(%I == %s))", US_Type, US_entries, UE_Type, UE_name, "carol")
   // typed:   field<US_entries>().any(field<UE_name>() == "carol")
   // differs: &&, || and ! take their operands over, so an operand held in a variable is given with
   //          std::move(); any() takes only a list field and a condition on its sub-type

   // --- CULL
   std::vector<std::string> old_seen;
   {
      lList *rows = old_rows();
      old_seen.push_back(old_keys(rows, lWhere("%T(%I < %u && %I == %b)", TEST_Type, TEST_uint32, 3U, TEST_bool, true)));
      old_seen.push_back(old_keys(rows, lWhere("%T(%I == %u || %I == %u)", TEST_Type, TEST_uint32, 1U, TEST_uint32, 4U)));
      old_seen.push_back(old_keys(rows, lWhere("%T(!(%I == %u || %I == %u))", TEST_Type, TEST_uint32, 1U, TEST_uint32, 4U)));
      lCondition *low = lWhere("%T(%I < %u)", TEST_Type, TEST_uint32, 2U);
      old_seen.push_back(old_keys(rows, lOrWhere(low, lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 3U))));
      lFreeList(&rows);

      lList *sets = old_sets();
      lCondition *with_carol = lWhere("%T(%I -> %T(%I == %s))", US_Type, US_entries, UE_Type, UE_name, "carol");
      const lListElem *found = lFindFirstRW(sets, with_carol);
      old_seen.emplace_back(found != nullptr ? lGetString(found, US_name) : "-");
      lFreeWhere(&with_carol);
      lFreeList(&sets);
   }

   // --- TOM
   std::vector<std::string> typed_seen;
   {
      const ListPtr<TEST> rows = typed_rows();
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint32>() < 3 && field<TEST_bool>() == true));
      typed_seen.push_back(typed_keys(*rows, field<TEST_uint32>() == 1 || field<TEST_uint32>() == 4));
      typed_seen.push_back(typed_keys(*rows, !(field<TEST_uint32>() == 1 || field<TEST_uint32>() == 4)));
      Where<TEST> low = field<TEST_uint32>() < 2;
      typed_seen.push_back(typed_keys(*rows, std::move(low) || field<TEST_uint32>() > 3));

      const ListPtr<US> sets = typed_sets();
      const ConstElement<US> found = sets->find_if(field<US_entries>().any(field<UE_name>() == "carol"));
      typed_seen.emplace_back(found ? found.get<US_name>() : "-");
   }

   const std::vector<std::string> expected{"1", "1,4", "2,3", "1,4", "dept"};
   report_difference("combinations", old_seen, typed_seen, expected);
   CHECK(5, "&&, || and ! combine and any() looks into a sub-list as lWhere, lAndWhere and lOrWhere do",
         old_seen == expected && typed_seen == expected);
}

/**
 * @brief Find forward and backward, and test one element
 */
void test_find() {
   // old:     lFindFirstRW(lp, cp), lFindNextRW(ep, cp), lFindLastRW(lp, cp), lFindPrevRW(ep, cp), lCompare(ep, cp)
   // typed:   rows.find_if(where), row.next_if(where), rows.find_last_if(where), row.prev_if(where),
   //          row.matches(where)
   // differs: find_if is named after std::find_if; an empty list handle finds nothing where a null list
   //          set LELISTNULL

   // --- CULL
   std::vector<std::uint32_t> old_seen;
   {
      lList *rows = old_rows();
      lCondition *odd = lWhere("%T(%I m= %u)", TEST_Type, TEST_uint32, 1U);
      const lListElem *last = lFindLastRW(rows, odd);
      old_seen.push_back(lGetUlong(lFindFirstRW(rows, odd), TEST_uint32));
      old_seen.push_back(lGetUlong(lFindNextRW(lFindFirstRW(rows, odd), odd), TEST_uint32));
      old_seen.push_back(lGetUlong(last, TEST_uint32));
      old_seen.push_back(lGetUlong(lFindPrevRW(last, odd), TEST_uint32));
      old_seen.push_back(lCompare(lFirst(rows), odd));
      old_seen.push_back(lCompare(lNext(lFirst(rows)), odd));
      lFreeWhere(&odd);
      lFreeList(&rows);
   }

   // --- TOM
   std::vector<std::uint32_t> typed_seen;
   {
      const ListPtr<TEST> rows = typed_rows();
      const Where<TEST> odd = field<TEST_uint32>().has_all_bits(1);
      const Element<TEST> last = rows->find_last_if(odd);
      typed_seen.push_back(rows->find_if(odd).get<TEST_uint32>());
      typed_seen.push_back(rows->find_if(odd).next_if(odd).get<TEST_uint32>());
      typed_seen.push_back(last.get<TEST_uint32>());
      typed_seen.push_back(last.prev_if(odd).get<TEST_uint32>());
      typed_seen.push_back(rows->front().matches(odd));
      typed_seen.push_back(rows->front().next().matches(odd));
   }

   const std::vector<std::uint32_t> expected{1, 3, 3, 1, 1, 0};
   report_difference("finds", old_seen, typed_seen, expected);
   CHECK(6, "find_if, next_if, find_last_if, prev_if and matches find what the lFind functions and lCompare find",
         old_seen == expected && typed_seen == expected);
}

/**
 * @brief Select with a condition and a reduction; remove what a condition matches
 */
void test_select_and_remove() {
   // old:     lSelect("acls", sets, where, what) and lSelectDestroy(lp, keep)
   // typed:   sets.select(where, what, "acls") and rows.remove_if(!keep)
   // differs: no match gives an empty list, not nullptr; remove_if() removes what matches, as
   //          std::list::remove_if does, where lSelectDestroy() keeps it, and leaves an emptied list in place

   // --- CULL
   Contents old_selected;
   std::vector<std::string> old_rest;
   {
      lList *sets = old_sets();
      lCondition *where = lWhere("%T(%I > %u)", US_Type, US_fshare, 20U);
      lEnumeration *what_name = lWhat("%T(%I)", US_Type, US_name);
      lList *selected = lSelect("selected", sets, where, what_name);
      old_selected = old_contents(selected);
      lFreeList(&selected);
      lCondition *nothing = lWhere("%T(%I > %u)", US_Type, US_fshare, 100U);
      selected = lSelect("nothing", sets, nothing, what_name);
      old_selected.emplace_back(selected == nullptr ? "none" : "some");
      lFreeWhere(&nothing);
      lFreeWhat(&what_name);
      lFreeWhere(&where);
      lFreeList(&sets);

      lList *rows = old_rows();
      lCondition *keep = lWhere("%T(%I m= %u)", TEST_Type, TEST_uint32, 1U);
      rows = lSelectDestroy(rows, keep);
      for (const lListElem *ep = lFirst(rows); ep != nullptr; ep = lNext(ep)) {
         old_rest.push_back(std::to_string(lGetUlong(ep, TEST_uint32)));
      }
      lFreeWhere(&keep);
      lFreeList(&rows);
   }

   // --- TOM
   Contents typed_selected;
   std::vector<std::string> typed_rest;
   {
      const ListPtr<US> sets = typed_sets();
      typed_selected = typed_contents(*sets->select(field<US_fshare>() > 20, what<US_name>(), "selected"));
      const ListPtr<US> nothing = sets->select(field<US_fshare>() > 100, what<US_name>(), "nothing");
      typed_selected.emplace_back(nothing->empty() ? "none" : "some");

      const ListPtr<TEST> rows = typed_rows();
      const std::size_t removed = rows->remove_if(!field<TEST_uint32>().has_all_bits(1));
      for (const ConstElement<TEST> row : *rows) {
         typed_rest.push_back(std::to_string(row.get<TEST_uint32>()));
      }
      typed_rest.push_back(std::to_string(removed));
   }

   CHECK(7, "select reduces what a condition matches as lSelect does, remove_if(!keep) keeps what lSelectDestroy keeps",
         (old_selected == Contents{"dept:", "empty:", "none"}) && (typed_selected == Contents{"dept:", "empty:", "none"}) &&
         (old_rest == std::vector<std::string>{"1", "3"}) && (typed_rest == std::vector<std::string>{"1", "3", "2"}));
}

/**
 * @brief The errors of the typed form
 */
void test_errors() {
   // old:     lOrWhere(nullptr, cp) returns nullptr with LECONDNULL
   // typed:   an empty Where combined with && or || throws NullElement; with<F>() on what_all<T>() throws
   //          ForeignField, with<F>() and nest<F>() on an empty selection throw EmptyHandle
   // differs: the typed form throws where the old one returned nullptr

   // --- CULL
   cull_state_set_lerrno(0);
   lCondition *one = lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, 1U);
   const bool old_refused = lOrWhere(nullptr, one) == nullptr && cull_state_get_lerrno() == LECONDNULL;
   lFreeWhere(&one);

   // --- TOM
   const bool empty_operand = throws([] { static_cast<void>(Where<TEST>() || field<TEST_uint32>() == 1); },
                                     FieldException::Reason::NullElement);
   const bool with_on_all = throws([] { static_cast<void>(what_all<US>().with<US_entries>(what<UE_name>())); },
                                   FieldException::Reason::ForeignField);
   // an empty selection that a member is called on: EmptyHandle, as on an empty element or list handle
   const bool on_empty = throws([] { static_cast<void>(What<US>().with<US_entries>(what<UE_name>())); },
                                FieldException::Reason::EmptyHandle) &&
                         throws([] { static_cast<void>(What<US>().nest<US_entries>(what<UE_name>())); },
                                FieldException::Reason::EmptyHandle);

   CHECK(8, "an empty operand throws NullElement where lOrWhere gives nullptr; with() on ALL throws ForeignField",
         old_refused && empty_operand && with_on_all);
   CHECK(11, "with() and nest() on an empty selection throw EmptyHandle", on_empty);
}

/**
 * @brief Collect conditions in a loop with |= and &=, and narrow a merged selection with nest()
 */
void test_collect_and_nest() {
   // old:     where = where == nullptr ? nw : lOrWhere(where, nw); the same with lAndWhere()
   // typed:   where |= std::move(nw); where &= std::move(nw)
   // old:     lMergeWhat(&what, &more); lWhatSetSubWhat(what, US_entries, &sub)
   // typed:   std::move(what).merge(std::move(more)).nest<US_entries>(std::move(sub))
   // differs: an empty side of |= and &= is left out, where || and && throw; nest() on a selection that does
   //          not select the field throws ForeignField where lWhatSetSubWhat() returns -1

   // --- CULL
   std::vector<std::string> old_seen;
   Contents old_nested;
   {
      lList *rows = old_rows();
      lCondition *where = nullptr;
      for (const std::uint32_t key : {1U, 3U, 4U}) {
         lCondition *nw = lWhere("%T(%I == %u)", TEST_Type, TEST_uint32, key);
         where = where == nullptr ? nw : lOrWhere(where, nw);
      }
      lCondition *below = lWhere("%T(%I < %u)", TEST_Type, TEST_uint32, 4U);
      where = where == nullptr ? below : lAndWhere(where, below);
      old_seen.push_back(old_keys(rows, where));
      lCondition *none = nullptr;
      lCondition *odd = lWhere("%T(%I m= %u)", TEST_Type, TEST_uint32, 1U);
      none = none == nullptr ? odd : lAndWhere(none, odd);
      old_seen.push_back(old_keys(rows, none));
      lFreeList(&rows);

      lList *sets = old_sets();
      lEnumeration *what_sets = lWhat("%T(%I)", US_Type, US_name);
      lEnumeration *more = lWhat("%T(%I)", US_Type, US_entries);
      lMergeWhat(&what_sets, &more);
      lEnumeration *sub = lWhat("%T(%I)", UE_Type, UE_name);
      lWhatSetSubWhat(what_sets, US_entries, &sub);
      lList *selected = lSelect("selected", sets, nullptr, what_sets);
      old_nested = old_contents(selected);
      lFreeList(&selected);
      lFreeWhat(&what_sets);
      lFreeList(&sets);
   }

   // --- TOM
   std::vector<std::string> typed_seen;
   Contents typed_nested;
   bool nest_refused = false;
   bool empty_collect = false;
   bool kept_collect = false;
   bool renested_same = false;
   {
      const ListPtr<TEST> rows = typed_rows();
      Where<TEST> where;
      for (const std::uint32_t key : {1U, 3U, 4U}) {
         where |= field<TEST_uint32>() == key;
      }
      where &= field<TEST_uint32>() < 4U;
      typed_seen.push_back(typed_keys(*rows, where));
      Where<TEST> none;
      none &= field<TEST_uint32>().has_all_bits(1U);
      typed_seen.push_back(typed_keys(*rows, none));
      Where<TEST> empty;
      empty |= Where<TEST>();
      empty &= Where<TEST>();
      empty_collect = !empty;
      // a non-empty side keeps its condition when the other side is empty
      Where<TEST> kept_or = field<TEST_uint32>() == 1U;
      kept_or |= Where<TEST>();
      Where<TEST> kept_and = field<TEST_uint32>() == 1U;
      kept_and &= Where<TEST>();
      kept_collect = typed_keys(*rows, kept_or) == "1" && typed_keys(*rows, kept_and) == "1";

      const ListPtr<US> sets = typed_sets();
      const What<US> what_sets = what<US_name>().merge(what<US_entries>()).nest<US_entries>(what<UE_name>());
      typed_nested = typed_contents(*sets->select(what_sets, "selected"));
      // nest() on a field that has a sub-selection already replaces it (the old one is freed)
      const What<US> renested = what<US_name>().merge(what<US_entries>()).nest<US_entries>(what_all<UE>())
                                   .nest<US_entries>(what<UE_name>());
      renested_same = typed_contents(*sets->select(renested, "selected")) == typed_nested;
      nest_refused = throws([] { static_cast<void>(what<US_name>().nest<US_entries>(what<UE_name>())); },
                            FieldException::Reason::ForeignField) &&
                     throws([] { static_cast<void>(what_all<US>().nest<US_entries>(what<UE_name>())); },
                            FieldException::Reason::ForeignField);
   }

   const std::vector<std::string> expected{"1,3", "1,3"};
   report_difference("collected conditions", old_seen, typed_seen, expected);
   CHECK(10, "|= and &= collect as lOrWhere and lAndWhere in a loop; nest() narrows as lWhatSetSubWhat",
         old_seen == expected && typed_seen == expected && empty_collect && old_nested == typed_nested &&
         typed_nested.size() == 3 && typed_nested[0] == "acl:alice,bob," && nest_refused && kept_collect &&
         renested_same);
}

/**
 * @brief In a Release build, time building a condition and selecting with both forms
 */
void test_timing() {
#ifndef NDEBUG
   ocs::module_test::unsupported("Release only: a debug build times its checks, not the operations");
   return;
#else
   constexpr int conditions = 1000000;
   constexpr int n = 10000;
   constexpr int rounds = 100;
   const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
   const auto owner = [](const int i) { return "user" + std::to_string(i % 10); };

   // --- CULL
   auto t0 = std::chrono::steady_clock::now();
   for (int i = 0; i < conditions; i++) {
      lCondition *cp = lWhere("%T(%I == %s && %I > %u)", JB_Type, JB_owner, "user3", JB_job_number, 5000U);
      lFreeWhere(&cp);
   }
   auto t1 = std::chrono::steady_clock::now();
   lList *old_jobs = lCreateList("jobs", JB_Type);
   for (int i = 1; i <= n; i++) {
      lListElem *ep = lAddElemUlong(&old_jobs, JB_job_number, i, JB_Type);
      lSetString(ep, JB_owner, owner(i).c_str());
   }
   lCondition *where = lWhere("%T(%I == %s && %I > %u)", JB_Type, JB_owner, "user3", JB_job_number, 5000U);
   lEnumeration *what_job = lWhat("%T(%I%I)", JB_Type, JB_job_number, JB_owner);
   int old_selected = 0;
   auto t2 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      lList *selected = lSelect("selected", old_jobs, where, what_job);
      old_selected += lGetNumberOfElem(selected);
      lFreeList(&selected);
   }
   auto t3 = std::chrono::steady_clock::now();
   lFreeWhat(&what_job);
   lFreeWhere(&where);
   lFreeList(&old_jobs);

   // --- TOM
   auto t4 = std::chrono::steady_clock::now();
   for (int i = 0; i < conditions; i++) {
      const Where<JB> typed_where = field<JB_owner>() == "user3" && field<JB_job_number>() > 5000;
   }
   auto t5 = std::chrono::steady_clock::now();
   ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
   for (int i = 1; i <= n; i++) {
      jobs->emplace_back<JB_job_number>(i).set<JB_owner>(owner(i).c_str());
   }
   const Where<JB> typed_where = field<JB_owner>() == "user3" && field<JB_job_number>() > 5000;
   const What<JB> typed_what = what<JB_job_number, JB_owner>();
   std::size_t typed_selected = 0;
   auto t6 = std::chrono::steady_clock::now();
   for (int r = 0; r < rounds; r++) {
      typed_selected += jobs->select(typed_where, typed_what, "selected")->size();
   }
   auto t7 = std::chrono::steady_clock::now();

   printf("timing, %d conditions built, %d selections from %d jobs:\n", conditions, rounds, n);
   printf("   build condition: old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t0, t1), ms(t4, t5),
          ms(t4, t5) / ms(t0, t1));
   printf("   select:          old %8.2f ms   typed %8.2f ms   typed/old %.2f\n", ms(t2, t3), ms(t6, t7),
          ms(t6, t7) / ms(t2, t3));
   CHECK(9, "the timing loops selected the same number of jobs",
         static_cast<std::size_t>(old_selected) == typed_selected && typed_selected == 500U * rounds);
#endif
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
      {"OPS-SEL-01", "what", "what<F...>() and what<T>(fields) select what lWhat and lIntVector2What select",
       {"select the names of user sets and the names of their entries, nested and flat"}, "the same contents",
       {{"OPS-01", "covers"}, {"WHT-01", "covers"}}, {1}, test_what},
      {"OPS-SEL-02", "foreign field", "a field of another type known only at run time is refused",
       {"build a selection of user sets from a list of field numbers holding a job field"},
       "ForeignField thrown; nullptr with LENAMENOT from the old form", {{"OPS-01", "covers"}, {"WHT-01", "covers"}},
       {2}, test_what_foreign},
      {"OPS-SEL-03", "comparisons", "field<F>() compares as lWhere does, for every value type",
       {"select from four elements with ==, !=, <, <=, > and >= on 32 and 64 bit, int, long, double, bool, "
        "string and host fields"}, "the same keys", {{"OPS-01", "covers"}, {"WHT-01", "covers"}}, {3}, test_compare},
      {"OPS-SEL-04", "named comparisons", "equals_nocase, like, same_host and has_all_bits select as c=, p=, h= and m=",
       {"select with each named comparison"}, "the same keys", {{"OPS-01", "covers"}, {"WHT-01", "covers"}}, {4},
       test_named},
      {"OPS-SEL-05", "combinations", "&&, || and ! combine and any() looks into a sub-list",
       {"select with &&, || and !, with a combination of a stored condition, and find by an entry of a sub-list"},
       "the same keys and the same user set", {{"OPS-01", "covers"}, {"WHT-01", "covers"}}, {5}, test_combine},
      {"OPS-SEL-06", "find", "find_if and its siblings find what the lFind functions find",
       {"find the odd keys forward and backward, test two elements"}, "the same keys and results",
       {{"OPS-01", "covers"}}, {6}, test_find},
      {"OPS-SEL-07", "select and remove", "select and remove_if as lSelect and lSelectDestroy",
       {"select user sets by a share and reduce them to their names, select nothing",
        "remove the even keys"}, "the same contents; an empty list for no match", {{"OPS-01", "covers"}}, {7},
       test_select_and_remove},
      {"OPS-SEL-08", "errors", "the typed form throws on an empty operand, on with() on ALL and on an empty selection",
       {"combine an empty condition; extend the selection of every field; extend and narrow an empty selection"},
       "NullElement, ForeignField and EmptyHandle thrown", {{"OPS-01", "covers"}}, {8, 11}, test_errors},
      {"OPS-SEL-10", "collect and nest", "|= and &= collect conditions, nest() narrows a field selected before",
       {"or three key conditions together in a loop and and a bound onto them, starting from no condition",
        "merge two selections of user sets and narrow their entries"},
       "the same keys and contents; ForeignField for nest() on a field not selected and on ALL",
       {{"OPS-01", "covers"}, {"WHT-01", "covers"}}, {10}, test_collect_and_nest},
      {"OPS-SEL-09", "timing", "a Release build times both forms",
       {"build a condition a million times; select 100 times from 10 000 jobs with a condition and a reduction"},
       "ratios printed", {{"OPS-01", "covers"}}, {9}, test_timing},
   });
}
