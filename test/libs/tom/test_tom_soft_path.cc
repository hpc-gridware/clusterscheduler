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
 * @brief Debug builds record the soft error paths of the old accessors (CS-3062, LEG-03 step 1)
 *
 * Each soft path an old accessor can still take under the contract of N-L-4
 * (CS-3062) is called once with a sink directory set: a null element, a null
 * argument, a bound element and a null list. A field the element does not hold
 * and a negative position abort since then and are no soft paths any more. The test checks that the return value and the error
 * number stay what they were and that one line names the accessor, the field,
 * the reason and this test as the caller. A hot loop on one soft path writes a
 * bounded number of lines, and without the sink directory nothing is written.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "cull/cull_test_api.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_DumpCore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_ElementCore.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_ListOpsCore.h"
#include "tom/ocs_Pack.h"
#include "tom/ocs_PackCore.h"
#include "tom/ocs_PrintCore.h"
#include "tom/ocs_Raw.h"
#include "tom/ocs_SelectCore.h"
#include "uti/sge_dstring.h"

#include "ocs_module_test.h"

namespace {

   /** @brief The sink directory of the test, created in the temporary directory */
   std::string sink;

   /** @brief The path this test was started with, to start it again for the check without a sink */
   const char *self = nullptr;

   /** @brief The argument that makes the started copy run the check without a sink */
   constexpr const char *NoSinkChild = "--no-sink-child";

   /**
    * @brief The check without a sink, run by a freshly started copy of this test
    *
    * The sink is looked up once per process, so the check needs a process that
    * has not reported anything yet: a forked child would inherit the sink.
    *
    * @param missing a sink directory that does not exist
    * @return 0 if the setter behaved as before and nothing was created, 1 otherwise
    */
   int no_sink_child(const char *missing) {
      setenv("OCS_SOFT_PATH_DIR", missing, 1);
      ocs::tom::install_model(ocs::tom::gcs_model());
      cull_state_set_lerrno(0);
      const int result = lSetPosBool(nullptr, 0, true);
      return result == -1 && cull_state_get_lerrno() == LEELEMNULL && access(missing, F_OK) != 0 ? 0 : 1;
   }

   /**
    * @brief The text of all sink files written so far
    * @return the concatenated contents
    */
   std::string sink_text() {
      std::string text;
      char host[256] = "unknown";
      gethostname(host, sizeof(host) - 1);
      std::ifstream in(sink + "/" + host + ".log");
      std::stringstream ss;
      ss << in.rdbuf();
      return ss.str();
   }

   /**
    * @brief Count the lines of the sink that record an accessor, a field and a reason
    * @param function the accessor's name
    * @param field the field name, "-" for none
    * @param reason the reason as written
    * @return the number of matching lines
    */
   [[maybe_unused]] int lines_for(const std::string &function, const std::string &field, const std::string &reason) {
      const std::string key = " | " + function + " | " + field + " | " + reason + " | ";
      std::istringstream in(sink_text());
      int n = 0;
      for (std::string line; std::getline(in, line);) {
         if (line.find(key) != std::string::npos) {
            n++;
         }
      }
      return n;
   }

   /**
    * @brief Whether the line recording an accessor, a field and a reason names this test's scenario as a caller
    * @param function the accessor's name
    * @param field the field name
    * @param reason the reason as written
    * @param caller part of the caller frame's function name
    * @return true if a matching line holds the caller
    */
   [[maybe_unused]] bool line_names_caller(const std::string &function, const std::string &field, const std::string &reason,
                          const std::string &caller) {
      const std::string key = " | " + function + " | " + field + " | " + reason + " | ";
      std::istringstream in(sink_text());
      for (std::string line; std::getline(in, line);) {
         if (line.find(key) != std::string::npos && line.find(caller) != std::string::npos) {
            return true;
         }
      }
      return false;
   }

   /**
    * @brief Call an old setter and check its result and error number
    * @param result the setter's return value
    * @param lerrno the error number expected with -1
    * @return true if the setter returned -1 with that error number
    */
   [[maybe_unused]] bool returned(const int result, const int lerrno) {
      return result == -1 && cull_state_get_lerrno() == lerrno;
   }

   /** @brief A TEST element in a list, a reduced copy of it, and a job with its task list */
   struct Fixture {
      lList *list = nullptr;         ///< holds the full element
      lListElem *full = nullptr;     ///< a full TEST element
      lList *reduced_list = nullptr; ///< holds the reduced element
      lListElem *reduced = nullptr;  ///< a TEST element holding TEST_obj_any only
      lEnumeration *what = nullptr;  ///< the selection of the reduced element
      lListElem *job = nullptr;      ///< a job, for the list field of lXchgList

      /** @brief Build the elements */
      Fixture() {
         list = lCreateList("full", ocs::tom::TEST::descriptor.e);
         full = lCreateElem(ocs::tom::TEST::descriptor.e);
         lAppendElem(list, full);
         what = lWhat("%T(%I)", ocs::tom::TEST::descriptor.e, TEST_obj_any);
         reduced_list = lSelect("reduced", list, nullptr, what);
         reduced = ocs::tom::RawList{reduced_list}.front();
         job = lCreateElem(ocs::tom::JB::descriptor.e);
      }

      /** @brief Free the elements */
      ~Fixture() {
         lFreeList(&list);
         lFreeList(&reduced_list);
         lFreeWhat(&what);
         lFreeElem(&job);
      }
   };

} // namespace

