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
 * @brief Module test of the checked accessors on raw pointers of the run-time typed interface (RAW-ACC)
 *
 * The field is given at run time and the value type by the caller, so every
 * mismatch is found at run time. For every field type the test runs the
 * good case and the four errors - a null element, a field of another type, a
 * field a reduced element leaves out, a value type that is not the field's -
 * through three forms: the free accessor on the raw pointer, the method of
 * the raw handle and, for plain values, the run-time member of the typed
 * handle. All three must give the same value and the same reason, since the
 * handles only forward and the typed member runs on the same core. Lists and
 * objects add ownership: a stored list or element is taken over, a list or
 * element of another sub-type than the model names is refused, a bound
 * element is refused and stays where it is bound. The soft accessors has()
 * and get_or() answer for a field the element does not hold without a throw,
 * but get_or() still refuses a wrong value type.
 *
 * That a replaced list or element is freed is shown by the leak checker
 * (valgrind, ASan) on this binary, not by a CHECK.
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <type_traits>
#include <utility>

#include "tom/ocs_Types.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Model.h"
#include "tom/ocs_Raw.h"

#include "ocs_module_test.h"

// the non-owning raw handles are made implicitly from the matching pointer and typed handle, never with a loss
// of const; the owners stay explicit (RAW-ACC-07)
static_assert(std::is_convertible_v<const lListElem *, ocs::tom::ConstRawElement>);
static_assert(std::is_convertible_v<lListElem *, ocs::tom::RawElement>);
static_assert(!std::is_convertible_v<const lListElem *, ocs::tom::RawElement>);
static_assert(!std::is_convertible_v<ocs::tom::ConstRawElement, ocs::tom::RawElement>);
static_assert(std::is_convertible_v<ocs::tom::ConstElement<ocs::tom::JB>, ocs::tom::ConstRawElement>);
static_assert(std::is_convertible_v<ocs::tom::Element<ocs::tom::JB>, ocs::tom::ConstRawElement>);
static_assert(std::is_convertible_v<ocs::tom::Element<ocs::tom::JB>, ocs::tom::RawElement>);
static_assert(!std::is_convertible_v<ocs::tom::ConstElement<ocs::tom::JB>, ocs::tom::RawElement>);
static_assert(std::is_convertible_v<const lList *, ocs::tom::ConstRawList>);
static_assert(std::is_convertible_v<lList *, ocs::tom::RawList>);
static_assert(!std::is_convertible_v<const lList *, ocs::tom::RawList>);
static_assert(!std::is_convertible_v<ocs::tom::ConstRawList, ocs::tom::RawList>);
static_assert(std::is_convertible_v<ocs::tom::ConstList<ocs::tom::JB>, ocs::tom::ConstRawList>);
static_assert(std::is_convertible_v<ocs::tom::List<ocs::tom::JB>, ocs::tom::RawList>);
static_assert(!std::is_convertible_v<ocs::tom::ConstList<ocs::tom::JB>, ocs::tom::RawList>);
static_assert(!std::is_convertible_v<lListElem *, ocs::tom::RawElementPtr>);
static_assert(!std::is_convertible_v<ocs::tom::ElementPtr<ocs::tom::JB>, ocs::tom::RawElementPtr>);
static_assert(!std::is_convertible_v<lList *, ocs::tom::RawListPtr>);

namespace {

   using Reason = ocs::tom::FieldException::Reason;

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
    * @brief Whether two values of a run-time accessor are equal; strings and host names by their text
    * @tparam V the value type
    * @param a the first value
    * @param b the second value
    * @return true if equal
    */
   template <typename V>
   bool same(const V a, const V b) {
      if constexpr (std::is_same_v<V, const char *>) {
         return a != nullptr && b != nullptr && std::strcmp(a, b) == 0;
      } else if constexpr (std::is_same_v<V, ocs::tom::Host>) {
         return a.c_str() != nullptr && b.c_str() != nullptr && std::strcmp(a.c_str(), b.c_str()) == 0;
      } else {
         return a == b;
      }
   }

   /**
    * @brief An environment variable, for the list field JB_env_list
    * @param name the name, the unique key of the type
    * @return the owner of the element
    */
   ocs::tom::RawElementPtr variable(const char *name) {
      auto ep = ocs::tom::create_raw_elem(ocs::tom::VA::descriptor.e);
      ep->set(VA_variable, name);
      return ep;
   }

   /**
    * @brief The layout of TEST reduced to TEST_obj_any: every other field of TEST is absent in it
    * @return the descriptor, alive for the whole run
    */
   const lDescr *reduced_test() {
      return ocs::tom::layout<TEST_obj_any>().get();
   }

