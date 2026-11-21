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
 * @brief Module test of the typed element handles of the typed object model (CS-3043, HDL-01 to HDL-04, TRT-02)
 *
 * Also holds the correct counterparts of the sources in compile_fail/, which
 * the build compiles as expected failures (TRT-02): each of them is a line
 * below that does compile.
 */

#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "tom/ocs_Types.h"
#include "tom/detail/ocs_Hash.h"
#include "tom/detail/ocs_State.h"
#include "tom/ocs_Descr.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_Select.h"

#include "ocs_module_test.h"

using namespace ocs::tom;

// HDL-01: a handle is the size of a pointer and is copied like one
static_assert(sizeof(ConstElement<JB>) == sizeof(void *) && std::is_trivially_copyable_v<ConstElement<JB>>);
static_assert(sizeof(Element<JB>) == sizeof(void *) && std::is_trivially_copyable_v<Element<JB>>);
static_assert(sizeof(ConstList<JB>) == sizeof(void *) && std::is_trivially_copyable_v<ConstList<JB>>);
static_assert(sizeof(List<JB>) == sizeof(void *) && std::is_trivially_copyable_v<List<JB>>);
// constness: a mutable handle converts to the read-only one and to both pointers, never the other way round
static_assert(std::is_convertible_v<Element<JB>, ConstElement<JB>> && !std::is_convertible_v<ConstElement<JB>, Element<JB>>);
static_assert(std::is_convertible_v<Element<JB>, lListElem *> && std::is_convertible_v<Element<JB>, const lListElem *>);
static_assert(std::is_convertible_v<ConstElement<JB>, const lListElem *> && !std::is_convertible_v<ConstElement<JB>, lListElem *>);
// a raw pointer becomes a handle only explicitly
static_assert(!std::is_convertible_v<lListElem *, Element<JB>> && std::is_constructible_v<Element<JB>, lListElem *>);

/**
 * @brief One of an overload pair, taking the read-only handle
 * @param job the handle
 * @return 1, which says that this overload was chosen
 */
static int pick(ConstElement<JB> job) {
   return job ? 1 : -1;
}

/**
 * @brief One of an overload pair, taking the raw pointer
 * @param job the element
 * @return 2, which says that this overload was chosen
 */
static int pick(const lListElem *job) {
   return job != nullptr ? 2 : -2;
}

/**
 * @brief A function of the old interface's kind, taking a raw pointer
 * @param job the element
 * @return its job number
 */
static std::uint32_t old_style(const lListElem *job) {
   return ocs::tom::get<JB_job_number>(job);
}

/**
 * @brief Whether a call throws a FieldException of a reason, with a message naming a text
 * @param call the call
 * @param reason the expected reason
 * @param text a text the message must contain, e.g. the field's name
 * @return true if it throws as expected
 */
template <typename Call>
static bool throws_field_exception(Call &&call, const FieldException::Reason reason, const char *text) {
   try {
      call();
   } catch (const FieldException &e) {
      if (e.reason() != reason || std::strstr(e.what(), text) == nullptr) {
         std::printf("unexpected error: %s\n", e.what());
         return false;
      }
      return true;
   }
   return false;
}

/**
 * @brief HDL-01: handles convert, compare and resolve overloads like pointers
 * @return nothing; CHECK records the results
 */
static void
test_pointer_like() {
   auto owner = ElementPtr<JB>::create();
   owner->set<JB_job_number>(42u);
   lListElem *raw = owner.get().raw();
   const Element<JB> h{raw};
   const Element<JB> h1 = h;
   const Element<JB> h2{nullptr};

   CHECK(1, "comparisons with nullptr and between handles", h != nullptr && !(h == nullptr) && h1 == h && h2 == nullptr &&
         !h2 && static_cast<bool>(h));
   CHECK(2, "a handle is passed to a function taking a raw pointer", old_style(h) == 42);
   CHECK(3, "an overload pair resolves to the read-only handle, and to the pointer for a pointer",
         pick(h) == 1 && pick(static_cast<const lListElem *>(raw)) == 2);
   const ConstElement<JB> c = h;
   CHECK(4, "the read-only handle holds the same element", c == h && c.raw() == raw);
}

/**
 * @brief HDL-02: the template accessors, the run-time accessors and the free accessors agree, on a full and a
 *        reduced element
 * @return nothing; CHECK records the results
 */
