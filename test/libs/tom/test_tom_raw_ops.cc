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

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "cull/cull_test_api.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_Pack.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"
#include "tom/cull_hash.h"
#include "tom/cull_listP.h"
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
      for (const ocs::tom::ConstRawElement ep : ocs::tom::ConstRawList{old}) {
         const auto other = *it;
         if (lGetUlong(ep, TEST_uint32) != other.get<std::uint32_t>(TEST_uint32) ||
             std::strcmp(lGetString(ep, TEST_string), other.get<const char *>(TEST_string)) != 0) {
            std::printf("the selections differ in element %u\n", lGetUlong(ep, TEST_uint32));
            return false;
         }
         ++it;
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
      ocs::tom::ConstRawElement other = ocs::tom::ConstRawList{selected}.front();
      for (ocs::tom::ConstRawElement ep = ocs::tom::ConstRawList{old}.front(); ep;
           ep = ep.next(), other = other.next()) {
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
   for (const ocs::tom::ConstRawElement ep : ocs::tom::ConstRawList{clp}) {
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
   lListElem *first = ocs::tom::RawList{lp}.front();
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
 * @brief remaining() counts the elements after one as lGetNumberOfRemainingElem() did
 * @return nothing; CHECK records the results
 */
static void
test_remaining() {
   const ocs::tom::RawListPtr list(make_test_list(5, false));
   bool same = true;
   std::size_t expected = 4;
   for (const ocs::tom::ConstRawElement ep : *list) {
      const std::size_t n = ocs::tom::remaining(ep);
      same = same && n == expected && n == lGetNumberOfRemainingElem(ep.raw());
      expected--;
   }
   const auto free_elem = test_elem(1);
   CHECK(26, "remaining() counts the successors of every element as lGetNumberOfRemainingElem(); none for the "
             "last, a free element and an empty handle",
         same && ocs::tom::remaining(free_elem.get()) == 0 &&
               ocs::tom::remaining(ocs::tom::ConstRawElement(static_cast<const lListElem *>(nullptr))) == 0);
}

/**
 * @brief is_free() tells a free element from a bound one as lIsFreeElem() did
 * @return nothing; CHECK records the results
 */
static void
test_is_free() {
   const ocs::tom::RawListPtr list(make_test_list(2, false));
   const lListElem *in_list = list->front().raw();

   auto holder = test_elem(1);
   holder->set(TEST_obj_any, test_elem(2));
   const lListElem *sub_object = holder->get<const lListElem *>(TEST_obj_any);

   const auto free_elem = test_elem(3);
   const lListElem *free_raw = free_elem.get().raw();

   const bool same = ocs::tom::is_free(free_raw) == lIsFreeElem(free_raw) &&
                     ocs::tom::is_free(in_list) == lIsFreeElem(in_list) &&
                     ocs::tom::is_free(sub_object) == lIsFreeElem(sub_object);
   CHECK(27, "is_free() is true for a free element and false for one in a list, a sub-object and an empty handle, "
             "as lIsFreeElem()",
         same && ocs::tom::is_free(free_raw) && !ocs::tom::is_free(in_list) && !ocs::tom::is_free(sub_object) &&
               !ocs::tom::is_free(ocs::tom::ConstRawElement(static_cast<const lListElem *>(nullptr))));
}

/**
 * @brief release_descr() gives up one reference on a shared descriptor and clears the holder
 * @return nothing; CHECK records the results
 */
static void
test_release_descr() {
   const ocs::tom::RawWhatPtr what(lWhat("%T(%I%I)", TEST_Type, TEST_uint32, TEST_string));
   const lDescr *reduced = ocs::tom::reduced_descr(TEST_Type, what.get());
   const bool counted = reduced != nullptr && ocs_cull_descr_is_counted(reduced);
   const lDescr *second = ocs_cull_descr_ref(reduced);
   const std::uint32_t refs = ocs_cull_descr_refs(reduced);

   ocs::tom::release_descr(second);
   const bool one_less = second == nullptr && ocs_cull_descr_refs(reduced) == refs - 1;

   const lDescr *static_descr = TEST_Type;
   ocs::tom::release_descr(static_descr);
   const lDescr *none = nullptr;
   ocs::tom::release_descr(none);
   ocs::tom::release_descr(reduced);

   CHECK(28, "release_descr() gives up one reference of a counted descriptor and clears the holder; a static "
             "descriptor and nullptr only clear it",
         counted && refs >= 2 && one_less && reduced == nullptr && static_descr == nullptr && none == nullptr);
}

/**
 * @brief make_hashable() lets a list created without hashing keep hash tables, as cull_hash_create_hashtables() did
 * @return nothing; CHECK records the results
 */
static void
test_make_hashable() {
   const ocs::tom::RawListPtr list(make_test_list(40, false));
   const ocs::tom::RawListPtr old_list(make_test_list(40, false));
   const bool unhashed = list.get().raw()->nslots == 0;
   ocs::tom::make_hashable(list.get());
   cull_hash_create_hashtables(old_list.get().raw());
   const std::uint32_t slots = list.get().raw()->nslots;

   // a hashable list stays as it is, an empty handle is ignored
   ocs::tom::make_hashable(list.get());
   ocs::tom::make_hashable(ocs::tom::RawList(static_cast<lList *>(nullptr)));

   const auto found = list->find_first(TEST_uint32, std::uint32_t{17});
   CHECK(29, "make_hashable() gives a list without hashing the slots cull_hash_create_hashtables() gives it; a "
             "hashable list and an empty handle are left as they are; lookups find as before",
         unhashed && slots > 0 && slots == old_list.get().raw()->nslots && list.get().raw()->nslots == slots &&
               found && found.get<std::uint32_t>(TEST_uint32) == 17);
}

/**
 * @brief free_raw_list() and free_raw_elem() free what a raw holder keeps and clear the holder
 *
 * The abort on a corrupted list cannot be checked here; that the list and the element are freed is shown by the
 * leak checker.
 *
 * @return nothing; CHECK records the results
 */
static void
test_free_raw() {
   lList *lp = make_test_list(3, true);
   lListElem *ep = test_elem(1).release();
   ocs::tom::free_raw_list(lp);
   ocs::tom::free_raw_elem(ep);

   lList *no_list = nullptr;
   lListElem *no_elem = nullptr;
   ocs::tom::free_raw_list(no_list);
   ocs::tom::free_raw_elem(no_elem);

   CHECK(30, "free_raw_list() and free_raw_elem() free a list and an element and clear the holder; nullptr frees "
             "nothing",
         lp == nullptr && ep == nullptr && no_list == nullptr && no_elem == nullptr);
}

/**
 * @brief descr_registry_shared() gives what ocs_cull_descr_registry_shared() gives
 * @return nothing; CHECK records the results
 */
static void
test_descr_registry_shared() {
   std::string own;
   std::string seen;
   const bool shared = ocs::tom::descr_registry_shared(RTLD_DEFAULT, own, seen);
   std::string old_own;
   std::string old_seen;
   const bool old_shared = ocs_cull_descr_registry_shared(RTLD_DEFAULT, &old_own, &old_seen);
   CHECK(31, "descr_registry_shared() gives the result and the object names of ocs_cull_descr_registry_shared() "
             "for the objects of this process",
         shared == old_shared && own == old_own && seen == old_seen && !own.empty() && !seen.empty());
}

/**
 * @brief Run the scenarios
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
/**
 * @brief The typed whole-object operations: dump and undump, element files, layouts and reductions, splitting,
 *        extra hash tables, packing and peeking, and the helpers on whole lists (CS-3077, OPS-01)
 *
 * Each scenario works on typed handles only and checks fixed expected values. Where a value is derived from the
 * format the core writes, a comment says so.
 */
namespace typed_ops {

   using ocs::tom::ConstElement;
   using ocs::tom::ConstList;
   using ocs::tom::EH;
   using ocs::tom::Element;
   using ocs::tom::ElementPtr;
   using ocs::tom::FieldException;
   using ocs::tom::Hashing;
   using ocs::tom::Host;
   using ocs::tom::JB;
   using ocs::tom::List;
   using ocs::tom::ListCoreError;
   using ocs::tom::ListPtr;
   using ocs::tom::NoneName;
   using ocs::tom::PackBuffer;
   using ocs::tom::PackException;
   using ocs::tom::ST;
   using ocs::tom::STN;
   using ocs::tom::TEST;
   using ocs::tom::UE;
   using ocs::tom::US;
   using ocs::tom::What;
   using ocs::tom::Where;
   using ocs::tom::field;
   using ocs::tom::layout;
   using ocs::tom::what;

   /** @brief The number of jobs the dump and pack scenarios write */
   constexpr std::uint32_t Jobs = 100;

   /** @brief A job reduced to plain values */
   struct Plain {
      std::uint32_t number;   ///< JB_job_number
      std::string owner;      ///< JB_owner, empty if not set

      /**
       * @brief Equal if number and owner are equal
       * @param other the other job
       * @return true if equal
       */
      bool operator==(const Plain &other) const = default;
   };

   /**
    * @brief The owner of a job
    * @param number the job number
    * @return the owner
    */
   std::string owner_of(const std::uint32_t number) {
      return "user" + std::to_string(number % 7);
   }

   /**
    * @brief The jobs of a list as plain values
    * @param jobs the list
    * @return the values
    */
   std::vector<Plain> plain(const ListPtr<JB> &jobs) {
      std::vector<Plain> values;
      for (const ConstElement<JB> job : *jobs) {
         const char *owner = job.get<JB_owner>();
         values.push_back({job.get<JB_job_number>(), owner != nullptr ? owner : ""});
      }
      return values;
   }

   /**
    * @brief The jobs 1 .. Jobs, each owned by owner_of() its number
    * @return the list
    */
   ListPtr<JB> owned_jobs() {
      ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
      for (std::uint32_t number = 1; number <= Jobs; number++) {
         jobs->emplace_back<JB_job_number>(number).set<JB_owner>(owner_of(number).c_str());
      }
      return jobs;
   }

   /**
    * @brief The plain values owned_jobs() holds
    * @return the values
    */
   std::vector<Plain> expected_jobs() {
      std::vector<Plain> values;
      for (std::uint32_t number = 1; number <= Jobs; number++) {
         values.push_back({number, owner_of(number)});
      }
      return values;
   }

   /**
    * @brief The text a writer writes into a stream
    * @param write the writer
    * @return the text
    */
   template <typename Write>
   std::string text_of(Write write) {
      char *buf = nullptr;
      size_t size = 0;
      FILE *fp = open_memstream(&buf, &size);
      write(fp);
      fclose(fp);
      std::string text(buf, size);
      free(buf);
      return text;
   }

   /**
    * @brief Read from a text as from a file
    * @param text the text
    * @param read the reader
    */
   template <typename Read>
   void from_text(const std::string &text, Read read) {
      FILE *fp = fmemopen(const_cast<char *>(text.data()), text.size(), "r");
      read(fp);
      fclose(fp);
   }

   /**
    * @brief Whether a function throws an exception of a given type
    * @param call the call
    * @return true if it threw one
    */
   template <typename E, typename Call>
   bool throws(Call call) {
      try {
         call();
      } catch (const E &) {
         return true;
      }
      return false;
   }

   /**
    * @brief Whether a function throws a FieldException with a given reason
    * @param call the call
    * @param reason the expected reason
    * @return true if it threw with that reason
    */
   template <typename Call>
   bool throws_reason(Call call, const FieldException::Reason reason) {
      try {
         call();
      } catch (const FieldException &e) {
         return e.reason() == reason;
      }
      return false;
   }

   /**
    * @brief Whether a function throws a PackException with a given code
    * @param call the call
    * @param code the expected code
    * @return true if it threw with that code
    */
   template <typename Call>
   bool throws_pack(Call call, const int code) {
      try {
         call();
      } catch (const PackException &e) {
         return e.code() == code;
      }
      return false;
   }

   /**
    * @brief Whether a text starts with a prefix and ends with a suffix
    * @param text the text
    * @param prefix the prefix
    * @param suffix the suffix
    * @return true if both hold
    */
   bool framed(const std::string &text, const std::string &prefix, const std::string &suffix) {
      return text.size() >= prefix.size() + suffix.size() && text.compare(0, prefix.size(), prefix) == 0 &&
             text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
   }

   /**
    * @brief DUMP-01: dump a job list as text and read it back
    * @return nothing; CHECK records the results
    */
   void test_dump_list() {
      const ListPtr<JB> jobs = owned_jobs();
      const std::string text = text_of([&](FILE *fp) { jobs->dump(fp); });
      std::vector<Plain> values;
      std::string again;
      from_text(text, [&](FILE *fp) {
         const ListPtr<JB> back = ListPtr<JB>::undump(fp);
         values = plain(back);
         again = text_of([&](FILE *out) { back->dump(out); });
      });
      // the frame write_list() writes: the list header with name and count, the descriptor, the elements
      const bool frame = framed(text,
                                "{ /* LIST BEGIN */\n/* LISTNAME               */ \"jobs\"\n"
                                "/* NUMBER OF ELEMENTS     */ 100\n{ /* DESCR BEGIN */\n",
                                "} /* LIST END */\n");
      CHECK(32, "dump() writes a list framed by its name and count, undump() reads the jobs back, and the jobs read "
                "back dump to the same text",
            frame && values == expected_jobs() && again == text);
   }

   /**
    * @brief DUMP-02: dump one element as text and read it back
    *
    * A quote inside a string is escaped. A quote at the start or the end of a
    * string, and one of two quotes in a row, is lost: the dump splits the
    * string at its quotes with sge_strtok(), which drops empty pieces.
    *
    * @return nothing; CHECK records the results
    */
   void test_dump_element() {
      ElementPtr<JB> job = ElementPtr<JB>::create();
      job->set<JB_job_number>(42);
      job->set<JB_owner>("say \"hi\" now");
      const std::string text = text_of([&](FILE *fp) { job->dump(fp); });
      std::string owner;
      std::uint32_t number = 0;
      from_text(text, [&](FILE *fp) {
         ElementPtr<JB> back = ElementPtr<JB>::undump(fp);
         number = back->get<JB_job_number>();
         owner = back->get<JB_owner>();
      });
      // the lines write_elem() writes: the field name padded to 20 columns, a string quoted with its quotes escaped
      const bool lines = framed(text, "{ \n", "}\n") &&
                         text.find("/* JB_job_number        */ 42\n") != std::string::npos &&
                         text.find("/* JB_owner             */ \"say \\\"hi\\\" now\"\n") != std::string::npos;
      CHECK(33, "an element dumps with its values, quotes escaped, and reads back with them",
            lines && number == 42 && owner == "say \"hi\" now");
   }

   /**
    * @brief DUMP-03: write a job into a file and read it back
    * @return nothing; CHECK records the results
    */
   void test_dump_file() {
      char dir[] = "/tmp/test_tom_raw_ops_XXXXXX";
      const bool made = mkdtemp(dir) != nullptr;
      ElementPtr<JB> job = ElementPtr<JB>::create();
      job->set<JB_job_number>(7);
      job->set<JB_owner>("alice");
      job->write_to_disk(dir, "typed", "job");
      const ElementPtr<JB> back = ElementPtr<JB>::read_from_disk(dir, "typed", "job");
      const bool read = back->get<JB_job_number>() == 7 && std::string(back->get<JB_owner>()) == "alice";
      ElementPtr<US> set = ElementPtr<US>::create();
      set->set<US_name>("staff");
      set->write_to_disk(dir, "set", "userset");
      const bool wrong_type = throws<FieldException>([&] {
         static_cast<void>(ElementPtr<JB>::read_from_disk(dir, "set", "job"));
      });
      const bool missing = throws<ocs::RuntimeException>([&] {
         static_cast<void>(ElementPtr<JB>::read_from_disk(dir, "missing", "job"));
      });
      for (const char *name : {"typed", "set"}) {
         unlink((std::string(dir) + "/" + name).c_str());
      }
      rmdir(dir);
      CHECK(34, "write_to_disk() and read_from_disk() carry a job through a file; a file of another type and a "
                "missing file throw",
            made && read && wrong_type && missing);
   }

   /**
    * @brief DUMP-04: errors: a cut text, a list of another type
    * @return nothing; CHECK records the results
    */
   void test_dump_errors() {
      const std::string text = text_of([](FILE *fp) { owned_jobs()->dump(fp); });
      const std::string cut = text.substr(0, text.size() / 2);
      bool cut_throws = false;
      from_text(cut, [&](FILE *fp) {
         cut_throws = throws<ListCoreError>([&] { static_cast<void>(ListPtr<JB>::undump(fp)); });
      });
      bool wrong_type = false;
      from_text(text, [&](FILE *fp) {
         wrong_type = throws<FieldException>([&] { static_cast<void>(ListPtr<US>::undump(fp)); });
      });
      CHECK(35, "a cut text throws ListCoreError; a list of another type throws WrongType", cut_throws && wrong_type);
   }

   /** @brief The user sets: name, type and entries; the last has no entries */
   constexpr const char *Sets[][3] = {{"acl", "alice", "bob"}, {"dept", "carol", nullptr}, {"empty", nullptr, nullptr}};

   /**
    * @brief The user sets of #Sets with the types 1, 2, 3
    * @return the owning handle
    */
   ListPtr<US> user_sets() {
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
    * @param us the user set, or an empty handle
    * @return the text, "none" for an empty handle
    */
   std::string set_text(const ConstElement<US> us) {
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
    * @brief The name and the keys of a list of TEST elements, in their order
    * @param rows the list, or an empty handle
    * @return "name:key,key,", "none" for an empty handle
    */
   std::string row_keys(const ConstList<TEST> rows) {
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
    * @return the owning handle
    */
   ListPtr<TEST> keyed_rows(const int n) {
      ListPtr<TEST> rows = ListPtr<TEST>::create("rows");
      for (int i = 1; i <= n; i++) {
         const Element<TEST> row = rows->emplace_back<TEST_uint32>(static_cast<std::uint32_t>(i));
         row.set<TEST_string>(("s" + std::to_string(i)).c_str());
      }
      return rows;
   }

   /**
    * @brief Whether a list holds a built hash table on a field
    * @param lp the list, or nullptr
    * @param nm the field
    * @return true if the table on the field is built; false for nullptr, a field the list does not hash or a
    *         table not built yet
    */
   bool has_table(const lList *lp, const int nm) {
      if (lp == nullptr) {
         return false;
      }
      const int pos = ocs::tom::field_pos(lp->descr, nm);
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
    * @return the owning handle
    */
   ListPtr<US> named_sets(const int n) {
      ListPtr<US> sets = ListPtr<US>::create("named");
      for (int i = 1; i <= n; i++) {
         sets->emplace_back<US_name>(set_name(i).c_str());
      }
      return sets;
   }

   /**
    * @brief Look up a numbered user set by its hashed name
    * @param sets the list
    * @param i the number of the set
    * @return "found" or "lost"
    */
   std::string lookup(const ConstList<US> sets, const int i) {
      return sets.find_first<US_name>(set_name(i).c_str()) ? "found" : "lost";
   }

   /**
    * @brief LAYOUT-01: lists and elements in the layout of a reduced list or element
    * @return nothing; CHECK records the results
    */
   void test_create_like() {
      std::vector<std::string> seen;
      const ListPtr<US> sets = user_sets();
      const ListPtr<US> reduced = sets->select(what<US_name>(), "reduced");
      auto like_list = ListPtr<US>::create_like("like list", *reduced);
      auto like_elem = ListPtr<US>::create_like("like elem", reduced->front());
      like_list->push_back(ElementPtr<US>::create_like(*reduced)).set<US_name>("new");
      like_elem->push_back(ElementPtr<US>::create_like(reduced->front())).set<US_name>("other");
      // a reduced element moves into a list of its layout
      like_list->push_back(reduced->extract(reduced->front()));
      for (const ConstElement<US> us : *like_list) {
         seen.push_back(set_text(us));
      }
      seen.push_back(set_text(like_elem->front()));
      seen.push_back(throws_reason([] { static_cast<void>(ListPtr<US>::create_like("null", List<US>())); },
                                   FieldException::Reason::NullDescriptor) &&
                           throws_reason([] { static_cast<void>(ElementPtr<US>::create_like(ConstElement<US>())); },
                                         FieldException::Reason::NullDescriptor)
                        ? "refused"
                        : "made");
      seen.emplace_back(ocs::tom::same_layout(like_list->descr(), reduced->descr()) &&
                              ocs::tom::same_layout(like_elem->descr(), reduced->descr())
                           ? "same"
                           : "other");
      CHECK(36, "create_like() makes lists and elements in the layout of a reduced list or element; an empty handle "
                "throws NullDescriptor",
            seen == std::vector<std::string>({"new:", "acl:", "other:", "refused", "same"}));
   }

   /**
    * @brief LAYOUT-02: a layout from fields known at compile time, and one from a selection known at run time
    * @return nothing; CHECK records the results
    */
   void test_layout() {
      std::vector<std::string> seen;
      auto lp = ListPtr<US>::create("by fields", layout<US_type, US_name>());
      lp->push_back(ElementPtr<US>::create(layout<US_name, US_type>())).set<US_name>("fields");
      seen.push_back(set_text(lp->front()));
      // the order of the fields does not matter, and every call gives the descriptor built once
      const bool once = layout<US_type, US_name>().get() == layout<US_type, US_name>().get() &&
                        ocs::tom::same_layout(layout<US_type, US_name>().get(), layout<US_name, US_type>().get());
      const auto selection = what<US_name, US_fshare>();
      auto by_what = ListPtr<US>::create("by selection", selection);
      by_what->push_back(ElementPtr<US>::create(selection)).set<US_name>("selection");
      seen.push_back(set_text(by_what->front()));
      seen.emplace_back(throws_reason([] { static_cast<void>(ElementPtr<US>::create(What<US>())); },
                                      FieldException::Reason::NullElement) &&
                              throws_reason([] { static_cast<void>(ListPtr<US>::create("none", What<US>())); },
                                            FieldException::Reason::NullElement)
                           ? "refused"
                           : "made");
      CHECK(37, "layout<F...>() and create(what) make reduced layouts holding just the fields named; an empty "
                "selection throws NullElement",
            seen == std::vector<std::string>({"fields: type", "selection: fshare", "refused"}) && once);
   }

   /**
    * @brief LAYOUT-03: reduce single elements to a selection of fields
    * @return nothing; CHECK records the results
    */
   void test_select_elem() {
      std::vector<std::string> seen;
      const ListPtr<US> sets = user_sets();
      const auto selection = what<US_name, US_entries>();
      const auto where = field<US_name>() == "dept";
      for (const ConstElement<US> us : *sets) {
         const auto selected = us.select(selection, Hashing::Off);
         // the same selection again: the cached reduction
         const auto again = us.select(selection, Hashing::Off);
         const auto matched = us.select(where, selection, Hashing::Off);
         seen.push_back(set_text(*selected) + "|" + set_text(*again) + "|" + set_text(*matched));
      }
      seen.push_back(set_text(*ConstElement<US>().select(selection)));
      const bool refused = throws_reason([&sets] { static_cast<void>(sets->front().select(What<US>())); },
                                         FieldException::Reason::NullElement);
      const std::vector<std::string> expected{"acl:alice,bob,|acl:alice,bob,|none",
                                              "dept:carol,|dept:carol,|dept:carol,", "empty:|empty:|none", "none"};
      CHECK(38, "select(what) and select(where, what) reduce an element, nothing for a mismatch or an empty handle",
            seen == expected && refused);
   }

   /**
    * @brief LAYOUT-04: split a list at an element
    * @return nothing; CHECK records the results
    */
   void test_split_at() {
      for (const int n : {5, 40}) {
         std::vector<std::string> seen;
         bool foreign = false;
         {
            const ListPtr<TEST> source = keyed_rows(n);
            const auto second_last = source->find_first<TEST_uint32>(static_cast<std::uint32_t>(n - 1));
            const ListPtr<TEST> target = source->split_at(second_last);
            seen.push_back(row_keys(*source));
            seen.push_back(row_keys(*target));
            auto existing = ListPtr<TEST>::create("existing");
            existing->emplace_back<TEST_uint32>(100);
            existing->splice(source->split_at(source->front()));
            seen.push_back(row_keys(*source));
            seen.push_back(row_keys(*existing));
            seen.emplace_back(existing->find_first<TEST_uint32>(2) ? "found" : "lost");
            foreign = throws_reason([&source, &target] { static_cast<void>(source->split_at(target->front())); },
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
         CHECK(39, "split_at() takes an element and the ones after it out, into a new list or spliced into an "
                   "existing one; an element of another list throws ForeignList",
               seen == expected && foreign);
      }

      // the same on a hashed unique field, US_name: every lookup below goes through a hash table, also on the
      // short lists, so a table not kept up to date by split_at() or splice() would still find a moved element
      // in the source or miss it in the target
      ocs_cull_hash_set_min_elems(0);
      for (const int n : {5, 40}) {
         std::vector<std::string> seen;
         bool tables = false;
         {
            const ListPtr<US> source = named_sets(n);
            // the first lookup builds the table of the source, before the split
            seen.push_back(lookup(*source, n));
            const ListPtr<US> target = source->split_at(source->find_first<US_name>(set_name(n - 1).c_str()));
            seen.push_back(lookup(*source, n));
            seen.push_back(lookup(*source, 1));
            seen.push_back(lookup(*target, n));
            auto existing = ListPtr<US>::create("existing");
            existing->emplace_back<US_name>(set_name(100).c_str());
            seen.push_back(lookup(*existing, 100));
            existing->splice(source->split_at(source->front()));
            seen.push_back(lookup(*source, 1));
            seen.push_back(lookup(*existing, 1));
            seen.push_back(lookup(*existing, n - 2));
            tables = has_table(source->raw(), US_name) && has_table(target->raw(), US_name) &&
                     has_table(existing->raw(), US_name);
         }
         const std::vector<std::string> expected{"found", "lost", "found", "found",
                                                 "found", "lost", "found", "found"};
         CHECK(40, "split_at() and splice() keep the hash tables of source and target up to date: a hashed lookup "
                   "finds what moved where it moved",
               seen == expected && tables);
      }
      ocs_cull_hash_set_min_elems(OCS_CULL_HASH_MIN_ELEMS);
   }

   /**
    * @brief LAYOUT-05: an extra hash table on a field the type does not hash
    * @return nothing; CHECK records the results
    */
   void test_add_hash() {
      for (const int n : {5, 40}) {
         std::vector<std::string> seen;
         const ListPtr<TEST> rows = keyed_rows(n);
         seen.push_back(std::to_string(static_cast<int>(rows->add_hash<TEST_string>())));
         seen.push_back(std::to_string(static_cast<int>(rows->add_hash<TEST_string>())));
         const auto row = rows->find_first<TEST_string>(("s" + std::to_string(n)).c_str());
         seen.push_back(row ? std::to_string(row.get<TEST_uint32>()) : "none");
         seen.push_back(std::to_string(static_cast<int>(List<TEST>().add_hash<TEST_string>())));
         // true for a new table, for one there already and for an empty handle; the lookup finds key n
         CHECK(41, "add_hash<F>() adds a table, again without harm, and a lookup by the field finds the element",
               seen == std::vector<std::string>({"1", "1", std::to_string(n), "1"}));
      }
   }

   /**
    * @brief LAYOUT-06: compare the layouts of two lists
    * @return nothing; CHECK records the results
    */
   void test_same_layout() {
      std::vector<std::string> seen;
      const auto same = [](const ConstList<US> a, const ConstList<US> b) {
         return std::string(a.same_layout(b) ? "same" : "differs");
      };
      const ListPtr<US> sets = user_sets();
      const ListPtr<US> others = user_sets();
      const ListPtr<US> empty = ListPtr<US>::create("empty");
      const ListPtr<US> by_name = sets->select(what<US_name>(), "by name");
      const ListPtr<US> by_name_too = others->select(what<US_name>(), "by name too");
      const ListPtr<US> by_type = sets->select(what<US_type>(), "by type");
      seen.push_back(same(*sets, *others));
      seen.push_back(same(*sets, *empty));
      seen.push_back(same(*sets, *by_name));
      seen.push_back(same(*by_name, *by_name_too));
      seen.push_back(same(*by_name, *by_type));
      seen.push_back(same(*sets, nullptr));
      seen.push_back(same(nullptr, nullptr));
      CHECK(42, "same_layout() tells full from reduced and reduced alike from reduced differently, and refuses "
                "missing lists",
            seen == std::vector<std::string>({"same", "same", "differs", "same", "differs", "differs", "differs"}));
   }

   /**
    * @brief PACK-01: pack a selection and a condition, unpack them and select with them
    * @return nothing; CHECK records the results
    */
   void test_pack_what_and_where() {
      ListPtr<US> sets = ListPtr<US>::create("sets");
      for (const char *name : {"alpha", "beta", "gamma"}) {
         sets->emplace_back<US_name>(name);
      }
      const What<US> selection = what<US_name>();
      const Where<US> where = field<US_name>().like("*a");
      PackBuffer out;
      selection.pack(out.raw());
      where.pack(out.raw());
      PackBuffer in = PackBuffer::reading(out.data(), out.size());
      const What<US> what_back = What<US>::unpack(in.raw());
      const Where<US> where_back = Where<US>::unpack(in.raw());
      const ListPtr<US> selected = sets->select(where_back, what_back);
      // every name ends in 'a'; the reduction keeps the name only
      std::string names;
      for (const ConstElement<US> us : *selected) {
         names += std::string(us.get<US_name>()) + (us.has<US_type>() ? "+type" : "") + ",";
      }
      CHECK(43, "a selection and a condition pack and unpack, and select what they selected before",
            selected->size() == 3 && names == "alpha,beta,gamma,");
   }

   /**
    * @brief PACK-02: errors: a truncated buffer, a list of another type, a buffer of another version
    * @return nothing; CHECK records the results
    */
   void test_pack_errors() {
      PackBuffer out;
      owned_jobs()->pack(out.raw());
      const std::size_t cut = out.size() - 5;
      const bool format = throws_pack([&] {
         PackBuffer cut_in = PackBuffer::reading(out.data(), cut);
         static_cast<void>(ListPtr<JB>::unpack(cut_in.raw()));
      }, PACK_FORMAT);
      const bool wrong_type = throws_reason([&] {
         PackBuffer jobs_in = PackBuffer::reading(out.data(), out.size());
         static_cast<void>(ListPtr<US>::unpack(jobs_in.raw()));
      }, FieldException::Reason::WrongType);
      std::vector<char> wrong_version(out.data(), out.data() + out.size());
      wrong_version[7] ^= 0x01;
      const bool version = throws_pack([&] {
         static_cast<void>(PackBuffer::reading(wrong_version.data(), wrong_version.size()));
      }, PACK_VERSION);
      CHECK(44, "a truncated buffer throws PACK_FORMAT, a list of another type WrongType, another version "
                "PACK_VERSION",
            format && wrong_type && version);
   }

   /**
    * @brief PACK-03: the counting buffer measures what packing writes
    * @return nothing; CHECK records the results
    */
   void test_pack_counting() {
      const ListPtr<JB> jobs = owned_jobs();
      PackBuffer counting = PackBuffer::counting();
      jobs->pack(counting.raw());
      PackBuffer out;
      const std::size_t header = out.size();
      jobs->pack(out.raw());
      CHECK(45, "PackBuffer::counting() measures the bytes a pack writes, without writing",
            counting.size() == out.size() - header && counting.data() == nullptr);
   }

   /**
    * @brief PACK-04: a receiver that does not know what comes: look at the type, then unpack
    * @return nothing; CHECK records the results
    */
   void test_pack_peek() {
      // a buffer holding a job list, a reduced user set list (its descriptor goes through the table), a null list,
      // and a free job element
      PackBuffer out;
      owned_jobs()->pack(out.raw());
      ListPtr<US> sets = ListPtr<US>::create("sets");
      sets->emplace_back<US_name>("alpha");
      sets->pack(out.raw(), what<US_name>());
      ListPtr<JB>().get().pack(out.raw());
      ElementPtr<JB> job = ElementPtr<JB>::create();
      job->set<JB_job_number>(7);
      job->pack(out.raw());

      PackBuffer in = PackBuffer::reading(out.data(), out.size());
      std::vector<int> types;
      std::size_t jobs_found = 0;
      std::size_t sets_found = 0;
      bool stays = true;
      for (int i = 0; i < 3; i++) {
         const std::size_t before = in.size();
         if (ocs::tom::next_list_is<JB>(in.raw())) {
            stays = stays && in.size() == before;
            jobs_found += ListPtr<JB>::unpack(in.raw())->size();
            types.push_back(JB::fields[0].nm);
         } else if (ocs::tom::next_list_is<US>(in.raw())) {
            stays = stays && in.size() == before;
            sets_found += ListPtr<US>::unpack(in.raw())->size();
            types.push_back(US::fields[0].nm);
         } else {
            stays = stays && ocs::tom::peek_list_type(&in.raw()) == NoName && in.size() == before;
            static_cast<void>(ListPtr<JB>::unpack(in.raw()));
            types.push_back(NoName);
         }
      }
      const bool elem_is_job = ocs::tom::next_elem_is<JB>(in.raw());
      types.push_back(elem_is_job ? JB::fields[0].nm : NoName);
      const ElementPtr<JB> job_back = ElementPtr<JB>::unpack(in.raw());

      // cut off inside the descriptor: looking throws as unpacking does
      const bool cut = throws_pack([&] {
         PackBuffer cut_in = PackBuffer::reading(out.data(), 2 * INTSIZE + 1 + 2 * INTSIZE + 6);
         static_cast<void>(ocs::tom::peek_list_type(&cut_in.raw()));
      }, PACK_FORMAT);

      // the first field of the type each packed object has: a job list, a user set list reduced to US_name (the
      // first field of US), a null list, a job
      const std::vector<int> expected{JB::fields[0].nm, US::fields[0].nm, NoName, JB::fields[0].nm};
      CHECK(46, "next_list_is and next_elem_is name the type of what follows without moving the buffer",
            types == expected && jobs_found == Jobs && sets_found == 1 && stays && elem_is_job &&
            job_back->get<JB_job_number>() == 7 && cut);
   }

   /**
    * @brief The names of a string list, separated by blanks
    * @param list the list
    * @return the names
    */
   std::string names(const ConstList<ST> list) {
      std::string text;
      for (const ConstElement<ST> st : list) {
         const char *name = st.get<ST_name>();
         text += (text.empty() ? "" : " ") + std::string(name != nullptr ? name : "(null)");
      }
      return text;
   }

   /**
    * @brief The names of a host list, separated by blanks
    * @param list the list
    * @return the names
    */
   std::string names(const ConstList<EH> list) {
      std::string text;
      for (const ConstElement<EH> eh : list) {
         text += (text.empty() ? "" : " ") + std::string(eh.get<EH_name>().c_str());
      }
      return text;
   }

   /**
    * @brief The host names of a list of test rows, separated by blanks
    * @param list the list
    * @return the names
    */
   std::string names(const ConstList<TEST> list) {
      std::string text;
      for (const ConstElement<TEST> row : list) {
         text += (text.empty() ? "" : " ") + std::string(row.get<TEST_host>().c_str());
      }
      return text;
   }

   /** @brief The number of jobs the split works on */
   constexpr std::uint32_t SplitJobs = 1000;

   /**
    * @brief The jobs 1 .. SplitJobs, job i owned by "user<i % 7>"
    * @return the list
    */
   ListPtr<JB> split_jobs() {
      ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
      for (std::uint32_t number = 1; number <= SplitJobs; number++) {
         jobs->emplace_back<JB_job_number>(number).set<JB_owner>(("user" + std::to_string(number % 7)).c_str());
      }
      return jobs;
   }

   /**
    * @brief The job numbers of a list, summed
    * @param jobs the list
    * @return the sum
    */
   std::uint64_t sum(const ConstList<JB> jobs) {
      std::uint64_t n = 0;
      for (const ConstElement<JB> job : jobs) {
         n += job.get<JB_job_number>();
      }
      return n;
   }

   /**
    * @brief REST-01: parse names into a list, with and without "none"
    * @return nothing; CHECK records the results
    */
   void test_from_string() {
      std::vector<std::string> seen;
      // a repeated name is kept once; host names compare without regard to case
      seen.push_back(names(*ListPtr<ST>::from_string<ST_name>("a b,a c", ", ")));
      seen.push_back(names(*ListPtr<EH>::from_string<EH_name>("h2 h1 H2", " ")));
      const ListPtr<ST> none = ListPtr<ST>::from_string<ST_name>("NONE", " ", NoneName::Empty);
      seen.push_back(none ? names(*none) : "empty");
      const bool mixed = throws<ListCoreError>([] {
         static_cast<void>(ListPtr<ST>::from_string<ST_name>("a none", " ", NoneName::Empty));
      });
      CHECK(47, "from_string<F>() parses names, keeps a repeated one once, gives no list for \"none\" and throws for "
                "\"none\" beside a name",
            seen == std::vector<std::string>({"a b c", "h2 h1", "empty"}) && mixed);
   }

   /**
    * @brief REST-02: split a list by a condition, make a host list unique, take out the names two lists share
    * @return nothing; CHECK records the results
    */
   void test_split_unique_common() {
      const ListPtr<JB> jobs = split_jobs();
      const ListPtr<JB> rest = jobs->split_off(field<JB_job_number>() <= 300U, "rest");
      // the jobs 1 .. 300 stay, sum 300 * 301 / 2 = 45150; 301 .. 1000 go, sum 500500 - 45150 = 455350
      const std::vector<std::uint64_t> split = {jobs->size(), sum(*jobs), rest->size(), sum(*rest)};
      const ListPtr<TEST> hosts = ListPtr<TEST>::create("hosts");
      for (const char *h : {"h3", "h1", "h3", "h2", "h1"}) {
         hosts->push_back(ElementPtr<TEST>::create()).set<TEST_host>(Host{h});
      }
      hosts->unique<TEST_host>();
      const ListPtr<ST> a = ListPtr<ST>::from_string<ST_name>("a b c", " ");
      const ListPtr<ST> b = ListPtr<ST>::from_string<ST_name>("b c d", " ");
      a->remove_common<ST_name>(*b);
      CHECK(48, "split_off() keeps the jobs the condition matches and returns the rest, unique<F>() sorts and drops "
                "repeated hosts, remove_common<F>() takes out the shared names",
            split == std::vector<std::uint64_t>({300, 45150, 700, 455350}) && names(*hosts) == "h1 h2 h3" &&
               names(*a) + " | " + names(*b) == "a | d");
   }

   /**
    * @brief REST-03: merge two selections and write them, a list and a condition as debug text
    * @return nothing; CHECK records the results
    */
   void test_merge_and_print() {
      What<JB> merged = what<JB_job_number>().with<JB_ja_tasks>(what<JAT_task_number>());
      // a field both select keeps the nested selections of both
      merged = std::move(merged).merge(what<JB_owner>().with<JB_ja_tasks>(what<JAT_status>()));
      const std::string what_text = text_of([&](FILE *fp) { merged.print(fp); });
      const ListPtr<JB> jobs = split_jobs();
      const auto where = field<JB_job_number>() == 7U || field<JB_owner>() == "user1";
      const ListPtr<JB> few = jobs->select(where, what<JB_job_number, JB_owner>(), "few");
      const std::string list_text = text_of([&](FILE *fp) { few->print(fp); });
      const std::string where_text = text_of([&](FILE *fp) { where.print(fp); });
      const auto has = [](const std::string &text, const char *part) { return text.find(part) != std::string::npos; };
      // the merged selection names the fields of both and both nested fields; job 7 and the jobs 1, 8, ..., 995
      // owned by user1 are 1 + 143 = 144 jobs; print_where() writes "<name>(<number>) <op> <value>" per comparison
      const bool what_ok = has(what_text, "JB_job_number") && has(what_text, "JB_owner") &&
                           has(what_text, "JB_ja_tasks") && has(what_text, "JAT_task_number") &&
                           has(what_text, "JAT_status");
      const bool list_ok = has(list_text, "List: <few> #Elements: 144");
      const bool where_ok = has(where_text, "JB_job_number(") && has(where_text, ") == 7\n") &&
                            has(where_text, "JB_owner(") && has(where_text, ") == \"user1\"\n") &&
                            has(where_text, "||");
      CHECK(49, "merge() keeps the fields and nested selections of both, print() writes the selection, the selected "
                "list and the condition",
            what_ok && list_ok && where_ok);
   }

   /**
    * @brief REST-04: count the nodes and leaves of a tree
    * @return nothing; CHECK records the results
    */
   void test_tree() {
      const ListPtr<STN> tree = ListPtr<STN>::create("tree");
      const Element<STN> root = tree->emplace_back<STN_name>("root");
      const Element<STN> left = root.get_or_create<STN_children>().emplace_back<STN_name>("left");
      root.get_or_create<STN_children>().emplace_back<STN_name>("right");
      left.get_or_create<STN_children>().emplace_back<STN_name>("leaf");
      const std::vector<int> counts = {root.count_nodes<STN_children>(), root.count_leaves<STN_children>(),
                                       tree->count_nodes<STN_children>(), tree->count_leaves<STN_children>()};
      // root, left, right and leaf are four nodes; right and leaf are the two leaves
      CHECK(50, "count_nodes<F>() and count_leaves<F>() count four nodes and two leaves from the root and from the "
                "list",
            counts == std::vector<int>({4, 2, 4, 2}));
   }

} // namespace typed_ops


/**
 * @brief A copy of the bytes of a buffer in memory of its own, as a buffer to read takes it over
 * @param pb the buffer whose bytes are copied
 * @return the copy, allocated with sge_malloc()
 */
static char *
copy_bytes(const sge_pack_buffer &pb) {
   char *copy = sge_malloc(pb.bytes_used > 0 ? pb.bytes_used : 1);
   std::memcpy(copy, pb.head_ptr, pb.bytes_used);
   return copy;
}

/**
 * @brief try_init_buffer(), try_init_buffer_from(), buffer_used(), buffer_filled() and buffer_has_unread() give
 *        what init_packbuffer(), init_packbuffer_from_buffer(), pb_used(), pb_filled() and pb_unused() give
 * @return nothing; CHECK records the results
 */
static void
test_try_init() {
   struct Setup {
      std::size_t initial_size;
      bool just_count;
      bool with_auth_info;
   };
   bool init = true;
   for (const Setup &setup : {Setup{0, false, false}, Setup{1024, false, false}, Setup{0, true, false},
                              Setup{16, false, false}}) {
      sge_pack_buffer old_pb;
      sge_pack_buffer new_pb;
      const int old_code = init_packbuffer(&old_pb, setup.initial_size, setup.just_count, setup.with_auth_info);
      const int new_code =
         ocs::tom::try_init_buffer(&new_pb, setup.initial_size, setup.just_count, setup.with_auth_info);
      const bool same = old_code == new_code && ocs::tom::buffers_equal(&old_pb, &new_pb) &&
                        old_pb.mem_size == new_pb.mem_size &&
                        static_cast<int>(ocs::tom::buffer_used(&new_pb)) == pb_used(&old_pb) &&
                        ocs::tom::buffer_filled(&new_pb) == (pb_filled(&old_pb) != 0);
      if (!same) {
         std::printf("try_init_buffer(%zu, %d, %d): codes %d, %d\n", setup.initial_size, setup.just_count,
                     setup.with_auth_info, old_code, new_code);
      }
      init = same && init;
      clear_packbuffer(&old_pb);
      ocs::tom::clear_buffer(&new_pb);
   }
   // a counting buffer that nothing was counted into is not filled; no buffer is a format error
   sge_pack_buffer counting;
   ocs::tom::try_init_buffer(&counting, 0, true, false);
   init = init && !ocs::tom::buffer_filled(&counting) && pb_filled(&counting) == 0 &&
          ocs::tom::try_init_buffer(nullptr, 0) == init_packbuffer(nullptr, 0) &&
          ocs::tom::try_init_buffer(nullptr, 0) == PACK_FORMAT;
   ocs::tom::clear_buffer(&counting);
   CHECK(51, "try_init_buffer() prepares the bytes, the size and the code init_packbuffer() prepares, with the "
             "default and a given size and counting; buffer_used() and buffer_filled() answer as pb_used() and "
             "pb_filled()",
         init);

   sge_pack_buffer out;
   init_packbuffer(&out, 0, false, false);
   packint(&out, 7);
   packstr(&out, "x");
   sge_pack_buffer old_in;
   sge_pack_buffer new_in;
   const int old_code = init_packbuffer_from_buffer(&old_in, copy_bytes(out), out.bytes_used, false);
   const int new_code = ocs::tom::try_init_buffer_from(&new_in, copy_bytes(out), out.bytes_used, false);
   bool from = old_code == PACK_SUCCESS && new_code == PACK_SUCCESS && old_in.bytes_used == new_in.bytes_used &&
               ocs::tom::buffer_has_unread(&new_in) && pb_unused(&old_in) > 0;
   std::uint32_t number = 0;
   char *str = nullptr;
   ocs::tom::try_get_uint32(&new_in, &number);
   ocs::tom::try_get_str(&new_in, &str);
   from = from && number == 7 && str != nullptr && std::strcmp(str, "x") == 0 &&
          !ocs::tom::buffer_has_unread(&new_in);
   sge_free(&str);
   clear_packbuffer(&old_in);
   ocs::tom::clear_buffer(&new_in);

   // another version: both refuse it with the same code, and the buffer frees the bytes it took over
   char *old_bytes = copy_bytes(out);
   char *new_bytes = copy_bytes(out);
   old_bytes[INTSIZE] ^= 0x01;
   new_bytes[INTSIZE] ^= 0x01;
   const int old_version = init_packbuffer_from_buffer(&old_in, old_bytes, out.bytes_used, false);
   const int new_version = ocs::tom::try_init_buffer_from(&new_in, new_bytes, out.bytes_used, false);
   clear_packbuffer(&old_in);
   ocs::tom::clear_buffer(&new_in);
   from = from && old_version == PACK_VERSION && new_version == old_version &&
          ocs::tom::try_init_buffer_from(nullptr, nullptr, 0) == init_packbuffer_from_buffer(nullptr, nullptr, 0);
   clear_packbuffer(&out);
   CHECK(52, "try_init_buffer_from() reads the header as init_packbuffer_from_buffer() does and returns its code "
             "for another version and no buffer; buffer_has_unread() is true until everything is read",
         from);
}

/**
 * @brief try_put_uint32(), try_put_double() and try_put_str() write the bytes and return the codes of packint(),
 *        packdouble() and packstr()
 * @return nothing; CHECK records the results
 */
static void
test_try_put() {
   bool puts = true;
   for (const bool just_count : {false, true}) {
      sge_pack_buffer old_pb;
      sge_pack_buffer new_pb;
      init_packbuffer(&old_pb, 0, just_count, false);
      ocs::tom::try_init_buffer(&new_pb, 0, just_count, false);
      int old_codes = 0;
      int new_codes = 0;
      for (const std::uint32_t value : {0U, 1U, 0xffffffffU}) {
         old_codes |= packint(&old_pb, value);
         new_codes |= ocs::tom::try_put_uint32(&new_pb, value);
      }
      for (const double value : {0.0, -1.5, 1e300}) {
         old_codes |= packdouble(&old_pb, value);
         new_codes |= ocs::tom::try_put_double(&new_pb, value);
      }
      for (const char *value : {static_cast<const char *>(nullptr), "", "abc"}) {
         old_codes |= packstr(&old_pb, value);
         new_codes |= ocs::tom::try_put_str(&new_pb, value);
      }
      puts = puts && old_codes == PACK_SUCCESS && new_codes == PACK_SUCCESS &&
             old_pb.bytes_used == new_pb.bytes_used &&
             (just_count || ocs::tom::buffers_equal(&old_pb, &new_pb));   // a counting buffer holds no bytes
      clear_packbuffer(&old_pb);
      ocs::tom::clear_buffer(&new_pb);
   }
   puts = puts && ocs::tom::try_put_uint32(nullptr, 1) == packint(nullptr, 1) &&
          ocs::tom::try_put_double(nullptr, 1.0) == packdouble(nullptr, 1.0) &&
          ocs::tom::try_put_str(nullptr, "a") == packstr(nullptr, "a") &&
          ocs::tom::try_put_uint32(nullptr, 1) == PACK_BADARG;
   CHECK(53, "try_put_uint32(), try_put_double() and try_put_str() write the bytes packint(), packdouble() and "
             "packstr() write, also counting, and return PACK_BADARG as they do for no buffer",
         puts);
}

/**
 * @brief try_get_uint32(), try_get_double() and try_get_str() read the values and return the codes of
 *        unpackint(), unpackdouble() and unpackstr(), also on a buffer that is read to its end
 * @return nothing; CHECK records the results
 */
static void
test_try_get() {
   sge_pack_buffer out;
   init_packbuffer(&out, 0, false, false);
   packint(&out, 42);
   packdouble(&out, 2.5);
   packstr(&out, "abc");
   packstr(&out, nullptr);

   sge_pack_buffer old_in;
   sge_pack_buffer new_in;
   init_packbuffer_from_buffer(&old_in, copy_bytes(out), out.bytes_used, false);
   ocs::tom::try_init_buffer_from(&new_in, copy_bytes(out), out.bytes_used, false);
   clear_packbuffer(&out);

   std::uint32_t old_number = 1;
   std::uint32_t new_number = 1;
   double old_double = 1.0;
   double new_double = 1.0;
   char *old_str = nullptr;
   char *new_str = nullptr;
   char *old_null = old_str;
   char *new_null = new_str;
   const bool read = unpackint(&old_in, &old_number) == ocs::tom::try_get_uint32(&new_in, &new_number) &&
                     unpackdouble(&old_in, &old_double) == ocs::tom::try_get_double(&new_in, &new_double) &&
                     unpackstr(&old_in, &old_str) == ocs::tom::try_get_str(&new_in, &new_str) &&
                     unpackstr(&old_in, &old_null) == ocs::tom::try_get_str(&new_in, &new_null) &&
                     old_number == 42 && new_number == 42 && old_double == 2.5 && new_double == 2.5 &&
                     old_str != nullptr && new_str != nullptr && std::strcmp(old_str, new_str) == 0 &&
                     old_null == nullptr && new_null == nullptr;
   sge_free(&old_str);
   sge_free(&new_str);

   // at the end of the buffer: the numbers are set to 0, the string stays as it was
   old_number = new_number = 9;
   old_double = new_double = 9.0;
   char sentinel[] = "kept";
   old_str = new_str = sentinel;
   const int old_end_int = unpackint(&old_in, &old_number);
   const int new_end_int = ocs::tom::try_get_uint32(&new_in, &new_number);
   const int old_end_double = unpackdouble(&old_in, &old_double);
   const int new_end_double = ocs::tom::try_get_double(&new_in, &new_double);
   const int old_end_str = unpackstr(&old_in, &old_str);
   const int new_end_str = ocs::tom::try_get_str(&new_in, &new_str);
   const bool end = old_end_int == PACK_FORMAT && new_end_int == old_end_int && old_end_double == PACK_FORMAT &&
                    new_end_double == old_end_double && old_end_str == PACK_FORMAT && new_end_str == old_end_str &&
                    old_number == 0 && new_number == 0 && old_double == 0.0 && new_double == 0.0 &&
                    old_str == sentinel && new_str == sentinel;
   clear_packbuffer(&old_in);
   ocs::tom::clear_buffer(&new_in);

   std::uint32_t number = 5;
   double value = 5.0;
   char *str = sentinel;
   const bool no_buffer = ocs::tom::try_get_uint32(nullptr, &number) == PACK_BADARG && number == 0 &&
                          ocs::tom::try_get_double(nullptr, &value) == PACK_BADARG && value == 0.0 &&
                          ocs::tom::try_get_str(nullptr, &str) == PACK_BADARG && str == sentinel;
   CHECK(54, "try_get_uint32(), try_get_double() and try_get_str() read what unpackint(), unpackdouble() and "
             "unpackstr() read; at the end of the buffer and without one they return the same codes, set the "
             "numbers to 0 and leave the string as it was",
         read && end && no_buffer);
}

/**
 * @brief front(), back(), next() and prev() of the raw handles walk a list both ways, as the list links it
 * @return nothing; CHECK records the results
 */
static void
test_walk() {
   const ocs::tom::RawListPtr owner(make_test_list(4, false));
   lList *lp = owner.get();

   // read only, front to back and back to front; the list's own links are the reference
   const ocs::tom::ConstRawList clist{lp};
   std::vector<const lListElem *> forward;
   for (ocs::tom::ConstRawElement ep = clist.front(); ep; ep = ep.next()) {
      forward.push_back(ep.raw());
   }
   std::vector<const lListElem *> backward;
   for (ocs::tom::ConstRawElement ep = clist.back(); ep; ep = ep.prev()) {
      backward.push_back(ep.raw());
   }
   std::vector<const lListElem *> linked;
   for (const lListElem *ep = lp->first; ep != nullptr; ep = ep->next) {
      linked.push_back(ep);
   }
   const bool read_only = forward == linked && forward.size() == 4 &&
                          std::equal(backward.rbegin(), backward.rend(), linked.begin(), linked.end()) &&
                          clist.front().raw() == lp->first && clist.back().raw() == lp->last &&
                          clist.front().get<std::uint32_t>(TEST_uint32) == 0 &&
                          clist.back().get<std::uint32_t>(TEST_uint32) == 3 && !clist.front().prev() &&
                          !clist.back().next();

   // mutable: the handles of a RawList and a RawElement stay mutable along the walk
   const ocs::tom::RawList list{lp};
   const ocs::tom::RawElement second = list.front().next();
   const ocs::tom::RawElement third = list.back().prev();
   second.set(TEST_uint64, std::uint64_t{20});
   third.set(TEST_uint64, std::uint64_t{30});
   const bool mutable_walk = second.raw() == lp->first->next && third.raw() == lp->last->prev &&
                             third.prev().raw() == second.raw() && second.next().raw() == third.raw() &&
                             lGetUlong64(lp->first->next, TEST_uint64) == 20 &&
                             lGetUlong64(lp->last->prev, TEST_uint64) == 30;

   // empty handles and an empty list give empty handles
   const ocs::tom::ConstRawList none;
   const ocs::tom::RawListPtr empty_owner(lCreateList("empty", TEST_Type));
   const ocs::tom::RawList empty{empty_owner.get()};
   const bool empty_ok = !none.front() && !none.back() && !empty.front() && !empty.back() &&
                         !ocs::tom::ConstRawElement{}.next() && !ocs::tom::ConstRawElement{}.prev() &&
                         !ocs::tom::RawElement{}.next() && !ocs::tom::RawElement{}.prev();

   CHECK(55, "front(), back(), next() and prev() of the raw handles walk a list as its links do, both ways, read "
             "only and mutable; empty handles and an empty list give empty handles",
         read_only && mutable_walk && empty_ok);
}

int main(int argc, char *argv[]) {
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
      {"RAW-OPS-15", "remaining", "remaining() counts the elements after one as lGetNumberOfRemainingElem()",
       {"count the successors of every element of a list, of a free element and of an empty handle"},
       "the same counts; 0 for the last, a free element and an empty handle", {}, {26}, test_remaining},
      {"RAW-OPS-16", "is_free", "is_free() tells a free element from a bound one as lIsFreeElem()",
       {"ask for a free element, one in a list, a sub-object and an empty handle"},
       "the answers of lIsFreeElem(); false for an empty handle", {}, {27}, test_is_free},
      {"RAW-OPS-17", "release_descr", "release_descr() gives up a reference on a shared descriptor",
       {"release a second reference of a reduced descriptor, a static descriptor and nullptr"},
       "one reference less, every holder cleared", {}, {28}, test_release_descr},
      {"RAW-OPS-18", "make_hashable",
       "make_hashable() lets a list without hashing keep hash tables, as cull_hash_create_hashtables()",
       {"make a list of 40 hashable, again, and an empty handle; look up a key"},
       "the slots of the old function; unchanged the second time; the key found", {}, {29}, test_make_hashable},
      {"RAW-OPS-19", "free_raw", "free_raw_list() and free_raw_elem() free what a raw holder keeps",
       {"free a list and an element through their holders, and holders of nullptr"},
       "every holder cleared; no leak", {}, {30}, test_free_raw},
      {"RAW-OPS-20", "descr_registry_shared",
       "descr_registry_shared() gives what ocs_cull_descr_registry_shared() gives",
       {"ask for the objects of this process through both"}, "the same result and object names", {}, {31},
       test_descr_registry_shared},
      {"RAW-OPS-21", "dump list", "dump() and undump() of a list write a framed text and read the list back",
       {"dump 100 jobs, read them back, dump what was read"}, "the frame of write_list(), the same jobs, the same text",
       {{"OPS-01", "covers"}}, {32}, typed_ops::test_dump_list},
      {"RAW-OPS-22", "dump element", "an element dumps with its values and reads back",
       {"dump a job whose owner holds quotes, read it back"}, "the field lines with the quotes escaped; the values",
       {{"OPS-01", "covers"}}, {33}, typed_ops::test_dump_element},
      {"RAW-OPS-23", "element file", "write_to_disk() and read_from_disk() carry a job through a file",
       {"write a job to a file and read it back; read a user set file as a job; read a missing file"},
       "the same job; WrongType and a runtime error thrown", {{"OPS-01", "covers"}}, {34}, typed_ops::test_dump_file},
      {"RAW-OPS-24", "dump errors", "undump() throws on a cut text and on a list of another type",
       {"read a cut text, read a job list as user sets"}, "ListCoreError and WrongType thrown",
       {{"OPS-01", "covers"}}, {35}, typed_ops::test_dump_errors},
      {"RAW-OPS-25", "create like", "create_like() makes lists and elements in the layout of a list or an element",
       {"create a list and an element like a reduced list and like a reduced element, fill and combine them",
        "create like an empty handle"},
       "the fields of the reduced layout; NullDescriptor for an empty handle", {{"OPS-01", "covers"}}, {36},
       typed_ops::test_create_like},
      {"RAW-OPS-26", "layout", "layout<F...>() and create(what) make reduced layouts",
       {"create a list and an element of a layout from fields and from a selection", "reduce to nothing"},
       "the fields named present; one descriptor per set of fields; NullElement for an empty selection",
       {{"OPS-01", "covers"}}, {37}, typed_ops::test_layout},
      {"RAW-OPS-27", "select element", "select(what) and select(where, what) reduce one element",
       {"reduce every user set to its name and entries, with and without a condition", "reduce no element"},
       "the selected fields; nothing for a mismatch; NullElement for an empty selection", {{"OPS-01", "covers"}},
       {38}, typed_ops::test_select_elem},
      {"RAW-OPS-28", "split at", "split_at() takes an element and the ones after it out of a list",
       {"split a short and a long list before its last two elements", "split the rest into an existing list",
        "the same on a hashed unique field: hashed lookups before and after the split and the splice"},
       "the keys in both lists; lookups by key still work; ForeignList for an element of another list",
       {{"OPS-01", "covers"}}, {39, 40}, typed_ops::test_split_at},
      {"RAW-OPS-29", "add hash", "add_hash<F>() adds a hash table on a field the type does not hash",
       {"add a table on the string field of a short and a long list, twice, and look up by it",
        "add one on an empty handle"},
       "true each time and the element found", {{"OPS-01", "covers"}}, {41}, typed_ops::test_add_hash},
      {"RAW-OPS-30", "same layout", "same_layout() compares the layouts of two lists",
       {"compare full lists, full and reduced lists, lists reduced alike and differently, and missing lists"},
       "same for equal layouts, differs otherwise and for a missing list", {{"OPS-01", "covers"}}, {42},
       typed_ops::test_same_layout},
      {"RAW-OPS-31", "pack selection and condition", "What and Where pack and unpack",
       {"pack a selection and a condition, unpack them, select with them"}, "the three names, reduced",
       {{"OPS-01", "covers"}}, {43}, typed_ops::test_pack_what_and_where},
      {"RAW-OPS-32", "pack errors", "unpack() throws on a truncated buffer, a foreign type and another version",
       {"unpack a truncated buffer, a list of another type, a buffer of another version"},
       "PACK_FORMAT, WrongType, PACK_VERSION", {{"OPS-01", "covers"}}, {44}, typed_ops::test_pack_errors},
      {"RAW-OPS-33", "pack counting", "the counting buffer measures what packing writes",
       {"count a pack of 100 jobs, pack them"}, "the same number of bytes", {{"OPS-01", "covers"}}, {45},
       typed_ops::test_pack_counting},
      {"RAW-OPS-34", "pack peek", "a receiver looks at the type of what follows, then unpacks it",
       {"pack a job list, a reduced user set list, a null list and a job; look at each type, then unpack it"},
       "the type of each object; the buffer unmoved by looking", {{"OPS-01", "covers"}}, {46},
       typed_ops::test_pack_peek},
      {"RAW-OPS-35", "from_string", "from_string<F>() parses names into a list",
       {"parse names with a repeated one, host names, \"none\" alone and beside a name"},
       "the lists; ListCoreError for \"none\" beside a name", {{"OPS-01", "covers"}}, {47},
       typed_ops::test_from_string},
      {"RAW-OPS-36", "split, unique, common", "split_off(), unique<F>() and remove_common<F>()",
       {"split 1 000 jobs by number, make five host names unique, take out the names two lists share"},
       "the expected counts, sums and names", {{"OPS-01", "covers"}}, {48}, typed_ops::test_split_unique_common},
      {"RAW-OPS-37", "merge and print", "merge() of selections and print() of a selection, a list and a condition",
       {"merge two job selections with nested task selections, print it, a selected list and the condition"},
       "the fields of both; the element count; the comparisons", {{"OPS-01", "covers"}}, {49},
       typed_ops::test_merge_and_print},
      {"RAW-OPS-38", "tree", "count_nodes<F>() and count_leaves<F>() count a tree",
       {"count a share tree of four nodes from the root and from the list"}, "4 nodes, 2 leaves",
       {{"OPS-01", "covers"}}, {50}, typed_ops::test_tree},
      {"RAW-OPS-39", "try_init_buffer",
       "try_init_buffer(), try_init_buffer_from() and the buffer state queries give what the old primitives give",
       {"prepare buffers to pack into with every setup, a counting buffer and no buffer, through both",
        "prepare buffers to read from bytes, from bytes of another version and from no buffer; read them out"},
       "the same bytes, sizes, codes and answers; PACK_FORMAT, PACK_VERSION", {}, {51, 52}, test_try_init},
      {"RAW-OPS-40", "try_put", "try_put_uint32(), try_put_double() and try_put_str() write what the old ones write",
       {"pack numbers, doubles and strings, null and empty among them, into a buffer and a counting buffer; pack "
        "into no buffer"},
       "the same bytes and codes; PACK_BADARG", {}, {53}, test_try_put},
      {"RAW-OPS-41", "try_get", "try_get_uint32(), try_get_double() and try_get_str() read what the old ones read",
       {"read a number, a double, a string and a null string, then read past the end, then read from no buffer"},
       "the same values and codes; 0 for the numbers and the string kept on an error", {}, {54}, test_try_get},
      {"RAW-OPS-42", "walk", "front(), back(), next() and prev() of the raw handles walk a list both ways",
       {"walk a list of four front to back and back to front with the read-only handles, compare with the links",
        "step from both ends with the mutable handles and write through them",
        "walk an empty handle, an empty list and empty element handles"},
       "the order of the links; mutable handles along the walk; empty handles at the ends", {}, {55}, test_walk},
   });
}
