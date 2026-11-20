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
 * @brief Module test of the owners and the generic list and selection ops of the run-time typed interface (RAW-OPS)
 *
 * The owners RawElementPtr and RawListPtr create, copy, release and free an
 * element or a list of any type; the typed owners convert to them explicitly
 * by move, and adopt<T>() hands them back to the typed owners after a check
 * that leaves the source owning when it fails. The list ops and adopt<T>()
 * also take a typed owner directly, with the same strong guarantee. The list ops append, insert, take,
 * erase and splice with the strong guarantee: a refused element or list stays
 * with its owner. find_first() must find what the old lookups find, with and
 * without a hash table, and select() what lSelect() selects. Each op is also
 * run through the method of the raw handle, which only forwards. The
 * selections with the old contract (select_as(), select_elem_as(),
 * select_into_descr(), select_elem_into_descr(), select_pack()) must give
 * what lSelect(), lSelectHashPack(), lSelectDPack(), lSelectElemPack() and
 * lSelectElemDPack() give: the same list name, no list where those gave
 * none, the same error number, the same packed bytes.
 *
 * That the owners free in their destructors is shown by the leak checker
 * (valgrind, ASan) on this binary, not by a CHECK.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cull/cull.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_cull_descr.h"

#include "ocs_module_test.h"

namespace {

   using Reason = ocs::tom::FieldException::Reason;
   using Numbers = std::vector<std::uint32_t>;

   /**
    * @brief The reason a call throws with, if it throws a FieldException
    * @tparam Call the type of the call
    * @param call the call
    * @return the reason, or std::nullopt if it threw nothing
    */
   template <typename Call>
   std::optional<Reason> reason_of(Call &&call) {
      try {
         call();
      } catch (const ocs::tom::FieldException &e) {
         return e.reason();
      }
      return std::nullopt;
   }

   /**
    * @brief Whether a call throws a FieldException of a reason; prints the case if not
    * @tparam Call the type of the call
    * @param label the case, for the message
    * @param step the step of the case, for the message
    * @param call the call
    * @param expected the reason it must throw with
    * @return true if it threw with @p expected
    */
   template <typename Call>
   bool expect_reason(const char *label, const char *step, Call &&call, const Reason expected) {
      const std::optional<Reason> got = reason_of(std::forward<Call>(call));
      if (got != expected) {
         std::printf("%s, %s: reason %d, expected %d\n", label, step, got ? static_cast<int>(*got) : -1,
                     static_cast<int>(expected));
         return false;
      }
      return true;
   }

   /**
    * @brief A list of TEST elements built with the old interface, with keys of every lookup type
    *
    * TEST_uint32 is i, TEST_uint64 i % 3, TEST_string "s<i % 4>", TEST_host "h<i % 2>", so that all but the
    * first key repeat.
    *
    * @param n the number of elements
    * @param hash whether the list keeps hash tables
    * @return the list; the caller owns it
    */
   lList *make_test_list(const int n, const bool hash) {
      lList *lp = lCreateListHash("test", TEST_Type, hash);
      for (int i = 0; i < n; i++) {
         lListElem *ep = lCreateElem(TEST_Type);
         lSetUlong(ep, TEST_uint32, i);
         lSetUlong64(ep, TEST_uint64, i % 3);
         const std::string s = "s" + std::to_string(i % 4);
         lSetString(ep, TEST_string, s.c_str());
         const std::string h = "h" + std::to_string(i % 2);
         lSetHost(ep, TEST_host, h.c_str());
         lAppendElem(lp, ep);
      }
      return lp;
   }

   /**
    * @brief A TEST element with a number, built with the raw interface
    * @param number the value of TEST_uint32
    * @return the owner of the element
    */
   ocs::tom::RawElementPtr test_elem(const std::uint32_t number) {
      auto ep = ocs::tom::create_raw_elem(TEST_Type);
      ep->set(TEST_uint32, number);
      return ep;
   }

   /**
    * @brief The values of a 32 bit field of the elements of a list, in order
    * @param list the list
    * @param nm the field
    * @return the values
    */
   Numbers numbers(const ocs::tom::ConstRawList list, const int nm = TEST_uint32) {
      Numbers out;
      for (const auto ep : list) {
         out.push_back(ep.get<std::uint32_t>(nm));
      }
      return out;
   }

