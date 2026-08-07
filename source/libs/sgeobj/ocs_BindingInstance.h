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
 * @brief One concrete binding of a job to hardware
 */

#include <string>

namespace ocs {
   /**
    * @brief Who acts on a binding once the job starts
    *
    * The request is the same in each case; what differs is who enforces it.
    */
   class BindingInstance {
      BindingInstance() = default; // prevent instantiation
   public:
      /// Who acts on the binding
      enum Instance {
         UNINITIALIZED = 0, ///< not set; the request has not been parsed yet
         NONE,              ///< nobody; no binding is applied
         SET,               ///< the shepherd applies the cpuset itself; the default
         ENV,               ///< only `$SGE_BINDING` is exported, and the job binds itself
         PE,                ///< the binding is written into a rankfile for the parallel environment
      };

      /**
       * @brief The keyword for an instance, as written in a request
       * @param mode the instance to name
       * @return its keyword, or `"???"` for an unknown value
       */
      static std::string to_string(Instance mode);
      /**
       * @brief Parse an instance keyword
       * @param mode the keyword to parse
       * @return the instance, or #UNINITIALIZED when it is not recognised
       */
      static Instance from_string(const std::string& mode);
   };
}