static void
test_accessors() {
   auto list = ListPtr<TEST>::create("test");
   lListElem *raw = list->push_back(ElementPtr<TEST>::create()).raw();
   const Element<TEST> t{raw};

   t.set<TEST_bool>(true);
   t.set<TEST_uint32>(4000000000u);
   t.set<TEST_uint64>(UINT64_C(1) << 40);
   t.set<TEST_int>(-7);
   t.set<TEST_long>(-(INT64_C(1) << 40));
   t.set<TEST_double>(2.5);
   t.set<TEST_host>(Host{"host1"});
   t.set<TEST_string>("value");
   int ref_target = 0;
   t.set<TEST_ref>(&ref_target);

   const bool template_path = t.get<TEST_bool>() && t.get<TEST_uint32>() == 4000000000u &&
                              t.get<TEST_uint64>() == (UINT64_C(1) << 40) && t.get<TEST_int>() == -7 &&
                              t.get<TEST_long>() == -(INT64_C(1) << 40) && t.get<TEST_double>() == 2.5 &&
                              t.get<TEST_host>() == "HOST1" && std::strcmp(t.get<TEST_string>(), "value") == 0 &&
                              t.get<TEST_ref>() == &ref_target;
   CHECK(5, "set<F>() and get<F>() on every field type", template_path);

   // the free getters on the raw pointer replaced the old getters; the host name reads back as written, as
   // lGetHost() gave it
   const lListElem *craw = raw;
   const bool free_path = ocs::tom::get<TEST_bool>(craw) && ocs::tom::get<TEST_uint32>(craw) == 4000000000u &&
                          ocs::tom::get<TEST_uint64>(craw) == (UINT64_C(1) << 40) &&
                          ocs::tom::get<TEST_int>(craw) == -7 &&
                          ocs::tom::get<TEST_long>(craw) == -(INT64_C(1) << 40) &&
                          ocs::tom::get<TEST_double>(craw) == 2.5 &&
                          std::strcmp(ocs::tom::get<TEST_host>(craw).c_str(), "host1") == 0 &&
                          std::strcmp(ocs::tom::get<TEST_string>(craw), "value") == 0 &&
                          ocs::tom::get<TEST_ref>(craw) == &ref_target;
   CHECK(6, "the free getters on the raw pointer read what set<F>() wrote", free_path);

   ocs::tom::set<TEST_uint32>(raw, 17u);
   ocs::tom::set<TEST_string>(raw, "other");
   t.set<std::int32_t>(TEST_int, 9);
   const bool runtime_path = t.get<std::uint32_t>(TEST_uint32) == 17 &&
                             std::strcmp(t.get<const char *>(TEST_string), "other") == 0 &&
                             ocs::tom::get<TEST_int>(craw) == 9 && t.get<Host>(TEST_host) == "host1";
   CHECK(7, "get(nm) and set(nm, v) agree with the free accessors", runtime_path);

   // a reduced element, as a what selects it
   auto reduced = list->select(what<TEST_uint32, TEST_string, TEST_double>(), "reduced");
   const ConstElement<TEST> r = reduced->front();
   CHECK(8, "the template accessors read a reduced element",
         r.get<TEST_uint32>() == 17 && std::strcmp(r.get<TEST_string>(), "other") == 0 && r.get<TEST_double>() == 2.5);
   const bool presence = r.has<TEST_uint32>() && r.has<TEST_double>() && !r.has<TEST_int>() && !r.has<TEST_host>() &&
                         r.has<TEST_uint32>() == (present_pos(r.descr(), TEST_uint32) >= 0) &&
                         r.has<TEST_int>() == (present_pos(r.descr(), TEST_int) >= 0) && t.has<TEST_int>();
   CHECK(9, "has<F>() agrees with the field's presence in the descriptor", presence);
}

/**
 * @brief HDL-03: the run-time errors of the handles throw, and the free setter keeps the old setter's behaviour
 * @return nothing; CHECK records the results
 */
static void
test_errors() {
   auto list = ListPtr<TEST>::create("test");
   list->push_back(ElementPtr<TEST>::create());
   auto reduced = list->select(what<TEST_uint32>(), "reduced");
   lListElem *r = reduced->front().raw();
   auto queue_owner = ElementPtr<QU>::create();
   lListElem *queue = queue_owner.get().raw();

   CHECK(10, "reading an absent field throws, naming field and type",
         throws_field_exception([&] { static_cast<void>(ConstElement<TEST>{r}.get<TEST_string>()); },
                            FieldException::Reason::AbsentField, "TEST_string"));
   CHECK(11, "access through an empty handle throws",
         throws_field_exception([&] { static_cast<void>(ConstElement<TEST>{}.get<TEST_uint32>()); },
                            FieldException::Reason::EmptyHandle, "TEST_uint32"));
   CHECK(12, "a job handle made from a queue element throws, naming both types",
         throws_field_exception([&] { static_cast<void>(Element<JB>{queue}); }, FieldException::Reason::WrongType, "QU"));
   CHECK(13, "the free getter with a job field on a queue element throws",
         throws_field_exception([&] { static_cast<void>(ocs::tom::get<JB_job_name>(queue)); },
                            FieldException::Reason::ForeignField, "JB_job_name"));

   // the free setter on a null element: -1 and LEELEMNULL, what the old setter gave
   ocs::tom::detail::state_set_lerrno(0);
   const int new_null = ocs::tom::set<TEST_uint32>(nullptr, 1u);
   CHECK(14, "a null element: -1 and LEELEMNULL, like the old setter",
         new_null == -1 && ocs::tom::detail::state_get_lerrno() == LEELEMNULL);
   ocs::tom::detail::state_set_lerrno(0);
   const int new_null_str = ocs::tom::set<TEST_string>(nullptr, "x");
   CHECK(15, "a null element for a string field: -1 and LEELEMNULL",
         new_null_str == -1 && ocs::tom::detail::state_get_lerrno() == LEELEMNULL);

   // an absent number field: the free setter throws, where the old setter aborted, as for every field type since
   // CS-3062 (N-L-4); before, both returned -1, with LENEGPOS for an int field and without one for a uint64 field
   CHECK(16, "an absent int field: the free setter throws",
         throws_field_exception([&] { ocs::tom::set<TEST_int>(r, 1); }, FieldException::Reason::AbsentField, "TEST_int"));
   CHECK(17, "an absent uint64 field: the free setter throws",
         throws_field_exception([&] { ocs::tom::set<TEST_uint64>(r, UINT64_C(1)); }, FieldException::Reason::AbsentField,
                                "TEST_uint64"));

   // an absent string field: the free setter throws, where the old setter aborted
   // CHECK 18 removed: lSetString() has no TOM counterpart for its abort (CS-3103)
   CHECK(19, "an absent string field: the free setter throws",
         throws_field_exception([&] { ocs::tom::set<TEST_string>(r, "x"); }, FieldException::Reason::AbsentField, "TEST_string"));
   CHECK(20, "the handle's setter throws on an absent number field",
         throws_field_exception([&] { Element<TEST>{r}.set<TEST_int>(1); }, FieldException::Reason::AbsentField, "TEST_int"));

   // the object a field holds already is set again without an error since CS-3062 (N-L-4)
   auto full_owner = ElementPtr<TEST>::create();
   lListElem *full = full_owner.get().raw();
   lListElem *held = ElementPtr<JB>::create().release();
   const bool same = ocs::tom::set<TEST_obj_jb>(full, held) == 0 && ocs::tom::set<TEST_obj_jb>(full, held) == 0 &&
                     full_owner->get<TEST_obj_jb>().raw() == held;
   CHECK(24, "the free setter sets the object a field holds again without an error", same);
}

