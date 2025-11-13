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
#include <unordered_set>

#include "cull/cull_list.h"

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"

#include "ocs_ObjectContainer.h"

#include "msg_sgeobjlib.h"

/** @brief Check for duplicate string entries in a list

 *  @param obj_list cull list to check for duplicates
 *  @param nm name of the attribute that holds the names to check for duplicates (type string)
 *  @param answer_list If not nullptr, error messages are added to this list
 *  @param object_name Optional name of the object owning the centry list
 *  @return true if duplicates were found, false otherwise
 */
bool
ocs::ObjectContainer::has_duplicates(const lList *obj_list, int nm, lList **answer_list, const std::string& object_name) {
   DENTER(TOP_LAYER);

   // early exit if there is nothing to do
   if (obj_list == nullptr || lGetNumberOfElem(obj_list) == 0) {
      DRETURN(false);
   }

   // to check for duplicates we try to add each element into a hash table
   // if an element is already in the hash table we have found a duplicate
   std::unordered_set<std::string> centry_names;
   const lListElem *obj;
   for_each_ep(obj, obj_list) {
      const char *name = lGetString(obj, nm);
      if (name == nullptr) {
         continue;
      }

      // try to insert the string; if insert succeeds then centry is no duplicate
      if (auto [iter, inserted] = centry_names.insert(std::string(name)); inserted) {
         continue;
      }

      // if we have no answer list, report the duplicate immediately
      if (answer_list == nullptr) {
         DRETURN(true);
      }

      // otherwise add an error message to the answer list and then return
      if (object_name.empty()) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_OBJCONT_DUP_S, name);
      } else {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_OBJCONT_DUP_SS, name, object_name.c_str());
      }
      DRETURN(true);
   }

   // found no duplicates otherwise we would have returned earlier
   DRETURN(false);
}