// The scenarios have external linkage, so that the stack written to the sink names them
// (-rdynamic exports external symbols only).

   /**
    * @brief Every kind of soft path is recorded once, and the setters behave as before
    */
   void test_each_soft_path() {
#ifndef ENABLE_DEBUG_CHECKS
      ocs::module_test::unsupported("debug only: release builds record nothing");
      return;
#else
      Fixture f;

      // a null list to copy: nullptr
      CHECK(1, "a null list of lCopyList is recorded, the result is unchanged",
            lCopyList("copy", nullptr) == nullptr && lines_for("lCopyList", "-", "null-list") == 1);

      // a null element to copy: nullptr
      CHECK(2, "a null element of lCopyElem is recorded, the result is unchanged",
            lCopyElem(nullptr) == nullptr && lines_for("lCopyElem", "-", "null-element") == 1);

      // a null element: -1 with LEELEMNULL
      cull_state_set_lerrno(0);
      const bool null_elem = returned(lSetDouble(nullptr, TEST_double, 1.0), LEELEMNULL);
      CHECK(3, "a null element is recorded, the result is unchanged",
            null_elem && lines_for("lSetDouble", "TEST_double", "null-element") == 1);

      // a null element of a setter by position: -1 with LEELEMNULL
      cull_state_set_lerrno(0);
      const bool null_pos = returned(lSetPosUlong(nullptr, 0, 1), LEELEMNULL);
      CHECK(4, "a null element of a setter by position is recorded, the result is unchanged",
            null_pos && lines_for("lSetPosUlong", "-", "null-element") == 1);

      // a null output pointer of lXchgList: -1 with LEELEMNULL
      cull_state_set_lerrno(0);
      const bool null_arg = returned(lXchgList(f.job, JB_ja_tasks, nullptr), LEELEMNULL);
      CHECK(5, "a null argument is recorded, the result is unchanged",
            null_arg && lines_for("lXchgList", "JB_ja_tasks", "null-argument") == 1);

      // an element bound in a list cannot become an object field: -1 with LEBOUNDELEM
      cull_state_set_lerrno(0);
      const bool bound = returned(lSetObject(f.reduced, TEST_obj_any, f.full), LEBOUNDELEM);
      CHECK(6, "a bound element is recorded, the result is unchanged",
            bound && lines_for("lSetObject", "TEST_obj_any", "bound-element") == 1);

      // the free setter on a null element, as the old lSetUlong
      cull_state_set_lerrno(0);
      const bool free_setter = returned(ocs::tom::set<TEST_uint32>(nullptr, 1u), LEELEMNULL);
      CHECK(7, "the free setter records a null element like the old setter",
            free_setter && lines_for("ocs::tom::set", "TEST_uint32", "null-element") == 1);

      CHECK(8, "a recorded line names the calling function",
            line_names_caller("lCopyList", "-", "null-list", "test_each_soft_path"));
#endif
   }

   /**
    * @brief A soft path in a hot loop writes a bounded number of lines
    */
   void test_hot_loop() {
#ifndef ENABLE_DEBUG_CHECKS
      ocs::module_test::unsupported("debug only: release builds record nothing");
      return;
#else
      Fixture f;
      for (int i = 0; i < 10000; i++) {
         static_cast<void>(lSetLong(nullptr, TEST_long, i));
      }
      const int n = lines_for("lSetLong", "TEST_long", "null-element");
      CHECK(9, "10 000 calls from one place write at least one and at most 8 lines", n >= 1 && n <= 8);
#endif
   }

   /**
    * @brief Without the sink directory nothing is written and the setters behave as before
    */
   void test_no_sink() {
#ifndef ENABLE_DEBUG_CHECKS
      ocs::module_test::unsupported("debug only: release builds record nothing");
      return;
#else
      // the sink is looked up once per process, so the check runs in a freshly started copy of this test
      std::fflush(nullptr);
      const std::string missing = sink + "/does-not-exist";
      const pid_t pid = fork();
      if (pid == 0) {
         execl(self, self, NoSinkChild, missing.c_str(), static_cast<char *>(nullptr));
         _exit(2);
      }
      int status = 0;
      waitpid(pid, &status, 0);
      CHECK(10, "without the sink directory nothing is written and the result is unchanged",
            WIFEXITED(status) && WEXITSTATUS(status) == 0 && lines_for("lSetPosBool", "-", "null-element") == 0);
#endif
   }

/**
 * @brief The error behaviour of the cores behind the old accessors: what the old wrappers turned into a return
 *        value or an abort, the core reports as an exception with a reason or an error number (CS-3077, LEG-01)
 *
 * The expectations were taken from the old wrappers, which only catch what the core throws: where a wrapper gave
 * -1, nullptr or an abort, the core throws the FieldException, ListCoreError or PackException named here. The
 * texts of dump and print are pinned by hashes recorded on the code before the cores took the old bodies over.
 */
namespace legacy_cores {

   using ocs::tom::ConstElement;
   using ocs::tom::Element;
   using ocs::tom::ElementPtr;
   using ocs::tom::FieldException;
   using ocs::tom::Hashing;
   using ocs::tom::Host;
   using ocs::tom::ListCoreError;
   using ocs::tom::ListPtr;
   using ocs::tom::PackBuffer;
   using ocs::tom::PackException;
   using ocs::tom::RawElementPtr;
   using ocs::tom::RawListPtr;
   using ocs::tom::TEST;
   using ocs::tom::UE;
   using ocs::tom::US;
   using ocs::tom::What;
   using ocs::tom::Where;
   using ocs::tom::field;
   using ocs::tom::what;

   /**
    * @brief Whether a call throws a FieldException with a given reason
    * @param call the call
    * @param reason the expected reason
    * @return true if it threw with that reason
    */
   bool throws_reason(const std::function<void()> &call, const FieldException::Reason reason) {
      try {
         call();
      } catch (const FieldException &e) {
         return e.reason() == reason;
      }
      return false;
   }

   /**
    * @brief Whether a call throws a PackException with a given code
    * @param call the call
    * @param code the expected code
    * @return true if it threw with that code
    */
   bool throws_pack(const std::function<void()> &call, const int code) {
      try {
         call();
      } catch (const PackException &e) {
         return e.code() == code;
      }
      return false;
   }

   /**
    * @brief Whether a call throws a ListCoreError and leaves a given CULL error number
    * @param call the call
    * @param lerrno the expected error number
    * @return true if it threw and the error number is the expected one
    */
   bool throws_lerrno(const std::function<void()> &call, const int lerrno) {
      cull_state_set_lerrno(0);
      try {
         call();
      } catch (const ListCoreError &) {
         return cull_state_get_lerrno() == lerrno;
      }
      return false;
   }

   /**
    * @brief Whether a call throws an ocs::RuntimeException or one derived from it
    * @param call the call
    * @return true if it threw one
    */
   bool throws_runtime(const std::function<void()> &call) {
      try {
         call();
      } catch (const ocs::RuntimeException &) {
         return true;
      }
      return false;
   }