/**
 * @brief HDL-04: the setters maintain the hash table of the owning list
 * @return nothing; CHECK records the results
 */
static void
test_hash_tables() {
   ListPtr<TEST> list = ListPtr<TEST>::create("hashed");
   for (std::uint32_t i = 0; i < 40; i++) {
      auto element = ElementPtr<TEST>::create();
      element.get().set<TEST_uint32>(i);
      element.get().set<TEST_int>(static_cast<std::int32_t>(i));
      list.get().push_back(std::move(element));
   }
   // the first lookup builds the table
   const Element<TEST> five = list.get().find_first<TEST_uint32>(5u);
   five.set<TEST_uint32>(1000u);
   five.set<TEST_int>(-5);
   CHECK(21, "after a change of a hashed field, the lookup finds the new key and not the old one",
         list.get().find_first<TEST_uint32>(1000u) == five && !list.get().find_first<TEST_uint32>(5u) &&
         static_cast<const lListElem *>(ocs::tom::find_first<TEST_uint32>(list.get().raw(), 1000u)) == five.raw());
   CHECK(22, "a change of an unhashed field leaves the lookups alone",
         list.get().find_first<TEST_uint32>(1000u).get<TEST_int>() == -5 && list.get().find_first<TEST_uint32>(6u));
}

/**
 * @brief The owning handles create a list and an element with a given descriptor, checking that it belongs to
 *        their type
 * @return nothing; CHECK records the results
 */
static void
test_create_with_descriptor() {
   const lDescr *reduced = ocs::tom::descr_reduce(ocs::tom::JB::descriptor.e, {JB_job_number, JB_job_name});

   auto full_list = ListPtr<JB>::create("full", ocs::tom::JB::descriptor.e);
   auto full_job = ElementPtr<JB>::create(ocs::tom::JB::descriptor.e);
   CHECK(25, "with the full descriptor: the list has it and the name, the element has it",
         full_list->descr() == ocs::tom::JB::descriptor.e && std::strcmp(full_list->name(), "full") == 0 &&
         full_job->descr() == ocs::tom::JB::descriptor.e);

   auto reduced_list = ListPtr<JB>::create("reduced", reduced, Hashing::Off);
   auto reduced_job = ElementPtr<JB>::create(reduced);
   reduced_job.get().set<JB_job_number>(7u);
   const auto pushed = reduced_list.get().push_back(std::move(reduced_job));
   CHECK(26, "with a reduced descriptor: list and element have it and hold the fields it keeps",
         reduced != nullptr && reduced_list->descr() == reduced && std::strcmp(reduced_list->name(), "reduced") == 0 &&
         pushed.descr() == reduced &&
         pushed.get<JB_job_number>() == 7 && !pushed.has<JB_owner>());

   CHECK(27, "with the descriptor of another type: list and element throw, naming the type",
         throws_field_exception([] { static_cast<void>(ListPtr<JB>::create("queue", ocs::tom::QU::descriptor.e)); },
                                FieldException::Reason::WrongType, "QU") &&
         throws_field_exception([] { static_cast<void>(ElementPtr<JB>::create(ocs::tom::QU::descriptor.e)); },
                                FieldException::Reason::WrongType, "QU"));
   CHECK(28, "without a descriptor: list and element throw",
         throws_field_exception([] { static_cast<void>(ListPtr<JB>::create("none", nullptr)); },
                                FieldException::Reason::NullDescriptor, "") &&
         throws_field_exception([] { static_cast<void>(ElementPtr<JB>::create(nullptr)); },
                                FieldException::Reason::NullDescriptor, ""));

   // the list holds its own reference on the reduced descriptor, so the caller's is released independently
   ocs::tom::release_descr(reduced);
}

/**
 * @brief An element linked into a list must have the layout of the list's descriptor (CS-3112)
 *
 * Elements are created with the descriptor of a list that exists already, and
 * an element or a list of another layout is refused instead of borrowing a
 * descriptor that gives its values other slots.
 *
 * @return nothing; CHECK records the results
 */
