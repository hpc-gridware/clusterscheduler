/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025-2026 HPC-Gridware GmbH
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
 * @brief Holding one cluster object list together with its metadata
 */

#include <string>
#include <unordered_set>

#include "cull/cull.h"

#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_RQRL.h"

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"

#include "ocs_ObjectContainer.h"

#include "msg_sgeobjlib.h"

/**
 * @brief Check for duplicate string entries in a list
 *
 * @tparam F the string field that holds the names to check for duplicates, e.g. CE_name
 * @param obj_list list to check for duplicates
 * @param answer_list If not nullptr, error messages are added to this list
 * @param object_name Optional name of the object owning the list
 * @return true if duplicates were found, false otherwise
 */
template <auto F>
bool
ocs::ObjectContainer::has_duplicates(const ocs::tom::ConstList<typename ocs::tom::FieldTraits<F>::Type> obj_list,
                                     ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const std::string& object_name) {
   DENTER(TOP_LAYER);

   // early exit if there is nothing to do
   if (obj_list.empty()) {
      DRETURN(false);
   }

   // to check for duplicates we try to add each element into a hash table
   // if an element is already in the hash table we have found a duplicate
   std::unordered_set<std::string> centry_names;
   for (const auto obj : obj_list) {
      const char *name = ocs::tom::get<F>(obj);
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
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_OBJCONT_DUP_SS, name,
                                 object_name.c_str());
      }
      DRETURN(true);
   }

   // found no duplicates otherwise we would have returned earlier
   DRETURN(false);
}

template bool ocs::ObjectContainer::has_duplicates<CE_name>(ocs::tom::ConstList<ocs::tom::CE> obj_list,
                                                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                            const std::string &object_name);
template bool ocs::ObjectContainer::has_duplicates<RQRL_name>(ocs::tom::ConstList<ocs::tom::RQRL> obj_list,
                                                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                                              const std::string &object_name);
