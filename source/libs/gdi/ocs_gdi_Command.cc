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

#include "ocs_gdi_Command.h"

// const char *s = toString().c_str();
std::string ocs::gdi::Command::toString(Cmd command) {
   switch (command) {
      case SGE_GDI_GET: return "GET";
      case SGE_GDI_ADD: return "ADD";
      case SGE_GDI_DEL: return "DEL";
      case SGE_GDI_MOD: return "MOD";
      case SGE_GDI_TRIGGER: return "TRIGGER";
      case SGE_GDI_PERMCHECK: return "PERMCHECK";
      case SGE_GDI_SPECIAL: return "SPECIAL";
      case SGE_GDI_COPY: return "COPY";
      case SGE_GDI_REPLACE: return "REPLACE";
      default: return "???";
   }
}
