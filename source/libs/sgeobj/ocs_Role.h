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
 * @brief RBAC roles and the permissions they grant
 */

#include <string>
#include <vector>

#include "cull/cull.h"

#include "sgeobj/cull/sge_role_RL_L.h"

namespace ocs {
   /**
    * @brief An RBAC role: a named set of permission rules
    *
    * A rule has six colon separated characteristics (see #PermRule). An
    * authorization check builds a #MatchContext from the request and asks
    * whether any of the user's roles has a rule matching it.
    */
   class Role {
   public:
      /** One parsed permission rule (six colon-separated characteristics). */
      struct PermRule {
         std::string source;           ///< source_of_request
         std::string origin;           ///< origin_of_request
         std::string operation;        ///< operation_type
         std::string object_type;      ///< object_type
         std::string object_key;       ///< object_key
         std::string value_constraint; ///< object_value_constraint
      };
      /// All rules of one role, in the order they were configured
      using PermRuleList = std::vector<PermRule>;

      /** Runtime context passed to match_rule() for a single authorization check. */
      struct MatchContext {
         std::string source;                                  ///< FQDN of the submitting host
         std::string origin;                                  ///< command name (e.g. "qsub")
         std::string operation;                               ///< operation type (e.g. "ADD")
         std::string object_type;                             ///< RBAC object type (e.g. "JOB")
         std::string object_key;                              ///< specific object name or ID
         std::string object_owner;                            ///< owner of the target object
         std::string request_user;                            ///< authenticated requesting user
         std::string request_group;                           ///< primary UNIX group of the requesting user
         const lList *request_grp_list{nullptr};              ///< supplementary UNIX groups (ST_Type list)
         std::vector<std::string> source_hostgroups;          ///< host groups the source host belongs to
         std::vector<std::string> required_value_constraints; ///< elevated permissions required by the request
      };

      static lListElem *locate(const lList *role_list, const char *name);

      static bool validate(const lListElem *role, lList **answer_list, bool startup);

      static void check_integrity(const lList *role_list, const lList *userset_list, lList **answer_list);

      static bool parse_perm_list(const char *perm_list_str, PermRuleList &rules, lList **answer_list);

      static lListElem *create_template();

      static bool match_rule(const PermRule &rule, const MatchContext &ctx);

      static bool would_create_cycle(const char *role_name, const char *candidate_parent, const lList *role_list);

      static void collect_perm_rules(const char *role_name, const lList *role_list, PermRuleList &rules);

      static bool is_authorized(const lList *role_list, const lList *userset_list, const MatchContext &ctx);
   };
}
