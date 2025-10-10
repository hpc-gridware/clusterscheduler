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

namespace ocs {
   class BindingStop {
      BindingStop() = default; // prevent instantiation
   public:
      enum Stop{
         UNINITIALIZED = 0,
         NONE,
         FIRST_FREE_SOCKET,
         FIRST_USED_SOCKET,
         FIRST_FREE_CORE,
         FIRST_USED_CORE,
         FIRST_FREE_NUMA,
         FIRST_USED_NUMA,
         FIRST_FREE_CACHE3,
         FIRST_USED_CACHE3,
         FIRST_FREE_CACHE2,
         FIRST_USED_CACHE2,
      };

      static std::string to_string(Stop mode);
      static Stop from_string(const std::string& mode);
      };
}