static void
test_layout() {
   const lDescr *reduced = ocs::tom::descr_reduce(ocs::tom::JB::descriptor.e, {JB_job_number, JB_job_name});

   // emplace_back and the free emplace_back (which replaced lAddElemUlong) on an existing reduced list create the
   // element with the list's descriptor
   auto reduced_list = ListPtr<JB>::create("reduced", reduced);
   const Element<JB> emplaced = reduced_list.get().emplace_back<JB_job_number>(7u);
   lList *other_list = ListPtr<JB>::create("other reduced", reduced).release();
   const lListElem *added = ocs::tom::emplace_back<JB_job_number>(&other_list, 8u);
   const ListPtr<JB> other_owner(other_list);
   CHECK(29, "emplace_back and the free emplace_back on a reduced list give an element with the list's descriptor",
         emplaced.descr() == reduced_list->descr() && emplaced.get<JB_job_number>() == 7 &&
         !emplaced.has<JB_owner>() && reduced_list.get().find_first<JB_job_number>(7u) == emplaced &&
         added != nullptr && ConstRawElement{added}.descr() == other_owner->descr() &&
         ocs::tom::get<JB_job_number>(added) == 8 && other_owner->find_first<JB_job_number>(8u).raw() == added);

   // a full element into a reduced list, and a reduced element into a full list, are refused and kept
   auto full_list = ListPtr<JB>::create("full");
   auto full_job = ElementPtr<JB>::create();
   auto reduced_job = ElementPtr<JB>::create(reduced);
   const bool refused =
      throws_field_exception([&] { static_cast<void>(reduced_list.get().push_back(std::move(full_job))); },
                             FieldException::Reason::WrongLayout, "layout") &&
      throws_field_exception([&] { static_cast<void>(reduced_list.get().push_front(std::move(full_job))); },
                             FieldException::Reason::WrongLayout, "layout") &&
      throws_field_exception([&] { static_cast<void>(full_list.get().push_back(std::move(reduced_job))); },
                             FieldException::Reason::WrongLayout, "layout");
   CHECK(30, "push_back and push_front of an element of another layout throw WrongLayout and keep the element",
         refused && full_job && reduced_job && reduced_list.get().size() == 1 && full_list.get().size() == 0);

   // the free forms that replaced lAppendElem() and lInsertElem() throw, where the old functions aborted
   CHECK(31, "the free append and insert_after of an element of another layout throw WrongLayout and keep the element",
         throws_field_exception([&] { ocs::tom::append(reduced_list.get().raw(), std::move(full_job)); },
                                FieldException::Reason::WrongLayout, "layout") &&
         throws_field_exception([&] { ocs::tom::insert_after(full_list.get().raw(), nullptr, std::move(reduced_job)); },
                                FieldException::Reason::WrongLayout, "layout") &&
         full_job && reduced_job && reduced_list.get().size() == 1 && full_list.get().size() == 0);

   // the same pointer, and an equal descriptor at another address, are accepted
   reduced_job.get().set<JB_job_number>(9u);
   const Element<JB> same = reduced_list.get().push_back(std::move(reduced_job));
   const lDescr *copy = ocs::tom::descr_private(reduced);
   auto copied_job = ElementPtr<JB>::create(copy);
   const bool other_address = copied_job->descr() != reduced_list->descr();
   copied_job.get().set<JB_job_number>(10u);
   const Element<JB> equal = reduced_list.get().push_back(std::move(copied_job));
   ocs::tom::release_descr(copy);
   full_list.get().push_back(std::move(full_job));
   CHECK(32, "an element with the list's descriptor or an equal one at another address is linked in",
         other_address && reduced_list.get().size() == 3 && full_list.get().size() == 1 &&
         same.descr() == reduced_list->descr() && equal.descr() == reduced_list->descr() &&
         equal.get<JB_job_number>() == 10 &&
         reduced_list.get().find_first<JB_job_number>(10u) == equal);

   // whole lists: splice and the raw splice compare the layouts as well; lAppendList() returned -1 here, its
   // successor splice() throws
   auto more_full = ListPtr<JB>::create("more full");
   more_full.get().emplace_back<JB_job_number>(11u);
   auto other_typed = ListPtr<JB>::create("other full");
   other_typed->emplace_back<JB_job_number>(12u);
   RawListPtr other_full(std::move(other_typed));
   CHECK(33, "splice and the raw splice between a reduced and a full list refuse, moving nothing",
         throws_field_exception([&] { reduced_list.get().splice(more_full.get()); },
                                FieldException::Reason::WrongLayout, "layout") &&
         throws_field_exception([&] { ocs::tom::splice(reduced_list.get().raw(), std::move(other_full)); },
                                FieldException::Reason::WrongLayout, "layout") &&
         reduced_list.get().size() == 3 && more_full.get().size() == 1 && other_full && other_full->size() == 1);

   // a list that got a hash table later holds a private copy of the type's descriptor whose flags differ
   // (CULL_HASH); elements of the type itself still have its layout and must be linked in
   auto hashed = ListPtr<JB>::create("hashed", Hashing::Off);
   const bool hash_created = ocs::tom::detail::hash_new(hashed.get().raw(), JB_owner, false) == 1;
   const bool private_copy = hashed->descr() != ocs::tom::JB::descriptor.e;
   auto plain_owner = ElementPtr<JB>::create();
   plain_owner->set<JB_job_number>(13u);
   plain_owner->set<JB_owner>("someone");
   const lListElem *plain_job = plain_owner.get().raw();
   // the free append on the raw list, which replaced lAppendElem(), and the typed push_back
   ocs::tom::append(hashed.get().raw(), std::move(plain_owner));
   auto typed_job = ElementPtr<JB>::create();
   typed_job.get().set<JB_job_number>(14u);
   const Element<JB> typed = hashed->push_back(std::move(typed_job));
   CHECK(34, "elements of the full type are linked into a list whose descriptor is a private copy with hash "
             "flags",
         hash_created && private_copy && hashed->size() == 2 &&
         hashed->find_first<JB_owner>("someone").raw() == plain_job && typed.get<JB_job_number>() == 14);

   ocs::tom::release_descr(reduced);
}

/**
 * @brief TRT-02: the correct counterparts of the sources the build must fail to compile
 * @return nothing; CHECK records the results
 */
static void
test_correct_counterparts() {
   auto job_owner = ElementPtr<JB>::create();
   auto queue_owner = ElementPtr<QU>::create();
   lListElem *raw = job_owner.get().raw();
   lListElem *raw_queue = queue_owner.get().raw();
   const Element<JB> job{raw};
   const ConstElement<JB> const_job = job;
   const Element<QU> queue{raw_queue};

   job.set<JB_job_name>("name");
   const char *name = job.get<JB_job_name>();                // compile_fail/string_into_number.cc
   const char *qname = queue.get<QU_qname>();               // compile_fail/job_field_on_queue.cc
   const Element<JB> from_raw{raw};                          // compile_fail/raw_pointer_assigned.cc
   job.set<JB_job_number>(1u);                              // compile_fail/setter_on_const.cc
   const lListElem *const_raw = const_job;                   // compile_fail/const_to_mutable_pointer.cc
   CHECK(23, "the correct counterparts compile and work",
         std::strcmp(name, "name") == 0 && qname == nullptr && from_raw == job && const_raw == raw &&
         const_job.get<JB_job_number>() == 1);
}

/**
 * @brief Add to a 32 bit, a 64 bit and a double field through the handle
 * @return nothing; CHECK records the results
 */
