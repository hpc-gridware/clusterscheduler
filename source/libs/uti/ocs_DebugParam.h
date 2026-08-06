#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Debug settings read from the environment
 */

namespace ocs {
   /// Debug settings a developer can switch on through the environment
   class DebugParam {
   public:
      /**
       * @brief Which threads should produce debug output
       * @return the pattern from the environment, or nullptr when unset;
       *         owned by the class, do not free
       */
      static const char *get_thread_name_pattern();

      /**
       * @brief Is this component running in no-daemon mode?
       * @return true when the component was asked to stay in the foreground
       */
      static bool is_component_in_nd_mode();
   };

}