   /**
    * @brief SOFT-04: the list structure cores refuse null and foreign arguments
    * @return nothing; CHECK records the results
    */
   void test_list_structure() {
      RawListPtr source = ocs::tom::create_raw_list(ocs::tom::TEST::descriptor.e, Hashing::On, "source");
      ocs::tom::append(source.get().raw(), ocs::tom::create_raw_elem(ocs::tom::TEST::descriptor.e));
      lList *src = source.get().raw();
      RawElementPtr ep = ocs::tom::create_raw_elem(ocs::tom::TEST::descriptor.e);
      lListElem *free_ep = ep.get().raw();
      // the old wrappers gave -1 with LELISTNULL and LEELEMNULL, and aborted on a bound element
      const bool append_errors =
         throws_reason([&] { ocs::tom::append_elem(nullptr, free_ep); }, FieldException::Reason::NullList) &&
         throws_reason([&] { ocs::tom::append_elem(src, nullptr); }, FieldException::Reason::NullElement) &&
         throws_reason([&] { ocs::tom::insert_elem(nullptr, nullptr, free_ep); }, FieldException::Reason::NullList) &&
         throws_reason([&] { ocs::tom::insert_elem(src, nullptr, nullptr); }, FieldException::Reason::NullElement) &&
         throws_reason([&] { ocs::tom::append_elem(src, src->first); }, FieldException::Reason::BoundElement) &&
         throws_reason([&] { ocs::tom::insert_elem(src, nullptr, src->first); }, FieldException::Reason::BoundElement);
      CHECK(11, "append_elem and insert_elem throw NullList, NullElement and BoundElement, where lAppendElem and "
                "lInsertElem gave -1 or aborted",
            append_errors && ocs::tom::size(src) == 1);

      // the old wrappers gave nullptr with LELISTNULL and LEELEMNULL, and aborted on an element of another list
      RawListPtr other = ocs::tom::create_raw_list(ocs::tom::TEST::descriptor.e, Hashing::On, "other");
      ocs::tom::append(other.get().raw(), ocs::tom::create_raw_elem(ocs::tom::TEST::descriptor.e));
      lListElem *foreign = other.get().raw()->first;
      const bool dechain_errors =
         throws_reason([&] { ocs::tom::dechain_elem(nullptr, src->first); }, FieldException::Reason::NullList) &&
         throws_reason([&] { ocs::tom::dechain_elem(src, nullptr); }, FieldException::Reason::NullElement) &&
         throws_reason([&] { ocs::tom::dechain_elem(src, foreign); }, FieldException::Reason::ForeignList) &&
         throws_reason([&] { static_cast<void>(ocs::tom::take(src, nullptr)); }, FieldException::Reason::NullElement);
      CHECK(12, "dechain_elem and take throw NullList, NullElement and ForeignList and change nothing",
            dechain_errors && ocs::tom::size(src) == 1 && ocs::tom::size(other.get().raw()) == 1);

      // the old wrapper gave -1 with LELISTNULL and LEDIFFDESCR
      RawListPtr jobs = ocs::tom::create_raw_list(ocs::tom::JB::descriptor.e, Hashing::On, "jobs");
      const bool list_errors =
         throws_reason([&] { ocs::tom::append_list(nullptr, src); }, FieldException::Reason::NullList) &&
         throws_reason([&] { ocs::tom::append_list(src, nullptr); }, FieldException::Reason::NullList) &&
         throws_reason([&] { ocs::tom::append_list(jobs.get().raw(), src); }, FieldException::Reason::WrongType);
      CHECK(13, "append_list throws NullList for a missing list and WrongType for a list of another type, moving "
                "nothing",
            list_errors && ocs::tom::size(src) == 1 && ocs::tom::size(jobs.get().raw()) == 0);
   }

   /**
    * @brief SOFT-05: the case-insensitive and prefix lookups by a string field
    * @return nothing; CHECK records the results
    */
   void test_string_lookups() {
      RawListPtr list = ocs::tom::create_raw_list(ocs::tom::TEST::descriptor.e, Hashing::On, "full");
      RawElementPtr element = ocs::tom::create_raw_elem(ocs::tom::TEST::descriptor.e);
      ocs::tom::set(element.get().raw(), TEST_string, "Name");
      lListElem *full = element.get().raw();
      ocs::tom::append(list.get().raw(), std::move(element));
      lList *lp = list.get().raw();
      const int any_field[] = {TEST_obj_any};
      const ocs::tom::RawWhatPtr reduce = ocs::tom::what_of(ocs::tom::TEST::descriptor.e, any_field);
      const RawListPtr reduced = ocs::tom::select(lp, nullptr, reduce.get());
      // a null value and a null list find nothing; a foreign field, a field of another type and a field the
      // reduced list does not hold throw, where the old wrappers gave nullptr
      const auto soft = [&](auto lookup) {
         return lookup(lp, TEST_string, nullptr) == nullptr && lookup(nullptr, TEST_string, "x") == nullptr &&
                throws_reason([&] { static_cast<void>(lookup(lp, QU_qname, "x")); },
                              FieldException::Reason::AbsentField) &&
                throws_reason([&] { static_cast<void>(lookup(lp, TEST_uint32, "x")); },
                              FieldException::Reason::WrongType) &&
                throws_reason([&] { static_cast<void>(lookup(reduced.get().raw(), TEST_string, "x")); },
                              FieldException::Reason::AbsentField);
      };
      const bool errors =
         soft([](const lList *l, int nm, const char *s) { return ocs::tom::lookup_first_str_nocase(l, nm, s); }) &&
         soft([](const lList *l, int nm, const char *s) { return ocs::tom::lookup_first_str_like(l, nm, s); });
      const bool found = ocs::tom::lookup_first_str_nocase(lp, TEST_string, "NAME") == full &&
                         ocs::tom::lookup_first_str_like(lp, TEST_string, "Na*") == full &&
                         ocs::tom::lookup_first_str_like(lp, TEST_string, "Name") == full &&
                         ocs::tom::lookup_first_str_like(lp, TEST_string, "name") == nullptr;
      CHECK(14, "lookup_first_str_nocase and lookup_first_str_like find nothing for null arguments, throw "
                "AbsentField or WrongType for a field the list does not hold as a string, and find by case or prefix",
            errors && found);
   }