static void
test_add() {
   ElementPtr<JAT> task = ElementPtr<JAT>::create();
   task->set<JAT_status>(1);
   task->set<JAT_start_time>(100);
   task->set<JAT_tix>(1.25);
   task->add<JAT_status>(2);
   task->add<JAT_start_time>(1000);
   task->add<JAT_tix>(0.5);
   // an offset of 0 changes nothing
   task->add<JAT_status>(0);
   CHECK(35, "add<F> adds to a 32 bit, a 64 bit and a double field, an offset of 0 changes nothing",
         task->get<JAT_status>() == 3 && task->get<JAT_start_time>() == 1100 && task->get<JAT_tix>() == 1.75);
}

/**
 * @brief The free form ocs::tom::add<F>(ep, offset) on raw elements, with the error behaviour of the old adders
 * @return nothing; CHECK records the results
 */
static void
test_free_add() {
   ListPtr<JAT> tasks = ListPtr<JAT>::create("tasks");
   lListElem *full = tasks->push_back(ElementPtr<JAT>::create()).raw();
   std::vector<int> seen;
   for (lListElem *ep : {static_cast<lListElem *>(nullptr), full}) {
      ocs::tom::detail::state_set_lerrno(0);
      seen.push_back(ocs::tom::add<JAT_status>(ep, 1));
      seen.push_back(ocs::tom::detail::state_get_lerrno());
      ocs::tom::detail::state_set_lerrno(0);
      seen.push_back(ocs::tom::add<JAT_tix>(ep, 1.0));
      seen.push_back(ocs::tom::detail::state_get_lerrno());
   }
   seen.push_back(static_cast<int>(tasks->front().get<JAT_status>()));

   // a field a reduced element does not hold throws AbsentField
   ElementPtr<JAT> reduced = ElementPtr<JAT>::create(ocs::tom::layout<JAT_task_number>());
   int absent_throws = 0;
   for (int i = 0; i < 2; i++) {
      try {
         static_cast<void>(i == 0 ? ocs::tom::add<JAT_status>(reduced.get().raw(), 1)
                                  : ocs::tom::add<JAT_tix>(reduced.get().raw(), 1.0));
      } catch (const FieldException &e) {
         absent_throws += e.reason() == FieldException::Reason::AbsentField ? 1 : 0;
      }
   }
   // a null element: -1 with LEELEMNULL; a full element: 0 without an error number, the status 0 + 1
   CHECK(36, "ocs::tom::add<F>(ep, offset) gives -1 with LEELEMNULL on a null element, 0 on a full one, and throws "
             "AbsentField on a field the element does not hold",
         seen == std::vector<int>({-1, LEELEMNULL, -1, LEELEMNULL, 0, 0, 0, 0, 1}) && absent_throws == 2);
}

/**
 * @brief Cloning an empty element handle gives an empty owning handle
 * @return nothing; CHECK records the results
 */
static void
test_clone_empty() {
   const Element<JB> no_job;
   const ConstElement<JB> no_const_job;
   CHECK(37, "clone of an empty element handle gives an empty owning handle", !no_job.clone() && !no_const_job.clone());
}

// the members on a field without a sub-type give raw handles; get_as() gives the typed views
static_assert(std::is_same_v<decltype(std::declval<ConstElement<SPA>>().get<SPA_argval_lListT>()), ConstRawList>);
static_assert(std::is_same_v<decltype(std::declval<Element<SPA>>().get<SPA_argval_lListT>()), RawList>);
static_assert(std::is_same_v<decltype(std::declval<ConstElement<TEST>>().get<TEST_obj_any>()), ConstRawElement>);
static_assert(std::is_same_v<decltype(std::declval<Element<TEST>>().get<TEST_obj_any>()), RawElement>);
static_assert(std::is_same_v<decltype(std::declval<ConstElement<SPA>>().get_as<SPA_argval_lListT, PN>()),
                             ConstList<PN>>);
static_assert(std::is_same_v<decltype(std::declval<Element<SPA>>().get_as<SPA_argval_lListT, PN>()), List<PN>>);
static_assert(std::is_same_v<decltype(std::declval<ConstElement<TEST>>().get_as<TEST_obj_any, JB>()),
                             ConstElement<JB>>);
static_assert(std::is_same_v<decltype(std::declval<Element<TEST>>().get_as<TEST_obj_any, JB>()), Element<JB>>);
// a field with a sub-type keeps its typed getter
static_assert(std::is_same_v<decltype(std::declval<Element<TEST>>().get<TEST_obj_jb>()), Element<JB>>);

/**
 * @brief The members on a list field and an object field for which the model names no sub-type: set and exchange
 *        take owners of any type, get gives a raw handle, get_as a checked typed view
 * @return nothing; CHECK records the results
 */