   /**
    * @brief Whether a selection of TEST_uint32 and TEST_string made by lSelect() equals one made by select()
    * @param old the result of lSelect(), or nullptr for no match
    * @param selected the result of select()
    * @return true if both have the same layout and the same elements in the same order
    */
   bool same_selection(const lList *old, const ocs::tom::ConstRawList selected) {
      if (old == nullptr) {
         return selected.empty();
      }
      if (static_cast<std::size_t>(lGetNumberOfElem(old)) != selected.size() ||
          !ocs::tom::same_layout(lGetListDescr(old), selected.descr())) {
         std::printf("the selections differ in size or layout\n");
         return false;
      }
      auto it = selected.begin();
      for (const lListElem *ep = lFirst(old); ep != nullptr; ep = lNext(ep), ++it) {
         const auto other = *it;
         if (lGetUlong(ep, TEST_uint32) != other.get<std::uint32_t>(TEST_uint32) ||
             std::strcmp(lGetString(ep, TEST_string), other.get<const char *>(TEST_string)) != 0) {
            std::printf("the selections differ in element %u\n", lGetUlong(ep, TEST_uint32));
            return false;
         }
      }
      return true;
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
    * @brief Whether a list made by an old select function equals one made by a select op with the old contract
    *
    * Both no list, or both a list with the same name, size and layout, both hashable or both not (the same
    * number of hash slots; the tables themselves are built by the first lookup), and the same TEST_uint32 in
    * each element where the layout holds it.
    *
    * @param label the case, for the message
    * @param old the result of the old function, or nullptr
    * @param selected the result of the op, or nullptr
    * @return true if they are equal
    */
   bool same_list(const char *label, const lList *old, const lList *selected) {
      if (old == nullptr || selected == nullptr) {
         if (old != selected) {
            std::printf("%s: one result is no list, the other is a list\n", label);
            return false;
         }
         return true;
      }
      if (std::strcmp(lGetListName(old), ocs::tom::list_name(selected)) != 0 ||
          lGetNumberOfElem(old) != lGetNumberOfElem(selected) ||
          !ocs::tom::same_layout(lGetListDescr(old), selected->descr)) {
         std::printf("%s: the lists differ in name, size or layout\n", label);
         return false;
      }
      if (old->nslots != selected->nslots) {
         std::printf("%s: the lists differ in their hash slots (%u, %u)\n", label, old->nslots, selected->nslots);
         return false;
      }
      const lListElem *other = lFirst(selected);
      for (const lListElem *ep = lFirst(old); ep != nullptr; ep = lNext(ep), other = lNext(other)) {
         if (ocs::tom::has(ep, TEST_uint32) &&
             lGetUlong(ep, TEST_uint32) != ocs::tom::get<std::uint32_t>(other, TEST_uint32)) {
            std::printf("%s: the lists differ in an element\n", label);
            return false;
         }
      }
      return true;
   }

   /**
    * @brief Whether an element made by an old select function equals one made by a select op with the old
    *        contract: both none, or both of one layout with the same TEST_uint32 where the layout holds it
    * @param label the case, for the message
    * @param old the result of the old function, or nullptr
    * @param selected the result of the op, or nullptr
    * @return true if they are equal
    */
   bool same_elem(const char *label, const lListElem *old, const lListElem *selected) {
      if (old == nullptr || selected == nullptr) {
         if (old != selected) {
            std::printf("%s: one result is no element, the other is an element\n", label);
            return false;
         }
         return true;
      }
      if (!ocs::tom::same_layout(lGetElemDescr(old), selected->descr) ||
          (ocs::tom::has(old, TEST_uint32) &&
           lGetUlong(old, TEST_uint32) != ocs::tom::get<std::uint32_t>(selected, TEST_uint32))) {
         std::printf("%s: the elements differ\n", label);
         return false;
      }
      return true;
   }

   /**
    * @brief The bytes a write function puts into a fresh pack buffer
    * @tparam Write the type of the write function
    * @param write writes into the buffer it is given
    * @return the bytes written
    */
   template <typename Write>
   std::string packed(Write &&write) {
      ocs::tom::PackBuffer buffer;
      write(&buffer.raw());
      return {buffer.data(), buffer.size()};
   }

} // namespace

/**
 * @brief The owners: create, read through, copy, release, reset, move; no descriptor is refused
 * @return nothing; CHECK records the results
 */
static void
test_owners() {
   auto ep = ocs::tom::RawElementPtr::create(TEST_Type);
   const bool created = static_cast<bool>(ep) && ocs::tom::same_type(ep.get().descr(), TEST_Type) &&
                        (*ep).raw() == ep.get().raw();
   ep->set(TEST_uint32, std::uint32_t{4});
   ep->set(TEST_string, "copy me");
   auto copy = ep.clone();
   const bool cloned = copy && copy.get().raw() != ep.get().raw() && copy->get<std::uint32_t>(TEST_uint32) == 4 &&
                       std::strcmp(copy->get<const char *>(TEST_string), "copy me") == 0;
   lListElem *released = copy.release();
   const bool released_ok = !copy && released != nullptr;
   lFreeElem(&released);
   ep.reset();
   const bool reset_ok = !ep;
   auto a = test_elem(1);
   const lListElem *a_raw = a.get();
   ocs::tom::RawElementPtr b(std::move(a));
   ocs::tom::RawElementPtr c;
   c = std::move(b);
   const bool moved = !a && !b && c.get().raw() == a_raw;

   auto list = ocs::tom::RawListPtr::create(TEST_Type, ocs::tom::Hashing::Off, "numbers");
   list->push_back(test_elem(1));
   list->push_back(test_elem(2));
   const bool named = std::strcmp(lGetListName(list.get()), "numbers") == 0 && (*list).size() == 2;
   auto list_copy = list.clone();
   const bool list_cloned = list_copy && list_copy.get().raw() != list.get().raw() &&
                            numbers(*list_copy) == numbers(*list);
   lList *list_released = list_copy.release();
   const bool list_released_ok = !list_copy && list_released != nullptr;
   lFreeList(&list_released);
   list.reset();
   CHECK(1, "the owners create, read through, copy, release, reset and move an element and a list",
         created && cloned && released_ok && reset_ok && moved && named && list_cloned && list_released_ok &&
         !list);

   const bool refused =
      expect_reason("owners", "element", [] { static_cast<void>(ocs::tom::RawElementPtr::create(nullptr)); },
                    Reason::NullDescriptor) &&
      expect_reason("owners", "list", [] { static_cast<void>(ocs::tom::RawListPtr::create(nullptr)); },
                    Reason::NullDescriptor) &&
      expect_reason("owners", "free element", [] { static_cast<void>(ocs::tom::create_raw_elem(nullptr)); },
                    Reason::NullDescriptor) &&
      expect_reason("owners", "free list", [] { static_cast<void>(ocs::tom::create_raw_list(nullptr)); },
                    Reason::NullDescriptor);
   CHECK(2, "creating without a descriptor throws NullDescriptor", refused);
}

/**
 * @brief Typed to raw by move, raw to typed by adopt<T>(); a failed adopt leaves the source owning
 * @return nothing; CHECK records the results
 */
static void
test_typed_raw() {
   auto typed = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   typed->set<JB_job_number>(7);
   ocs::tom::RawElementPtr raw(std::move(typed));
   const bool to_raw = !typed && raw && raw->get<std::uint32_t>(JB_job_number) == 7;
   auto back = ocs::tom::adopt<ocs::tom::JB>(std::move(raw));
   const bool to_typed = !raw && back && back->get<JB_job_number>() == 7;
   auto typed_list = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs");
   typed_list->push_back(std::move(back));
   ocs::tom::RawListPtr raw_list(std::move(typed_list));
   const bool list_to_raw = !typed_list && raw_list && raw_list->size() == 1;
   auto typed_again = ocs::tom::adopt<ocs::tom::JB>(std::move(raw_list));
   const bool list_to_typed = !raw_list && typed_again && typed_again->size() == 1 &&
                              typed_again->front().get<JB_job_number>() == 7;
   CHECK(3, "a typed owner moves into a raw one and back through adopt<T>(), keeping its content",
         to_raw && to_typed && list_to_raw && list_to_typed);

   auto queue = ocs::tom::create_raw_elem(QU_Type);
   const lListElem *queue_raw = queue.get();
   auto queues = ocs::tom::create_raw_list(QU_Type);
   const lList *queues_raw = queues.get();
   const bool refused =
      expect_reason("adopt", "element",
                    [&] { static_cast<void>(ocs::tom::adopt<ocs::tom::JB>(std::move(queue))); }, Reason::WrongType) &&
      expect_reason("adopt", "list",
                    [&] { static_cast<void>(ocs::tom::adopt<ocs::tom::JB>(std::move(queues))); }, Reason::WrongType);
   const bool kept = queue && queue.get().raw() == queue_raw && queues && queues.get().raw() == queues_raw;
   const bool empty = !ocs::tom::adopt<ocs::tom::JB>(ocs::tom::RawElementPtr{}) &&
                      !ocs::tom::adopt<ocs::tom::JB>(ocs::tom::RawListPtr{});
   CHECK(4, "adopt<T>() of another type throws WrongType and leaves the source owning; an empty source gives an "
            "empty typed owner",
         refused && kept && empty);

   ocs::tom::RawWherePtr where(lWhere("%T(%I == %u)", JB_Type, JB_job_number, 7U));
   const lCondition *where_raw = where.get();
   ocs::tom::RawWhatPtr what(lWhat("%T(%I)", JB_Type, JB_job_number));
   const lEnumeration *what_raw = what.get();
   const bool selections_refused =
      expect_reason("adopt", "condition",
                    [&] { static_cast<void>(ocs::tom::adopt<ocs::tom::QU>(std::move(where))); },
                    Reason::ForeignField) &&
      expect_reason("adopt", "selection",
                    [&] { static_cast<void>(ocs::tom::adopt<ocs::tom::QU>(std::move(what))); },
                    Reason::ForeignField);
   const bool selections_kept = where.get() == where_raw && what.get() == what_raw;
   const auto typed_where = ocs::tom::adopt<ocs::tom::JB>(std::move(where));
   const auto typed_what = ocs::tom::adopt<ocs::tom::JB>(std::move(what));
   const bool selections_adopted = !where && !what && typed_where.get() == where_raw && typed_what.get() == what_raw;
   CHECK(5, "adopt<T>() of a condition or selection on another type throws ForeignField and leaves the source "
            "owning; one of the type is handed over",
         selections_refused && selections_kept && selections_adopted);
}

/**
 * @brief append, insert_after and their methods; every refused element stays with its owner
 * @return nothing; CHECK records the results
 */
static void
test_add() {
   auto list = ocs::tom::create_raw_list(TEST_Type);
   lList *lp = list.get();
   ocs::tom::append(lp, test_elem(2));
   lListElem *three = list->push_back(test_elem(3));
   ocs::tom::insert_after(lp, nullptr, test_elem(1));
   list->insert_after(ocs::tom::ConstRawElement(three), test_elem(5));
   ocs::tom::insert_after(lp, three, test_elem(4));
   list->insert_after(nullptr, test_elem(0));
   CHECK(6, "append(), insert_after() and the methods push_back(), insert_after() link the elements in order",
         (numbers(*list) == Numbers{0, 1, 2, 3, 4, 5}) && ocs::tom::size(lp) == 6 && list->size() == 6);

   lList *null_list = nullptr;
   auto spare = test_elem(91);
   auto queue = ocs::tom::create_raw_elem(QU_Type);
   auto reduced = ocs::tom::create_raw_elem(ocs::tom::layout<TEST_uint32>().get());
   ocs::tom::set(reduced.get(), TEST_uint32, std::uint32_t{90});
   auto other = ocs::tom::create_raw_list(TEST_Type);
   lListElem *stranger = other->push_back(test_elem(50));
   const bool refused =
      expect_reason("add", "null list", [&] { ocs::tom::append(null_list, std::move(spare)); }, Reason::NullList) &&
      expect_reason("add", "empty owner", [&] { ocs::tom::append(lp, ocs::tom::RawElementPtr{}); },
                    Reason::NullElement) &&
      expect_reason("add", "another type", [&] { ocs::tom::append(lp, std::move(queue)); }, Reason::WrongType) &&
      expect_reason("add", "method, another type", [&] { list->push_back(std::move(queue)); }, Reason::WrongType) &&
      expect_reason("add", "another layout", [&] { ocs::tom::append(lp, std::move(reduced)); },
                    Reason::WrongLayout) &&
      expect_reason("add", "insert after a stranger", [&] { ocs::tom::insert_after(lp, stranger, std::move(spare)); },
                    Reason::ForeignList) &&
      expect_reason("add", "method, insert after a stranger",
                    [&] { list->insert_after(ocs::tom::ConstRawElement(stranger), std::move(spare)); },
                    Reason::ForeignList);
   // an element of the list in an owner: refused, and it stays in the list
   ocs::tom::RawElementPtr bound(three);
   const bool bound_refused =
      expect_reason("add", "bound", [&] { ocs::tom::append(lp, std::move(bound)); }, Reason::BoundElement);
   const bool bound_kept = bound.get().raw() == three;
   static_cast<void>(bound.release());
   const bool kept = spare && queue && reduced && list->size() == 6 && other->size() == 1;
   CHECK(7, "a refused element stays with its owner: NullList, NullElement, WrongType, WrongLayout, ForeignList, "
            "BoundElement",
         refused && bound_refused && bound_kept && kept);
}

/**
 * @brief take, erase, splice and their methods
 * @return nothing; CHECK records the results
 */
static void
test_remove_and_splice() {
   auto list = ocs::tom::create_raw_list(TEST_Type);
   lList *lp = list.get();
   for (std::uint32_t i = 0; i < 6; i++) {
      list->push_back(test_elem(i));
   }
   auto two = ocs::tom::take(lp, ocs::tom::find_first<std::uint32_t>(lp, TEST_uint32, 2U));
   const bool took = two && two->get<std::uint32_t>(TEST_uint32) == 2 && (numbers(*list) == Numbers{0, 1, 3, 4, 5});
   ocs::tom::erase(lp, ocs::tom::find_first<std::uint32_t>(lp, TEST_uint32, 0U));
   auto five = list->take(list->find_first<std::uint32_t>(TEST_uint32, 5U));
   list->erase(list->find_first<std::uint32_t>(TEST_uint32, 1U));
   const bool removed = took && five && (numbers(*list) == Numbers{3, 4});
   // a free element goes into another list again
   ocs::tom::append(lp, std::move(two));
   lList *null_list = nullptr;
   auto other = ocs::tom::create_raw_list(TEST_Type);
   lListElem *stranger = other->push_back(test_elem(50));
   const bool refused =
      expect_reason("remove", "take a stranger", [&] { static_cast<void>(ocs::tom::take(lp, stranger)); },
                    Reason::ForeignList) &&
      expect_reason("remove", "erase a stranger", [&] { ocs::tom::erase(lp, stranger); }, Reason::ForeignList) &&
      expect_reason("remove", "method, take a stranger",
                    [&] { static_cast<void>(list->take(ocs::tom::RawElement(stranger))); }, Reason::ForeignList) &&
      expect_reason("remove", "take from null", [&] { static_cast<void>(ocs::tom::take(null_list, stranger)); },
                    Reason::NullList) &&
      expect_reason("remove", "erase from null", [&] { ocs::tom::erase(null_list, stranger); }, Reason::NullList);
   CHECK(8, "take(), erase() and the methods take an element out and own or free it; a stranger throws ForeignList, "
            "a null list NullList",
         removed && refused && (numbers(*list) == Numbers{3, 4, 2}) && other->size() == 1);

   auto dst = ocs::tom::create_raw_list(TEST_Type);
   dst->push_back(test_elem(1));
   auto src = ocs::tom::create_raw_list(TEST_Type);
   src->push_back(test_elem(2));
   src->push_back(test_elem(3));
   ocs::tom::splice(dst.get(), std::move(src));
   auto more = ocs::tom::create_raw_list(TEST_Type);
   more->push_back(test_elem(4));
   dst->splice(std::move(more));
   ocs::tom::splice(dst.get(), ocs::tom::RawListPtr{});
   const bool spliced = !src && !more && (numbers(*dst) == Numbers{1, 2, 3, 4});
   auto queues = ocs::tom::create_raw_list(QU_Type);
   queues->push_back(ocs::tom::create_raw_elem(QU_Type));
   auto reduced = ocs::tom::create_raw_list(ocs::tom::layout<TEST_uint32>().get());
   auto reduced_elem = ocs::tom::create_raw_elem(ocs::tom::layout<TEST_uint32>().get());
   reduced_elem->set(TEST_uint32, std::uint32_t{9});
   reduced->push_back(std::move(reduced_elem));
   auto spare = ocs::tom::create_raw_list(TEST_Type);
   const bool splice_refused =
      expect_reason("splice", "another type", [&] { ocs::tom::splice(dst.get(), std::move(queues)); },
                    Reason::WrongType) &&
      expect_reason("splice", "method, another type", [&] { dst->splice(std::move(queues)); }, Reason::WrongType) &&
      expect_reason("splice", "another layout", [&] { ocs::tom::splice(dst.get(), std::move(reduced)); },
                    Reason::WrongLayout) &&
      expect_reason("splice", "into null", [&] { ocs::tom::splice(null_list, std::move(spare)); }, Reason::NullList);
   const bool splice_kept = queues && queues->size() == 1 && reduced && reduced->size() == 1 && spare &&
                            dst->size() == 4;
   CHECK(9, "splice() and the method move every element and free the source; a refused list stays with its owner",
         spliced && splice_refused && splice_kept);
}

/**
 * @brief Walking a list as a range of raw handles, read-only and mutable
 * @return nothing; CHECK records the results
 */
static void
test_iteration() {
   const ocs::tom::RawListPtr owner(make_test_list(5, true));
   const Numbers seen = numbers(ocs::tom::ConstRawList(owner.get()));
   for (const auto ep : *owner) {
      ep.set(TEST_uint64, ep.get<std::uint64_t>(TEST_uint64) + 10);
   }
   bool written = true;
   const lList *clp = owner.get();
   for (const lListElem *ep = lFirst(clp); ep != nullptr; ep = lNext(ep)) {
      written = written && lGetUlong64(ep, TEST_uint64) == lGetUlong(ep, TEST_uint32) % 3 + 10;
   }
   const ocs::tom::ConstRawList empty;
   const bool none = empty.begin() == empty.end() && empty.empty() && ocs::tom::size(nullptr) == 0 &&
                     !empty.front() && std::distance(owner->begin(), owner->end()) == 5;
   CHECK(10, "a list walks as a range of read-only and of mutable handles; an empty handle is an empty range",
         (seen == Numbers{0, 1, 2, 3, 4}) && written && none);
}

/**
 * @brief find_first() finds what the old lookups find, with and without a hash table, in both forms
 * @return nothing; CHECK records the results
 */
static void
test_find_first() {
   bool found = true;
   for (const bool hash : {true, false}) {
      const ocs::tom::RawListPtr owner(make_test_list(40, hash));
      lList *lp = owner.get();
      const lList *clp = lp;
      const lListElem *by_number = lGetElemUlong(clp, TEST_uint32, 17);
      const lListElem *by_number64 = lGetElemUlong64(clp, TEST_uint64, 2);
      const lListElem *by_string = lGetElemStr(clp, TEST_string, "s3");
      const lListElem *by_host = lGetElemHost(clp, TEST_host, "h1");
      const bool ok = by_number != nullptr && by_number64 != nullptr && by_string != nullptr && by_host != nullptr &&
                      ocs::tom::find_first<std::uint32_t>(clp, TEST_uint32, 17U) == by_number &&
                      ocs::tom::find_first<std::uint64_t>(clp, TEST_uint64, std::uint64_t{2}) == by_number64 &&
                      ocs::tom::find_first(clp, TEST_string, "s3") == by_string &&
                      ocs::tom::find_first(clp, TEST_host, ocs::tom::Host("h1")) == by_host &&
                      ocs::tom::find_first<std::uint32_t>(clp, TEST_uint32, 99U) == nullptr &&
                      ocs::tom::find_first<std::uint32_t>(lp, TEST_uint32, 17U) == by_number &&
                      ocs::tom::ConstRawList(clp).find_first<std::uint32_t>(TEST_uint32, 17U).raw() == by_number &&
                      ocs::tom::RawList(lp).find_first(TEST_string, "s3").raw() == by_string &&
                      ocs::tom::RawList(lp).find_first(TEST_host, ocs::tom::Host("h1")).raw() == by_host;
      if (!ok) {
         std::printf("find_first() differs from the old lookups in a list %s hash tables\n",
                     hash ? "with" : "without");
      }
      found = found && ok;
   }
   CHECK(11, "find_first() and the method find the elements lGetElemUlong/Ulong64/Str/Host find, hashed or not",
         found);

   const ocs::tom::RawListPtr owner(make_test_list(5, true));
   const lList *clp = owner.get();
   const lList *null_list = nullptr;
   const auto reduced = ocs::tom::create_raw_list(ocs::tom::layout<TEST_uint32>().get());
   const lList *creduced = reduced.get();
   const bool refused =
      expect_reason("find", "wrong number type",
                    [&] { static_cast<void>(ocs::tom::find_first<std::uint64_t>(clp, TEST_uint32, std::uint64_t{1})); },
                    Reason::WrongType) &&
      expect_reason("find", "host for a string",
                    [&] { static_cast<void>(ocs::tom::find_first(clp, TEST_string, ocs::tom::Host("s1"))); },
                    Reason::WrongType) &&
      expect_reason("find", "foreign",
                    [&] { static_cast<void>(ocs::tom::find_first<std::uint32_t>(clp, JB_job_number, 1U)); },
                    Reason::ForeignField) &&
      expect_reason("find", "absent", [&] { static_cast<void>(ocs::tom::find_first(creduced, TEST_string, "s1")); },
                    Reason::AbsentField) &&
      expect_reason("find", "null key",
                    [&] {
                       static_cast<void>(ocs::tom::find_first(clp, TEST_string, static_cast<const char *>(nullptr)));
                    },
                    Reason::NullKey) &&
      expect_reason("find", "method, null key",
                    [&] {
                       static_cast<void>(
                          ocs::tom::ConstRawList(clp).find_first(TEST_string, static_cast<const char *>(nullptr)));
                    },
                    Reason::NullKey);
   const bool soft = ocs::tom::find_first<std::uint32_t>(null_list, TEST_uint32, 1U) == nullptr &&
                     !ocs::tom::ConstRawList().find_first<std::uint32_t>(TEST_uint32, 1U);
   CHECK(12, "find_first() throws WrongType, ForeignField, AbsentField and NullKey, and finds nothing in a null list",
         refused && soft);
}

/**
 * @brief select() selects what lSelect() selects; select of an element, matches() and clone of the selections
 * @return nothing; CHECK records the results
 */
static void
test_select() {
   const ocs::tom::RawListPtr owner(make_test_list(10, true));
   const lList *lp = owner.get();
   const ocs::tom::RawWherePtr where(lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 4U));
   const ocs::tom::RawWhatPtr what(lWhat("%T(%I%I)", TEST_Type, TEST_uint32, TEST_string));
   const ocs::tom::RawListPtr old(lSelect("selected", lp, where.get(), what.get()));
   const auto selected = ocs::tom::select(lp, where.get(), what.get());
   const auto by_method = ocs::tom::ConstRawList(lp).select(where.get(), what.get());
   const auto everything = ocs::tom::select(lp, nullptr, what.get());
   const ocs::tom::RawListPtr old_everything(lSelect("all", lp, nullptr, what.get()));
   const bool lists = same_selection(old.get(), *selected) && same_selection(old.get(), *by_method) &&
                      same_selection(old_everything.get(), *everything) && selected->size() == 5 &&
                      ocs::tom::has(selected->front(), TEST_string) && !ocs::tom::has(selected->front(), TEST_uint64) &&
                      !ocs::tom::select(static_cast<const lList *>(nullptr), where.get(), what.get());
   CHECK(13, "select() and the method select what lSelect() selects, with the same layout; a null list gives an "
            "empty owner",
         lists);