   /**
    * @brief SOFT-06: adding and removing by key in the core
    * @return nothing; CHECK records the results
    */
   void test_add_and_remove_by_key() {
      lList *lp = nullptr;
      lListElem *added = ocs::tom::add_elem_str(lp, TEST_string, "a", ocs::tom::TEST::descriptor.e);
      // the string variants name a list they create "", the 32 bit variant "ulong_sublist"
      const bool add_str =
         added != nullptr && lp != nullptr && std::string(ocs::tom::list_name(lp)).empty() &&
         ocs::tom::size(lp) == 1 && std::string(ocs::tom::get<const char *>(added, TEST_string)) == "a" &&
         throws_reason([&] { ocs::tom::add_elem_str(lp, TEST_string, nullptr, ocs::tom::TEST::descriptor.e); },
                       FieldException::Reason::NullKey) &&
         throws_reason([&] { ocs::tom::add_elem_str(lp, TEST_string, "a", nullptr); },
                       FieldException::Reason::NullDescriptor) &&
         throws_reason([&] { ocs::tom::add_elem_str(lp, QU_qname, "a", ocs::tom::TEST::descriptor.e); },
                       FieldException::Reason::AbsentField) &&
         throws_reason([&] { ocs::tom::add_elem_str(lp, TEST_uint32, "a", ocs::tom::TEST::descriptor.e); },
                       FieldException::Reason::WrongType) &&
         throws_reason([&] { ocs::tom::add_elem_host(lp, TEST_string, "h", ocs::tom::TEST::descriptor.e); },
                       FieldException::Reason::WrongType) &&
         ocs::tom::size(lp) == 1;
      ocs::tom::free_raw_list(lp);
      lList *numbers = nullptr;
      const bool add_ulong =
         ocs::tom::add_elem_ulong(numbers, TEST_uint32, 7, ocs::tom::TEST::descriptor.e) != nullptr &&
         std::string(ocs::tom::list_name(numbers)) == "ulong_sublist" &&
         throws_reason([&] { ocs::tom::add_elem_ulong(numbers, QU_qname, 7, ocs::tom::TEST::descriptor.e); },
                       FieldException::Reason::AbsentField) &&
         throws_reason([&] { ocs::tom::add_elem_ulong(numbers, TEST_string, 7, ocs::tom::TEST::descriptor.e); },
                       FieldException::Reason::WrongType) &&
         ocs::tom::size(numbers) == 1;
      ocs::tom::free_raw_list(numbers);
      CHECK(15, "add_elem_str/host/ulong create the list and throw NullKey, NullDescriptor, AbsentField or WrongType "
                "where lAddElemStr/Host/Ulong gave nullptr",
            add_str && add_ulong);

      lList *strings = nullptr;
      ocs::tom::add_elem_str(strings, TEST_string, "a", ocs::tom::TEST::descriptor.e);
      ocs::tom::add_elem_str(strings, TEST_string, "b", ocs::tom::TEST::descriptor.e);
      const bool del_str = !ocs::tom::remove_elem_str(strings, TEST_string, "x") &&
                           ocs::tom::remove_elem_str(strings, TEST_string, "a") && ocs::tom::size(strings) == 1 &&
                           !ocs::tom::remove_elem_str(nullptr, TEST_string, "a") &&
                           throws_reason([&] { ocs::tom::remove_elem_str(strings, TEST_string, nullptr); },
                                         FieldException::Reason::NullKey);
      ocs::tom::free_raw_list(strings);
      lList *ulongs = nullptr;
      ocs::tom::add_elem_ulong(ulongs, TEST_uint32, 1, ocs::tom::TEST::descriptor.e);
      const bool del_ulong = !ocs::tom::remove_elem_ulong(ulongs, TEST_uint32, 9) &&
                             ocs::tom::remove_elem_ulong(ulongs, TEST_uint32, 1) && ocs::tom::size(ulongs) == 0 &&
                             !ocs::tom::remove_elem_ulong(nullptr, TEST_uint32, 1);
      ocs::tom::free_raw_list(ulongs);
      CHECK(16, "remove_elem_str and remove_elem_ulong tell a removal from nothing found and throw NullKey for a "
                "null string key",
            del_str && del_ulong);
   }

   /** @brief One row of the TEST elements the selection pins work on */
   struct Row {
      std::uint32_t key;     ///< TEST_uint32, the unique key
      std::uint64_t u64;     ///< TEST_uint64
      const char *s;         ///< TEST_string, may be nullptr
   };

   /**
    * @brief Four TEST elements keyed 1 to 4; the fourth holds no string
    * @return the owning handle
    */
   ListPtr<TEST> selection_rows() {
      constexpr Row rows[] = {{1, 10, "alpha"}, {2, 20, "beta"}, {3, 30, "Gamma"}, {4, 40, nullptr}};
      ListPtr<TEST> list = ListPtr<TEST>::create("selection");
      for (const Row &r : rows) {
         const Element<TEST> row = list->push_back(ElementPtr<TEST>::create());
         row.set<TEST_uint32>(r.key);
         row.set<TEST_uint64>(r.u64);
         row.set<TEST_string>(r.s);
      }
      return list;
   }

   /**
    * @brief The keys of a list of TEST elements, in list order
    * @param lp the list, or nullptr
    * @return the keys joined by commas, "null" for no list
    */
   std::string keys_of(const lList *lp) {
      if (lp == nullptr) {
         return "null";
      }
      std::string keys;
      for (const auto ep : ocs::tom::ConstRawList(lp)) {
         keys += (keys.empty() ? "" : ",") + std::to_string(ep.get<std::uint32_t>(TEST_uint32));
      }
      return keys;
   }

   /**
    * @brief SOFT-07: the selection core on a bit mask of a 64 bit field, a copied condition, a sub-list condition
    *        on an element without the sub-list and a search in no list
    * @return nothing; CHECK records the results
    */
   void test_selections() {
      const ListPtr<TEST> rows = selection_rows();
      const lList *lp = rows.get().raw();
      const ocs::tom::RawWhatPtr all = ocs::tom::what_all();

      // m= takes a 32 bit field only: on a 64 bit field the comparison reports an unknown type and is false, so
      // nothing is selected, and the old contract gives no list then
      const ocs::tom::RawWherePtr bits64 =
         ocs::tom::where_compare(ocs::tom::TEST::descriptor.e, TEST_uint64, ocs::tom::CompareOp::Bitmask,
                                 std::uint64_t{8});
      const RawListPtr none = ocs::tom::select_as("selected", lp, bits64.get(), all.get(), Hashing::On);
      const bool bitmask = bits64 != nullptr && !none;

      // a copy of a combined condition selects what the original selects: "*a" matches alpha, beta and Gamma, the
      // key 2 is left out
      const Where<TEST> original = field<TEST_string>().like("*a") && field<TEST_uint32>() != 2U;
      const ocs::tom::RawWherePtr copied = ocs::tom::clone(original.get());
      const RawListPtr selected = ocs::tom::select_as("selected", lp, copied.get(), all.get(), Hashing::On);
      const bool copy = keys_of(selected.get().raw()) == "1,3";

      // a condition on a sub-list: the set without entries holds no list and does not match
      ListPtr<US> sets = ListPtr<US>::create("sets");
      const char *const set_entries[][2] = {{"a", "x"}, {"b", "y"}, {"c", nullptr}};
      for (const auto &entry : set_entries) {
         const Element<US> set = sets->emplace_back<US_name>(entry[0]);
         if (entry[1] != nullptr) {
            set.get_or_create<US_entries>().emplace_back<UE_name>(entry[1]);
         }
      }
      const ListPtr<US> with_y = sets->select(field<US_entries>().any(field<UE_name>() == "y"),
                                              ocs::tom::what_all<US>());
      const bool sub_list = with_y->size() == 1 && std::strcmp(with_y->front().get<US_name>(), "b") == 0;

      // no list to search in throws, where lFindFirstRW() gave nullptr; no condition matches the first element
      const bool no_list = throws_reason([&] { static_cast<void>(ocs::tom::find_first(nullptr, copied.get())); },
                                         FieldException::Reason::NullList) &&
                           ocs::tom::find_first(lp, nullptr) == lp->first;
      CHECK(17, "m= on a 64 bit field selects nothing, a copied condition selects what the original selects, a "
                "sub-list condition skips an element without the sub-list, and a search in no list throws NullList",
            bitmask && copy && sub_list && no_list);
   }