   /**
    * @brief Run the good case and the four errors of one plain field through the free, the raw and the typed form
    *
    * Writes @p a with the free form and reads it with all forms, writes @p b
    * with the method of the raw handle, and @p a again with the typed member;
    * then reads and writes with a null element, a foreign field (JB_job_number),
    * on a reduced element without the field, and with the value type @p W;
    * @p W on the reduced element must give AbsentField (presence before type).
    *
    * @tparam V the value type of the field
    * @tparam W a value type of another field type
    * @param label the case, for messages
    * @param nm the field of TEST
    * @param a a value
    * @param b another value
    * @return true if every form gave the expected value and reason
    */
   template <typename V, typename W>
   bool field_case(const char *label, const int nm, const V a, const V b) {
      const auto full = ocs::tom::create_raw_elem(ocs::tom::TEST::descriptor.e);
      const auto reduced = ocs::tom::create_raw_elem(reduced_test());
      lListElem *ep = full.get();
      const lListElem *cep = ep;
      lListElem *rp = reduced.get();
      const lListElem *crp = rp;
      const lListElem *null_ep = nullptr;
      lListElem *null_mut = nullptr;
      const ocs::tom::ConstRawElement handle(cep);
      const ocs::tom::RawElement handle_rw(ep);
      const ocs::tom::ConstElement<ocs::tom::TEST> typed(cep);
      const ocs::tom::Element<ocs::tom::TEST> typed_rw(ep);

      ocs::tom::set(ep, nm, a);
      bool values = same(ocs::tom::get<V>(cep, nm), a) && same(ocs::tom::get<V>(ep, nm), a) &&
                    same(handle.get<V>(nm), a) && same(handle_rw.get<V>(nm), a) && same(typed.get<V>(nm), a);
      handle_rw.set(nm, b);
      values = values && same(ocs::tom::get<V>(cep, nm), b) && same(typed.get<V>(nm), b);
      typed_rw.set(nm, a);
      values = values && same(ocs::tom::get<V>(cep, nm), a) && same(handle.get<V>(nm), a);
      if (!values) {
         std::printf("%s: the forms read or write different values\n", label);
      }

      const bool reads =
         expect_reason(label, "get, null element", [&] { static_cast<void>(ocs::tom::get<V>(null_ep, nm)); },
                       Reason::NullElement) &&
         expect_reason(label, "get, empty raw handle",
                       [&] { static_cast<void>(ocs::tom::ConstRawElement().get<V>(nm)); }, Reason::NullElement) &&
         expect_reason(label, "get, foreign", [&] { static_cast<void>(ocs::tom::get<V>(cep, JB_job_number)); },
                       Reason::ForeignField) &&
         expect_reason(label, "raw handle get, foreign", [&] { static_cast<void>(handle.get<V>(JB_job_number)); },
                       Reason::ForeignField) &&
         expect_reason(label, "typed get, foreign", [&] { static_cast<void>(typed.get<V>(JB_job_number)); },
                       Reason::ForeignField) &&
         expect_reason(label, "get, absent", [&] { static_cast<void>(ocs::tom::get<V>(crp, nm)); },
                       Reason::AbsentField) &&
         expect_reason(label, "raw handle get, absent",
                       [&] { static_cast<void>(ocs::tom::ConstRawElement(crp).get<V>(nm)); }, Reason::AbsentField) &&
         expect_reason(label, "typed get, absent",
                       [&] { static_cast<void>(ocs::tom::ConstElement<ocs::tom::TEST>(crp).get<V>(nm)); },
                       Reason::AbsentField) &&
         // presence is checked before the type: a wrong value type on an absent field is AbsentField
         expect_reason(label, "get, wrong type, absent", [&] { static_cast<void>(ocs::tom::get<W>(crp, nm)); },
                       Reason::AbsentField) &&
         expect_reason(label, "raw handle get, wrong type, absent",
                       [&] { static_cast<void>(ocs::tom::ConstRawElement(crp).get<W>(nm)); }, Reason::AbsentField) &&
         expect_reason(label, "typed get, wrong type, absent",
                       [&] { static_cast<void>(ocs::tom::ConstElement<ocs::tom::TEST>(crp).get<W>(nm)); },
                       Reason::AbsentField) &&
         expect_reason(label, "get, wrong type", [&] { static_cast<void>(ocs::tom::get<W>(cep, nm)); },
                       Reason::WrongType) &&
         expect_reason(label, "raw handle get, wrong type", [&] { static_cast<void>(handle.get<W>(nm)); },
                       Reason::WrongType) &&
         expect_reason(label, "typed get, wrong type", [&] { static_cast<void>(typed.get<W>(nm)); },
                       Reason::WrongType);

      const bool writes =
         expect_reason(label, "set, null element", [&] { ocs::tom::set(null_mut, nm, a); }, Reason::NullElement) &&
         expect_reason(label, "set, empty raw handle", [&] { ocs::tom::RawElement().set(nm, a); },
                       Reason::NullElement) &&
         expect_reason(label, "set, foreign", [&] { ocs::tom::set(ep, JB_job_number, a); }, Reason::ForeignField) &&
         expect_reason(label, "raw handle set, foreign", [&] { handle_rw.set(JB_job_number, a); },
                       Reason::ForeignField) &&
         expect_reason(label, "set, absent", [&] { ocs::tom::set(rp, nm, a); }, Reason::AbsentField) &&
         expect_reason(label, "set, wrong type", [&] { ocs::tom::set(ep, nm, W{}); }, Reason::WrongType) &&
         expect_reason(label, "raw handle set, wrong type", [&] { handle_rw.set(nm, W{}); }, Reason::WrongType) &&
         expect_reason(label, "typed set, wrong type", [&] { typed_rw.set(nm, W{}); }, Reason::WrongType);

      // a refused write changes nothing
      const bool kept = same(ocs::tom::get<V>(cep, nm), a);
      if (!kept) {
         std::printf("%s: a refused write changed the value\n", label);
      }
      return values && reads && writes && kept;
   }

} // namespace

