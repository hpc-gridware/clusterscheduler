#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2026 HPC-Gridware GmbH
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
 * @brief Complex entries: the resource types a cluster knows about
 */

#include <string>
#include <cinttypes>

#include "cull/cull.h"

namespace ocs {
   /**
    * @brief Complex entries: the resource types a cluster knows about
    *
    * A complex entry declares a resource — its name, its type, and how
    * requests for it are compared. The type decides how a value is parsed and
    * matched: `MEM` accepts suffixes such as `4G`, `TIME` accepts
    * `hh:mm:ss`, `RSMAP` counts consumable instances.
    */
   class CEntry {
   public:
      /// How a complex value is parsed, stored and compared
      enum class Type : std::uint32_t {
         NONE = 0,        ///< no type; an unset entry
         FIRST  = 1,      ///< lowest value used for a complex type, for iterating
         // used for complexes
         INT    = FIRST,  ///< an integer value
         STR    = 2,      ///< a string, compared literally
         TIME    = 3,     ///< a time span, written `hh:mm:ss` or with a suffix
         MEM    = 4,      ///< a memory size, written with a suffix such as `K`, `M` or `G`
         BOOL    = 5,     ///< a boolean
         CSTR   = 6,      ///< a string compared without regard to case
         HOST   = 7,      ///< a host name, compared with host aliasing applied
         DOUBLE = 8,      ///< a floating point value
         RESTR  = 9,      ///< a string restricted to a fixed set of values
         RSMAP  = 10,     ///< a resource map: consumable instances that are handed out individually

         CE_LAST = RSMAP, ///< highest value used for a complex type, for iterating

         // @todo Cleanup: These constants are not CEntry related. Requires some cleanup in the config.
         // used in config
         TYPE_ACC  = 11,  ///< accounting value; used in the configuration, not on a complex
         TYPE_LOG  = 12,  ///< log level; used in the configuration, not on a complex
         TYPE_LAST = TYPE_LOG ///< highest value used at all, for iterating
      };
      static bool has_duplicates(const lList *centry_list, lList **answer_list, const std::string& object_name);
   };
}
