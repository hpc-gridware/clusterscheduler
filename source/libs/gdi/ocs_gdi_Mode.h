#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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
 * @brief Whether a GDI request adds, modifies or deletes
 */

#include <cstdint>
#include <string>

namespace ocs::gdi {
   /// Whether a request is collected for later sending or sent immediately
   enum class Mode : uint32_t {
      RECORD, ///< add the task to a multi request, to be sent later
      SEND,   ///< send the request now and wait for the answer
   };

   /**
    * @brief The name of a mode, for logging and error messages
    * @param mode the mode to name
    * @return its name
    */
   std::string to_string(Mode mode);
}
