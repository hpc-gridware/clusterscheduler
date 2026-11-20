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

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

#include "cull/cull_lerrno.h"
#include "cull/cull_state.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/tom/ocs_tom_all.h"
#include "tom/ocs_Element.h"

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
      lInit(nmv);
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
         list = lCreateList("full", TEST_Type);
         full = lCreateElem(TEST_Type);
         lAppendElem(list, full);
         what = lWhat("%T(%I)", TEST_Type, TEST_obj_any);
         reduced_list = lSelect("reduced", list, nullptr, what);
         reduced = lFirstRW(reduced_list);
         job = lCreateElem(JB_Type);
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
   lInit(nmv);
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
   });
   const std::string cleanup = std::string("rm -rf ") + dir;
   [[maybe_unused]] const int removed = std::system(cleanup.c_str());
   return rc;
}