static void
test_no_sub_type() {
   // the list field SPA_argval_lListT takes a list of any type
   ElementPtr<SPA> opt = ElementPtr<SPA>::create();
   ListPtr<PN> paths = ListPtr<PN>::create("paths");
   paths->emplace_back<PN_path>("/tmp/out");
   const lList *paths_raw = paths.get().raw();
   opt->set<SPA_argval_lListT>(std::move(paths));
   const bool list_pn = !paths && opt->get<SPA_argval_lListT>().raw() == paths_raw &&
                        std::strcmp(opt->get_as<SPA_argval_lListT, PN>().front().get<PN_path>(), "/tmp/out") == 0;
   ListPtr<ST> names = ListPtr<ST>::create("names");
   names->emplace_back<ST_name>("a");
   names->emplace_back<ST_name>("b");
   opt->set<SPA_argval_lListT>(std::move(names));   // frees the path list
   const bool list_st = !names && opt->get_as<SPA_argval_lListT, ST>().size() == 2;

   // the object field TEST_obj_any takes an element of any type
   ElementPtr<TEST> test = ElementPtr<TEST>::create();
   ElementPtr<JB> job = ElementPtr<JB>::create();
   job->set<JB_job_number>(7u);
   test->set<TEST_obj_any>(std::move(job));
   const bool obj_jb = !job && test->get_as<TEST_obj_any, JB>().get<JB_job_number>() == 7;
   ElementPtr<QU> queue = ElementPtr<QU>::create();
   queue->set<QU_qname>("all.q");
   test->set<TEST_obj_any>(std::move(queue));   // frees the job
   const bool obj_qu = !queue && std::strcmp(test->get_as<TEST_obj_any, QU>().get<QU_qname>(), "all.q") == 0;
   CHECK(38, "set takes an owner of any type into a list field and an object field without a sub-type",
         list_pn && list_st && obj_jb && obj_qu);

   // exchange gives back the content held before as a raw owner
   ListPtr<PN> more = ListPtr<PN>::create("more");
   more->emplace_back<PN_path>("/tmp/err");
   RawListPtr old_list = opt->exchange<SPA_argval_lListT>(std::move(more));
   const ListPtr<ST> old_names = adopt<ST>(std::move(old_list));
   ElementPtr<JB> other_job = ElementPtr<JB>::create();
   RawElementPtr old_elem = test->exchange<TEST_obj_any>(std::move(other_job));
   const ElementPtr<QU> old_queue = adopt<QU>(std::move(old_elem));
   CHECK(39, "exchange puts in the new list or element and gives back the one held before, free",
         (!more && old_names.get().size() == 2 && opt->get_as<SPA_argval_lListT, PN>().size() == 1 && !other_job &&
          std::strcmp(old_queue->get<QU_qname>(), "all.q") == 0 && is_free(old_queue.get()) &&
          test->get_as<TEST_obj_any, JB>()));

   // get gives a raw handle on what the field holds, read-only through a read-only handle
   const ConstElement<SPA> const_opt = opt.get();
   const ConstRawList const_list = const_opt.get<SPA_argval_lListT>();
   const RawList list = opt->get<SPA_argval_lListT>();
   const ConstElement<TEST> const_test = test.get();
   const ConstRawElement const_elem = const_test.get<TEST_obj_any>();
   const RawElement elem = test->get<TEST_obj_any>();
   CHECK(40, "get gives a raw handle on the list or element the field holds",
         const_list.raw() == list.raw() && const_list.size() == 1 && const_elem.raw() == elem.raw() &&
         elem.get<std::uint32_t>(JB_job_number) == 0);

   // get_as checks the type: the right one gives the view, another one throws WrongType
   const Element<JB> job_view = test->get_as<TEST_obj_any, JB>();
   job_view.set<JB_job_number>(11u);
   const bool right = const_test.get_as<TEST_obj_any, JB>().get<JB_job_number>() == 11 &&
                      const_opt.get_as<SPA_argval_lListT, PN>().front().get<PN_path>() != nullptr;
   const bool wrong_list = throws_field_exception([&] { static_cast<void>(opt->get_as<SPA_argval_lListT, ST>()); },
                                                  FieldException::Reason::WrongType, "as type ST");
   const bool wrong_const_list = throws_field_exception(
           [&] { static_cast<void>(const_opt.get_as<SPA_argval_lListT, JB>()); }, FieldException::Reason::WrongType,
           "as type JB");
   const bool wrong_elem = throws_field_exception([&] { static_cast<void>(test->get_as<TEST_obj_any, QU>()); },
                                                  FieldException::Reason::WrongType, "as type QU");
   CHECK(41, "get_as gives the typed view for the type the field holds and throws WrongType for another type",
         right && wrong_list && wrong_const_list && wrong_elem);

   // an empty owner empties the field; an empty field gives an empty view and an empty raw handle
   opt->set<SPA_argval_lListT>(ListPtr<PN>{});
   const RawElementPtr last = test->exchange<TEST_obj_any>(ElementPtr<JB>{});
   const ElementPtr<SPA> unset = ElementPtr<SPA>::create();
   CHECK(42, "an empty field gives an empty view and an empty raw handle; an empty owner empties the field",
         (!opt->get<SPA_argval_lListT>() && !opt->get_as<SPA_argval_lListT, ST>() && last &&
          !test->get<TEST_obj_any>() && !const_test.get_as<TEST_obj_any, QU>() &&
          !unset->get_as<SPA_argval_lListT, PN>()));

   // the contract of all members: EmptyHandle on an empty handle, AbsentField on a reduced element
   const Element<SPA> no_opt;
   const ConstElement<TEST> no_test;
   const ElementPtr<SPA> reduced = ElementPtr<SPA>::create(layout<SPA_switch_val>());
   ListPtr<PN> kept = ListPtr<PN>::create("kept");
   const bool empty = throws_field_exception([&] { static_cast<void>(no_opt.get_as<SPA_argval_lListT, PN>()); },
                                             FieldException::Reason::EmptyHandle, "SPA_argval_lListT") &&
                      throws_field_exception([&] { static_cast<void>(no_test.get_as<TEST_obj_any, JB>()); },
                                             FieldException::Reason::EmptyHandle, "TEST_obj_any") &&
                      throws_field_exception([&] { no_opt.set<SPA_argval_lListT>(std::move(kept)); },
                                             FieldException::Reason::EmptyHandle, "SPA_argval_lListT") &&
                      kept;   // the list stays with the caller
   const bool absent = throws_field_exception([&] { static_cast<void>(reduced->get<SPA_argval_lListT>()); },
                                              FieldException::Reason::AbsentField, "SPA_argval_lListT") &&
                       throws_field_exception([&] { static_cast<void>(reduced->get_as<SPA_argval_lListT, PN>()); },
                                              FieldException::Reason::AbsentField, "SPA_argval_lListT") &&
                       throws_field_exception(
                               [&] { static_cast<void>(reduced->exchange<SPA_argval_lListT>(std::move(kept))); },
                               FieldException::Reason::AbsentField, "SPA_argval_lListT") &&
                       kept;
   // an element bound in a list is refused, the owner gives it up and the element stays in its list
   const ListPtr<JB> jobs = ListPtr<JB>::create("jobs");
   ElementPtr<JB> bound = ElementPtr<JB>::from_typed(jobs->emplace_back<JB_job_number>(1u).raw());
   const bool refused = throws_field_exception([&] { test->set<TEST_obj_any>(std::move(bound)); },
                                               FieldException::Reason::BoundElement, "type TEST ") &&
                        !bound && jobs.get().size() == 1 && !test->get<TEST_obj_any>();
   CHECK(43, "an empty handle throws EmptyHandle, a reduced element without the field AbsentField, a bound element "
             "BoundElement; a list or a free element stays with the caller",
         empty && absent && refused);
}