/**
 * @brief Every plain field type: values and reasons of the free, the raw and the typed form
 * @return nothing; CHECK records the results
 */
static void
test_plain_fields() {
   int ref_a = 1;
   int ref_b = 2;
   CHECK(1, "Bool: the forms agree on values and reasons",
         (field_case<bool, std::uint32_t>("Bool", TEST_bool, true, false)));
   CHECK(2, "Uint32: the forms agree on values and reasons",
         (field_case<std::uint32_t, std::uint64_t>("Uint32", TEST_uint32, 17U, 4000000000U)));
   CHECK(3, "Uint64: the forms agree on values and reasons",
         (field_case<std::uint64_t, std::uint32_t>("Uint64", TEST_uint64, std::uint64_t{1} << 40, 3)));
   CHECK(4, "Int32: the forms agree on values and reasons",
         (field_case<std::int32_t, std::uint32_t>("Int32", TEST_int, -5, 7)));
   CHECK(5, "Int64: the forms agree on values and reasons",
         (field_case<std::int64_t, std::uint32_t>("Int64", TEST_long, -(std::int64_t{1} << 40), 9)));
   CHECK(6, "Double: the forms agree on values and reasons",
         (field_case<double, std::uint32_t>("Double", TEST_double, 1.5, -2.25)));
   CHECK(7, "Host: the forms agree on values and reasons",
         (field_case<ocs::tom::Host, std::uint32_t>("Host", TEST_host, ocs::tom::Host("host-a"),
                                                    ocs::tom::Host("host-b"))));
   CHECK(8, "String: the forms agree on values and reasons",
         (field_case<const char *, std::uint32_t>("String", TEST_string, "alpha", "beta")));
   CHECK(9, "Ref: the forms agree on values and reasons",
         (field_case<void *, std::uint32_t>("Ref", TEST_ref, &ref_a, &ref_b)));
}

/**
 * @brief An object field: store, read, replace, refuse another sub-type and a bound element, take out
 * @return nothing; CHECK records the results
 */
