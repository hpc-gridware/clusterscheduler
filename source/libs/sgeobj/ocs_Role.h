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

#include <string>
#include <vector>

#include "cull/cull.h"

#include "sgeobj/cull/sge_role_RL_L.h"

namespace ocs {
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
         std::vector<std::string> source_hostgroups;          ///< @groups the source host belongs to
         std::vector<std::string> required_value_constraints; ///< elevated permissions required by the request
      };

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
       * Parse and structurally validate a perm_list string.
       * Splits the comma-separated rule set into individual PermRule objects.
       * Each rule must have exactly six colon-separated non-empty fields.
       * The keyword "NONE" is accepted as an empty rule set.
       * Semantic validation of individual field values (operation types, object
       * types, etc.) is deferred to the rule engine.
       * @param perm_list_str  The raw perm_list string from the role object.
       * @param rules          Receives the parsed rules on success.
       * @param answer_list    Receives error messages on failure.
       * @return               True on success, false if the syntax is invalid.
       */
      static bool parse_perm_list(const char *perm_list_str, PermRuleList &rules, lList **answer_list);

      /**
       * Create a Role template element with default attribute values.
       * Used by qconf -arole when opening an editor for a new role.
       * The caller is responsible for freeing the returned element.
       * @return  A new RL_Type element with default values, or nullptr on error.
       */
      static lListElem *create_template();

      /**
       * Evaluate a single permission rule against a request context.
       * Returns true only when all six characteristics match simultaneously.
       * @param rule  A parsed PermRule (from parse_perm_list).
       * @param ctx   The request context to match against.
       * @return      True if the rule matches the context.
       */
      static bool match_rule(const PermRule &rule, const MatchContext &ctx);

      /**
       * Check whether adding candidate_parent to role_name's parent_role_list would
       * create a cycle in the role hierarchy.
       * @param role_name        The role being modified.
       * @param candidate_parent The proposed new parent.
       * @param role_list        The master role list.
       * @return                 True if a cycle would result.
       */
      static bool would_create_cycle(const char *role_name, const char *candidate_parent, const lList *role_list);

      /**
       * Transitively collect all PermRules from role_name and its ancestors.
       * Rules are appended in DFS order: role's own rules first, then parent roles
       * left-to-right, recursively. A visited set prevents duplicate entries when
       * multiple inheritance paths converge on the same ancestor.
       * @param role_name   Starting role.
       * @param role_list   The master role list.
       * @param rules       Receives the collected rules (appended, not replaced).
       */
      static void collect_perm_rules(const char *role_name, const lList *role_list, PermRuleList &rules);

      /**
       * Evaluate whether a request is authorized under the current role configuration.
       * Iterates all enabled roles; for each role the requesting user belongs to,
       * collects the effective rule set (own + inherited) and evaluates each rule.
       * Returns true on the first matching rule. Returns false if no rule matches
       * across all applicable roles (default-deny).
       * @param role_list     The master role list.
       * @param userset_list  The master userset list (for membership checks).
       * @param ctx           The request context.
       * @return              True if the request is authorized.
       */
      static bool is_authorized(const lList *role_list, const lList *userset_list, const MatchContext &ctx);
   };
}
