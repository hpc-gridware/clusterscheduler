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
 * @brief Loadable module for test_uti_cpp_features (CS-3039)
 *
 * Built and loaded like a spooling plugin (dlopen with RTLD_LOCAL). It throws
 * exceptions to the executable, lets exceptions of the executable pass through its
 * frames, catches them, and hands one over as std::exception_ptr.
 */

#include <cstdio>

#include "test_uti_cpp_features.h"

/**
 * @brief Throw an exception of the given kind
 * @param kind a ocs::probe::ThrowKind
 * @param message the message of the exception
 * @return never returns normally
 */
extern "C" void test_uti_cpp_features_throw(int kind, const char *message) {
   switch (static_cast<ocs::probe::ThrowKind>(kind)) {
      case ocs::probe::ThrowKind::not_found:
         throw ocs::probe::NotFound(message, TEST_UTI_CPP_FEATURES_KEY);
      case ocs::probe::ThrowKind::logic:
         throw ocs::probe::LogicError(message);
      case ocs::probe::ThrowKind::standard:
      default:
         throw std::runtime_error(message);
   }
}

/**
 * @brief Call a callback of the executable without catching anything
 *
 * An exception of the callback passes through the frames of this module.
 *
 * @param cb the callback
 * @param arg its argument
 * @return nothing
 */
extern "C" void test_uti_cpp_features_call(ocs::probe::Callback cb, void *arg) {
   cb(arg);
}

/**
 * @brief Call a callback of the executable and catch what it throws
 * @param cb the callback
 * @param arg its argument
 * @param buf receives the message of a caught exception
 * @param size size of buf
 * @return a ocs::probe::Caught
 */
extern "C" int test_uti_cpp_features_catch(ocs::probe::Callback cb, void *arg, char *buf, size_t size) {
   try {
      cb(arg);
   } catch (const ocs::probe::NotFound &e) {
      snprintf(buf, size, "%s", e.what());
      return static_cast<int>(ocs::probe::Caught::not_found);
   } catch (const std::exception &e) {
      snprintf(buf, size, "%s", e.what());
      return static_cast<int>(ocs::probe::Caught::exception);
   }
   return static_cast<int>(ocs::probe::Caught::nothing);
}

/**
 * @brief Throw a NotFound, catch it here and hand it over as std::exception_ptr
 * @param out receives the exception
 * @param message the message of the exception
 * @return nothing
 */
extern "C" void test_uti_cpp_features_capture(std::exception_ptr *out, const char *message) {
   try {
      throw ocs::probe::NotFound(message, TEST_UTI_CPP_FEATURES_KEY);
   } catch (...) {
      *out = std::current_exception();
   }
}