static void
test_object_field() {
   const auto test = ocs::tom::create_raw_elem(ocs::tom::TEST::descriptor.e);
   lListElem *ep = test.get();
   const lListElem *cep = ep;
   const ocs::tom::RawElement handle(ep);

   // store, read, write through the mutable read, replace, clear
   const bool empty_before = ocs::tom::get<const lListElem *>(cep, TEST_obj_jb) == nullptr;
   auto job = ocs::tom::create_raw_elem(ocs::tom::JB::descriptor.e);
   ocs::tom::set(job.get(), JB_job_number, std::uint32_t{5});
   const lListElem *stored = job.get();
   ocs::tom::set(ep, TEST_obj_jb, std::move(job));
   const bool taken = !job;
   const auto held = ocs::tom::get<ocs::tom::ConstRawElement>(cep, TEST_obj_jb);
   const bool read = held.raw() == stored && held.get<std::uint32_t>(JB_job_number) == 5;
   ocs::tom::set(ocs::tom::get<lListElem *>(ep, TEST_obj_jb), JB_job_number, std::uint32_t{6});
   const bool written = handle.get<ocs::tom::RawElement>(TEST_obj_jb).get<std::uint32_t>(JB_job_number) == 6;
   auto second = ocs::tom::RawElementPtr::create(ocs::tom::JB::descriptor.e);
   const lListElem *second_raw = second.get();
   handle.set(TEST_obj_jb, std::move(second));
   const bool replaced = ocs::tom::get<const lListElem *>(cep, TEST_obj_jb) == second_raw;
   handle.set(TEST_obj_jb, ocs::tom::RawElementPtr{});
   const bool cleared = !ocs::tom::get<ocs::tom::ConstRawElement>(cep, TEST_obj_jb);
   CHECK(10, "an object field takes the element over, reads it in every form, replaces and clears it",
         empty_before && taken && read && written && replaced && cleared);

   // another sub-type than the model names is refused; the field without a sub-type takes any type
   const bool sub_refused =
      expect_reason("object", "free set, sub-type",
                    [&] { ocs::tom::set(ep, TEST_obj_jb, ocs::tom::create_raw_elem(ocs::tom::QU::descriptor.e)); },
                    Reason::WrongSubType) &&
      expect_reason("object", "raw handle set, sub-type",
                    [&] { handle.set(TEST_obj_jb, ocs::tom::create_raw_elem(ocs::tom::QU::descriptor.e)); },
                    Reason::WrongSubType);
   ocs::tom::set(ep, TEST_obj_any, ocs::tom::create_raw_elem(ocs::tom::QU::descriptor.e));
   const char *any_type = ocs::tom::type_name(ocs::tom::get<ocs::tom::ConstRawElement>(cep, TEST_obj_any).descr());
   const bool any_taken = any_type != nullptr && std::strcmp(any_type, "QU") == 0 &&
                          !ocs::tom::get<ocs::tom::ConstRawElement>(cep, TEST_obj_jb);
   CHECK(11, "an element of another sub-type is refused with WrongSubType; an untyped object field takes any type",
         sub_refused && any_taken);

   // a bound element is refused and stays in its list
   auto jobs = ocs::tom::create_raw_list(ocs::tom::JB::descriptor.e);
   auto first_job = ocs::tom::create_raw_elem(ocs::tom::JB::descriptor.e);
   first_job->set(JB_job_number, std::uint32_t{100});
   lListElem *bound = jobs->push_back(std::move(first_job));
   ocs::tom::RawElementPtr wrong_owner(bound);
   ocs::tom::RawElementPtr wrong_owner_rw(bound);
   const bool bound_refused =
      expect_reason("object", "free set, bound", [&] { ocs::tom::set(ep, TEST_obj_jb, std::move(wrong_owner)); },
                    Reason::BoundElement) &&
      expect_reason("object", "raw handle set, bound", [&] { handle.set(TEST_obj_jb, std::move(wrong_owner_rw)); },
                    Reason::BoundElement);
   const bool bound_kept = jobs->size() == 1 && jobs->front().raw() == bound && !wrong_owner && !wrong_owner_rw &&
                           ocs::tom::get<const lListElem *>(cep, TEST_obj_jb) == nullptr;
   CHECK(12, "a bound element is refused with BoundElement and stays in its list", bound_refused && bound_kept);

   // a bound element in an owner stays in its list also when another check of set() throws first: the
   // by-value parameter must not free it
   auto queues = ocs::tom::create_raw_list(ocs::tom::QU::descriptor.e);
   lListElem *bound_queue = queues->push_back(ocs::tom::create_raw_elem(ocs::tom::QU::descriptor.e));
   const auto reduced_target = ocs::tom::create_raw_elem(reduced_test());   // TEST_obj_jb is absent in it
   lListElem *null_target = nullptr;
   ocs::tom::RawElementPtr to_null(bound);
   ocs::tom::RawElementPtr to_foreign(bound);
   ocs::tom::RawElementPtr to_absent(bound);
   ocs::tom::RawElementPtr to_number(bound);
   ocs::tom::RawElementPtr to_sub_type(bound_queue);
   const bool other_reasons =
      expect_reason("object", "bound, null element",
                    [&] { ocs::tom::set(null_target, TEST_obj_jb, std::move(to_null)); }, Reason::NullElement) &&
      expect_reason("object", "bound, foreign", [&] { ocs::tom::set(ep, JB_job_number, std::move(to_foreign)); },
                    Reason::ForeignField) &&
      expect_reason("object", "bound, absent",
                    [&] { ocs::tom::set(reduced_target.get(), TEST_obj_jb, std::move(to_absent)); },
                    Reason::AbsentField) &&
      expect_reason("object", "bound, wrong type", [&] { ocs::tom::set(ep, TEST_uint32, std::move(to_number)); },
                    Reason::WrongType) &&
      expect_reason("object", "bound, raw handle, sub-type",
                    [&] { handle.set(TEST_obj_jb, std::move(to_sub_type)); }, Reason::WrongSubType);
   const bool other_kept = jobs->size() == 1 && jobs->front().raw() == bound && queues->size() == 1 &&
                           queues->front().raw() == bound_queue && !to_null && !to_foreign && !to_absent &&
                           !to_number && !to_sub_type && ocs::tom::get<const lListElem *>(cep, TEST_obj_jb) == nullptr;
   CHECK(24, "set() of a bound element that fails another check first (NullElement, ForeignField, AbsentField, "
             "WrongType, WrongSubType) leaves the element in its list",
         other_reasons && other_kept);

   // take the element out: it is free and can go into a list; an empty field gives an empty owner
   auto next_job = ocs::tom::create_raw_elem(ocs::tom::JB::descriptor.e);
   next_job->set(JB_job_number, std::uint32_t{101});
   ocs::tom::set(ep, TEST_obj_jb, std::move(next_job));
   const lListElem *inside = ocs::tom::get<const lListElem *>(cep, TEST_obj_jb);
   auto taken_out = ocs::tom::take_raw_object(ep, TEST_obj_jb);
   const bool out = taken_out.get().raw() == inside && ocs::tom::get<const lListElem *>(cep, TEST_obj_jb) == nullptr;
   jobs->push_back(std::move(taken_out));
   handle.set(TEST_obj_jb, ocs::tom::create_raw_elem(ocs::tom::JB::descriptor.e));
   const auto again = handle.take(TEST_obj_jb);
   const auto none = handle.take(TEST_obj_jb);
   CHECK(13, "take_raw_object() and the method take() hand the element out free; an empty field gives none",
         out && jobs->size() == 2 && again && !none && ocs::tom::get<const lListElem *>(cep, TEST_obj_jb) == nullptr);

   // the reasons of the object accessors
   const auto reduced = ocs::tom::create_raw_elem(reduced_test());
   lListElem *rp = reduced.get();
   const lListElem *crp = rp;
   const lListElem *null_ep = nullptr;
   lListElem *null_mut = nullptr;
   const bool reasons =
      expect_reason("object", "get, null",
                    [&] { static_cast<void>(ocs::tom::get<ocs::tom::ConstRawElement>(null_ep, TEST_obj_jb)); },
                    Reason::NullElement) &&
      expect_reason("object", "get, foreign",
                    [&] { static_cast<void>(ocs::tom::get<const lListElem *>(cep, JB_job_number)); },
                    Reason::ForeignField) &&
      expect_reason("object", "get, absent",
                    [&] { static_cast<void>(ocs::tom::get<ocs::tom::ConstRawElement>(crp, TEST_obj_jb)); },
                    Reason::AbsentField) &&
      expect_reason("object", "raw handle get, absent",
                    [&] { static_cast<void>(ocs::tom::RawElement(rp).get<ocs::tom::RawElement>(TEST_obj_jb)); },
                    Reason::AbsentField) &&
      expect_reason("object", "get as a list",
                    [&] { static_cast<void>(ocs::tom::get<ocs::tom::ConstRawList>(cep, TEST_obj_jb)); },
                    Reason::WrongType) &&
      expect_reason("object", "get as a number",
                    [&] { static_cast<void>(ocs::tom::get<std::uint32_t>(cep, TEST_obj_jb)); }, Reason::WrongType) &&
      expect_reason("object", "take a number field",
                    [&] { static_cast<void>(ocs::tom::take_raw_object(ep, TEST_uint32)); }, Reason::WrongType) &&
      expect_reason("object", "take, null",
                    [&] { static_cast<void>(ocs::tom::take_raw_object(null_mut, TEST_obj_jb)); },
                    Reason::NullElement) &&
      expect_reason("object", "set, absent",
                    [&] { ocs::tom::set(rp, TEST_obj_jb, ocs::tom::create_raw_elem(ocs::tom::JB::descriptor.e)); },
                    Reason::AbsentField) &&
      expect_reason("object", "set an element into a number field",
                    [&] { ocs::tom::set(ep, TEST_uint32, ocs::tom::create_raw_elem(ocs::tom::JB::descriptor.e)); },
                    Reason::WrongType);
   CHECK(14, "the object accessors throw NullElement, ForeignField, AbsentField and WrongType", reasons);
}