   /**
    * @brief SOFT-08: a nested selection makes a sub-list that comes out empty no list
    * @return nothing; CHECK records the results
    */
   void test_nested_selection() {
      ListPtr<US> sets = ListPtr<US>::create("sets");
      sets->emplace_back<US_name>("with").get_or_create<US_entries>().emplace_back<UE_name>("bob");
      sets->emplace_back<US_name>("empty").set<US_entries>(ListPtr<UE>::create("empty"));
      const ListPtr<US> selected = sets->select(what<US_name>().with<US_entries>(what<UE_name>()));
      const auto with = selected->find_first<US_name>("with");
      const auto empty = selected->find_first<US_name>("empty");
      CHECK(18, "a nested selection keeps a sub-list with entries and makes an emptied one no list",
            with && with.get<US_entries>().size() == 1 && empty && !empty.get<US_entries>());
   }

   /**
    * @brief Whether unpacking from every cut of a packed object, after the header, throws PACK_FORMAT
    * @param bytes the packed object with its header
    * @param header the size of the header
    * @param unpack unpacks one object from a buffer and frees it again
    * @return true if every cut throws PACK_FORMAT and the whole buffer unpacks
    */
   bool every_cut_fails(const std::vector<char> &bytes, const std::size_t header,
                        const std::function<void(sge_pack_buffer *)> &unpack) {
      for (std::size_t n = header; n < bytes.size(); n++) {
         if (!throws_pack([&] {
                PackBuffer in = PackBuffer::reading(bytes.data(), n);
                unpack(&in.raw());
             }, PACK_FORMAT)) {
            printf("cut at %zu of %zu does not throw PACK_FORMAT\n", n, bytes.size());
            return false;
         }
      }
      PackBuffer in = PackBuffer::reading(bytes.data(), bytes.size());
      unpack(&in.raw());
      return true;
   }

   /**
    * @brief SOFT-09: the pack core: error texts, a missing buffer, raw bytes, hex strings, and every cut of a
    *        packed selection and condition
    * @return nothing; CHECK records the results
    */
   void test_pack() {
      bool texts = std::string(ocs::tom::pack_error_text(12345)).empty();
      for (const int code : {PACK_SUCCESS, PACK_ENOMEM, PACK_FORMAT, PACK_BADARG, PACK_VERSION, PACK_AUTHINFO}) {
         texts = texts && !std::string(ocs::tom::pack_error_text(code)).empty();
      }
      const bool no_buffer = throws_pack([] { ocs::tom::init_buffer(nullptr, 0, false, false); }, PACK_FORMAT);
      CHECK(19, "every pack code has a text and an unknown one none; no buffer throws PACK_FORMAT, where "
                "init_packbuffer() gave it",
            texts && no_buffer);

      PackBuffer out;
      ocs::tom::put_buf(&out.raw(), "xyz", 3);
      PackBuffer in = PackBuffer::reading(out.data(), out.size());
      char *buf = ocs::tom::get_buf(&in.raw(), 3);
      const bool bytes = buf != nullptr && std::memcmp(buf, "xyz", 3) == 0 &&
                         ocs::tom::get_buf(&in.raw(), 0) == nullptr &&
                         throws_pack([&] { static_cast<void>(ocs::tom::get_buf(&in.raw(), 3)); }, PACK_FORMAT);
      std::free(buf);
      // each byte is written low nibble first: 0x1f gives "F1", 0xa5 "5A"
      const char raw[] = {0x00, 0x1f, static_cast<char>(0xa5), 0x7f, static_cast<char>(0xff)};
      char *back = nullptr;
      const int size = ocs::tom::hex_to_bytes("00F15AF7FF", &back);
      const bool hex = ocs::tom::bytes_to_hex(raw, sizeof(raw)) == "00F15AF7FF" &&
                       size == static_cast<int>(sizeof(raw)) && std::memcmp(back, raw, sizeof(raw)) == 0;
      std::free(back);
      // the second digit of the first byte is no hex digit: -(0 * 2) - 1
      char *bad = nullptr;
      const int bad_size = ocs::tom::hex_to_bytes("0G12", &bad);
      std::free(bad);
      CHECK(20, "put_buf and get_buf carry bytes and throw PACK_FORMAT past the end; bytes_to_hex and hex_to_bytes "
                "carry bytes through a hex string and report a bad digit by a negative size",
            bytes && hex && bad_size == -1);

      const std::size_t header = PackBuffer().size();
      const What<US> selection = what<US_name>().with<US_entries>(what<UE_name>());
      PackBuffer what_out;
      ocs::tom::pack_what(&what_out.raw(), selection.get());
      const std::vector<char> what_bytes(what_out.data(), what_out.data() + what_out.size());
      const Where<US> where = field<US_name>().like("t*") || field<US_entries>().any(field<UE_name>() == "bob");
      PackBuffer where_out;
      ocs::tom::pack_where(&where_out.raw(), where.get());
      const std::vector<char> where_bytes(where_out.data(), where_out.data() + where_out.size());
      const bool what_cuts = every_cut_fails(what_bytes, header, [](sge_pack_buffer *pb) {
         lEnumeration *ep = ocs::tom::unpack_what(pb);
         ocs::tom::free_what(ep);
      });
      const bool where_cuts = every_cut_fails(where_bytes, header, [](sge_pack_buffer *pb) {
         lCondition *cp = ocs::tom::unpack_where(pb);
         ocs::tom::free_where(cp);
      });
      CHECK(21, "unpacking a nested selection and a condition with a sub-list cut at any byte throws PACK_FORMAT "
                "and leaves nothing behind",
            what_cuts && where_cuts);
   }

   /// the hash of the dump text, recorded on the code before the cores took the old bodies over
   constexpr std::uint64_t DumpTextHash = 0x77f15bc08feb7b97ULL;
   /// the hash of the error numbers of every cut of the dump text, recorded on the same code
   constexpr std::uint64_t DumpCutHash = 0xc5f21f6804def9fbULL;
   /// the hash of the text the print functions write, recorded on the same code
   constexpr std::uint64_t RestTextHash = 0x6ba2bcf5a05b2a91ULL;

   /**
    * @brief FNV-1a over bytes
    * @param bytes the bytes
    * @param n how many
    * @param h the hash so far
    * @return the hash
    */
   std::uint64_t fnv1a(const char *bytes, const std::size_t n, std::uint64_t h = 0xcbf29ce484222325ULL) {
      for (std::size_t i = 0; i < n; i++) {
         h ^= static_cast<unsigned char>(bytes[i]);
         h *= 0x100000001b3ULL;
      }
      return h;
   }

