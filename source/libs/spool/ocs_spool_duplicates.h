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
 * @brief Objects found twice while a list is read from the spool (N-H-18)
 *
 * A spool may hold two objects with the same key in a unique attribute, for example two files of user sets
 * with the same name. Only while the list is read is the spool key of each copy known, so the reading
 * backends filter the second copy out there and record it; the qmaster reports every recorded duplicate at
 * start and stops, so that one restart after the repair is enough.
 */

#include <string>
#include <unordered_map>
#include <vector>

#include "tom/ocs_Types.h"
#include "tom/ocs_Raw.h"

namespace ocs::spool {
   /** @brief An object that a spool holds twice, by the key of a unique attribute */
   struct DuplicateObject {
      std::string type;             ///< the object type, e.g. USERSET
      std::string field;            ///< the unique attribute, e.g. US_name
      std::string key;              ///< the key, as the second copy holds it
      std::string first_spool_key;  ///< the spool key of the copy that was kept
      std::string second_spool_key; ///< the spool key of the copy that was filtered out
   };

   /**
    * @brief Filters out the objects of one list read from the spool whose key is already taken
    *
    * One instance per list being read. It remembers the keys of every unique attribute of the objects it
    * admitted, with their spool keys, and refuses an object whose key is taken, recording it as a duplicate.
    * Host keys compare as the hash table of the list compares them.
    */
   class DuplicateFilter {
   public:
      DuplicateFilter(const ocs::tom::FieldDescr *descr, std::string type);

      bool admit(ocs::tom::ConstRawElement ep, const char *spool_key);

   private:
      std::string type_;                                                       ///< the object type
      std::vector<int> fields_;                                                ///< the unique attributes
      std::vector<std::unordered_map<std::string, std::string>> taken_;        ///< per attribute: key to spool key
   };

   const std::vector<DuplicateObject> &duplicate_objects();

   void clear_duplicate_objects();
} // namespace ocs::spool