/**
 * @brief A list field: store, read, append through the mutable read, exchange, refuse another sub-type
 * @return nothing; CHECK records the results
 */
static void
test_list_field() {
   const auto job = ocs::tom::create_raw_elem(ocs::tom::JB::descriptor.e);
   lListElem *jp = job.get();
   const lListElem *cjp = jp;
   const ocs::tom::RawElement handle(jp);

   // store, read, append through the mutable read, replace by the method, clear
   auto env = ocs::tom::create_raw_list(ocs::tom::VA::descriptor.e, ocs::tom::Hashing::On, "env");
   env->push_back(variable("A"));
   const lList *env_raw = env.get();
   ocs::tom::set(jp, JB_env_list, std::move(env));
   const bool stored = !env && ocs::tom::get<const lList *>(cjp, JB_env_list) == env_raw &&
                       ocs::tom::get<ocs::tom::ConstRawList>(cjp, JB_env_list).size() == 1;
   ocs::tom::append(ocs::tom::get<lList *>(jp, JB_env_list), variable("B"));
   const bool appended = handle.get<ocs::tom::RawList>(JB_env_list).size() == 2;
   handle.set(JB_env_list, ocs::tom::create_raw_list(ocs::tom::VA::descriptor.e));
   const bool replaced = handle.get<ocs::tom::ConstRawList>(JB_env_list).empty() &&
                         handle.get<const lList *>(JB_env_list) != nullptr;
   handle.set(JB_env_list, ocs::tom::RawListPtr{});
   const bool cleared = ocs::tom::get<const lList *>(cjp, JB_env_list) == nullptr;
   CHECK(15, "a list field takes the list over, reads it in every form, replaces and clears it",
         stored && appended && replaced && cleared);

   // exchange gives the old list back, in the free and the member form
   auto one = ocs::tom::create_raw_list(ocs::tom::VA::descriptor.e);
   one->push_back(variable("C"));
   const lList *one_raw = one.get();
   ocs::tom::set(jp, JB_env_list, std::move(one));
   auto two = ocs::tom::create_raw_list(ocs::tom::VA::descriptor.e);
   two->push_back(variable("D"));
   two->push_back(variable("E"));
   const lList *two_raw = two.get();
   auto old = ocs::tom::exchange(jp, JB_env_list, std::move(two));
   const bool first = old.get().raw() == one_raw && old->size() == 1 &&
                      ocs::tom::get<const lList *>(cjp, JB_env_list) == two_raw;
   auto back = handle.exchange(JB_env_list, std::move(old));
   const bool second = back.get().raw() == two_raw && ocs::tom::get<const lList *>(cjp, JB_env_list) == one_raw;
   auto last = handle.exchange(JB_env_list, ocs::tom::RawListPtr{});
   const bool third = last.get().raw() == one_raw && ocs::tom::get<const lList *>(cjp, JB_env_list) == nullptr;
   CHECK(16, "exchange() and the method exchange() put the new list in and hand the old one out",
         first && second && third);

   // another sub-type than the model names is refused, the field keeps its list
   ocs::tom::set(jp, JB_env_list, ocs::tom::create_raw_list(ocs::tom::VA::descriptor.e));
   const lList *kept_raw = ocs::tom::get<const lList *>(cjp, JB_env_list);
   const bool sub_refused =
      expect_reason("list", "free set, sub-type",
                    [&] { ocs::tom::set(jp, JB_env_list, ocs::tom::create_raw_list(ocs::tom::QU::descriptor.e)); },
                    Reason::WrongSubType) &&
      expect_reason("list", "raw handle set, sub-type",
                    [&] { handle.set(JB_env_list, ocs::tom::create_raw_list(ocs::tom::QU::descriptor.e)); },
                    Reason::WrongSubType) &&
      expect_reason("list", "exchange, sub-type",
                    [&] {
                       static_cast<void>(ocs::tom::exchange(jp, JB_env_list,
                                                            ocs::tom::create_raw_list(ocs::tom::QU::descriptor.e)));
                    },
                    Reason::WrongSubType);
   CHECK(17, "a list of another sub-type is refused with WrongSubType and the field keeps its list",
         sub_refused && ocs::tom::get<const lList *>(cjp, JB_env_list) == kept_raw);

   // the reasons of the list accessors
   const auto reduced = ocs::tom::create_raw_elem(ocs::tom::layout<JB_job_number>().get());
   lListElem *rp = reduced.get();
   const lListElem *crp = rp;
   const lListElem *null_ep = nullptr;
   const bool reasons =
      expect_reason("list", "get, null",
                    [&] { static_cast<void>(ocs::tom::get<ocs::tom::ConstRawList>(null_ep, JB_env_list)); },
                    Reason::NullElement) &&
      expect_reason("list", "get, foreign",
                    [&] { static_cast<void>(ocs::tom::get<ocs::tom::ConstRawList>(cjp, TEST_uint32)); },
                    Reason::ForeignField) &&
      expect_reason("list", "get, absent",
                    [&] { static_cast<void>(ocs::tom::get<const lList *>(crp, JB_env_list)); },
                    Reason::AbsentField) &&
      expect_reason("list", "get as an object",
                    [&] { static_cast<void>(ocs::tom::get<ocs::tom::ConstRawElement>(cjp, JB_env_list)); },
                    Reason::WrongType) &&
      expect_reason("list", "raw handle get as an object",
                    [&] { static_cast<void>(handle.get<lListElem *>(JB_env_list)); }, Reason::WrongType) &&
      expect_reason("list", "exchange a number field",
                    [&] {
                       static_cast<void>(ocs::tom::exchange(jp, JB_job_number,
                                                            ocs::tom::create_raw_list(ocs::tom::VA::descriptor.e)));
                    },
                    Reason::WrongType) &&
      expect_reason("list", "set, absent",
                    [&] { ocs::tom::set(rp, JB_env_list, ocs::tom::create_raw_list(ocs::tom::VA::descriptor.e)); },
                    Reason::AbsentField);
   CHECK(18, "the list accessors throw NullElement, ForeignField, AbsentField and WrongType", reasons);
}