// a raw pointer and nullptr still reach the raw setter of a field without a sub-type: the setters taking owners
// cannot deduce the owner's type from them
static_assert(requires(const Element<SPA> opt, lList *lp) {
   opt.set<SPA_argval_lListT>(nullptr);
   opt.set<SPA_argval_lListT>(lp);
});
static_assert(requires(const Element<TEST> test, lListElem *ep) {
   test.set<TEST_obj_any>(nullptr);
   test.set<TEST_obj_any>(ep);
});
static_assert(std::is_same_v<detail::SetterArg<SPA_argval_lListT>::type, lList *>);
static_assert(std::is_same_v<detail::SetterArg<TEST_obj_any>::type, lListElem *>);
// the raw setter is the one with the raw pointer parameter
[[maybe_unused]] static constexpr void (Element<SPA>::*raw_list_setter)(lList *) const =
        &Element<SPA>::set<SPA_argval_lListT>;
[[maybe_unused]] static constexpr void (Element<TEST>::*raw_object_setter)(lListElem *) const =
        &Element<TEST>::set<TEST_obj_any>;

/**
 * @brief The edges of the members on a field without a sub-type: a reduced element without the field, an
 *        element bound elsewhere or in the field itself, contents with a reduced layout, and the raw setter
 * @return nothing; CHECK records the results
 */