   const ocs::tom::ConstRawElement first = ocs::tom::ConstRawList(lp).front();
   const auto reduced = ocs::tom::select(first.raw(), what.get());
   const auto reduced_by_method = first.select(what.get());
   const bool element = reduced && ocs::tom::has(reduced.get(), TEST_uint32) &&
                        !ocs::tom::has(reduced.get(), TEST_uint64) && reduced_by_method &&
                        reduced->get<std::uint32_t>(TEST_uint32) == first.get<std::uint32_t>(TEST_uint32) &&
                        !ocs::tom::select(static_cast<const lListElem *>(nullptr), what.get());
   bool matches = true;
   for (const auto ep : ocs::tom::ConstRawList(lp)) {
      const bool expected = lCompare(ep, where.get()) != 0;
      matches = matches && ocs::tom::matches(ep, where.get()) == expected && ep.matches(where.get()) == expected;
   }
   const auto where_copy = ocs::tom::clone(where.get());
   const auto what_copy = ocs::tom::clone(what.get());
   const bool copies = where_copy && where_copy.get() != where.get() && what_copy && what_copy.get() != what.get() &&
                       ocs::tom::select(lp, where_copy.get(), what_copy.get())->size() == 5 &&
                       !ocs::tom::clone(static_cast<const lCondition *>(nullptr)) &&
                       !ocs::tom::clone(static_cast<const lEnumeration *>(nullptr));
   const bool refused = expect_reason("select", "matches on null",
                                      [&] {
                                         static_cast<void>(
                                            ocs::tom::matches(static_cast<const lListElem *>(nullptr), where.get()));
                                      },
                                      Reason::NullElement) &&
                        expect_reason("select", "no selection",
                                      [&] { static_cast<void>(ocs::tom::select(lp, where.get(), nullptr)); },
                                      Reason::NullElement);
   CHECK(14, "select() of an element reduces it, matches() decides as lCompare(), clone() copies conditions and "
             "selections",
         element && matches && copies && refused);
}