/**
 * @brief has() and get_or(): a field the element does not hold is answered without a throw, a wrong type is not
 * @return nothing; CHECK records the results
 */
static void
test_soft() {
   const auto full = ocs::tom::create_raw_elem(ocs::tom::TEST::descriptor.e);
   const auto reduced = ocs::tom::create_raw_elem(reduced_test());
   ocs::tom::set(full.get(), TEST_uint32, std::uint32_t{3});
   ocs::tom::set(full.get(), TEST_string, "text");
   const lListElem *cep = full.get();
   const lListElem *crp = reduced.get();
   const lListElem *null_ep = nullptr;
   const ocs::tom::ConstRawElement handle(crp);

   const bool has = !ocs::tom::has(null_ep, TEST_uint32) && !ocs::tom::has(cep, JB_job_number) &&
                    !ocs::tom::has(crp, TEST_uint32) && ocs::tom::has(cep, TEST_uint32) &&
                    ocs::tom::has(crp, TEST_obj_any) && !ocs::tom::ConstRawElement().has(TEST_uint32) &&
                    !handle.has(TEST_uint32) && handle.has(TEST_obj_any);
   CHECK(19, "has() is false for a null element, a foreign and an absent field, true for a held one, in both forms",
         has);

   const bool fallback =
      ocs::tom::get_or<std::uint32_t>(null_ep, TEST_uint32, 7) == 7 &&
      ocs::tom::get_or<std::uint32_t>(cep, JB_job_number, 7) == 7 &&
      ocs::tom::get_or<std::uint32_t>(crp, TEST_uint32, 7) == 7 &&
      ocs::tom::get_or<std::uint32_t>(cep, TEST_uint32, 7) == 3 &&
      std::strcmp(ocs::tom::get_or<const char *>(crp, TEST_string, "none"), "none") == 0 &&
      std::strcmp(ocs::tom::get_or<const char *>(cep, TEST_string, "none"), "text") == 0 &&
      !ocs::tom::get_or<ocs::tom::ConstRawElement>(crp, TEST_obj_jb, nullptr) &&
      handle.get_or<std::uint32_t>(TEST_uint32, 7) == 7 &&
      ocs::tom::ConstRawElement().get_or<std::uint32_t>(TEST_uint32, 8) == 8;
   CHECK(20, "get_or() gives the fallback for a null element, a foreign and an absent field, the value otherwise",
         fallback);

   const bool wrong =
      expect_reason("soft", "get_or, wrong type",
                    [&] { static_cast<void>(ocs::tom::get_or<std::uint64_t>(cep, TEST_uint32, 0)); },
                    Reason::WrongType) &&
      expect_reason("soft", "get_or, wrong type of an absent field",
                    [&] { static_cast<void>(ocs::tom::get_or<std::uint64_t>(crp, TEST_uint32, 0)); },
                    Reason::WrongType) &&
      expect_reason("soft", "raw handle get_or, wrong type of an absent field",
                    [&] { static_cast<void>(handle.get_or<std::uint64_t>(TEST_uint32, 0)); }, Reason::WrongType) &&
      expect_reason("soft", "get_or, a list as a number",
                    [&] { static_cast<void>(ocs::tom::get_or<ocs::tom::ConstRawList>(cep, TEST_uint32, nullptr)); },
                    Reason::WrongType);
   CHECK(21, "get_or() still throws WrongType for a wrong value type, also on a field the element leaves out", wrong);
}

