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

#include "cull/cull.h"

#include "sgeobj/cull/sge_role_RL_L.h"

namespace ocs {
   class Role {
   public:
      /**
       * Locate a Role by name in a role list.
       * @param role_list  The list to search.
       * @param name       The role name to look up.
       * @return           Pointer to the matching element, or nullptr if not found.
       */
      static lListElem *locate(const lList *role_list, const char *name);

      /**
       * Validate a Role element.
       * Checks that the role name is a valid GCS object name.
       * @param role         The role element to validate.
       * @param answer_list  Receives error messages.
       * @param startup      True when called during qmaster startup (relaxes some checks).
       * @return             True if the role is valid.
       */
      static bool validate(const lListElem *role, lList **answer_list, bool startup);

      /**
       * Post-startup integrity scan for dangling role references.
       * Logs a WARNING for each RL_user_list entry whose userset does not exist in
       * userset_list and each RL_parent_role_list entry whose parent role does not
       * exist in role_list. Does not abort startup.
       */
      static void check_integrity(const lList *role_list, const lList *userset_list, lList **answer_list);

      /**
       * Create a Role template element with default attribute values.
       * Used by qconf -arole when opening an editor for a new role.
       * The caller is responsible for freeing the returned element.
       * @return  A new RL_Type element with default values, or nullptr on error.
       */
      static lListElem *create_template();
   };
}
