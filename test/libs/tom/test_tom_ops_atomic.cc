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
 * @brief The atomic accessors of 64 bit fields through the typed handles and through the old interface (OPS-ATOM)
 *
 * get_atomic<F>() and set_atomic<F>() replace lGetUlong64Atomic() and
 * lSetUlong64Atomic(); both forms run on the same core
 * (ocs::tom::load_uint64_atomic(), ocs::tom::store_uint64_atomic()). The
 * scenarios compare the typed form with the old one, check the empty handle,
 * and let writers race a reader to show that no torn value is ever seen. A
 * field declared hashed does not compile with set_atomic<F>(), see
 * compile_fail/set_atomic_on_hashed_field.cc, nor does a 32 bit field with
 * get_atomic<F>() (get_atomic_on_32_bit_field.cc) or a constant handle with
 * set_atomic<F>() (set_atomic_on_const.cc).
 */

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

#include "cull/cull_multitype.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_List.h"

#include "ocs_module_test.h"

using ocs::tom::ConstElement;
using ocs::tom::Element;
using ocs::tom::ElementPtr;
using ocs::tom::FieldException;
using ocs::tom::HM;
using ocs::tom::Host;
using ocs::tom::ListPtr;

namespace {

   /** @brief A value whose two halves differ from those of #Pattern_b, so a torn read shows */
   constexpr std::uint64_t Pattern_a = 0x0000'0000'ffff'ffffULL;

   /** @brief The other value the writers store */
   constexpr std::uint64_t Pattern_b = 0xffff'ffff'0000'0000ULL;

   /**
    * @brief The typed form reads and writes what the old form reads and writes, also in a hashed list
    */
   void test_round_trip() {
      auto entries = ListPtr<HM>::create("match cache");
      const auto entry = entries->emplace_back<HM_name>(Host{"host_a"});

      // old:   lSetUlong64Atomic(ep, HM_last_used, v); typed: set_atomic<HM_last_used>(entry, v)
      CHECK(1, "set_atomic stores what lGetUlong64Atomic reads",
            ocs::tom::set_atomic<HM_last_used>(entry, 4242) == 0 &&
            lGetUlong64Atomic(entry, HM_last_used) == 4242);
      CHECK(2, "lSetUlong64Atomic stores what get_atomic reads",
            lSetUlong64Atomic(entry, HM_last_used, 4711) == 0 &&
            ocs::tom::get_atomic<HM_last_used>(ConstElement<HM>(entry)) == 4711);
      // the ordinary setter and the atomic getter see the same slot
      ocs::tom::set<HM_last_used>(entry, Pattern_b);
      CHECK(3, "get_atomic reads what the ordinary setter stored",
            ocs::tom::get_atomic<HM_last_used>(entry) == Pattern_b);
      // the atomic store bypasses the hash table, which only the name field has
      CHECK(4, "the element is still found by its hashed name",
            entries->find_first<HM_name>(Host{"host_a"}).raw() == entry.raw());
   }

   /**
    * @brief An empty handle: set_atomic gives -1 as lSetUlong64Atomic(nullptr), get_atomic throws as get does
    */
   void test_empty_handle() {
      CHECK(5, "set_atomic on an empty handle gives -1", ocs::tom::set_atomic<HM_last_used>(Element<HM>(), 1) == -1);
      bool thrown = false;
      try {
         static_cast<void>(ocs::tom::get_atomic<HM_last_used>(ConstElement<HM>()));
      } catch (const FieldException &e) {
         thrown = e.reason() == FieldException::Reason::NullElement;
      }
      CHECK(6, "get_atomic on an empty handle throws NullElement", thrown);
   }

   /**
    * @brief Two writers store two values whose halves differ while a reader checks that it sees one of them whole
    */
   void test_no_torn_value() {
      auto entry = ElementPtr<HM>::create();
      ocs::tom::set_atomic<HM_last_used>(entry.get(), Pattern_a);
      const Element<HM> shared = entry.get();

      constexpr int rounds = 200000;
      std::atomic<bool> stop{false};
      auto writer = [&](const std::uint64_t value) {
         while (!stop.load(std::memory_order_relaxed)) {
            ocs::tom::set_atomic<HM_last_used>(shared, value);
         }
      };
      std::thread writer_a(writer, Pattern_a);
      std::thread writer_b(writer, Pattern_b);

      int torn = 0;
      for (int i = 0; i < rounds; i++) {
         const std::uint64_t seen = ocs::tom::get_atomic<HM_last_used>(shared);
         if (seen != Pattern_a && seen != Pattern_b) {
            torn++;
         }
      }
      stop.store(true, std::memory_order_relaxed);
      writer_a.join();
      writer_b.join();
      printf("%d reads while two writers race, %d torn\n", rounds, torn);
      CHECK(7, "no read sees half of one value and half of the other", torn == 0);
   }

