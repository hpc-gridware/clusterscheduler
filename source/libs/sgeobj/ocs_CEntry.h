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
   class CEntry {
   public:
      enum class Type : std::uint32_t {
         NONE = 0,

         FIRST  = 1,

         // used for complexes
         INT    = FIRST,
         STR    = 2,
         TIME    = 3,
         MEM    = 4,
         BOOL    = 5,
         CSTR   = 6,
         HOST   = 7,
         DOUBLE = 8,
         RESTR  = 9,
         RSMAP  = 10,

         CE_LAST = RSMAP,

         // @todo Cleanup: These constants are not CEntry related. Requires some cleanup in the config.
         // used in config
         TYPE_ACC  = 11,
         TYPE_LOG  = 12,
         TYPE_LAST = TYPE_LOG
      };
      static bool has_duplicates(const lList *centry_list, lList **answer_list, const std::string& object_name);
   };
}
