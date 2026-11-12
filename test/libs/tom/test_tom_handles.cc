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

#include <sys/wait.h>
#include <unistd.h>

#include "cull/cull_lerrno.h"
#include "cull/cull_state.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

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
   lListElem *raw = lCreateElem(JB_Type);
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
   lList *list = lCreateList("test", TEST_Type);
   lListElem *raw = lCreateElem(TEST_Type);
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
   lEnumeration *what = lWhat("%T(%I %I %I)", TEST_Type, TEST_uint32, TEST_string, TEST_double);
   lList *reduced = lSelect("reduced", list, nullptr, what);
   const ConstElement<TEST> r{lFirst(reduced)};
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
   lList *list = lCreateList("test", TEST_Type);
   lAppendElem(list, lCreateElem(TEST_Type));
   lEnumeration *what = lWhat("%T(%I)", TEST_Type, TEST_uint32);
   lList *reduced = lSelect("reduced", list, nullptr, what);
   lListElem *r = lFirstRW(reduced);
   lListElem *queue = lCreateElem(QU_Type);

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
   lListElem *full = lCreateElem(TEST_Type);
   lListElem *held = lCreateElem(JB_Type);
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
 * @brief TRT-02: the correct counterparts of the sources the build must fail to compile
 * @return nothing; CHECK records the results
 */
static void
test_correct_counterparts() {
   lListElem *raw = lCreateElem(JB_Type);
   lListElem *raw_queue = lCreateElem(QU_Type);
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

int main(int argc, char *argv[]) {
   lInit(nmv);
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
   });
}
