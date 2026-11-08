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
 * @brief Module test of the exception classes ocs::LogicError and ocs::RuntimeError (CS-3048)
 */

#include <exception>
#include <stdexcept>
#include <string>

#include "uti/ocs_Exception.h"

#include "ocs_module_test.h"

/**
 * @brief Throw an exception and report through which handler it was caught
 *
 * @tparam Thrown the exception class to throw
 * @tparam Caught the class of the handler
 * @param message the message to throw with
 * @return the message `what()` returned in the handler, or an empty string if
 *         the handler did not match
 */
template <typename Thrown, typename Caught>
static std::string
caught_message(const std::string &message) {
   try {
      try {
         throw Thrown(message);
      } catch (const Caught &e) {
         return e.what();
      }
   } catch (...) {
   }
   return "";
}

/**
 * @brief EXC-01 for ocs::LogicError
 * @return nothing; CHECK records the results
 */
static void
test_logic_error() {
   const std::string message = "logic error message";
   CHECK(1, "LogicError is caught as LogicError with its message",
         (caught_message<ocs::LogicError, ocs::LogicError>(message) == message));
   CHECK(2, "LogicError is caught as std::logic_error with its message",
         (caught_message<ocs::LogicError, std::logic_error>(message) == message));
   CHECK(3, "LogicError is caught as std::exception with its message",
         (caught_message<ocs::LogicError, std::exception>(message) == message));
   CHECK(4, "LogicError built from a C string keeps it",
         std::string(ocs::LogicError("c string").what()) == "c string");
}

/**
 * @brief EXC-01 for ocs::RuntimeError
 * @return nothing; CHECK records the results
 */
static void
test_runtime_error() {
   const std::string message = "runtime error message";
   CHECK(5, "RuntimeError is caught as RuntimeError with its message",
         (caught_message<ocs::RuntimeError, ocs::RuntimeError>(message) == message));
   CHECK(6, "RuntimeError is caught as std::runtime_error with its message",
         (caught_message<ocs::RuntimeError, std::runtime_error>(message) == message));
   CHECK(7, "RuntimeError is caught as std::exception with its message",
         (caught_message<ocs::RuntimeError, std::exception>(message) == message));
   CHECK(8, "the two bases do not catch each other",
         (caught_message<ocs::RuntimeError, std::logic_error>(message).empty()
          && caught_message<ocs::LogicError, std::runtime_error>(message).empty()));
}

/**
 * @brief Run the scenarios of ocs::LogicError and ocs::RuntimeError
 * @param argc number of arguments
 * @param argv the arguments; the module test frame takes its own options from them
 * @return 0 if every check passed, 1 otherwise
 */
int main(int argc, char *argv[]) {
   return ocs::module_test::run(argc, argv, {
      {"EXCEPT-01", "logic error",
       "ocs::LogicError keeps its message and is caught through its standard bases",
       {"throw LogicError with a message", "catch it as LogicError, std::logic_error and std::exception"},
       "every handler matches and what() returns the message",
       {{"EXC-01", "covers"}}, {1, 2, 3, 4}, test_logic_error},
      {"EXCEPT-02", "runtime error",
       "ocs::RuntimeError keeps its message and is caught through its standard bases",
       {"throw RuntimeError with a message", "catch it as RuntimeError, std::runtime_error and std::exception",
        "catch each class with the other's standard base"},
       "every handler of its own hierarchy matches and what() returns the message; the other base does not match",
       {{"EXC-01", "covers"}}, {5, 6, 7, 8}, test_runtime_error},
   });
}
