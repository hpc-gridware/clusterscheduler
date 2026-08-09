/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2025 HPC-Gridware GmbH
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
 * @brief How a binding walks the topology: linear, striding or explicit
 */

#include <string>

#include "ocs_BindingStrategy.h"

/**
 * @brief The keyword for a strategy, as written in a request
 * @param mode the strategy to name
 * @return its keyword, or `"???"` for an unknown value
 */
std::string ocs::BindingStrategy::to_string(const Strategy mode) {
   switch (mode) {
      case NONE: return "NONE";
      case PACKED: return "packed";
      default: return "???";
   }
}

/**
 * @brief Parse a strategy keyword
 * @param mode the keyword to parse
 * @return the strategy, or #UNINITIALIZED when it is not recognised
 */
ocs::BindingStrategy::Strategy
ocs::BindingStrategy::from_string(const std::string& mode) {
   if (mode == "NONE") {
      return NONE;
   } else if (mode == "packed") {
      return PACKED;
   } else {
      return UNINITIALIZED;
   }
}