   /**
    * @brief What a writer writes into a stream, as text
    * @param write writes to the stream
    * @return the text
    */
   std::string text_of(const std::function<void(FILE *)> &write) {
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
    * @param read reads from the stream
    */
   void read_text(const std::string &text, const std::function<void(FILE *)> &read) {
      FILE *fp = fmemopen(const_cast<char *>(text.data()), text.size(), "r");
      read(fp);
      fclose(fp);
   }

   /**
    * @brief Run a reader, catching the ListCoreError of a failure
    * @param read the reader, returning what it read or nullptr
    * @return true if the reader threw or read nothing; what it read is freed
    */
   template <typename Read>
   bool reads_nothing(Read read) {
      try {
         auto *result = read();
         const bool nothing = result == nullptr;
         if constexpr (std::is_same_v<decltype(result), lList *>) {
            ocs::tom::free_raw_list(result);
         } else {
            ocs::tom::free_raw_elem(result);
         }
         return nothing;
      } catch (const ListCoreError &) {
         return true;
      }
   }

   /**
    * @brief The elements the dump pins use: every value type, a quote in a string, a sub-object, a sub-list
    *
    * Built through the raw interface and the free forms on the same cores as before, so that the text is byte
    * for byte that of the elements the hashes were recorded on.
    *
    * @param[out] list a list of user sets with entries, owned by the caller
    * @return a TEST element
    */
   RawElementPtr dump_fixture(lList *&list) {
      RawElementPtr ep = ocs::tom::create_raw_elem(ocs::tom::TEST::descriptor.e);
      lListElem *e = ep.get().raw();
      ocs::tom::set(e, TEST_bool, true);
      ocs::tom::set(e, TEST_uint32, std::uint32_t{42});
      ocs::tom::set(e, TEST_uint64, std::uint64_t{1ULL << 40});
      ocs::tom::set(e, TEST_int, std::int32_t{-7});
      ocs::tom::set(e, TEST_long, std::int64_t{-123456789L});
      ocs::tom::set(e, TEST_double, 2.5);
      ocs::tom::set(e, TEST_host, Host{"host1"});
      ocs::tom::set(e, TEST_string, "say \"hi\" now");
      RawElementPtr job = ocs::tom::create_raw_elem(ocs::tom::JB::descriptor.e);
      ocs::tom::set(job.get().raw(), JB_job_number, std::uint32_t{7});
      ocs::tom::set(job.get().raw(), JB_owner, "alice");
      ocs::tom::set(e, TEST_obj_jb, std::move(job));
      list = ocs::tom::create_raw_list(ocs::tom::US::descriptor.e, Hashing::On, "sets").release();
      for (const char *name : {"one", "two"}) {
         lListElem *set = ocs::tom::emplace_back<US_name>(&list, name);
         ocs::tom::emplace_back_in<US_entries, UE_name>(set, "bob");
      }
      return ep;
   }

   /**
    * @brief SOFT-10: the dump core: the text of before, reading it back, every cut of it, and element files
    * @return nothing; CHECK records the results
    */
   void test_dump() {
      lList *sets = nullptr;
      const RawElementPtr owner = dump_fixture(sets);
      const lListElem *ep = owner.get().raw();
      const lListElem *job = ocs::tom::get<const lListElem *>(ep, TEST_obj_jb);

      const std::string elem_text = text_of([&](FILE *fp) { ocs::tom::dump_elem(fp, ep, 1); });
      const std::string list_text = text_of([&](FILE *fp) { ocs::tom::dump_list(fp, sets, 0); });
      const std::string object_text = text_of([&](FILE *fp) { ocs::tom::dump_object(fp, job, 0); });
      const std::string descr_text =
         text_of([&](FILE *fp) { ocs::tom::dump_descr(fp, ocs::tom::US::descriptor.e, 2); });
      std::uint64_t hash = fnv1a(elem_text.data(), elem_text.size());
      hash = fnv1a(list_text.data(), list_text.size(), hash);
      hash = fnv1a(object_text.data(), object_text.size(), hash);
      hash = fnv1a(descr_text.data(), descr_text.size(), hash);

      std::string elem_again;
      std::string list_again;
      std::string object_again;
      read_text(elem_text, [&](FILE *fp) {
         lListElem *back = ocs::tom::undump_elem(fp, ocs::tom::TEST::descriptor.e);
         elem_again = text_of([&](FILE *out) { ocs::tom::dump_elem(out, back, 1); });
         ocs::tom::free_raw_elem(back);
      });
      read_text(list_text, [&](FILE *fp) {
         lList *back = ocs::tom::undump_list(fp, nullptr, nullptr);
         list_again = text_of([&](FILE *out) { ocs::tom::dump_list(out, back, 0); });
         ocs::tom::free_raw_list(back);
      });
      read_text(object_text, [&](FILE *fp) {
         lListElem *back = ocs::tom::undump_object(fp);
         object_again = text_of([&](FILE *out) { ocs::tom::dump_object(out, back, 0); });
         ocs::tom::free_raw_elem(back);
      });
      CHECK(22, "dumping writes the text of before, and reading it back gives the same text",
            hash == DumpTextHash && elem_again == elem_text && list_again == list_text &&
               object_again == object_text);

      // every cut of the text after a whole line: nothing read, and the error numbers of before
      std::uint64_t cuts = 0xcbf29ce484222325ULL;
      bool all_null = true;
      for (const std::string *text : {&elem_text, &list_text, &object_text}) {
         for (std::size_t n = 0; n < text->size(); n++) {
            if (n > 0 && (*text)[n - 1] != '\n') {
               continue;
            }
            const std::string cut = text->substr(0, n);
            int lerrno = 0;
            bool nothing = false;
            read_text(cut.empty() ? std::string(" ") : cut, [&](FILE *fp) {
               cull_state_set_lerrno(0);
               if (text == &elem_text) {
                  nothing = reads_nothing([fp] { return ocs::tom::undump_elem(fp, ocs::tom::TEST::descriptor.e); });
               } else if (text == &list_text) {
                  nothing = reads_nothing([fp] { return ocs::tom::undump_list(fp, nullptr, nullptr); });
               } else {
                  nothing = reads_nothing([fp] { return ocs::tom::undump_object(fp); });
               }
               lerrno = cull_state_get_lerrno();
            });
            all_null = all_null && nothing;
            cuts = fnv1a(reinterpret_cast<const char *>(&lerrno), sizeof(lerrno), cuts);
         }
      }
      const bool null_file = throws_lerrno([&] { ocs::tom::dump_elem(nullptr, ep, 0); }, LEFILENULL) &&
                             reads_nothing([] {
                                return ocs::tom::undump_elem(nullptr, ocs::tom::TEST::descriptor.e);
                             }) &&
                             reads_nothing([] { return ocs::tom::undump_list(nullptr, nullptr, nullptr); });
      const bool no_file = throws_lerrno([] {
         lListElem *read = ocs::tom::undump_elem_from("/nonexistent/test_tom_soft_path", ocs::tom::TEST::descriptor.e);
         ocs::tom::free_raw_elem(read);
      }, LEOPEN);
      CHECK(23, "reading a cut text reads nothing and leaves the error numbers of before; a null stream and a "
                "missing file fail with LEFILENULL and LEOPEN",
            all_null && cuts == DumpCutHash && null_file && no_file);

      // files of packed elements; a TEST element holds int and long fields, which are not packed, so a job is used
      char dir[] = "/tmp/test_tom_soft_path_XXXXXX";
      bool files = mkdtemp(dir) != nullptr;
      if (files) {
         ocs::tom::write_elem_to_disk(job, dir, "elem", "test");
         lListElem *read = ocs::tom::read_elem_from_disk(dir, "elem", ocs::tom::JB::descriptor.e, "test");
         files = read != nullptr && ocs::tom::get<std::uint32_t>(read, JB_job_number) == 7 &&
                 std::strcmp(ocs::tom::get<const char *>(read, JB_owner), "alice") == 0;
         ocs::tom::free_raw_elem(read);
      }
      const bool not_packed = throws_runtime([&] { ocs::tom::write_elem_to_disk(ep, dir, "test", "test"); });
      const bool no_name = throws_runtime([&] { ocs::tom::write_elem_to_disk(job, nullptr, nullptr, "test"); });
      const bool missing = throws_runtime([&] {
         lListElem *read = ocs::tom::read_elem_from_disk(dir, "missing", ocs::tom::JB::descriptor.e, "test");
         ocs::tom::free_raw_elem(read);
      });
      const std::string path = std::string(dir) + "/elem";
      bool cut_file = truncate(path.c_str(), 20) == 0;
      cut_file = cut_file && throws_runtime([&] {
         lListElem *read = ocs::tom::read_elem_from_disk(dir, "elem", ocs::tom::JB::descriptor.e, "test");
         ocs::tom::free_raw_elem(read);
      });
      unlink(path.c_str());
      unlink((std::string(dir) + "/test").c_str());
      rmdir(dir);
      ocs::tom::free_raw_list(sets);
      CHECK(24, "write_elem_to_disk and read_elem_from_disk carry an element through a file and throw on fields that "
                "are not packed, no name, a missing file and a cut file, where the old wrappers gave 1 or nullptr",
            files && not_packed && no_name && missing && cut_file);
   }

   /**
    * @brief The names of the elements of a string list, in list order
    * @param lp the list, or nullptr
    * @return the names, joined by blanks
    */
   std::string names_of(const lList *lp) {
      std::string names;
      if (lp == nullptr) {
         return names;
      }
      for (const auto ep : ocs::tom::ConstRawList(lp)) {
         if (!names.empty()) {
            names += ' ';
         }
         const char *name = ep.get<const char *>(ST_name);
         names += name != nullptr ? name : "(null)";
      }
      return names;
   }

   /**
    * @brief SOFT-11: the rest of the cores: type names, removing shared names, reduced descriptors, merging and
    *        nesting selections, and the text of the print core
    * @return nothing; CHECK records the results
    */
   void test_rest() {
      const bool types = std::strcmp(ocs::tom::type_name(-1), "unknown multitype") == 0 &&
                         std::strcmp(ocs::tom::type_name(lUlong64T), "lUlong64T") == 0;

      // shared names: taken out of both; nothing happens for a missing list
      lList *a = ListPtr<ocs::tom::ST>::from_string<ST_name>("a b c", " ").release();
      lList *b = ListPtr<ocs::tom::ST>::from_string<ST_name>("b c d", " ").release();
      ocs::tom::remove_common_str(a, b, ST_name, true);
      bool common = names_of(a) == "a" && names_of(b) == "d";
      lList *no_list = nullptr;
      ocs::tom::remove_common_str(no_list, b, ST_name, true);
      ocs::tom::remove_common_str(a, no_list, ST_name, true);
      common = common && names_of(a) == "a" && names_of(b) == "d" && no_list == nullptr;
      ocs::tom::free_raw_list(a);
      ocs::tom::free_raw_list(b);

      // a reduced descriptor, and none for the selection of no field
      const What<US> name_only = what<US_name>();
      const lDescr *reduced = ocs::tom::reduced_descr(ocs::tom::US::descriptor.e, name_only.get());
      // a reduced descriptor keeps the full length and marks the fields it leaves out as absent
      const bool reduction = reduced != nullptr && ocs::tom::present_pos(reduced, US_name) >= 0 &&
                             ocs::tom::present_pos(reduced, US_type) < 0;
      ocs::tom::release_descr(reduced);
      lEnumeration *none = ocs::tom::make_what_special(WHAT_NONE);
      const bool no_reduction = ocs::tom::reduced_descr(ocs::tom::US::descriptor.e, none) == nullptr;
      ocs::tom::free_what(none);
      CHECK(25, "type_name names an unknown type, remove_common_str takes the shared names out and leaves a missing "
                "list alone, reduced_descr reduces and gives nothing for the selection of no field",
            types && common && reduction && no_reduction);

      // merging: the fields of both, in type order; for a field in both, the sub-selections of both
      lEnumeration *merged = ocs::tom::merge_what(
         what<JB_job_number>().with<JB_ja_tasks>(what<JAT_task_number>()).release(),
         what<JB_owner>().with<JB_ja_tasks>(what<JAT_status>()).release());
      int merged_fields = 0;
      const lEnumeration *tasks_sub = nullptr;
      for (int i = 0; merged != nullptr && merged[i].nm != NoName; i++) {
         merged_fields++;
         if (merged[i].nm == JB_ja_tasks) {
            tasks_sub = merged[i].ep;
         }
      }
      bool sub_task_number = false;
      bool sub_status = false;
      for (int i = 0; tasks_sub != nullptr && tasks_sub[i].nm != NoName; i++) {
         sub_task_number = sub_task_number || tasks_sub[i].nm == JAT_task_number;
         sub_status = sub_status || tasks_sub[i].nm == JAT_status;
      }
      ocs::tom::free_what(merged);
      // a sub-selection on a selected field is taken, also on one that is no list; one on a field not selected
      // throws ForeignField and frees the sub-selection
      lEnumeration *what_name = what<US_name>().release();
      ocs::tom::set_sub_what(what_name, US_name, what<UE_name>().release());
      const bool set_missing =
         throws_reason([&] { ocs::tom::set_sub_what(what_name, US_type, what<UE_name>().release()); },
                       FieldException::Reason::ForeignField);

      // the text of the print core on a list, an element, a selection and a condition
      lList *sets = ocs::tom::create_raw_list(ocs::tom::US::descriptor.e, Hashing::On, "sets").release();
      for (const char *name : {"one", "two", "three"}) {
         ocs::tom::emplace_back<US_name>(&sets, name);
      }
      ocs::tom::emplace_back_in<US_entries, UE_name>(sets->first, "alice");
      std::string text = text_of([&](FILE *fp) { ocs::tom::write_to(fp, ocs::tom::ConstRawList(sets)); });
      text += text_of([&](FILE *fp) { ocs::tom::write_to(fp, ocs::tom::ConstRawElement(sets->first)); });
      dstring buffer = DSTRING_INIT;
      ocs::tom::print_elem(&buffer, sets->first, 0);
      text += sge_dstring_get_string(&buffer) != nullptr ? sge_dstring_get_string(&buffer) : "";
      sge_dstring_free(&buffer);
      text += text_of([&](FILE *fp) { ocs::tom::write_to(fp, what_name); });
      const Where<US> where = field<US_name>() == "one" && (field<US_type>() > 3U || !field<US_name>().like("t*"));
      text += text_of([&](FILE *fp) { ocs::tom::write_to(fp, where.get()); });
      for (int mt = -1; mt <= lUlong64T + 1; mt++) {
         text += ocs::tom::type_name(mt);
      }
      const std::uint64_t hash = fnv1a(text.data(), text.size());
      ocs::tom::free_what(what_name);
      ocs::tom::free_raw_list(sets);
      CHECK(26, "merge_what keeps the fields and sub-selections of both, set_sub_what refuses a field not selected, "
                "and the print core writes the text of before",
            merged_fields == 3 && sub_task_number && sub_status && set_missing && hash == RestTextHash);
   }

} // namespace legacy_cores

/**
 * @brief Run the scenarios with a fresh sink directory in the temporary directory
 *
 * Started with `--no-sink-child <dir>`, it runs only the check without a sink
 * (#no_sink_child) and returns its result.
 *
 * @param argc argument count
 * @param argv arguments
 * @return the exit code of the module test framework
 */
int main(int argc, char *argv[]) {
   if (argc == 3 && std::string(argv[1]) == NoSinkChild) {
      return no_sink_child(argv[2]);
   }
   self = argv[0];
   ocs::tom::install_model(ocs::tom::gcs_model());
   char dir[] = "/tmp/test_tom_soft_path.XXXXXX";
   if (mkdtemp(dir) == nullptr) {
      perror("mkdtemp");
      return 1;
   }
   sink = dir;
   setenv("OCS_SOFT_PATH_DIR", dir, 1);
   const int rc = ocs::module_test::run(argc, argv, {
      {"SOFT-01", "each soft path",
       "every soft error path the contract of N-L-4 keeps in the old accessors and the free setter is recorded "
       "with accessor, field, reason and caller, and the result stays as it was",
       {"call each soft path once with a sink directory"},
       "one line per call, return value and error number unchanged", {{"LEG-03", "covers"}},
       {1, 2, 3, 4, 5, 6, 7, 8}, test_each_soft_path},
      {"SOFT-02", "hot loop",
       "a soft path taken in a loop writes a bounded number of lines",
       {"call one soft path 10 000 times"}, "between 1 and 8 lines", {{"LEG-03", "covers"}}, {9}, test_hot_loop},
      {"SOFT-03", "no sink",
       "without the sink directory nothing is written",
       {"call a soft path in a process whose sink directory does not exist"},
       "no file, result unchanged", {{"LEG-03", "covers"}}, {10}, test_no_sink},
      {"SOFT-04", "list structure cores", "the list structure cores refuse null, bound and foreign arguments",
       {"append, insert and dechain with a null list, a null element, a bound element and an element of another "
        "list; append a null list and a list of another type"},
       "NullList, NullElement, BoundElement, ForeignList and WrongType thrown, nothing changed",
       {{"LEG-01", "covers"}}, {11, 12, 13}, legacy_cores::test_list_structure},
      {"SOFT-05", "string lookup cores", "the case-insensitive and prefix lookup cores",
       {"look up with a null value, in a null list, by a foreign field, a field of another type and an absent "
        "field; look up by case and by prefix"},
       "nothing for null arguments; AbsentField and WrongType thrown; the element found by case and prefix",
       {{"LEG-01", "covers"}}, {14}, legacy_cores::test_string_lookups},
      {"SOFT-06", "keyed add and remove cores", "adding and removing by key in the core",
       {"add with null arguments, a foreign field and a field of another type; remove found, missing and null keys"},
       "the list created and named; NullKey, NullDescriptor, AbsentField and WrongType thrown; removals told",
       {{"LEG-01", "covers"}}, {15, 16}, legacy_cores::test_add_and_remove_by_key},
      {"SOFT-07", "selection core", "the selection core on unusual conditions",
       {"select by m= on a 64 bit field, by a copied combined condition, by a sub-list condition over an element "
        "without the sub-list; search in no list"},
       "no list; the keys 1 and 3; the one set; NullList thrown", {{"LEG-01", "covers"}}, {17},
       legacy_cores::test_selections},
      {"SOFT-08", "nested selection", "a nested selection makes an emptied sub-list no list",
       {"select user sets with a nested selection, one with an entry and one with an empty sub-list"},
       "the sub-list kept with its entry; no list for the empty one", {{"LEG-01", "covers"}}, {18},
       legacy_cores::test_nested_selection},
      {"SOFT-09", "pack core", "the pack core reports its errors by PackException",
       {"ask for every error text, init no buffer, carry raw bytes and hex strings, unpack every cut of a nested "
        "selection and a condition with a sub-list"},
       "the texts; PACK_FORMAT for no buffer, past the end and every cut; -1 for a bad hex digit",
       {{"LEG-01", "covers"}}, {19, 20, 21}, legacy_cores::test_pack},
      {"SOFT-10", "dump core", "the dump core writes and reads the text of before",
       {"dump an element of every value type, a list, an object and a descriptor; read them back, read every cut, "
        "read from no stream and a missing file; carry a packed element through a file and fail on bad ones"},
       "the recorded hashes; the same text read back; LEFILENULL and LEOPEN; a runtime error for every bad file",
       {{"LEG-01", "covers"}}, {22, 23, 24}, legacy_cores::test_dump},
      {"SOFT-11", "rest of the cores", "type names, shared names, reductions, merged selections and the print text",
       {"name every type; take out shared names, also with a missing list; reduce a descriptor; merge and nest "
        "selections; print a list, an element, a selection and a condition"},
       "the names and lists; the recorded hash of the printed text", {{"LEG-01", "covers"}}, {25, 26},
       legacy_cores::test_rest},
   });
   const std::string cleanup = std::string("rm -rf ") + dir;
   [[maybe_unused]] const int removed = std::system(cleanup.c_str());
   return rc;
}