/**
 * @brief unpack_raw_list() reads a packed list of the type the buffer names
 * @return nothing; CHECK records the results
 */
static void
test_unpack() {
   auto jobs = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs");
   for (std::uint32_t i = 1; i <= 3; i++) {
      auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
      job->set<JB_job_number>(i);
      job->set<JB_owner>("owner");
      jobs->push_back(std::move(job));
   }
   ocs::tom::PackBuffer out;
   jobs->pack(out.raw());
   ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(out.data(), out.size());
   auto raw = ocs::tom::unpack_raw_list(&in.raw());
   const char *type = raw ? ocs::tom::type_name(raw->descr()) : nullptr;
   const bool unpacked = raw && type != nullptr && std::strcmp(type, "JB") == 0 &&
                         (numbers(*raw, JB_job_number) == Numbers{1, 2, 3});
   const auto typed = ocs::tom::adopt<ocs::tom::JB>(std::move(raw));
   CHECK(15, "unpack_raw_list() reads the packed job list as a list of jobs, which adopt<JB>() takes",
         unpacked && typed && typed->size() == 3);
}

/**
 * @brief A job in a typed owner, for the list ops on typed owners
 * @param number the value of JB_job_number
 * @return the typed owner of the job
 */
static ocs::tom::ElementPtr<ocs::tom::JB>
typed_job(const std::uint32_t number) {
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   job->set<JB_job_number>(number);
   return job;
}

/**
 * @brief The list ops and adopt<T>() on typed owners: a refused element or list stays in the typed owner
 *
 * The raw owners take a typed owner only explicitly; the ops have typed overloads, which must keep the
 * strong guarantee of the raw ops: a throw leaves the element or list in the caller's typed owner (no
 * temporary raw owner frees it), a success empties the owner.
 *
 * @return nothing; CHECK records the results
 */
