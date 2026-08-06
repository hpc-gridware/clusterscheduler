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
 * @brief The base exception type of the `ocs` namespace
 */

#include <exception>
#include <string>

namespace ocs {
   /**
    * @brief Base class for exceptions thrown by `ocs` code
    *
    * @warning Currently a placeholder. The constructor **discards** its
    *          message: it neither stores it nor passes it to `std::exception`,
    *          so `what()` returns the default text and the reason for the
    *          throw is lost. Do not rely on the message surviving until this
    *          is implemented.
    */
   class Exception : public std::exception {
   public:
      /**
       * @brief Construct an exception
       *
       * @param message the reason for the throw; currently ignored, see the
       *        class warning
       */
      explicit Exception(std::string const &message) {};
   };
}
