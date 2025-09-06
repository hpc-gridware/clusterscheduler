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

#include "ocs_BindingInstance.h"

std::string ocs::BindingInstance::to_string(const Instance mode) {
   switch (mode) {
      case NONE: return "NONE";
      case SET: return "set";
      case ENV: return "env";
      case PE: return "pe";
      default: return "???";
   }
}

ocs::BindingInstance::Instance
ocs::BindingInstance::from_string(const std::string& mode) {
   if (mode == "NONE") {
      return NONE;
   } else if (mode == "set" || mode == "SET") {
      return SET;
   } else if (mode == "env" || mode == "ENV") {
      return ENV;
   } else if (mode == "pe" || mode == "PE") {
      return PE;
   } else {
      return UNINITIALIZED;
   }
}