static void
test_typed_owners() {
   // a job into a list of TEST: every op throws WrongType and the typed owner keeps its element
   auto tests = ocs::tom::create_raw_list(TEST_Type);
   lList *lp = tests.get();
   auto job = typed_job(1);
   const lListElem *job_raw = job.get().raw();
   auto jobs = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs");
   jobs->push_back(typed_job(2));
   const lList *jobs_raw = jobs.get().raw();
   const bool refused =
      expect_reason("typed", "append", [&] { ocs::tom::append(lp, std::move(job)); }, Reason::WrongType) &&
      expect_reason("typed", "insert_after", [&] { ocs::tom::insert_after(lp, nullptr, std::move(job)); },
                    Reason::WrongType) &&
      expect_reason("typed", "method push_back", [&] { tests->push_back(std::move(job)); }, Reason::WrongType) &&
      expect_reason("typed", "method insert_after", [&] { tests->insert_after(nullptr, std::move(job)); },
                    Reason::WrongType) &&
      expect_reason("typed", "splice", [&] { ocs::tom::splice(lp, std::move(jobs)); }, Reason::WrongType) &&
      expect_reason("typed", "method splice", [&] { tests->splice(std::move(jobs)); }, Reason::WrongType) &&
      expect_reason("typed", "adopt a job as a queue",
                    [&] { static_cast<void>(ocs::tom::adopt<ocs::tom::QU>(std::move(job))); }, Reason::WrongType) &&
      expect_reason("typed", "adopt a job list as queues",
                    [&] { static_cast<void>(ocs::tom::adopt<ocs::tom::QU>(std::move(jobs))); }, Reason::WrongType);
   const bool kept = job && job.get().raw() == job_raw && job->get<JB_job_number>() == 1 && jobs &&
                     jobs.get().raw() == jobs_raw && jobs->size() == 1 && tests->size() == 0;
   CHECK(16, "the list ops and adopt<T>() on a typed owner throw WrongType and leave the element or list in the "
             "typed owner",
         refused && kept);

   // a job into a list of jobs: every op succeeds and empties the typed owner
   auto raw_jobs = ocs::tom::create_raw_list(JB_Type);
   lList *jlp = raw_jobs.get();
   auto two = typed_job(2);
   ocs::tom::append(jlp, std::move(two));
   auto four = typed_job(4);
   lListElem *four_raw = raw_jobs->push_back(std::move(four));
   auto one = typed_job(1);
   ocs::tom::insert_after(jlp, nullptr, std::move(one));
   auto three = typed_job(3);
   ocs::tom::insert_after(jlp, ocs::tom::find_first<std::uint32_t>(jlp, JB_job_number, 2U), std::move(three));
   auto five = typed_job(5);
   raw_jobs->insert_after(ocs::tom::ConstRawElement(four_raw), std::move(five));
   auto six = ocs::tom::ListPtr<ocs::tom::JB>::create("six");
   six->push_back(typed_job(6));
   ocs::tom::splice(jlp, std::move(six));
   auto seven = ocs::tom::ListPtr<ocs::tom::JB>::create("seven");
   seven->push_back(typed_job(7));
   raw_jobs->splice(std::move(seven));
   auto eight = typed_job(8);
   const lListElem *eight_raw = eight.get().raw();
   const auto adopted = ocs::tom::adopt<ocs::tom::JB>(std::move(eight));
   const auto adopted_list = ocs::tom::adopt<ocs::tom::JB>(std::move(jobs));
   const bool emptied = !two && !four && !one && !three && !five && !six && !seven && !eight && !jobs;
   const bool linked = (numbers(*raw_jobs, JB_job_number) == Numbers{1, 2, 3, 4, 5, 6, 7});
   const bool handed_over = adopted && adopted.get().raw() == eight_raw && adopted_list &&
                            adopted_list.get().raw() == jobs_raw && adopted_list->size() == 1;
   CHECK(17, "the list ops and adopt<T>() on a typed owner of the right type link or hand over the element or "
             "list and empty the typed owner",
         emptied && linked && handed_over);
}

/**
 * @brief write_to() writes the bytes of lWriteListTo(), lWriteElemTo(), lWriteWhereTo() and lWriteWhatTo()
 *
 * Every argument form that the overloads take is used once: a list and an element pointer, the raw handles,
 * a raw owner dereferenced, typed list and element handles, a condition and a selection pointer.
 *
 * @return nothing; CHECK records the results
 */
static void
test_write_to() {
   const ocs::tom::RawListPtr owner(make_test_list(3, false));
   lList *lp = owner.get();
   lListElem *first = lFirstRW(lp);
   const ocs::tom::RawWherePtr where(
      lWhere("%T(%I > %u && %I == %s)", TEST_Type, TEST_uint32, 0U, TEST_string, "s2"));
   const ocs::tom::RawWhatPtr what(lWhat("%T(%I%I)", TEST_Type, TEST_uint32, TEST_string));
   auto jobs = ocs::tom::ListPtr<ocs::tom::JB>::create("jobs");
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   job->set<JB_job_number>(42);
   job->set<JB_owner>("owner");
   jobs->push_back(std::move(job));
   auto single = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   single->set<JB_job_number>(43);
   single->set<JB_owner>("single");
   const ocs::tom::ConstElement<ocs::tom::JB> single_const = *single;
   const lListElem *const_first = first;

   const auto old_list = written([&](FILE *fp) { lWriteListTo(lp, fp); });
   const auto old_elem = written([&](FILE *fp) { lWriteElemTo(first, fp); });
   const auto old_where = written([&](FILE *fp) { lWriteWhereTo(where.get(), fp); });
   const auto old_what = written([&](FILE *fp) { lWriteWhatTo(what.get(), fp); });
   const auto old_jobs = written([&](FILE *fp) { lWriteListTo(jobs.get().raw(), fp); });
   const auto old_single = written([&](FILE *fp) { lWriteElemTo(single.get().raw(), fp); });
   const bool list =
      old_list && !old_list->empty() && written([&](FILE *fp) { ocs::tom::write_to(fp, lp); }) == old_list &&
      written([&](FILE *fp) { ocs::tom::write_to(fp, ocs::tom::ConstRawList(lp)); }) == old_list &&
      written([&](FILE *fp) { ocs::tom::write_to(fp, *owner); }) == old_list && old_jobs && !old_jobs->empty() &&
      written([&](FILE *fp) { ocs::tom::write_to(fp, *jobs); }) == old_jobs;
   const bool elem =
      old_elem && !old_elem->empty() && written([&](FILE *fp) { ocs::tom::write_to(fp, first); }) == old_elem &&
      written([&](FILE *fp) { ocs::tom::write_to(fp, ocs::tom::RawElement(first)); }) == old_elem &&
      written([&](FILE *fp) { ocs::tom::write_to(fp, ocs::tom::ConstRawElement(const_first)); }) == old_elem &&
      old_single && !old_single->empty() &&
      written([&](FILE *fp) { ocs::tom::write_to(fp, *single); }) == old_single &&
      written([&](FILE *fp) { ocs::tom::write_to(fp, single_const); }) == old_single;
   const bool condition = old_where && !old_where->empty() &&
                          written([&](FILE *fp) { ocs::tom::write_to(fp, where.get()); }) == old_where;
   const bool selection = old_what && !old_what->empty() &&
                          written([&](FILE *fp) { ocs::tom::write_to(fp, what.get()); }) == old_what;
   CHECK(18, "write_to() writes the bytes of lWriteListTo(), lWriteElemTo(), lWriteWhereTo() and lWriteWhatTo() "
             "for every argument form",
         list && elem && condition && selection);

   // empty handles and null pointers: nothing written, the error number of the old functions
   bool empty = true;
   const auto same_as_old = [&empty](const char *step, auto &&old_write, auto &&new_write) {
      cull_state_set_lerrno(0);
      const auto old_text = written(old_write);
      const int old_errno = cull_state_get_lerrno();
      cull_state_set_lerrno(0);
      const auto new_text = written(new_write);
      const int new_errno = cull_state_get_lerrno();
      // no temporary file fails the case: two missing texts must not pass as two empty ones
      if (!old_text || !new_text || !old_text->empty() || !new_text->empty() || old_errno == 0 ||
          old_errno != new_errno) {
         std::printf("write_to, %s: old \"%s\" %d, new \"%s\" %d\n", step,
                     old_text ? old_text->c_str() : "(no file)", old_errno,
                     new_text ? new_text->c_str() : "(no file)", new_errno);
         empty = false;
      }
   };
   same_as_old("empty list", [](FILE *fp) { lWriteListTo(nullptr, fp); },
               [](FILE *fp) { ocs::tom::write_to(fp, ocs::tom::ConstRawList()); });
   same_as_old("empty element", [](FILE *fp) { lWriteElemTo(nullptr, fp); },
               [](FILE *fp) { ocs::tom::write_to(fp, ocs::tom::ConstRawElement()); });
   same_as_old("null condition", [](FILE *fp) { lWriteWhereTo(nullptr, fp); },
               [](FILE *fp) { ocs::tom::write_to(fp, static_cast<const lCondition *>(nullptr)); });
   same_as_old("null selection", [](FILE *fp) { lWriteWhatTo(nullptr, fp); },
               [](FILE *fp) { ocs::tom::write_to(fp, static_cast<const lEnumeration *>(nullptr)); });
   CHECK(19, "write_to() of an empty handle or a null pointer writes nothing and sets the error number of the "
             "old function",
         empty);
}

