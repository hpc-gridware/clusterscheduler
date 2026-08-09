#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
 * @brief Where a binding stops, and what to do when it does not fit
 */

#include <string>

namespace ocs {
   /// Where on the host topology a binding stops
   class BindingStop {
      BindingStop() = default; // prevent instantiation
   public:
      /// Which topology object the binding stops at
      enum Stop{
         UNINITIALIZED = 0,      ///< not set; the request has not been parsed yet
         NONE,                   ///< no anchor given
         FIRST_FREE_SOCKET,      ///< the first socket with no job bound to it
         FIRST_USED_SOCKET,      ///< the first socket that already carries a job
         FIRST_FREE_CORE,        ///< the first unbound core
         FIRST_USED_CORE,        ///< the first core that already carries a job
         FIRST_FREE_NUMA,        ///< the first unbound NUMA node
         FIRST_USED_NUMA,        ///< the first NUMA node that already carries a job
         FIRST_FREE_CACHE3,      ///< the first unbound level 3 cache domain
         FIRST_USED_CACHE3,      ///< the first level 3 cache domain already in use
         FIRST_FREE_CACHE2,      ///< the first unbound level 2 cache domain
         FIRST_USED_CACHE2,      ///< the first level 2 cache domain already in use
      };

      static std::string to_string(Stop mode);
      static Stop from_string(const std::string& mode);
      };
}