/**
 * @brief The order of the checks, and the reason of an empty handle in the typed and the raw form
 * @return nothing; CHECK records the results
 */
static void
test_order() {
   const auto reduced = ocs::tom::create_raw_elem(reduced_test());
   const lListElem *crp = reduced.get();
   // get checks the presence before the type: a wrong type on an absent field is AbsentField in every form
   const bool order =
      expect_reason("order", "free", [&] { static_cast<void>(ocs::tom::get<std::uint64_t>(crp, TEST_uint32)); },
                    Reason::AbsentField) &&
      expect_reason("order", "raw handle",
                    [&] { static_cast<void>(ocs::tom::ConstRawElement(crp).get<std::uint64_t>(TEST_uint32)); },
                    Reason::AbsentField) &&
      expect_reason("order", "typed member",
                    [&] {
                       static_cast<void>(
                          ocs::tom::ConstElement<ocs::tom::TEST>(crp).get<std::uint64_t>(TEST_uint32));
                    },
                    Reason::AbsentField);
   CHECK(22, "get() checks presence before type in the free, the raw and the typed form", order);

   // the typed handle reports its emptiness, the raw handle forwards nullptr to the free form
   const bool empty =
      expect_reason("empty", "typed",
                    [] { static_cast<void>(ocs::tom::ConstElement<ocs::tom::TEST>().get<std::uint32_t>(TEST_uint32)); },
                    Reason::EmptyHandle) &&
      expect_reason("empty", "raw",
                    [] { static_cast<void>(ocs::tom::ConstRawElement().get<std::uint32_t>(TEST_uint32)); },
                    Reason::NullElement);
   CHECK(23, "an empty typed handle throws EmptyHandle, an empty raw handle NullElement as the free form", empty);
}

/**
 * @brief get<V>() and find_first<V>() take a raw handle wherever they take the raw pointer
 *
 * A mutable handle converts to the const and to the mutable pointer, so without the handle overloads a
 * call with a handle could not choose between the pointer overloads. This test compiles every handle form
 * and checks that it gives what the pointer form gives.
 *
 * @return nothing; CHECK records the results
 */