   /**
    * @brief Call a function and give the reason of the FieldException it throws
    * @tparam Fn the function's type
    * @param fn the function
    * @return the reason, or NullList when nothing was thrown (no atomic accessor throws NullList)
    */
   template <typename Fn>
   FieldException::Reason reason_of(Fn &&fn) {
      try {
         fn();
      } catch (const FieldException &e) {
         return e.reason();
      }
      return FieldException::Reason::NullList;
   }

   /**
    * @brief A reduced element without the field: both accessors throw AbsentField
    *
    * The empty handle differs (set_atomic gives -1, get_atomic throws NullElement, OPS-ATOM-02); a reduced
    * element is a programming error for both.
    */
   void test_absent_field() {
      // a layout without HM_last_used
      auto entry = ElementPtr<HM>::create(ocs::tom::layout<HM_name>());
      CHECK(8, "get_atomic on a reduced element without the field throws AbsentField",
            reason_of([&] { static_cast<void>(ocs::tom::get_atomic<HM_last_used>(entry.get())); }) ==
               FieldException::Reason::AbsentField);
      CHECK(9, "set_atomic on a reduced element throws AbsentField rather than returning -1",
            reason_of([&] { ocs::tom::set_atomic<HM_last_used>(entry.get(), 1); }) ==
               FieldException::Reason::AbsentField);
      CHECK(10, "the reduced element still holds its name field",
            entry->has<HM_name>() && !entry->has<HM_last_used>());
   }

   /**
    * @brief The owning handle takes the atomic accessors as get, set and add do (L3)
    */
   void test_owning_handle() {
      auto entry = ElementPtr<HM>::create();
      CHECK(11, "set_atomic on an owning handle stores what get_atomic on it reads",
            ocs::tom::set_atomic<HM_last_used>(entry, Pattern_b) == 0 &&
            ocs::tom::get_atomic<HM_last_used>(entry) == Pattern_b &&
            ocs::tom::get<HM_last_used>(entry) == Pattern_b);
      ElementPtr<HM> empty;
      CHECK(12, "on an empty owning handle set_atomic gives -1 and get_atomic throws NullElement",
            ocs::tom::set_atomic<HM_last_used>(empty, 1) == -1 &&
            reason_of([&] { static_cast<void>(ocs::tom::get_atomic<HM_last_used>(empty)); }) ==
               FieldException::Reason::NullElement);
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
      {"OPS-ATOM-01", "round trip", "get_atomic<F>() and set_atomic<F>() as lGetUlong64Atomic and lSetUlong64Atomic",
       {"write with one form and read with the other, read what the ordinary setter wrote, look the element up"},
       "the same values; the hashed name still finds the element", {}, {1, 2, 3, 4}, test_round_trip},
      {"OPS-ATOM-02", "empty handle", "set_atomic<F>() and get_atomic<F>() on an empty handle",
       {"set and get through empty handles"}, "-1 from the setter, NullElement from the getter", {}, {5, 6},
       test_empty_handle},
      {"OPS-ATOM-03", "no torn value", "a value written with set_atomic<F>() is never read half old, half new",
       {"two threads store two values whose halves differ, a third reads 200 000 times"}, "no torn read", {}, {7},
       test_no_torn_value},
      {"OPS-ATOM-04", "reduced element", "get_atomic<F>() and set_atomic<F>() on an element without the field",
       {"create an element with a layout that lacks the field, read and write the field atomically"},
       "AbsentField from both accessors; the element keeps its other field", {}, {8, 9, 10}, test_absent_field},
      {"OPS-ATOM-05", "owning handle", "get_atomic<F>() and set_atomic<F>() on an ElementPtr (L3)",
       {"write and read through an owning handle, then through an empty one"},
       "the value round trips; -1 from the setter and NullElement from the getter on the empty one", {}, {11, 12},
       test_owning_handle},
   });
}