/**
 * @brief unpack_raw_elem() reads a packed element of the type the buffer names, and reports a cut-off buffer
 *        with the code cull_unpack_elem() returns
 * @return nothing; CHECK records the results
 */
static void
test_unpack_elem() {
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   job->set<JB_job_number>(7);
   job->set<JB_owner>("owner");
   ocs::tom::PackBuffer out;
   job->pack(out.raw());

   ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(out.data(), out.size());
   auto raw = ocs::tom::unpack_raw_elem(&in.raw(), nullptr);
   const char *type = raw ? ocs::tom::type_name(raw->descr()) : nullptr;
   const bool unpacked = raw && type != nullptr && std::strcmp(type, "JB") == 0 &&
                         raw->get<std::uint32_t>(JB_job_number) == 7 &&
                         std::strcmp(raw->get<const char *>(JB_owner), "owner") == 0;
   const auto typed = ocs::tom::adopt<ocs::tom::JB>(std::move(raw));

   ocs::tom::PackBuffer old_cut = ocs::tom::PackBuffer::reading(out.data(), out.size() / 2);
   lListElem *old_ep = nullptr;
   const int old_code = cull_unpack_elem(&old_cut.raw(), &old_ep, nullptr);
   lFreeElem(&old_ep);
   ocs::tom::PackBuffer cut = ocs::tom::PackBuffer::reading(out.data(), out.size() / 2);
   int code = PACK_SUCCESS;
   try {
      static_cast<void>(ocs::tom::unpack_raw_elem(&cut.raw(), nullptr));
   } catch (const ocs::tom::PackException &e) {
      code = e.code();
   }
   CHECK(20, "unpack_raw_elem() reads the packed job as a job, which adopt<JB>() takes; a cut-off buffer throws "
             "PackException with the code of cull_unpack_elem()",
         unpacked && typed && typed->get<JB_job_number>() == 7 && code != PACK_SUCCESS && code == old_code);
}

/**
 * @brief The error number a call leaves, after clearing it
 * @tparam Call the type of the call
 * @param call the call
 * @return the CULL error number afterwards
 */
template <typename Call>
static int
lerrno_after(Call &&call) {
   cull_state_set_lerrno(0);
   call();
   return cull_state_get_lerrno();
}

/**
 * @brief select_as() and select_elem_as() give what lSelect(), lSelectHashPack() and lSelectElemPack() give
 * @return nothing; CHECK records the results
 */
static void
test_select_as() {
   const ocs::tom::RawListPtr owner(make_test_list(10, true));
   const lList *lp = owner.get();
   const ocs::tom::RawWherePtr where(lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 4U));
   const ocs::tom::RawWherePtr none(lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 100U));
   const ocs::tom::RawWhatPtr what(lWhat("%T(%I%I)", TEST_Type, TEST_uint32, TEST_string));
   const ocs::tom::RawWhatPtr all(lWhat("%T(ALL)", TEST_Type));
   const ocs::tom::RawWhatPtr foreign(lWhat("%T(%I)", JB_Type, JB_job_number));

   bool lists = true;
   for (const bool hash : {true, false}) {
      const auto hashing = hash ? ocs::tom::Hashing::On : ocs::tom::Hashing::Off;
      // a given name, the default name, no match, every field, no selection (the condition not applied), a
      // foreign selection, a null list
      const lList *sources[] = {lp, lp, lp, lp, lp, lp, nullptr};
      const lCondition *conditions[] = {where.get(), where.get(), none.get(), where.get(), where.get(), nullptr,
                                        where.get()};
      const lEnumeration *selections[] = {what.get(), what.get(), what.get(), all.get(), nullptr, foreign.get(),
                                          what.get()};
      const char *names[] = {"selected", nullptr, "nothing", "all", "ignored", "foreign", "null list"};
      const char *labels[] = {"selected", "default name", "nothing", "all", "ignored", "foreign", "null list"};
      for (std::size_t i = 0; i < std::size(names); i++) {
         ocs::tom::RawListPtr old;
         ocs::tom::RawListPtr selected;
         const int old_errno = lerrno_after([&] {
            old = ocs::tom::RawListPtr(
               lSelectHashPack(names[i], sources[i], conditions[i], selections[i], hash, nullptr));
         });
         const int new_errno = lerrno_after(
            [&] { selected = ocs::tom::select_as(names[i], sources[i], conditions[i], selections[i], hashing); });
         if (old_errno != new_errno) {
            std::printf("%s: the error numbers differ (%d, %d)\n", labels[i], old_errno, new_errno);
         }
         lists = same_list(labels[i], old.get(), selected.get()) && old_errno == new_errno && lists;
      }
   }
   const ocs::tom::RawListPtr old_select(lSelect("selected", lp, where.get(), what.get()));
   const auto hashed = ocs::tom::select_as("selected", lp, where.get(), what.get(), ocs::tom::Hashing::On);
   const ocs::tom::RawListPtr old_nothing(lSelect("nothing", lp, none.get(), what.get()));
   const auto nothing = ocs::tom::select_as("nothing", lp, none.get(), what.get(), ocs::tom::Hashing::On);
   const auto empty = ocs::tom::select_as("nothing", lp, none.get(), what.get(), ocs::tom::Hashing::Off);
   const auto whole = ocs::tom::select_as("ignored", lp, where.get(), nullptr, ocs::tom::Hashing::Off);
   int foreign_errno[2] = {0, 0};
   bool foreign_refused = true;
   for (const bool hash : {true, false}) {
      const int errno_now = lerrno_after([&] {
         foreign_refused = !ocs::tom::select_as("", lp, nullptr, foreign.get(),
                                                hash ? ocs::tom::Hashing::On : ocs::tom::Hashing::Off) &&
                           foreign_refused;
      });
      foreign_errno[hash ? 0 : 1] = errno_now;
   }
   // the hash slots are compared with the old result by same_list()
   lists = lists && same_list("lSelect", old_select.get(), hashed.get()) && !old_nothing && !nothing && empty &&
           empty->size() == 0 && std::strcmp(ocs::tom::list_name(hashed.get()), "selected") == 0 && whole &&
           whole->size() == 10 && std::strcmp(ocs::tom::list_name(whole.get()), "test") == 0 && foreign_refused &&
           foreign_errno[0] != 0 && foreign_errno[1] != 0 &&
           !ocs::tom::select_as("", nullptr, where.get(), what.get(), ocs::tom::Hashing::On) &&
           !ocs::tom::select_as("", nullptr, where.get(), what.get(), ocs::tom::Hashing::Off);
   CHECK(21, "select_as() gives what lSelect() and lSelectHashPack() give, hashed and not: the given name, the "
             "same hash slots, no list for no match when hashed, every field without a selection, no list and the same "
             "error number for a foreign selection and a null list",
         lists);

   bool elements = true;
   const std::initializer_list<const lEnumeration *> element_selections{what.get(), all.get(), nullptr,
                                                                        foreign.get()};
   std::vector<const lListElem *> sources{nullptr};
   for (const auto ep : ocs::tom::ConstRawList(lp)) {
      sources.push_back(ep.raw());
   }
   for (const bool hash : {true, false}) {
      const auto hashing = hash ? ocs::tom::Hashing::On : ocs::tom::Hashing::Off;
      for (const lListElem *ep : sources) {
         for (const lEnumeration *selection : element_selections) {
            ocs::tom::RawElementPtr old;
            ocs::tom::RawElementPtr selected;
            const int old_errno = lerrno_after([&] {
               old = ocs::tom::RawElementPtr(lSelectElemPack(ep, where.get(), selection, hash, nullptr));
            });
            const int new_errno =
               lerrno_after([&] { selected = ocs::tom::select_elem_as(ep, where.get(), selection, hashing); });
            if (old_errno != new_errno) {
               std::printf("lSelectElemPack: the error numbers differ (%d, %d)\n", old_errno, new_errno);
            }
            elements = same_elem("lSelectElemPack", old.get(), selected.get()) && old_errno == new_errno &&
                       elements;
         }
      }
   }
   const ocs::tom::ConstRawElement first = ocs::tom::ConstRawList(lp).front();
   elements = elements && !ocs::tom::select_elem_as(first.raw(), where.get(), what.get(), ocs::tom::Hashing::Off) &&
              ocs::tom::select_elem_as(first.raw(), where.get(), nullptr, ocs::tom::Hashing::Off) &&
              !ocs::tom::select_elem_as(nullptr, where.get(), what.get(), ocs::tom::Hashing::Off) &&
              !ocs::tom::select_elem_as(first.raw(), nullptr, foreign.get(), ocs::tom::Hashing::Off);
   CHECK(22, "select_elem_as() gives what lSelectElemPack() gives, hashed and not, with the same error numbers: no "
             "copy for no match, a whole copy without a selection, no copy for a foreign selection or a null "
             "element",
         elements);
}

