#pragma once
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

namespace ocs::gdi {
   class Command {
         Command() = default; // prevent instantiation
   public:
      enum Cmd {
         SGE_GDI_NONE = 0,
         SGE_GDI_GET = 1,
         SGE_GDI_ADD,
         SGE_GDI_DEL,
         SGE_GDI_MOD,
         SGE_GDI_TRIGGER,
         SGE_GDI_PERMCHECK,
         SGE_GDI_SPECIAL,
         SGE_GDI_COPY,
         SGE_GDI_REPLACE,
      };

      static std::string toString(Cmd mode);
   };
}