static void
test_handle_arguments() {
   auto owner = ocs::tom::create_raw_list(ocs::tom::TEST::descriptor.e);
   auto elem = ocs::tom::create_raw_elem(ocs::tom::TEST::descriptor.e);
   elem->set(TEST_uint32, std::uint32_t{12});
   const ocs::tom::RawElement added = owner->push_back(std::move(elem));
   const ocs::tom::ConstRawElement cadded = added;
   const ocs::tom::RawList list = owner.get();
   const ocs::tom::ConstRawList clist = list;
   const auto reduced = ocs::tom::create_raw_elem(reduced_test());
   const ocs::tom::RawElement reduced_handle = reduced.get();

   static_assert(std::is_same_v<decltype(ocs::tom::find_first<std::uint32_t>(list, TEST_uint32, 12U)), lListElem *>);
   static_assert(
      std::is_same_v<decltype(ocs::tom::find_first<std::uint32_t>(clist, TEST_uint32, 12U)), const lListElem *>);
   const bool reads = ocs::tom::get<std::uint32_t>(cadded, TEST_uint32) == 12 &&
                      ocs::tom::get<std::uint32_t>(added, TEST_uint32) == 12 &&
                      ocs::tom::get<const lListElem *>(cadded, TEST_obj_jb) == nullptr &&
                      ocs::tom::get<lListElem *>(added, TEST_obj_jb) == nullptr &&
                      !ocs::tom::get<ocs::tom::RawElement>(added, TEST_obj_jb) &&
                      !ocs::tom::get<ocs::tom::ConstRawElement>(cadded, TEST_obj_jb);
   const bool finds = ocs::tom::find_first<std::uint32_t>(clist, TEST_uint32, 12U) == added.raw() &&
                      ocs::tom::find_first<std::uint32_t>(list, TEST_uint32, 12U) == added.raw() &&
                      ocs::tom::find_first(list, TEST_uint32, std::uint32_t{13}) == nullptr &&
                      ocs::tom::find_first<std::uint32_t>(ocs::tom::ConstRawList(), TEST_uint32, 12U) == nullptr;
   const bool reasons =
      expect_reason("handle", "get, empty read-only handle",
                    [] { static_cast<void>(ocs::tom::get<std::uint32_t>(ocs::tom::ConstRawElement(), TEST_uint32)); },
                    Reason::NullElement) &&
      expect_reason("handle", "get, empty mutable handle",
                    [] { static_cast<void>(ocs::tom::get<std::uint32_t>(ocs::tom::RawElement(), TEST_uint32)); },
                    Reason::NullElement) &&
      expect_reason("handle", "get, absent",
                    [&] { static_cast<void>(ocs::tom::get<std::uint64_t>(reduced_handle, TEST_uint32)); },
                    Reason::AbsentField) &&
      expect_reason("handle", "get, wrong type",
                    [&] { static_cast<void>(ocs::tom::get<std::uint64_t>(added, TEST_uint32)); }, Reason::WrongType) &&
      expect_reason("handle", "find_first, wrong type",
                    [&] {
                       static_cast<void>(ocs::tom::find_first<std::uint64_t>(clist, TEST_uint32, std::uint64_t{1}));
                    },
                    Reason::WrongType);
   // the counterpart of compile_fail/raw_set_typed_owner.cc: a typed owner goes to set() converted explicitly
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   const lListElem *job_raw = job.get().raw();
   ocs::tom::set(added, TEST_obj_jb, ocs::tom::RawElementPtr(std::move(job)));
   const bool explicit_set = !job && ocs::tom::get<const lListElem *>(cadded, TEST_obj_jb) == job_raw;
   CHECK(25, "get<V>() and find_first<V>() take a read-only and a mutable raw handle and give what the pointer "
             "forms give, with the same reasons; a typed owner goes to set() converted explicitly",
         reads && finds && reasons && explicit_set);
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
      {"RAW-ACC-01", "plain fields",
       "every plain field type reads and writes through the free accessor, the raw handle and the typed run-time "
       "member with the same values and the same reasons",
       {"for Bool, Uint32, Uint64, Int32, Int64, Double, Host, String and Ref: write and read in every form",
        "read and write with a null element, a foreign field, on a reduced element and with a wrong value type",
        "read with a wrong value type on a reduced element"},
       "the same values; NullElement, ForeignField, AbsentField and WrongType in every form; refused writes "
       "change nothing; AbsentField for the wrong value type on the reduced element in every form",
       {}, {1, 2, 3, 4, 5, 6, 7, 8, 9}, test_plain_fields},
      {"RAW-ACC-02", "object field",
       "an object field given at run time takes an element over, checks its sub-type and refuses a bound element",
       {"store, read, write through the mutable read, replace and clear a job in TEST_obj_jb",
        "store a queue into TEST_obj_jb and into TEST_obj_any", "store an element bound in a list",
        "take the element out with take_raw_object() and the method take()", "provoke every reason",
        "store a bound element so that another check fails first"},
       "the element is taken over and read in every form; WrongSubType, BoundElement (the element stays in its "
       "list); a taken element is free; NullElement, ForeignField, AbsentField, WrongType; the bound element "
       "stays in its list on every reason",
       {}, {10, 11, 12, 13, 14, 24}, test_object_field},
      {"RAW-ACC-03", "list field",
       "a list field given at run time takes a list over, exchanges it and checks its sub-type",
       {"store, read, append through the mutable read, replace and clear JB_env_list",
        "exchange the list in the free and the member form", "store a queue list", "provoke every reason"},
       "the list is taken over and read in every form; exchange hands the old list out; WrongSubType keeps the "
       "field; NullElement, ForeignField, AbsentField, WrongType",
       {}, {15, 16, 17, 18}, test_list_field},
      {"RAW-ACC-04", "soft accessors",
       "has() and get_or() answer for a field the element does not hold, get_or() refuses a wrong value type",
       {"ask has() for a null element, a foreign, an absent and a held field",
        "ask get_or() for the same and for strings and handles", "ask get_or() with a wrong value type"},
       "false and the fallback for every field not held; the value for a held one; WrongType for a wrong type",
       {}, {19, 20, 21}, test_soft},
      {"RAW-ACC-05", "order and empty handles",
       "the checks run in one order in every form; the typed and the raw handle report emptiness as specified",
       {"read an absent field with a wrong value type in every form", "read through an empty typed and raw handle"},
       "AbsentField in every form; EmptyHandle for the typed handle, NullElement for the raw one",
       {}, {22, 23}, test_order},
      {"RAW-ACC-06", "handle arguments",
       "get<V>() and find_first<V>() take a raw handle wherever they take the raw pointer",
       {"read and look up through read-only and mutable handles, empty handles, a reduced element"},
       "the values and elements of the pointer forms; NullElement, AbsentField, WrongType", {}, {25},
       test_handle_arguments},
   });
}