/**
 * @brief select_into_descr() and select_elem_into_descr() give what lSelectDPack() and lSelectElemDPack() give
 * @return nothing; CHECK records the results
 */
static void
test_select_into_descr() {
   const ocs::tom::RawListPtr owner(make_test_list(10, true));
   const lList *lp = owner.get();
   const ocs::tom::RawWherePtr where(lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 4U));
   const ocs::tom::RawWherePtr none(lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 100U));
   const ocs::tom::RawWhatPtr what(lWhat("%T(%I%I)", TEST_Type, TEST_uint32, TEST_string));
   // a selection with a field the reduced descriptor does not hold
   const ocs::tom::RawWhatPtr wider(lWhat("%T(%I%I)", TEST_Type, TEST_uint32, TEST_uint64));
   const lDescr *reduced = lGetReducedDescr(TEST_Type, what.get());

   bool lists = reduced != nullptr;
   for (const bool hash : {true, false}) {
      const auto hashing = hash ? ocs::tom::Hashing::On : ocs::tom::Hashing::Off;
      // a match, no match, no selection, no descriptor, a field the descriptor lacks, the default name, a null
      // list
      const lList *sources[] = {lp, lp, lp, lp, lp, lp, nullptr};
      const lCondition *conditions[] = {where.get(), none.get(), where.get(), where.get(), where.get(),
                                        where.get(), where.get()};
      const lEnumeration *selections[] = {what.get(), what.get(), nullptr, what.get(), wider.get(), what.get(),
                                          what.get()};
      const lDescr *descriptors[] = {reduced, reduced, reduced, nullptr, reduced, reduced, reduced};
      const char *names[] = {"updating list", "nothing", "no selection", "no descriptor", "lacking field", nullptr,
                             "null list"};
      const char *labels[] = {"updating list", "nothing", "no selection", "no descriptor", "lacking field",
                              "default name", "null list"};
      for (std::size_t i = 0; i < std::size(names); i++) {
         ocs::tom::RawListPtr old;
         ocs::tom::RawListPtr selected;
         const int old_errno = lerrno_after([&] {
            old = ocs::tom::RawListPtr(
               lSelectDPack(names[i], sources[i], conditions[i], descriptors[i], selections[i], hash, nullptr));
         });
         const int new_errno = lerrno_after([&] {
            selected = ocs::tom::select_into_descr(names[i], sources[i], conditions[i], descriptors[i],
                                                   selections[i], hashing);
         });
         if (old_errno != new_errno) {
            std::printf("%s: the error numbers differ (%d, %d)\n", labels[i], old_errno, new_errno);
         }
         lists = same_list(labels[i], old.get(), selected.get()) && old_errno == new_errno && lists;
      }
   }
   const auto updating = ocs::tom::select_into_descr("updating list", lp, where.get(), reduced, what.get(),
                                                     ocs::tom::Hashing::Off);
   const auto lacking = ocs::tom::select_into_descr("lacking field", lp, where.get(), reduced, wider.get(),
                                                    ocs::tom::Hashing::Off);
   lists = lists && updating && updating->size() == 5 &&
           std::strcmp(ocs::tom::list_name(updating.get()), "updating list") == 0 &&
           !ocs::tom::has(updating->front(), TEST_uint64) &&
           !ocs::tom::select_into_descr("", lp, none.get(), reduced, what.get(), ocs::tom::Hashing::On) &&
           !ocs::tom::select_into_descr("", nullptr, where.get(), reduced, what.get(), ocs::tom::Hashing::Off) &&
           lacking && lacking->size() == 0;
   CHECK(23, "select_into_descr() gives what lSelectDPack() gives, hashed and not: the given or default name and "
             "descriptor, no list for no match when hashed, no element without a selection or for a field the "
             "descriptor lacks, no list for a null list or descriptor, the same error numbers",
         lists);

   bool elements = reduced != nullptr;
   std::vector<const lListElem *> sources{nullptr};
   for (const auto ep : ocs::tom::ConstRawList(lp)) {
      sources.push_back(ep.raw());
   }
   const std::initializer_list<const lEnumeration *> element_selections{what.get(), nullptr, wider.get()};
   for (const lListElem *ep : sources) {
      for (const lEnumeration *selection : element_selections) {
         for (const lDescr *descr : std::initializer_list<const lDescr *>{reduced, nullptr}) {
            ocs::tom::RawElementPtr old;
            ocs::tom::RawElementPtr selected;
            const int old_errno = lerrno_after([&] {
               old = ocs::tom::RawElementPtr(lSelectElemDPack(ep, where.get(), descr, selection, false, nullptr));
            });
            const int new_errno = lerrno_after([&] {
               selected =
                  ocs::tom::select_elem_into_descr(ep, where.get(), descr, selection, ocs::tom::Hashing::Off);
            });
            if (old_errno != new_errno) {
               std::printf("lSelectElemDPack: the error numbers differ (%d, %d)\n", old_errno, new_errno);
            }
            elements =
               same_elem("lSelectElemDPack", old.get(), selected.get()) && old_errno == new_errno && elements;
         }
      }
   }
   const lListElem *last = nullptr;
   for (const auto ep : ocs::tom::ConstRawList(lp)) {
      last = ep.raw();
   }
   const auto copy = ocs::tom::select_elem_into_descr(last, where.get(), reduced, what.get(), ocs::tom::Hashing::Off);
   elements = elements && copy && copy->get<std::uint32_t>(TEST_uint32) == 9 &&
              !ocs::tom::has(copy.get(), TEST_uint64) &&
              !ocs::tom::select_elem_into_descr(last, where.get(), nullptr, what.get(), ocs::tom::Hashing::Off) &&
              !ocs::tom::select_elem_into_descr(last, where.get(), reduced, wider.get(), ocs::tom::Hashing::Off);
   CHECK(24, "select_elem_into_descr() gives what lSelectElemDPack() gives: the copy in the given descriptor, none "
             "for a null element, no match, no selection, no descriptor or a field the descriptor lacks, the same "
             "error numbers",
         elements);
   ocs_cull_descr_release(&reduced);
}

