/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#include <string>

#include "ocs_BindingType.h"

std::string ocs::BindingType::to_string(const Type mode) {
   switch (mode) {
      case NONE: return "NONE";
      case HOST: return "host";
      case SLOT: return "slot";
      default: return "???";
   }
}

ocs::BindingType::Type
ocs::BindingType::from_string(const std::string& mode) {
   if (mode == "NONE") {
      return NONE;
   } else if (mode == "host") {
      return HOST;
   } else if (mode == "slot") {
      return SLOT;
   } else {
      return UNINITIALIZED;
   }
}
