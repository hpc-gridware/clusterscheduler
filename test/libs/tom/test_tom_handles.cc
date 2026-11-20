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

#include <csignal>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "cull/cull_test_api.h"
#include "cull/ocs_cull_descr.h"
#include "tom/cull_hash.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_Raw.h"

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
static lUlong old_style(const lListElem *job) {
   return lGetUlong(job, JB_job_number);
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
 * @brief Whether a call aborts, run in a child process
 * @param call the call
 * @return true if the child ended with SIGABRT
 */
template <typename Call>
static bool aborts(Call &&call) {
   const pid_t pid = fork();
   if (pid == 0) {
      // keep the expected abort quiet in the test output
      [[maybe_unused]] FILE *quiet_stderr = std::freopen("/dev/null", "w", stderr);
      call();
      _exit(0);
   }
   int status = 0;
   waitpid(pid, &status, 0);
   return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

/**
 * @brief HDL-01: handles convert, compare and resolve overloads like pointers
 * @return nothing; CHECK records the results
 */
static void
test_pointer_like() {
   lListElem *raw = lCreateElem(ocs::tom::JB::descriptor.e);
   lSetUlong(raw, JB_job_number, 42);
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
   lFreeElem(&raw);
}

/**
 * @brief HDL-02: the template accessors, the run-time accessors and the old accessors agree, on a full and a reduced element
 * @return nothing; CHECK records the results
 */
static void
test_accessors() {
   lList *list = lCreateList("test", ocs::tom::TEST::descriptor.e);
   lListElem *raw = lCreateElem(ocs::tom::TEST::descriptor.e);
   lAppendElem(list, raw);
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

   const bool old_path = lGetBool(raw, TEST_bool) && lGetUlong(raw, TEST_uint32) == 4000000000u &&
                         lGetUlong64(raw, TEST_uint64) == (UINT64_C(1) << 40) && lGetInt(raw, TEST_int) == -7 &&
                         lGetLong(raw, TEST_long) == -(INT64_C(1) << 40) && lGetDouble(raw, TEST_double) == 2.5 &&
                         std::strcmp(lGetHost(raw, TEST_host), "host1") == 0 &&
                         std::strcmp(lGetString(raw, TEST_string), "value") == 0 && lGetRef(raw, TEST_ref) == &ref_target;
   CHECK(6, "the old accessors read what set<F>() wrote", old_path);

   lSetUlong(raw, TEST_uint32, 17);
   lSetString(raw, TEST_string, "other");
   t.set<std::int32_t>(TEST_int, 9);
   const bool runtime_path = t.get<std::uint32_t>(TEST_uint32) == 17 &&
                             std::strcmp(t.get<const char *>(TEST_string), "other") == 0 && lGetInt(raw, TEST_int) == 9 &&
                             t.get<Host>(TEST_host) == "host1";
   CHECK(7, "get(nm) and set(nm, v) agree with the old accessors", runtime_path);

   // a reduced element, as a what selects it
   lEnumeration *what = lWhat("%T(%I %I %I)", ocs::tom::TEST::descriptor.e, TEST_uint32, TEST_string, TEST_double);
   lList *reduced = lSelect("reduced", list, nullptr, what);
   const ConstElement<TEST> r{ConstRawList{reduced}.front().raw()};
   CHECK(8, "the template accessors read a reduced element",
         r.get<TEST_uint32>() == 17 && std::strcmp(r.get<TEST_string>(), "other") == 0 && r.get<TEST_double>() == 2.5);
   const bool presence = r.has<TEST_uint32>() && r.has<TEST_double>() && !r.has<TEST_int>() && !r.has<TEST_host>() &&
                         r.has<TEST_uint32>() == (lGetPosViaElem(r, TEST_uint32, SGE_NO_ABORT) >= 0) &&
                         r.has<TEST_int>() == (lGetPosViaElem(r, TEST_int, SGE_NO_ABORT) >= 0) && t.has<TEST_int>();
   CHECK(9, "has<F>() agrees with the old presence probe", presence);

   lFreeList(&reduced);
   lFreeWhat(&what);
   lFreeList(&list);
}

/**
 * @brief HDL-03: the run-time errors of the handles throw, and the free setter keeps the old setter's behaviour
 * @return nothing; CHECK records the results
 */
static void
test_errors() {
   lList *list = lCreateList("test", ocs::tom::TEST::descriptor.e);
   lAppendElem(list, lCreateElem(ocs::tom::TEST::descriptor.e));
   lEnumeration *what = lWhat("%T(%I)", ocs::tom::TEST::descriptor.e, TEST_uint32);
   lList *reduced = lSelect("reduced", list, nullptr, what);
   lListElem *r = RawList{reduced}.front();
   lListElem *queue = lCreateElem(ocs::tom::QU::descriptor.e);

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

   // the free setter against the old setter: a null element
   cull_state_set_lerrno(0);
   const int old_null = lSetUlong(nullptr, TEST_uint32, 1);
   const int old_null_errno = cull_state_get_lerrno();
   cull_state_set_lerrno(0);
   const int new_null = ocs::tom::set<TEST_uint32>(nullptr, 1u);
   CHECK(14, "a null element: -1 and LEELEMNULL, like the old setter",
         old_null == -1 && new_null == -1 && cull_state_get_lerrno() == old_null_errno && old_null_errno == LEELEMNULL);
   cull_state_set_lerrno(0);
   const int new_null_str = ocs::tom::set<TEST_string>(nullptr, "x");
   CHECK(15, "a null element for a string field: -1 and LEELEMNULL", new_null_str == -1 && cull_state_get_lerrno() == LEELEMNULL);

   // an absent number field: the old setter aborts and the free setter throws, as for every field type since
   // CS-3062 (N-L-4); before, both returned -1, with LENEGPOS for an int field and without one for a uint64 field
   CHECK(16, "an absent int field: the old setter aborts, the free setter throws",
         aborts([&] { lSetInt(r, TEST_int, 1); }) &&
         throws_field_exception([&] { ocs::tom::set<TEST_int>(r, 1); }, FieldException::Reason::AbsentField, "TEST_int"));
   CHECK(17, "an absent uint64 field: the old setter aborts, the free setter throws",
         aborts([&] { lSetUlong64(r, TEST_uint64, 1); }) &&
         throws_field_exception([&] { ocs::tom::set<TEST_uint64>(r, UINT64_C(1)); }, FieldException::Reason::AbsentField,
                                "TEST_uint64"));

   // an absent string field: the old setter aborts, the free setter throws
   CHECK(18, "an absent string field: the old setter aborts", aborts([&] { lSetString(r, TEST_string, "x"); }));
   CHECK(19, "an absent string field: the free setter throws",
         throws_field_exception([&] { ocs::tom::set<TEST_string>(r, "x"); }, FieldException::Reason::AbsentField, "TEST_string"));
   CHECK(20, "the handle's setter throws on an absent number field",
         throws_field_exception([&] { Element<TEST>{r}.set<TEST_int>(1); }, FieldException::Reason::AbsentField, "TEST_int"));

   // the object a field holds already is set again without an error since CS-3062 (N-L-4)
   lListElem *full = lCreateElem(ocs::tom::TEST::descriptor.e);
   lListElem *held = lCreateElem(ocs::tom::JB::descriptor.e);
   const bool same = ocs::tom::set<TEST_obj_jb>(full, held) == 0 && ocs::tom::set<TEST_obj_jb>(full, held) == 0 &&
                     lGetObject(full, TEST_obj_jb) == held;
   lFreeElem(&full);
   CHECK(24, "the free setter sets the object a field holds again without an error", same);

   lFreeElem(&queue);
   lFreeList(&reduced);
   lFreeWhat(&what);
   lFreeList(&list);
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
         lGetElemUlong(list.get(), TEST_uint32, 1000) == five.raw());
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
   const lDescr *reduced = ocs_cull_descr_reduce(ocs::tom::JB::descriptor.e, {JB_job_number, JB_job_name});

   auto full_list = ListPtr<JB>::create("full", ocs::tom::JB::descriptor.e);
   auto full_job = ElementPtr<JB>::create(ocs::tom::JB::descriptor.e);
   CHECK(25, "with the full descriptor: the list has it and the name, the element has it",
         lGetListDescr(full_list.get()) == ocs::tom::JB::descriptor.e &&
         std::strcmp(lGetListName(full_list.get()), "full") == 0 &&
         lGetElemDescr(full_job.get()) == ocs::tom::JB::descriptor.e);

   auto reduced_list = ListPtr<JB>::create("reduced", reduced, Hashing::Off);
   auto reduced_job = ElementPtr<JB>::create(reduced);
   reduced_job.get().set<JB_job_number>(7u);
   const auto pushed = reduced_list.get().push_back(std::move(reduced_job));
   CHECK(26, "with a reduced descriptor: list and element have it and hold the fields it keeps",
         reduced != nullptr && lGetListDescr(reduced_list.get()) == reduced &&
         std::strcmp(lGetListName(reduced_list.get()), "reduced") == 0 && lGetElemDescr(pushed) == reduced &&
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
   ocs_cull_descr_release(&reduced);
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
   const lDescr *reduced = ocs_cull_descr_reduce(ocs::tom::JB::descriptor.e, {JB_job_number, JB_job_name});

   // emplace_back and lAddElemUlong on an existing reduced list create the element with the list's descriptor
   auto reduced_list = ListPtr<JB>::create("reduced", reduced);
   const Element<JB> emplaced = reduced_list.get().emplace_back<JB_job_number>(7u);
   lList *old_list = lCreateList("old reduced", reduced);
   const lListElem *added = lAddElemUlong(&old_list, JB_job_number, 8, ocs::tom::JB::descriptor.e);
   CHECK(29, "emplace_back and lAddElemUlong on a reduced list give an element with the list's descriptor",
         lGetElemDescr(emplaced) == lGetListDescr(reduced_list.get()) && emplaced.get<JB_job_number>() == 7 &&
         !emplaced.has<JB_owner>() && reduced_list.get().find_first<JB_job_number>(7u) == emplaced &&
         added != nullptr && lGetElemDescr(added) == lGetListDescr(old_list) &&
         lGetUlong(added, JB_job_number) == 8 && lGetElemUlong(old_list, JB_job_number, 8) == added);
   lFreeList(&old_list);

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

   // the old interface aborts, as for the other programming errors of lAppendElem()
   CHECK(31, "lAppendElem and lInsertElem of an element of another layout abort",
         aborts([&] { lAppendElem(reduced_list.get(), full_job.release()); }) &&
         aborts([&] { lInsertElem(full_list.get(), nullptr, reduced_job.release()); }));

   // the same pointer, and an equal descriptor at another address, are accepted
   reduced_job.get().set<JB_job_number>(9u);
   const Element<JB> same = reduced_list.get().push_back(std::move(reduced_job));
   const lDescr *copy = ocs_cull_descr_private(reduced);
   auto copied_job = ElementPtr<JB>::create(copy);
   const bool other_address = lGetElemDescr(copied_job.get()) != lGetListDescr(reduced_list.get());
   copied_job.get().set<JB_job_number>(10u);
   const Element<JB> equal = reduced_list.get().push_back(std::move(copied_job));
   ocs_cull_descr_release(&copy);
   full_list.get().push_back(std::move(full_job));
   CHECK(32, "an element with the list's descriptor or an equal one at another address is linked in",
         other_address && reduced_list.get().size() == 3 && full_list.get().size() == 1 &&
         lGetElemDescr(same) == lGetListDescr(reduced_list.get()) &&
         lGetElemDescr(equal) == lGetListDescr(reduced_list.get()) && equal.get<JB_job_number>() == 10 &&
         reduced_list.get().find_first<JB_job_number>(10u) == equal);

   // whole lists: splice and lAppendList compare the layouts as well
   auto more_full = ListPtr<JB>::create("more full");
   more_full.get().emplace_back<JB_job_number>(11u);
   lList *old_full = lCreateList("old full", ocs::tom::JB::descriptor.e);
   lAddElemUlong(&old_full, JB_job_number, 12, ocs::tom::JB::descriptor.e);
   CHECK(33, "splice and lAppendList between a reduced and a full list refuse, moving nothing",
         throws_field_exception([&] { reduced_list.get().splice(more_full.get()); },
                                FieldException::Reason::WrongLayout, "layout") &&
         lAppendList(reduced_list.get(), old_full) == -1 && reduced_list.get().size() == 3 &&
         more_full.get().size() == 1 && lGetNumberOfElem(old_full) == 1);
   lFreeList(&old_full);

   // a list that got a hash table later holds a private copy of the type's descriptor whose flags differ
   // (CULL_HASH); elements of the type itself still have its layout and must be linked in
   lList *hashed = lCreateListHash("hashed", ocs::tom::JB::descriptor.e, false);
   const bool hash_created = cull_hash_new(hashed, JB_owner, false) == 1;
   const bool private_copy = lGetListDescr(hashed) != ocs::tom::JB::descriptor.e;
   lListElem *plain_job = lCreateElem(ocs::tom::JB::descriptor.e);
   lSetUlong(plain_job, JB_job_number, 13);
   lSetString(plain_job, JB_owner, "someone");
   lAppendElem(hashed, plain_job);
   auto typed_job = ElementPtr<JB>::create();
   typed_job.get().set<JB_job_number>(14u);
   const Element<JB> typed = List<JB>(hashed).push_back(std::move(typed_job));
   CHECK(34, "elements of the full type are linked into a list whose descriptor is a private copy with hash "
             "flags",
         hash_created && private_copy && lGetNumberOfElem(hashed) == 2 &&
         lGetElemStr(hashed, JB_owner, "someone") == plain_job && typed.get<JB_job_number>() == 14);
   lFreeList(&hashed);

   ocs_cull_descr_release(&reduced);
}

/**
 * @brief TRT-02: the correct counterparts of the sources the build must fail to compile
 * @return nothing; CHECK records the results
 */
static void
test_correct_counterparts() {
   lListElem *raw = lCreateElem(ocs::tom::JB::descriptor.e);
   lListElem *raw_queue = lCreateElem(ocs::tom::QU::descriptor.e);
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
   lFreeElem(&raw_queue);
   lFreeElem(&raw);
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
      cull_state_set_lerrno(0);
      seen.push_back(ocs::tom::add<JAT_status>(ep, 1));
      seen.push_back(cull_state_get_lerrno());
      cull_state_set_lerrno(0);
      seen.push_back(ocs::tom::add<JAT_tix>(ep, 1.0));
      seen.push_back(cull_state_get_lerrno());
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
       "the template, the run-time and the old accessors agree on full and reduced elements",
       {"set every field type with set<F>() and read it back with get<F>() and the old getters",
        "write with the old setters and set(nm, v), read with get(nm)",
        "read a reduced element and ask has<F>() for present and absent fields"},
       "all paths agree on every value; has<F>() agrees with the old presence probe",
       {{"HDL-02", "covers"}}, {5, 6, 7, 8, 9}, test_accessors},
      {"HANDLE-03", "errors",
       "errors the compiler cannot catch throw a logic error; the free setter keeps the old setter's behaviour",
       {"read an absent field, read through an empty handle, make a job handle from a queue",
        "call the free getter with a job field on a queue",
        "call the free and the old setter with a null element and with absent number and string fields"},
       "each error throws with field and type named; the free setter returns -1 with the same error number "
       "where the old one does, and throws where the old one aborts",
       {{"HDL-03", "covers"}}, {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 24}, test_errors},
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
       {"emplace_back and lAddElemUlong on an existing reduced list",
        "push_back, push_front, lAppendElem and lInsertElem of a full element into a reduced list and the opposite",
        "push_back of an element with the list's descriptor and with an equal one at another address",
        "splice and lAppendList between a reduced and a full list",
        "lAppendElem and push_back of elements of the full type into a list with a hashed private descriptor copy"},
       "the added element has the list's descriptor and its key is found; another layout throws WrongLayout or "
       "aborts in the old interface and leaves both sides unchanged; the same or an equal layout is linked in",
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
   });
}
