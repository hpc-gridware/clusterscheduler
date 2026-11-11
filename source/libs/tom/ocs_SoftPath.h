#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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
 * @brief Records, in debug builds, the soft error paths that the old accessors take
 *
 * The old setters return -1 for some errors instead of aborting: a null
 * element, a field the element does not hold, a negative position. Before
 * their error behaviour is made uniform, a debug build records every such
 * path that is taken, with the caller's stack, so that a full test-suite run
 * lists the code that relies on one (CS-3062). Release builds compile the
 * report to nothing, and a debug build writes only when the sink directory
 * exists.
 */

#include <cstdint>

namespace ocs::tom {

   /** @brief Which soft error path an old accessor took */
   enum class SoftPathReason : std::uint8_t {
      NullElement,       ///< the element is nullptr
      MissingField,      ///< the reduced element does not hold the field
      ForeignField,      ///< the field does not belong to the element's type
      NegativePosition,  ///< a position setter got a negative position
      NullArgument,      ///< a pointer argument other than the element is nullptr
      BoundElement       ///< an object field was to take an element that is bound elsewhere
   };

   /** @brief Field number passed when the soft path concerns no particular field */
   inline constexpr int SoftPathNoName = -1;

#ifdef ENABLE_DEBUG_CHECKS
   /**
    * @brief Record that an old accessor took a soft error path
    *
    * Writes one line to `<sink>/<hostname>.log`: time, component and thread
    * (or the program name where no component is set up), pid, the accessor,
    * the field, the reason, the hit count and the caller's stack. The
    * sink is `$OCS_SOFT_PATH_DIR` if set, else `soft_paths` in the
    * installation root; nothing is written if that directory does not exist.
    * Per process and per accessor, field and reason, at most 8 lines with
    * distinct stacks are written, and a stack is taken only for the 1st, 2nd,
    * 4th, 8th, ... hit, so a soft path in a hot loop costs little.
    *
    * @param function the name of the old accessor
    * @param name the field number, or #SoftPathNoName
    * @param reason which soft path was taken
    */
   [[gnu::cold]] void report_soft_path(const char *function, int name, SoftPathReason reason) noexcept;
#else
   /**
    * @brief Release builds record nothing
    * @param function unused
    * @param name unused
    * @param reason unused
    */
   inline void report_soft_path([[maybe_unused]] const char *function, [[maybe_unused]] int name,
                                [[maybe_unused]] SoftPathReason reason) noexcept {
   }
#endif

} // namespace ocs::tom
