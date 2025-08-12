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

#include "ocs_BindingStart.h"

std::string ocs::BindingStart::to_string(const Start mode) {
   switch (mode) {
      case NONE: return "NONE";
      case FIRST_FREE_SOCKET: return "S";
      case FIRST_USED_SOCKET: return "s";
      case FIRST_FREE_CORE: return "C";
      case FIRST_USED_CORE: return "c";
      default: return "???";
   }
}