static void
test_no_sub_type_edges() {
   // set(ElementPtr) on a reduced element that does not hold the object field: AbsentField, the element stays
   const ElementPtr<TEST> reduced = ElementPtr<TEST>::create(layout<TEST_uint32>());
   ElementPtr<JB> job = ElementPtr<JB>::create();
   job->set<JB_job_number>(3u);
   const lListElem *job_raw = job.get().raw();
   const bool absent = throws_field_exception([&] { reduced->set<TEST_obj_any>(std::move(job)); },
                                              FieldException::Reason::AbsentField, "TEST_obj_any") &&
                       job && job.get().raw() == job_raw && job->get<JB_job_number>() == 3 &&
                       is_free(job.get());
   CHECK(44, "set of an element into a reduced element without the object field throws AbsentField, the element "
             "stays with the caller", absent);

   // exchange with an element bound in a list: BoundElement, the element stays in its list, the field unchanged
   ElementPtr<TEST> test = ElementPtr<TEST>::create();
   test->set<TEST_obj_any>(std::move(job));
   const ListPtr<QU> queues = ListPtr<QU>::create("queues");
   ElementPtr<QU> bound = ElementPtr<QU>::from_typed(queues->emplace_back<QU_qname>("all.q").raw());
   const bool bound_refused =
           throws_field_exception([&] { static_cast<void>(test->exchange<TEST_obj_any>(std::move(bound))); },
                                  FieldException::Reason::BoundElement, "type TEST ") &&
           !bound && queues.get().size() == 1 && test->get<TEST_obj_any>().raw() == job_raw;
   // exchange with the element the field holds: it is bound in the field, refused alike, the field keeps it
   ElementPtr<JB> held = ElementPtr<JB>::from_typed(test->get<TEST_obj_any>().raw());
   const bool held_refused =
           throws_field_exception([&] { static_cast<void>(test->exchange<TEST_obj_any>(std::move(held))); },
                                  FieldException::Reason::BoundElement, "type TEST ") &&
           !held && test->get<TEST_obj_any>().raw() == job_raw &&
           test->get_as<TEST_obj_any, JB>().get<JB_job_number>() == 3;
   CHECK(45, "exchange of an object field with an element bound in a list or held by the field itself throws "
             "BoundElement, the element stays where it is bound and the field is unchanged",
         bound_refused && held_refused);

   // get_as accepts a list and an element of the type with a reduced layout
   ElementPtr<SPA> opt = ElementPtr<SPA>::create();
   ListPtr<PN> paths = ListPtr<PN>::create("paths", layout<PN_host>());
   paths->emplace_back<PN_host>(Host("host_a"));
   opt->set<SPA_argval_lListT>(std::move(paths));
   ElementPtr<JB> reduced_job = ElementPtr<JB>::create(layout<JB_job_name>());
   reduced_job->set<JB_job_name>("sleeper");
   test->set<TEST_obj_any>(std::move(reduced_job));
   const ConstElement<SPA> const_opt = opt.get();
   const ConstList<PN> path_view = const_opt.get_as<SPA_argval_lListT, PN>();
   const Element<JB> job_view = test->get_as<TEST_obj_any, JB>();
   CHECK(46, "get_as accepts a list and an element of the expected type with a reduced layout",
         path_view.size() == 1 && std::strcmp(path_view.front().get<PN_host>(), "host_a") == 0 &&
         !path_view.front().has<PN_path>() && std::strcmp(job_view.get<JB_job_name>(), "sleeper") == 0 &&
         !job_view.has<JB_job_number>());

   // the raw setter takes a raw list or element over and nullptr empties the field
   lList *raw_list = ListPtr<ST>::create("names").release();
   opt->set<SPA_argval_lListT>(raw_list);
   lListElem *raw_elem = ElementPtr<QU>::create().release();
   test->set<TEST_obj_any>(raw_elem);
   const bool raw_taken =
           opt->get<SPA_argval_lListT>().raw() == raw_list && test->get<TEST_obj_any>().raw() == raw_elem;
   opt->set<SPA_argval_lListT>(nullptr);
   test->set<TEST_obj_any>(nullptr);
   CHECK(47, "a raw pointer and nullptr reach the raw setter of a field without a sub-type: the list or element is "
             "taken over, nullptr empties the field",
         raw_taken && !opt->get<SPA_argval_lListT>() && !test->get<TEST_obj_any>());
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
      {"HANDLE-01", "pointer-like",
       "a handle is a pointer: it converts, compares and resolves overloads like one",
       {"compare handles with nullptr and with each other",
        "pass a handle to a function taking a raw pointer and to an overload pair"},
       "every comparison holds, the old function reads the element, the pair resolves to the read-only handle",
       {{"HDL-01", "covers"}}, {1, 2, 3, 4}, test_pointer_like},
      {"HANDLE-02", "accessors",
       "the template, the run-time and the free accessors agree on full and reduced elements",
       {"set every field type with set<F>() and read it back with get<F>() and the free getters on the raw pointer",
        "write with the free setters and set(nm, v), read with get(nm)",
        "read a reduced element and ask has<F>() for present and absent fields"},
       "all paths agree on every value; has<F>() agrees with the field's presence in the descriptor",
       {{"HDL-02", "covers"}}, {5, 6, 7, 8, 9}, test_accessors},
      {"HANDLE-03", "errors",
       "errors the compiler cannot catch throw a logic error; the free setter keeps the old setter's behaviour",
       {"read an absent field, read through an empty handle, make a job handle from a queue",
        "call the free getter with a job field on a queue",
        "call the free setter with a null element and with absent number and string fields"},
       "each error throws with field and type named; the free setter returns -1 with the error number the old "
       "one gave, and throws where the old one aborted",
       {{"HDL-03", "covers"}}, {10, 11, 12, 13, 14, 15, 16, 17, 19, 20, 24}, test_errors},
      {"HANDLE-04", "hash tables",
       "the setters keep the hash table of the owning list",
       {"change a hashed field after the first lookup in a list above the hash threshold",
        "change an unhashed field"},
       "the lookup finds the element under its new key and not under the old one",
       {{"HDL-04", "covers"}}, {21, 22}, test_hash_tables},
      {"HANDLE-05", "correct counterparts",
       "what the sources in compile_fail/ do wrong compiles when done right",
       {"read a string field into a string, a queue field from a queue, make a handle explicitly, "
        "set through a mutable handle, convert a read-only handle to a const pointer"},
       "each compiles and works",
       {{"TRT-02", "covers"}}, {23}, test_correct_counterparts},
      {"HANDLE-06", "creation with a descriptor",
       "the owning handles create a list and an element with a given, possibly reduced, descriptor of their type",
       {"create a list and an element with the full descriptor and with a reduced one",
        "create them with the descriptor of another type and without a descriptor"},
       "the list and the element have the given descriptor and the list the given name; another type throws "
       "WrongType naming it, a missing descriptor throws NullDescriptor",
       {{"HDL-03", "covers"}}, {25, 26, 27, 28}, test_create_with_descriptor},
      {"HANDLE-07", "layout",
       "an element or a list linked into a list must have the layout of the list's descriptor (CS-3112)",
       {"emplace_back and the free emplace_back on an existing reduced list",
        "push_back, push_front, the free append and insert_after of a full element into a reduced list and the "
        "opposite",
        "push_back of an element with the list's descriptor and with an equal one at another address",
        "splice and the raw splice between a reduced and a full list",
        "the free append and push_back of elements of the full type into a list with a hashed private descriptor "
        "copy"},
       "the added element has the list's descriptor and its key is found; another layout throws WrongLayout "
       "and leaves both sides unchanged; the same or an equal layout is linked in",
       {{"HDL-03", "covers"}}, {29, 30, 31, 32, 33, 34}, test_layout},
      {"HANDLE-08", "add", "add<F> adds to a number field of the type the field gives",
       {"add to a 32 bit, a 64 bit and a double field of an array task, and add 0"}, "the sums; 0 changes nothing",
       {{"OPS-01", "covers"}}, {35}, test_add},
      {"HANDLE-09", "free add", "the free form add<F>(ep, offset) for the rewrite keeps the error behaviour of the "
       "old adders",
       {"add to a null element, a full element and a reduced element without the field"},
       "-1 with LEELEMNULL, 0 and the sum; AbsentField thrown for the absent field", {{"OPS-01", "covers"}}, {36},
       test_free_add},
      {"HANDLE-10", "clone of nothing", "clone of an empty element handle is not an error",
       {"clone an empty Element and an empty ConstElement"}, "an empty owning handle", {{"OPS-01", "covers"}}, {37},
       test_clone_empty},
      {"HANDLE-11", "fields without a sub-type",
       "a list or object field for which the model names no sub-type takes an owner of any type, gives a raw "
       "handle and a checked typed view (CS-3103)",
       {"set a list field to a PN list and then to an ST list, an object field to a JB and then to a QU element",
        "exchange both fields and adopt what comes back",
        "read both fields with get and with get_as for the right and for a wrong type",
        "empty both fields with an empty owner and read them",
        "use the members on an empty handle, a reduced element and with a bound element"},
       "set and exchange take every type and give back the old content; get gives a raw handle; get_as gives the "
       "view for the right type, throws WrongType for a wrong one and gives an empty view for an empty field; "
       "EmptyHandle, AbsentField and BoundElement as for every member",
       {{"OPS-01", "covers"}}, {38, 39, 40, 41, 42, 43}, test_no_sub_type},
      {"HANDLE-12", "fields without a sub-type: edges",
       "the members on a field without a sub-type keep the contract of the members at the edges, and a raw pointer "
       "still reaches the raw setter (CS-3103)",
       {"set an element into a reduced element that does not hold the object field",
        "exchange an object field with an element bound in a list and with the element the field holds",
        "read a list and an element of the expected type with a reduced layout with get_as",
        "set a raw list, a raw element and nullptr"},
       "AbsentField with the element left to the caller; BoundElement with the element left where it is bound and "
       "the field unchanged; get_as accepts the reduced layouts; the raw setter takes the pointers over and nullptr "
       "empties the field (checked at compile time as well)",
       {{"OPS-01", "covers"}}, {44, 45, 46, 47}, test_no_sub_type_edges},
   });
}
