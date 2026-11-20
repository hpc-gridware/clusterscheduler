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
 * @brief Layouts, reductions, splitting and extra hash tables through the typed handles and through the old
 *        interface (CS-3046, OPS-01)
 *
 * This test is also an example of how the typed object model makes lists and
 * elements in the layout of others, reduces elements to a selection of
 * fields, splits a list at an element, adds a hash table on a field and
 * compares the layouts of two lists.
 * Every scenario does the same thing twice: a block marked CULL uses only the
 * old functions of the list library, a block marked TOM only the typed
 * handles. Each block builds its own data and reduces what came out to plain
 * values (names, keys, which fields are present), and the scenario compares
 * those. Each step carries a comment of the form
 *
 *     old:     the call of the list library
 *     typed:   the call on the handles
 *     differs: what the typed form does differently, if anything
 *
 * The old interface passes descriptors around: lGetListDescr() and
 * lGetElemDescr() give them, lReduceDescr() and ocs_cull_descr_reduce() make
 * reduced ones, and the caller releases them. The typed form takes the handle
 * or the selection itself (create_like(), create(what), select(what)) or a
 * layout that lives as long as the process (layout<F...>()), so no caller
 * holds or releases a descriptor.
 */

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "cull/cull.h"
#include "cull/cull_listP.h"
#include "cull/ocs_cull_descr.h"
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
using ocs::tom::Hashing;
using ocs::tom::List;
using ocs::tom::ListPtr;
using ocs::tom::TEST;
using ocs::tom::UE;
using ocs::tom::US;
using ocs::tom::field;
using ocs::tom::layout;
using ocs::tom::what;

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

   /** @brief The user sets: name, type and entries; the last has no entries */
   constexpr const char *Sets[][3] = {{"acl", "alice", "bob"}, {"dept", "carol", nullptr}, {"empty", nullptr, nullptr}};

   /**
    * @brief The user sets as a list of the old interface
    * @return the list, owned by the caller
    */
   lList *old_sets() {
      lList *sets = lCreateList("sets", US_Type);
      std::uint32_t type = 1;
      for (const auto &set : Sets) {
         lListElem *ep = lAddElemStr(&sets, US_name, set[0], US_Type);
         lSetUlong(ep, US_type, type++);
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
      std::uint32_t type = 1;
      for (const auto &set : Sets) {
         const Element<US> us = sets->emplace_back<US_name>(set[0]);
         us.set<US_type>(type++);
         for (int i = 1; i < 3 && set[i] != nullptr; i++) {
            us.get_or_create<US_entries>().emplace_back<UE_name>(set[i]);
         }
      }
      return sets;
   }

   /**
    * @brief What a user set holds, as plain text: "name:entry,entry," plus " type" if US_type is present and
    *        " fshare" if US_fshare is present
    * @param ep the user set, or nullptr
    * @return the text, "none" for nullptr
    */
   std::string old_text(const lListElem *ep) {
      if (ep == nullptr) {
         return "none";
      }
      std::string text = std::string(lGetString(ep, US_name)) + ":";
      if (lGetPosViaElem(ep, US_entries, SGE_NO_ABORT) >= 0) {
         for (const lListElem *entry = lFirst(lGetList(ep, US_entries)); entry != nullptr; entry = lNext(entry)) {
            text += std::string(lGetString(entry, UE_name)) + ",";
         }
      }
      if (lGetPosViaElem(ep, US_type, SGE_NO_ABORT) >= 0) {
         text += " type";
      }
      if (lGetPosViaElem(ep, US_fshare, SGE_NO_ABORT) >= 0) {
         text += " fshare";
      }
      return text;
   }

   /**
    * @brief What a typed user set holds, as old_text() gives it
    * @param us the user set, or an empty handle
    * @return the text, "none" for an empty handle
    */
   std::string typed_text(const ConstElement<US> us) {
      if (!us) {
         return "none";
      }
      std::string text = std::string(us.get<US_name>()) + ":";
      if (ocs::tom::has<US_entries>(us)) {
         for (const ConstElement<UE> entry : us.get<US_entries>()) {
            text += std::string(entry.get<UE_name>()) + ",";
         }
      }
      if (ocs::tom::has<US_type>(us)) {
         text += " type";
      }
      if (ocs::tom::has<US_fshare>(us)) {
         text += " fshare";
      }
      return text;
   }

   /**
    * @brief The keys of a list of TEST elements of the old interface, in their order
    * @param lp the list, or nullptr
    * @return the keys joined by commas, "none" for nullptr
    */
   std::string old_keys(const lList *lp) {
      if (lp == nullptr) {
         return "none";
      }
      std::string keys = std::string(lGetListName(lp)) + ":";
      for (const lListElem *ep = lFirst(lp); ep != nullptr; ep = lNext(ep)) {
         keys += std::to_string(lGetUlong(ep, TEST_uint32)) + ",";
      }
      return keys;
   }

   /**
    * @brief The keys of a typed list of TEST elements, as old_keys() gives them
    * @param rows the list, or an empty handle
    * @return the keys joined by commas, "none" for an empty handle
    */
   std::string typed_keys(const ConstList<TEST> rows) {
      if (!rows) {
         return "none";
      }
      std::string keys = std::string(rows.name()) + ":";
      for (const ConstElement<TEST> row : rows) {
         keys += std::to_string(row.get<TEST_uint32>()) + ",";
      }
      return keys;
   }

   /**
    * @brief A list of TEST elements with the keys 1 to n and the string "s<key>"
    * @param n the number of elements
    * @return the list, owned by the caller
    */
   lList *old_rows(const int n) {
      lList *lp = lCreateList("rows", TEST_Type);
      for (int i = 1; i <= n; i++) {
         lListElem *ep = lAddElemUlong(&lp, TEST_uint32, static_cast<lUlong>(i), TEST_Type);
         lSetString(ep, TEST_string, ("s" + std::to_string(i)).c_str());
      }
      return lp;
   }

   /**
    * @brief The list of old_rows() as a typed list
    * @param n the number of elements
    * @return the owning handle
    */
   ListPtr<TEST> typed_rows(const int n) {
      ListPtr<TEST> rows = ListPtr<TEST>::create("rows");
      for (int i = 1; i <= n; i++) {
         const Element<TEST> row = rows->emplace_back<TEST_uint32>(static_cast<std::uint32_t>(i));
         row.set<TEST_string>(("s" + std::to_string(i)).c_str());
      }
      return rows;
   }

   /**
    * @brief Whether a list holds a built hash table on a field
    *
    * Tells a lookup that went through a table from a linear search, so a
    * scenario can show that a table it relies on exists.
    *
    * @param lp the list, or nullptr
    * @param nm the field
    * @return true if the table on the field is built; false for nullptr, a field the list does not hash or a
    *         table not built yet
    */
   bool has_table(const lList *lp, const int nm) {
      if (lp == nullptr) {
         return false;
      }
      const int pos = lGetPosInDescr(lp->descr, nm);
      const cull_hash_slot *slot = pos >= 0 ? list_hash_slot(lp, pos) : nullptr;
      return slot != nullptr && slot->get() != nullptr;
   }

   /**
    * @brief The name of a numbered user set
    * @param i the number
    * @return "u<i>"
    */
   std::string set_name(const int i) {
      return "u" + std::to_string(i);
   }

   /**
    * @brief A list of user sets with the names u1 to un; US_name is a hashed unique field
    * @param n the number of elements
    * @return the list, owned by the caller
    */
   lList *old_named_sets(const int n) {
      lList *lp = lCreateList("named", US_Type);
      for (int i = 1; i <= n; i++) {
         lAddElemStr(&lp, US_name, set_name(i).c_str(), US_Type);
      }
      return lp;
   }

   /**
    * @brief The list of old_named_sets() as a typed list
    * @param n the number of elements
    * @return the owning handle
    */
   ListPtr<US> typed_named_sets(const int n) {
      ListPtr<US> sets = ListPtr<US>::create("named");
      for (int i = 1; i <= n; i++) {
         sets->emplace_back<US_name>(set_name(i).c_str());
      }
      return sets;
   }

   /**
    * @brief Look up a numbered user set by its hashed name in a list of the old interface
    * @param lp the list
    * @param i the number of the set
    * @return "found" or "lost"
    */
   std::string old_lookup(const lList *lp, const int i) {
      return lGetElemStr(lp, US_name, set_name(i).c_str()) != nullptr ? "found" : "lost";
   }

   /**
    * @brief Look up a numbered user set by its hashed name in a typed list
    * @param sets the list
    * @param i the number of the set
    * @return "found" or "lost"
    */
   std::string typed_lookup(const ConstList<US> sets, const int i) {
      return sets.find_first<US_name>(set_name(i).c_str()) ? "found" : "lost";
   }

   /**
    * @brief The layout of a list or element: the descriptor of the old interface
    */
   void test_descr() {
      // old:     lGetListDescr(lp), lGetElemDescr(ep); both nullptr (with an error number) for nullptr
      // typed:   list.descr(), element.descr(); both nullptr for an empty handle
      // differs: the typed form sets no error number for an empty handle

      lList *sets = old_sets();
      lEnumeration *what_name = lWhat("%T(%I)", US_Type, US_name);
      lList *reduced = lSelect("reduced", sets, nullptr, what_name);

      const auto typed_reduced = ConstList<US>(reduced);
      const bool same = ConstList<US>(sets).descr() == lGetListDescr(sets) &&
                        typed_reduced.descr() == lGetListDescr(reduced) &&
                        typed_reduced.front().descr() == lGetElemDescr(lFirst(reduced)) &&
                        typed_reduced.descr() != ConstList<US>(sets).descr();
      const bool empty = ConstList<US>().descr() == nullptr && lGetListDescr(nullptr) == nullptr &&
                         ConstElement<US>().descr() == nullptr && lGetElemDescr(nullptr) == nullptr;

      lFreeList(&reduced);
      lFreeWhat(&what_name);
      lFreeList(&sets);

      CHECK(1, "descr() gives what lGetListDescr and lGetElemDescr give, also for a reduced list; nullptr when empty",
            same && empty);
   }

   /**
    * @brief Lists and elements in the layout of a reduced list or element
    */
   void test_create_like() {
      // old:     lCreateList(name, lGetListDescr(lp)), lCreateList(name, lGetElemDescr(ep)),
      //          lCreateElem(lGetListDescr(lp)), lCreateElem(lGetElemDescr(ep))
      // typed:   ListPtr<T>::create_like(name, list), ListPtr<T>::create_like(name, element),
      //          ElementPtr<T>::create_like(list), ElementPtr<T>::create_like(element)
      // differs: an empty handle throws NullDescriptor, where lCreateList(name, nullptr) gave nullptr

      // --- CULL
      std::vector<std::string> old_seen;
      {
         lList *sets = old_sets();
         lEnumeration *what_name = lWhat("%T(%I)", US_Type, US_name);
         lList *reduced = lSelect("reduced", sets, nullptr, what_name);
         lList *like_list = lCreateList("like list", lGetListDescr(reduced));
         lList *like_elem = lCreateList("like elem", lGetElemDescr(lFirst(reduced)));
         lListElem *ep = lCreateElem(lGetListDescr(reduced));
         lSetString(ep, US_name, "new");
         lAppendElem(like_list, ep);
         ep = lCreateElem(lGetElemDescr(lFirst(reduced)));
         lSetString(ep, US_name, "other");
         lAppendElem(like_elem, ep);
         // a reduced element moves into a list of its layout
         lAppendElem(like_list, lDechainElem(reduced, lFirstRW(reduced)));
         for (const lListElem *e = lFirst(like_list); e != nullptr; e = lNext(e)) {
            old_seen.push_back(old_text(e));
         }
         old_seen.push_back(old_text(lFirst(like_elem)));
         old_seen.push_back(lCreateList("null", lGetListDescr(nullptr)) == nullptr ? "refused" : "made");
         lFreeList(&like_elem);
         lFreeList(&like_list);
         lFreeList(&reduced);
         lFreeWhat(&what_name);
         lFreeList(&sets);
      }

      // --- TOM
      std::vector<std::string> typed_seen;
      {
         const ListPtr<US> sets = typed_sets();
         const ListPtr<US> reduced = sets->select(what<US_name>(), "reduced");
         auto like_list = ListPtr<US>::create_like("like list", *reduced);
         auto like_elem = ListPtr<US>::create_like("like elem", reduced->front());
         like_list->push_back(ElementPtr<US>::create_like(*reduced)).set<US_name>("new");
         like_elem->push_back(ElementPtr<US>::create_like(reduced->front())).set<US_name>("other");
         like_list->push_back(reduced->extract(reduced->front()));
         for (const ConstElement<US> us : *like_list) {
            typed_seen.push_back(typed_text(us));
         }
         typed_seen.push_back(typed_text(like_elem->front()));
         typed_seen.push_back(throws([] { static_cast<void>(ListPtr<US>::create_like("null", List<US>())); },
                                     FieldException::Reason::NullDescriptor) &&
                              throws([] { static_cast<void>(ElementPtr<US>::create_like(ConstElement<US>())); },
                                     FieldException::Reason::NullDescriptor)
                                 ? "refused"
                                 : "made");
         typed_seen.emplace_back(ocs::tom::same_layout(like_list->descr(), reduced->descr()) &&
                                 ocs::tom::same_layout(like_elem->descr(), reduced->descr())
                                    ? "same"
                                    : "other");
      }

      const std::vector<std::string> expected{"new:", "acl:", "other:", "refused"};
      CHECK(2, "create_like() makes lists and elements in the layout lGetListDescr and lGetElemDescr give",
            old_seen == expected && typed_seen.size() == 5 &&
            std::vector<std::string>(typed_seen.begin(), typed_seen.end() - 1) == expected &&
            typed_seen.back() == "same");
   }

   /**
    * @brief A layout from fields known at compile time, and one from a selection known at run time
    */
   void test_layout() {
      // old:     static const lDescr *const d = ocs_cull_descr_reduce(US_Type, {US_name, US_type});
      //          lCreateElem(d), lCreateList(name, d)
      // typed:   ElementPtr<US>::create(layout<US_name, US_type>()), ListPtr<US>::create(name, layout<...>())
      // old:     lReduceDescr(&rdp, US_Type, what); lCreateElem(rdp); ...; ocs_cull_descr_release(&rdp)
      // typed:   ElementPtr<US>::create(what), ListPtr<US>::create(name, what)
      // differs: nothing to release; an empty selection handle throws NullElement (the reason every operation
      //          gives for an empty selection, see ocs_Select.h) where lReduceDescr() gave no descriptor

      // --- CULL
      std::vector<std::string> old_seen;
      {
         const lDescr *reduced = ocs_cull_descr_reduce(US_Type, {US_type, US_name});
         lList *lp = lCreateList("by fields", reduced);
         lListElem *ep = lCreateElem(reduced);
         lSetString(ep, US_name, "fields");
         lAppendElem(lp, ep);
         old_seen.push_back(old_text(lFirst(lp)));
         lFreeList(&lp);
         ocs_cull_descr_release(&reduced);

         lEnumeration *selection = lWhat("%T(%I%I)", US_Type, US_name, US_fshare);
         const lDescr *rdp = nullptr;
         lReduceDescr(&rdp, US_Type, selection);
         lp = lCreateList("by selection", rdp);
         ep = lCreateElem(rdp);
         lSetString(ep, US_name, "selection");
         lAppendElem(lp, ep);
         old_seen.push_back(old_text(lFirst(lp)));
         lFreeList(&lp);
         ocs_cull_descr_release(&rdp);
         lFreeWhat(&selection);

         lEnumeration *none = lWhat("%T(NONE)", US_Type);
         lReduceDescr(&rdp, US_Type, none);
         old_seen.emplace_back(rdp == nullptr ? "refused" : "made");
         lFreeWhat(&none);
      }

      // --- TOM
      std::vector<std::string> typed_seen;
      bool once = false;
      {
         auto lp = ListPtr<US>::create("by fields", layout<US_type, US_name>());
         lp->push_back(ElementPtr<US>::create(layout<US_name, US_type>())).set<US_name>("fields");
         typed_seen.push_back(typed_text(lp->front()));
         // the order of the fields does not matter, and every call gives the descriptor built once
         once = layout<US_type, US_name>().get() == layout<US_type, US_name>().get() &&
                ocs::tom::same_layout(layout<US_type, US_name>().get(), layout<US_name, US_type>().get());

         const auto selection = what<US_name, US_fshare>();
         auto by_what = ListPtr<US>::create("by selection", selection);
         by_what->push_back(ElementPtr<US>::create(selection)).set<US_name>("selection");
         typed_seen.push_back(typed_text(by_what->front()));

         typed_seen.emplace_back(
                 throws([] { static_cast<void>(ElementPtr<US>::create(ocs::tom::What<US>())); },
                        FieldException::Reason::NullElement) &&
                 throws([] { static_cast<void>(ListPtr<US>::create("none", ocs::tom::What<US>())); },
                        FieldException::Reason::NullElement)
                    ? "refused"
                    : "made");
      }

      const std::vector<std::string> expected{"fields: type", "selection: fshare", "refused"};
      CHECK(3, "layout<F...>() and create(what) make the layouts ocs_cull_descr_reduce and lReduceDescr make",
            old_seen == expected && typed_seen == expected && once);
   }

   /**
    * @brief Reduce single elements to a selection of fields
    */
   void test_select_elem() {
      // old:     lSelectElemPack(ep, nullptr, what, false, nullptr)
      // old:     lReduceDescr(&rdp, US_Type, what); lSelectElemDPack(ep, nullptr, rdp, what, false, nullptr)
      // old:     lSelectElemPack(ep, where, what, false, nullptr) with a condition
      // typed:   element.select(what, Hashing::Off), element.select(where, what, Hashing::Off)
      // differs: an empty selection throws NullElement where lSelectElemPack() made a full copy; the copy
      //          needs no descriptor of the caller

      // --- CULL
      std::vector<std::string> old_seen;
      {
         lList *sets = old_sets();
         lEnumeration *selection = lWhat("%T(%I%I)", US_Type, US_name, US_entries);
         const lDescr *rdp = nullptr;
         lReduceDescr(&rdp, US_Type, selection);
         lCondition *where = lWhere("%T(%I == %s)", US_Type, US_name, "dept");
         for (const lListElem *ep = lFirst(sets); ep != nullptr; ep = lNext(ep)) {
            lListElem *packed = lSelectElemPack(ep, nullptr, selection, false, nullptr);
            lListElem *dpacked = lSelectElemDPack(ep, nullptr, rdp, selection, false, nullptr);
            lListElem *matched = lSelectElemPack(ep, where, selection, false, nullptr);
            old_seen.push_back(old_text(packed) + "|" + old_text(dpacked) + "|" + old_text(matched));
            lFreeElem(&matched);
            lFreeElem(&dpacked);
            lFreeElem(&packed);
         }
         old_seen.push_back(old_text(lSelectElemPack(nullptr, nullptr, selection, false, nullptr)));
         lFreeWhere(&where);
         ocs_cull_descr_release(&rdp);
         lFreeWhat(&selection);
         lFreeList(&sets);
      }

      // --- TOM
      std::vector<std::string> typed_seen;
      bool refused = false;
      {
         const ListPtr<US> sets = typed_sets();
         const auto selection = what<US_name, US_entries>();
         const auto where = field<US_name>() == "dept";
         for (const ConstElement<US> us : *sets) {
            const auto selected = us.select(selection, Hashing::Off);
            // the same selection again: the cached reduction, as lSelectElemDPack() with one descriptor
            const auto again = us.select(selection, Hashing::Off);
            const auto matched = us.select(where, selection, Hashing::Off);
            typed_seen.push_back(typed_text(*selected) + "|" + typed_text(*again) + "|" + typed_text(*matched));
         }
         typed_seen.push_back(typed_text(*ConstElement<US>().select(selection)));
         refused = throws([&sets] { static_cast<void>(sets->front().select(ocs::tom::What<US>())); },
                          FieldException::Reason::NullElement);
      }

      const std::vector<std::string> expected{"acl:alice,bob,|acl:alice,bob,|none",
                                              "dept:carol,|dept:carol,|dept:carol,", "empty:|empty:|none", "none"};
      CHECK(4, "select(what) and select(where, what) reduce an element as lSelectElemPack and lSelectElemDPack do",
            old_seen == expected && typed_seen == expected && refused);
   }

   /**
    * @brief Split a list at an element
    */
   void test_split_at() {
      // old:     lDechainList(source, &target, ep) with target nullptr (created) and with an existing target
      // typed:   tail = list.split_at(element); target.splice(std::move(tail)) for an existing target
      // differs: an element of another list throws ForeignList where lDechainList() aborted; the hash tables of
      //          the source are kept up to date rather than dropped

      for (const int n : {5, 40}) {
         // --- CULL
         std::vector<std::string> old_seen;
         {
            lList *source = old_rows(n);
            lList *target = nullptr;
            lDechainList(source, &target, lGetElemUlongRW(source, TEST_uint32, static_cast<lUlong>(n - 1)));
            old_seen.push_back(old_keys(source));
            old_seen.push_back(old_keys(target));
            lList *existing = lCreateList("existing", TEST_Type);
            lAddElemUlong(&existing, TEST_uint32, 100, TEST_Type);
            lDechainList(source, &existing, lFirstRW(source));
            old_seen.push_back(old_keys(source));
            old_seen.push_back(old_keys(existing));
            old_seen.push_back(lGetElemUlong(existing, TEST_uint32, 2) != nullptr ? "found" : "lost");
            lFreeList(&existing);
            lFreeList(&target);
            lFreeList(&source);
         }

         // --- TOM
         std::vector<std::string> typed_seen;
         bool foreign = false;
         {
            const ListPtr<TEST> source = typed_rows(n);
            const auto second_last = source->find_first<TEST_uint32>(static_cast<std::uint32_t>(n - 1));
            const ListPtr<TEST> target = source->split_at(second_last);
            typed_seen.push_back(typed_keys(*source));
            typed_seen.push_back(typed_keys(*target));
            auto existing = ListPtr<TEST>::create("existing");
            existing->emplace_back<TEST_uint32>(100);
            existing->splice(source->split_at(source->front()));
            typed_seen.push_back(typed_keys(*source));
            typed_seen.push_back(typed_keys(*existing));
            typed_seen.emplace_back(existing->find_first<TEST_uint32>(2) ? "found" : "lost");
            foreign = throws([&source, &target] { static_cast<void>(source->split_at(target->front())); },
                             FieldException::Reason::ForeignList);
         }

         // the keys 1 to n - 2 that stay in the source at the first split
         std::string head;
         for (int i = 1; i <= n - 2; i++) {
            head += std::to_string(i) + ",";
         }
         const std::vector<std::string> expected{"rows:" + head,
                                                 "rows:" + std::to_string(n - 1) + "," + std::to_string(n) + ",",
                                                 "rows:", "existing:100," + head, "found"};
         CHECK(5, "split_at() takes out what lDechainList moves, into a new list or spliced into an existing one",
               old_seen == expected && typed_seen == expected && foreign);
      }

      // the same on a hashed unique field, US_name: every lookup below goes through a hash table, also on the
      // short lists, so a table not kept up to date by split_at() or splice() would still find a moved element
      // in the source or miss it in the target
      ocs_cull_hash_set_min_elems(0);
      for (const int n : {5, 40}) {
         // --- CULL
         std::vector<std::string> old_seen;
         {
            lList *source = old_named_sets(n);
            // the first lookup builds the table of the source, before the split
            old_seen.push_back(old_lookup(source, n));
            lList *target = nullptr;
            lDechainList(source, &target, lGetElemStrRW(source, US_name, set_name(n - 1).c_str()));
            old_seen.push_back(old_lookup(source, n));
            old_seen.push_back(old_lookup(source, 1));
            old_seen.push_back(old_lookup(target, n));
            lList *existing = lCreateList("existing", US_Type);
            lAddElemStr(&existing, US_name, set_name(100).c_str(), US_Type);
            old_seen.push_back(old_lookup(existing, 100));
            lDechainList(source, &existing, lFirstRW(source));
            old_seen.push_back(old_lookup(source, 1));
            old_seen.push_back(old_lookup(existing, 1));
            old_seen.push_back(old_lookup(existing, n - 2));
            lFreeList(&existing);
            lFreeList(&target);
            lFreeList(&source);
         }

         // --- TOM
         std::vector<std::string> typed_seen;
         bool tables = false;
         {
            const ListPtr<US> source = typed_named_sets(n);
            // the first lookup builds the table of the source, before the split
            typed_seen.push_back(typed_lookup(*source, n));
            const ListPtr<US> target = source->split_at(source->find_first<US_name>(set_name(n - 1).c_str()));
            typed_seen.push_back(typed_lookup(*source, n));
            typed_seen.push_back(typed_lookup(*source, 1));
            typed_seen.push_back(typed_lookup(*target, n));
            auto existing = ListPtr<US>::create("existing");
            existing->emplace_back<US_name>(set_name(100).c_str());
            typed_seen.push_back(typed_lookup(*existing, 100));
            existing->splice(source->split_at(source->front()));
            typed_seen.push_back(typed_lookup(*source, 1));
            typed_seen.push_back(typed_lookup(*existing, 1));
            typed_seen.push_back(typed_lookup(*existing, n - 2));
            // the lookups above went through the tables: the source kept the one built before the split, the
            // existing list the one built before the splice
            tables = has_table(source->raw(), US_name) && has_table(target->raw(), US_name) &&
                     has_table(existing->raw(), US_name);
         }

         const std::vector<std::string> expected{"found", "lost", "found", "found",
                                                 "found", "lost", "found", "found"};
         CHECK(7, "split_at() and splice() keep the hash tables of source and target up to date: a hashed lookup "
               "finds what lDechainList leaves and moves",
               old_seen == expected && typed_seen == expected && tables);
      }
      ocs_cull_hash_set_min_elems(OCS_CULL_HASH_MIN_ELEMS);
   }

   /**
    * @brief An extra hash table on a field the type does not hash
    */
   void test_add_hash() {
      // old:     cull_hash_new_check(lp, TEST_string, false)
      // typed:   list.add_hash<TEST_string>()
      // differs: nothing; a field of another type does not compile

      for (const int n : {5, 40}) {
         // --- CULL
         std::vector<std::string> old_seen;
         {
            lList *rows = old_rows(n);
            old_seen.push_back(std::to_string(cull_hash_new_check(rows, TEST_string, false)));
            old_seen.push_back(std::to_string(cull_hash_new_check(rows, TEST_string, false)));
            const lListElem *ep = lGetElemStr(rows, TEST_string, ("s" + std::to_string(n)).c_str());
            old_seen.push_back(ep != nullptr ? std::to_string(lGetUlong(ep, TEST_uint32)) : "none");
            old_seen.push_back(std::to_string(cull_hash_new_check(nullptr, TEST_string, false)));
            lFreeList(&rows);
         }

         // --- TOM
         std::vector<std::string> typed_seen;
         {
            const ListPtr<TEST> rows = typed_rows(n);
            typed_seen.push_back(std::to_string(static_cast<int>(rows->add_hash<TEST_string>())));
            typed_seen.push_back(std::to_string(static_cast<int>(rows->add_hash<TEST_string>())));
            const auto row = rows->find_first<TEST_string>(("s" + std::to_string(n)).c_str());
            typed_seen.push_back(row ? std::to_string(row.get<TEST_uint32>()) : "none");
            typed_seen.push_back(std::to_string(static_cast<int>(List<TEST>().add_hash<TEST_string>())));
         }

         const std::vector<std::string> expected{"1", "1", std::to_string(n), "1"};
         CHECK(6, "add_hash<F>() adds a table as cull_hash_new_check does, and lookups by the field find the same",
               old_seen == expected && typed_seen == expected);
      }
   }

   /**
    * @brief Compare the layouts of two lists
    */
   void test_same_layout() {
      // old:     lCompListDescr(lGetListDescr(a), lGetListDescr(b)) == 0
      // typed:   a.same_layout(b)
      // differs: the typed form sets no error number when the layouts differ or a list is missing

      // --- CULL
      std::vector<std::string> old_seen;
      {
         const auto same = [](const lList *a, const lList *b) {
            return std::string(lCompListDescr(lGetListDescr(a), lGetListDescr(b)) == 0 ? "same" : "differs");
         };
         lList *sets = old_sets();
         lList *others = old_sets();
         lList *empty = lCreateList("empty", US_Type);
         lEnumeration *what_name = lWhat("%T(%I)", US_Type, US_name);
         lEnumeration *what_type = lWhat("%T(%I)", US_Type, US_type);
         lList *by_name = lSelect("by name", sets, nullptr, what_name);
         lList *by_name_too = lSelect("by name too", others, nullptr, what_name);
         lList *by_type = lSelect("by type", sets, nullptr, what_type);
         old_seen.push_back(same(sets, others));
         old_seen.push_back(same(sets, empty));
         old_seen.push_back(same(sets, by_name));
         old_seen.push_back(same(by_name, by_name_too));
         old_seen.push_back(same(by_name, by_type));
         old_seen.push_back(same(sets, nullptr));
         old_seen.push_back(same(nullptr, nullptr));
         lFreeList(&by_type);
         lFreeList(&by_name_too);
         lFreeList(&by_name);
         lFreeWhat(&what_type);
         lFreeWhat(&what_name);
         lFreeList(&empty);
         lFreeList(&others);
         lFreeList(&sets);
      }

      // --- TOM
      std::vector<std::string> typed_seen;
      {
         const auto same = [](const ConstList<US> a, const ConstList<US> b) {
            return std::string(a.same_layout(b) ? "same" : "differs");
         };
         const ListPtr<US> sets = typed_sets();
         const ListPtr<US> others = typed_sets();
         const ListPtr<US> empty = ListPtr<US>::create("empty");
         const ListPtr<US> by_name = sets->select(what<US_name>(), "by name");
         const ListPtr<US> by_name_too = others->select(what<US_name>(), "by name too");
         const ListPtr<US> by_type = sets->select(what<US_type>(), "by type");
         typed_seen.push_back(same(*sets, *others));
         typed_seen.push_back(same(*sets, *empty));
         typed_seen.push_back(same(*sets, *by_name));
         typed_seen.push_back(same(*by_name, *by_name_too));
         typed_seen.push_back(same(*by_name, *by_type));
         typed_seen.push_back(same(*sets, nullptr));
         typed_seen.push_back(same(nullptr, nullptr));
      }

      const std::vector<std::string> expected{"same", "same", "differs", "same", "differs", "differs", "differs"};
      CHECK(8, "same_layout() tells the same layouts lCompListDescr tells, full and reduced, and refuses missing "
            "lists",
            old_seen == expected && typed_seen == expected);
   }

} // namespace

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"OPS-LAY-01", "descriptor", "descr() gives the layout lGetListDescr and lGetElemDescr give",
       {"read the layout of a full and a reduced list and element, and of empty handles"}, "the same descriptors",
       {{"OPS-01", "covers"}}, {1}, test_descr},
      {"OPS-LAY-02", "create like", "create_like() makes lists and elements in the layout of a list or an element",
       {"create a list and an element like a reduced list and like a reduced element, fill and combine them",
        "create like an empty handle"},
       "the same contents; NullDescriptor for an empty handle", {{"OPS-01", "covers"}}, {2}, test_create_like},
      {"OPS-LAY-03", "layout", "layout<F...>() and create(what) make reduced layouts as ocs_cull_descr_reduce and "
       "lReduceDescr do",
       {"create a list and an element of a layout from fields and from a selection", "reduce to nothing"},
       "the same fields present; one descriptor per set of fields; NullElement for an empty selection",
       {{"OPS-01", "covers"}}, {3}, test_layout},
      {"OPS-LAY-04", "select element", "select(what) and select(where, what) reduce one element",
       {"reduce every user set to its name and entries, with and without a condition", "reduce no element"},
       "the same contents; NullElement for an empty selection", {{"OPS-01", "covers"}}, {4}, test_select_elem},
      {"OPS-LAY-05", "split at", "split_at() takes an element and the ones after it out of a list",
       {"split a short and a long list before its last two elements", "split the rest into an existing list",
        "the same on a hashed unique field: hashed lookups before and after the split and the splice"},
       "the same keys in both lists; lookups by key still work; ForeignList for an element of another list",
       {{"OPS-01", "covers"}}, {5, 7}, test_split_at},
      {"OPS-LAY-06", "add hash", "add_hash<F>() adds a hash table on a field the type does not hash",
       {"add a table on the string field of a short and a long list, twice, and look up by it",
        "add one on an empty handle"},
       "the same results and the same element found", {{"OPS-01", "covers"}}, {6}, test_add_hash},
      {"OPS-LAY-07", "same layout", "same_layout() compares the layouts of two lists as lCompListDescr does",
       {"compare full lists, full and reduced lists, lists reduced alike and differently, and missing lists"},
       "the same verdicts", {{"OPS-01", "covers"}}, {8}, test_same_layout},
   });
}