/**
 * @brief select_pack() writes the bytes lSelectHashPack() with a pack buffer writes
 * @return nothing; CHECK records the results
 */
static void
test_select_pack() {
   const ocs::tom::RawListPtr owner(make_test_list(10, false));
   const lList *lp = owner.get();
   const ocs::tom::RawWherePtr where(lWhere("%T(%I > %u)", TEST_Type, TEST_uint32, 4U));
   const ocs::tom::RawWhatPtr what(lWhat("%T(%I%I)", TEST_Type, TEST_uint32, TEST_string));
   const ocs::tom::RawWhatPtr all(lWhat("%T(ALL)", TEST_Type));
   const ocs::tom::RawWhatPtr nothing(lWhat("%T(NONE)", TEST_Type));

   // a reduction, every field, no field, no selection (the condition not applied); each also without condition,
   // with no list, and with the name of the list
   const lEnumeration *selections[] = {what.get(), all.get(), nothing.get(), nullptr};
   bool bytes = true;
   for (const lEnumeration *selection : selections) {
      for (const lCondition *condition : std::initializer_list<const lCondition *>{where.get(), nullptr}) {
         for (const lList *list : std::initializer_list<const lList *>{lp, nullptr}) {
            for (const char *name : std::initializer_list<const char *>{"", nullptr}) {
               const std::string old = packed([&](sge_pack_buffer *pb) {
                  static_cast<void>(lSelectHashPack(name, list, condition, selection, false, pb));
               });
               const std::string selected =
                  packed([&](sge_pack_buffer *pb) { ocs::tom::select_pack(pb, name, list, condition, selection); });
               if (old != selected || old.empty()) {
                  std::printf("select_pack: the bytes differ (%zu, %zu)\n", old.size(), selected.size());
                  bytes = false;
               }
            }
         }
      }
   }

   // the result reads back as the list the old one gave, named as given
   ocs::tom::PackBuffer out;
   ocs::tom::select_pack(&out.raw(), "sent", lp, where.get(), what.get());
   ocs::tom::PackBuffer in = ocs::tom::PackBuffer::reading(out.data(), out.size());
   const auto read = ocs::tom::unpack_raw_list(&in.raw());
   const bool unpacked = read && read->size() == 5 && std::strcmp(ocs::tom::list_name(read.get()), "sent") == 0 &&
                         (numbers(*read) == Numbers{5, 6, 7, 8, 9}) && !ocs::tom::has(read->front(), TEST_uint64);
   CHECK(25, "select_pack() writes the bytes of lSelectHashPack() with a pack buffer for a reduction, every field, "
             "no field and no selection, with and without condition, list and name; they read back as the selection",
         bytes && unpacked);
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
      {"RAW-OPS-01", "owners",
       "RawElementPtr and RawListPtr create, read through, copy, release, reset and move; no descriptor is refused",
       {"create an element and a list, read and write through them, clone, release, reset, move",
        "create without a descriptor"},
       "every step as specified; NullDescriptor", {}, {1, 2}, test_owners},
      {"RAW-OPS-02", "typed and raw",
       "a typed owner becomes a raw one by move and comes back through adopt<T>(), which checks first",
       {"move a typed job and job list into raw owners and adopt them back",
        "adopt a queue and a queue list as jobs, and empty owners",
        "adopt a job condition and selection as queue and as job"},
       "content kept; WrongType and ForeignField leave the source owning; empty gives empty", {}, {3, 4, 5},
       test_typed_raw},
      {"RAW-OPS-03", "add",
       "append(), insert_after() and the methods link elements; a refused element stays with its owner",
       {"build a list of six elements at the end, the front and in the middle",
        "add to a null list, an empty owner, an element of another type and layout, after a stranger, a bound "
        "element"},
       "the order 0..5; NullList, NullElement, WrongType, WrongLayout, ForeignList, BoundElement; nothing lost",
       {}, {6, 7}, test_add},
      {"RAW-OPS-04", "remove and splice",
       "take(), erase(), splice() and the methods; a refused list stays with its owner",
       {"take and erase in both forms, take a stranger and from a null list",
        "splice two lists in both forms, a list of another type and layout, into a null list"},
       "the elements left in order; ForeignList, NullList; spliced in order; WrongType, WrongLayout, NullList",
       {}, {8, 9}, test_remove_and_splice},
      {"RAW-OPS-05", "iteration", "a list walks as a range of raw handles",
       {"walk a list read-only and mutably, change a field through the mutable handles", "walk an empty handle"},
       "the elements in order, the change visible to the old interface, an empty range", {}, {10}, test_iteration},
      {"RAW-OPS-06", "find_first",
       "find_first() finds what the old lookups find, with and without hash tables, and reports misuse",
       {"look up every key type in lists of 40 with and without hash tables, in both forms",
        "look up with a wrong key type, a foreign and an absent field, a null key, in a null list"},
       "the same elements as lGetElemUlong/Ulong64/Str/Host; WrongType, ForeignField, AbsentField, NullKey; none",
       {}, {11, 12}, test_find_first},
      {"RAW-OPS-07", "select",
       "select() selects what lSelect() selects; element selection, matches() and clone() of the selections",
       {"select with and without a condition in both forms and compare with lSelect()",
        "select an element, compare matches() with lCompare(), clone the condition and selection"},
       "the same elements and layout; the reduced element; the same decisions; independent copies", {}, {13, 14},
       test_select},
      {"RAW-OPS-08", "unpack", "unpack_raw_list() reads a list of the type the buffer names",
       {"pack a typed job list, unpack it raw, adopt it as jobs"}, "a job list with the same numbers", {}, {15},
       test_unpack},
      {"RAW-OPS-09", "typed owners",
       "append(), insert_after(), splice(), the methods push_back(), insert_after(), splice() and adopt<T>() take "
       "a typed owner with the strong guarantee",
       {"hand a typed job and job list to every op on a list of TEST and to adopt<QU>()",
        "hand typed jobs and job lists to every op on a list of jobs and to adopt<JB>()"},
       "WrongType, the element or list still in the typed owner; linked in order, the typed owners empty", {},
       {16, 17}, test_typed_owners},
      {"RAW-OPS-10", "write_to",
       "write_to() writes lists, elements, conditions and selections as the old lWrite*To() functions do",
       {"write a list, an element, a condition and a selection in every argument form and through the old "
        "functions",
        "write empty handles and null pointers in both forms"},
       "the same bytes; nothing written and the same error number", {}, {18, 19}, test_write_to},
      {"RAW-OPS-11", "unpack element", "unpack_raw_elem() reads an element of the type the buffer names",
       {"pack a typed job, unpack it raw, adopt it as a job", "unpack a cut-off buffer in both forms"},
       "a job with the same fields; PackException with the code of cull_unpack_elem()", {}, {20},
       test_unpack_elem},
      {"RAW-OPS-12", "select_as",
       "select_as() and select_elem_as() give what lSelect(), lSelectHashPack() and lSelectElemPack() give",
       {"select a list with a name, no name, no match, every field, no selection, a foreign selection, a null "
        "list, hashed and not, and compare with the old functions",
        "select each element with a selection, every field and none, and compare with lSelectElemPack()"},
       "the same lists and elements with the same names and error numbers; no list where the old function gave "
       "none",
       {}, {21, 22}, test_select_as},
      {"RAW-OPS-13", "select_into_descr",
       "select_into_descr() and select_elem_into_descr() give what lSelectDPack() and lSelectElemDPack() give",
       {"select a list into a reduced descriptor with a match, no match, no selection, no descriptor, hashed and "
        "not, and compare with lSelectDPack()",
        "select each element into the reduced descriptor with and without a selection, and compare with "
        "lSelectElemDPack()"},
       "the same lists and elements with the same names and error numbers; no list where the old function gave "
       "none",
       {}, {23, 24}, test_select_into_descr},
      {"RAW-OPS-14", "select_pack",
       "select_pack() writes the bytes lSelectHashPack() with a pack buffer writes",
       {"pack a reduction, every field, no field and no selection, with and without condition, list and name, "
        "through both", "unpack the result of select_pack()"},
       "the same bytes; the selection read back with the given name", {}, {25}, test_select_pack},
   });
}
