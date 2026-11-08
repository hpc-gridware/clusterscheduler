#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief The exception classes of the `ocs` namespace
 *
 * Two bases, each derived from its standard counterpart, so that a handler
 * catching `std::logic_error`, `std::runtime_error` or `std::exception`
 * catches them as well. Further classes are added only when a caller has to
 * tell them apart by type.
 */

#include <stdexcept>

namespace ocs {
   /**
    * @brief A programming error found at run time
    *
    * Thrown for a condition the code should never reach, e.g. an absent
    * field or an element of the wrong type. The message is kept and returned
    * by `what()`.
    */
   class LogicError : public std::logic_error {
   public:
      using std::logic_error::logic_error;

      /** @brief Destroy the exception; defined out of line as the key function */
      ~LogicError() override;
   };

   /**
    * @brief An expected error at run time
    *
    * Thrown for a condition the caller has to expect, e.g. a key that was not
    * found or input that cannot be parsed. The message is kept and returned
    * by `what()`.
    */
   class RuntimeError : public std::runtime_error {
   public:
      using std::runtime_error::runtime_error;

      /** @brief Destroy the exception; defined out of line as the key function */
      ~RuntimeError() override;
   };
}
