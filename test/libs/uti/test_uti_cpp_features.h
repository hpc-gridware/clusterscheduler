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
 * @brief Types shared by test_uti_cpp_features and its loadable module (CS-3039)
 *
 * The exception classes are defined inline in this header, so the executable and
 * the module each carry their own copy of their type information - as a class
 * hierarchy in a static library linked into a daemon and into a spooling plugin
 * would. Whether a throw on one side is caught by type on the other side is what
 * the test finds out.
 */

#pragma once

#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>

namespace ocs::probe {

   /** @brief A broken precondition: a programming error found at run time */
   class LogicError : public std::logic_error {
   public:
      using std::logic_error::logic_error;
   };

   /** @brief An expected runtime error */
   class RuntimeError : public std::runtime_error {
   public:
      using std::runtime_error::runtime_error;
   };

   /** @brief A runtime error with a detail of its own: a required entry is missing */
   class NotFound : public RuntimeError {
      int key_;   ///< the key that was not found

   public:
      /**
       * @brief Create the error
       * @param message what happened
       * @param key the key that was not found
       */
      NotFound(const std::string &message, int key) : RuntimeError(message), key_(key) {}

      /**
       * @brief The key that was not found
       * @return the key
       */
      [[nodiscard]] int key() const noexcept { return key_; }
   };

   /// what the module throws in test_uti_cpp_features_throw()
   enum class ThrowKind : int {
      not_found = 1,   ///< NotFound
      logic = 2,       ///< LogicError
      standard = 3     ///< std::runtime_error, no class of ours
   };

   /// what the module caught in test_uti_cpp_features_catch()
   enum class Caught : int {
      nothing = 0,     ///< the callback returned normally
      not_found = 1,   ///< caught as NotFound
      exception = 2    ///< caught only as std::exception
   };

   /// a callback the executable hands to the module
   using Callback = void (*)(void *arg);

}

/// the key the module puts into a NotFound
constexpr int TEST_UTI_CPP_FEATURES_KEY = 4711;

extern "C" {
   /// signature of test_uti_cpp_features_throw(): throws the given kind with the message
   using test_uti_cpp_features_throw_func = void (*)(int kind, const char *message);
   /// signature of test_uti_cpp_features_call(): calls the callback without catching
   using test_uti_cpp_features_call_func = void (*)(ocs::probe::Callback cb, void *arg);
   /// signature of test_uti_cpp_features_catch(): calls the callback and catches, the message goes into buf
   using test_uti_cpp_features_catch_func = int (*)(ocs::probe::Callback cb, void *arg, char *buf, size_t size);
   /// signature of test_uti_cpp_features_capture(): stores a NotFound in *out
   using test_uti_cpp_features_capture_func = void (*)(std::exception_ptr *out, const char *message);
}
