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
 * @brief Module test of ocs::AnswerException: message, copies and the shared answer list (CS-3048)
 */

#include <exception>
#include <string>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/tom/ocs_tom_model.h"
#include "sgeobj/ocs_AnswerException.h"
#include "sgeobj/sge_answer.h"
#include "tom/ocs_Element.h"

#include "ocs_module_test.h"

static const std::string error_text = "the error";
static const std::string warning_text = "the warning";
static const std::string no_details = "No error details available";

/**
 * @brief Build an answer list holding an error followed by a warning
 * @return the new list; the caller owns it
 */
static ocs::tom::ListPtr<ocs::tom::AN>
error_and_warning() {
   ocs::tom::ListPtr<ocs::tom::AN> answer_list;
   answer_list_add(&answer_list, error_text.c_str(), STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   answer_list_add(&answer_list, warning_text.c_str(), STATUS_OK, ANSWER_QUALITY_WARNING);
   return answer_list;
}

/**
 * @brief EXC-02: the message, and the ownership taken at construction
 * @return nothing; CHECK records the results
 */
static void
test_message() {
   auto answer_list = error_and_warning();
   ocs::AnswerException e(&answer_list);
   CHECK(1, "the constructor takes the list and clears the caller's pointer", answer_list == nullptr);
   CHECK(2, "what() is the text of the last error entry, not of the warning after it", e.what() == error_text);

   ocs::tom::ListPtr<ocs::tom::AN> warning_only;
   answer_list_add(&warning_only, warning_text.c_str(), STATUS_OK, ANSWER_QUALITY_WARNING);
   ocs::AnswerException w(&warning_only);
   ocs::tom::ListPtr<ocs::tom::AN> no_list;
   ocs::AnswerException n(&no_list);
   CHECK(3, "a list without an error and no list give the fixed text",
         w.what() == no_details && n.what() == no_details && n.take_answer_list() == nullptr);
}

/**
 * @brief EXC-02: copies share the list, which is freed once and handed out once
 * @return nothing; CHECK records the results
 */
static void
test_copies() {
   auto answer_list = error_and_warning();
   std::exception_ptr captured;
   ocs::AnswerException copy(&answer_list);
   try {
      try {
         throw copy;
      } catch (const ocs::AnswerException &) {
         captured = std::current_exception();
      }
   } catch (...) {
   }
   CHECK(4, "the thrown copy is captured in a std::exception_ptr", captured != nullptr);

   ocs::tom::ListPtr<ocs::tom::AN> taken;
   std::string rethrown_what;
   try {
      std::rethrow_exception(captured);
   } catch (ocs::AnswerException &e) {
      rethrown_what = e.what();
      taken = e.take_answer_list();
   } catch (...) {
   }
   CHECK(5, "the rethrown copy keeps the message", rethrown_what == error_text);
   CHECK(6, "the first take returns the whole list with both entries",
         taken != nullptr && taken->size() == 2
         && ocs::tom::get<AN_text>(taken->front()) == error_text
         && ocs::tom::get<AN_text>(taken->back()) == warning_text);
   CHECK(7, "a later take on another copy returns nullptr, and the message stays",
         copy.take_answer_list() == nullptr && copy.what() == error_text);
}

/**
 * @brief Run the scenarios of ocs::AnswerException
 * @param argc number of arguments
 * @param argv the arguments; the module test frame takes its own options from them
 * @return 0 if every check passed, 1 otherwise
 */
int main(int argc, char *argv[]) {
   ocs::tom::install_model(ocs::tom::gcs_model());
   return ocs::module_test::run(argc, argv, {
      {"ANSEXC-01", "message",
       "the exception takes the list and its message is the text of the last error entry",
       {"build a list with an error and a warning and construct the exception from it",
        "construct one from a list with only a warning and one from no list"},
       "the caller's pointer is cleared; what() is the error's text, otherwise the fixed text",
       {{"EXC-02", "covers"}}, {1, 2, 3}, test_message},
      {"ANSEXC-02", "copies",
       "copies share one answer list, it is freed once and handed out once",
       {"copy the exception, throw it, capture it in a std::exception_ptr and rethrow it",
        "take the list from the rethrown copy, then from the first copy"},
       "what() is the error's text on every copy; the first take returns both entries, the second nullptr",
       {{"EXC-02", "covers"}}, {4, 5, 6, 7}, test_copies},
   });
}
