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

#include <fnmatch.h>
#include <unordered_map>
#include <unordered_set>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/cull/sge_str_ST_L.h"
#include "sgeobj/ocs_Role.h"

#include "msg_common.h"

lListElem *
ocs::Role::locate(const lList *role_list, const char *name) {
   DENTER(TOP_LAYER);
   DRETURN(lGetElemStrRW(role_list, RL_name, name));
}

bool
ocs::Role::validate(const lListElem *role, lList **answer_list, bool startup) {
   DENTER(TOP_LAYER);

   if (const char *name = lGetString(role, RL_name);
       verify_str_key(answer_list, name, MAX_VERIFY_STRING, MSG_OBJ_ROLE, KEY_TABLE) != STATUS_OK) {
      DRETURN(false);
   }

   DRETURN(true);
}

void
ocs::Role::check_integrity(const lList *role_list, const lList *userset_list, lList **answer_list) {
   DENTER(TOP_LAYER);

   for_each_ep_lv(role, role_list) {
      const char *role_name = lGetString(role, RL_name);

      // check each user_list entry against the master userset list
      for_each_ep_lv(user_ep, lGetList(role, RL_user_list)) {
         const char *userset_name = lGetString(user_ep, US_name);
         if (!lGetElemStr(userset_list, US_name, userset_name)) {
            WARNING(MSG_ROLE_INTEGRITY_USERSET_SS, role_name, userset_name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_WARNING);
         }
      }

      // check each parent_role_list entry against the master role list
      for_each_ep_lv(parent_ep, lGetList(role, RL_parent_role_list)) {
         const char *parent_name = lGetString(parent_ep, ST_name);
         if (!lGetElemStr(role_list, RL_name, parent_name)) {
            WARNING(MSG_ROLE_INTEGRITY_PARENT_SS, role_name, parent_name);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_WARNING);
         }
      }
   }

   DRETURN_VOID;
}

bool
ocs::Role::parse_perm_list(const char *perm_list_str, PermRuleList &rules, lList **answer_list) {
   DENTER(TOP_LAYER);

   rules.clear();

   // "NONE" means an empty rule set — always valid
   if (perm_list_str == nullptr || strcmp(perm_list_str, "NONE") == 0) {
      DRETURN(true);
   }

   auto trim = [](const std::string &s) -> std::string {
      auto start = s.find_first_not_of(" \t\r\n");
      if (start == std::string::npos) return "";
      auto end = s.find_last_not_of(" \t\r\n");
      return s.substr(start, end - start + 1);
   };

   // split perm_list by commas into individual rule strings
   const std::string perm_list(perm_list_str);
   size_t pos = 0;
   while (true) {
      const size_t comma = perm_list.find(',', pos);
      const std::string rule_str = trim(perm_list.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos));

      if (!rule_str.empty()) {
         // count colons to report an accurate field count on error
         int n_colons = 0;
         for (const char c : rule_str) if (c == ':') ++n_colons;

         if (n_colons < 5) {
            ERROR(MSG_ROLE_PERMLIST_NFIELDS_SI, rule_str.c_str(), n_colons + 1);
            answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(false);
         }

         // split on the first 5 colons; field 6 receives everything after the 5th colon
         PermRule rule;
         std::string *field_ptrs[6] = {
            &rule.source, &rule.origin, &rule.operation,
            &rule.object_type, &rule.object_key, &rule.value_constraint
         };
         size_t rpos = 0;
         for (int i = 0; i < 5; ++i) {
            const size_t colon = rule_str.find(':', rpos);
            *field_ptrs[i] = trim(rule_str.substr(rpos, colon - rpos));
            rpos = colon + 1;
         }
         rule.value_constraint = trim(rule_str.substr(rpos));

         // validate that no field is empty
         for (int i = 0; i < 6; ++i) {
            if (field_ptrs[i]->empty()) {
               ERROR(MSG_ROLE_PERMLIST_EMPTYFIELD_SI, rule_str.c_str(), i + 1);
               answer_list_add(answer_list, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
               DRETURN(false);
            }
         }

         rules.push_back(std::move(rule));
      }

      if (comma == std::string::npos) break;
      pos = comma + 1;
   }

   DRETURN(true);
}

// ---- helpers for match_rule() -----------------------------------------------

namespace {

/** Split @a s on '|' into non-empty tokens. */
static std::vector<std::string> split_pipe(const std::string &s) {
   DENTER(TOP_LAYER);
   std::vector<std::string> tokens;
   size_t pos = 0;
   while (true) {
      const size_t bar = s.find('|', pos);
      const std::string tok = s.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
      if (!tok.empty()) {
         tokens.push_back(tok);
      }
      if (bar == std::string::npos) {
         break;
      }
      pos = bar + 1;
   }
   DRETURN(tokens);
}

/** Origin-variable expansion table (spec §Origin of Request). */
static const std::unordered_map<std::string, std::vector<std::string>> &origin_vars() {
   DENTER(TOP_LAYER);
   static const std::unordered_map<std::string, std::vector<std::string>> m = {
      {"$job_cmd",        {"qsub","qsh","qrsh","qmake","qselect","qalter","qrerun","qdel","qmod","qhold","qrls","drmaa"}},
      {"$ar_cmd",         {"qrsub","qrstat","qrdel","drmaa"}},
      {"$admin_cmd",      {"qconf","qmod"}},
      {"$monitor_cmd",    {"qstat","qhost","qquota","drmaa"}},
      {"$diag_cmd",       {"qping"}},
      {"$reporting_cmd",  {"qacct"}},
      {"$system_service", {"sge_qmaster","sge_execd","sge_shadowd","sge_shepherd"}},
   };
   DRETURN(m);
}

/** Object-type group expansion table (spec §Object Types). */
static const std::unordered_map<std::string, std::vector<std::string>> &object_type_groups() {
   DENTER(TOP_LAYER);
   static const std::unordered_map<std::string, std::vector<std::string>> m = {
      {"$submit_objects", {"JOB","AR"}},
      {"$comp_objects",   {"THREAD","COMP"}},
      {"$conf_objects",   {"CAL","CKPT","CONF","CPLX","CQUEUE","HGROUP","HOST",
                           "ECLIENT","PE","PRJ","ROLE","QINSTANCE","RQS","SCONF",
                           "STREE","STREENODE","USER","USET"}},
      {"$all_objects",    {"AR","CAL","CKPT","CONF","CPLX","CQUEUE","HGROUP","HOST",
                           "JOB","ECLIENT","PE","PRJ","ROLE","QINSTANCE","RQS","SCONF",
                           "STREE","STREENODE","USER","USET","COMP","THREAD"}},
   };
   DRETURN(m);
}

/**
 * Match characteristic 1 (source of request).
 * @p @-prefixed tokens are fnmatch-matched against ctx.source_hostgroups;
 * plain tokens are fnmatch-matched against ctx.source (hostname).
 * @param field  Rule source field; may contain '|'-separated alternatives.
 * @param ctx    Request context.
 * @return       True if any alternative matches.
 */
static bool match_source(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") {
      DRETURN(true);
   }
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") {
         DRETURN(true);
      }
      if (!tok.empty() && tok[0] == '@') {
         for (const auto &hg : ctx.source_hostgroups) {
            if (fnmatch(tok.c_str(), hg.c_str(), 0) == 0) {
               DRETURN(true);
            }
         }
      } else {
         if (fnmatch(tok.c_str(), ctx.source.c_str(), 0) == 0) {
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/**
 * Match characteristic 2 (origin of request).
 * @p $-prefixed tokens are expanded via origin_vars() before matching.
 * @param field  Rule origin field.
 * @param ctx    Request context.
 * @return       True if any alternative matches.
 */
static bool match_origin(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") {
      DRETURN(true);
   }
   const auto &vars = origin_vars();
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") {
         DRETURN(true);
      }
      if (!tok.empty() && tok[0] == '$') {
         const auto it = vars.find(tok);
         if (it != vars.end()) {
            for (const auto &client : it->second) {
               if (ctx.origin == client) {
                  DRETURN(true);
               }
            }
         }
      } else {
         if (fnmatch(tok.c_str(), ctx.origin.c_str(), 0) == 0) {
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/**
 * Match characteristic 3 (operation type).
 * Supports fnmatch patterns (e.g. MO* matches MOD and MOD_STATE).
 * @param field  Rule operation field.
 * @param ctx    Request context.
 * @return       True if any alternative matches.
 */
static bool match_operation(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") {
      DRETURN(true);
   }
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") {
         DRETURN(true);
      }
      if (fnmatch(tok.c_str(), ctx.operation.c_str(), 0) == 0) {
         DRETURN(true);
      }
   }
   DRETURN(false);
}

/**
 * Match characteristic 4 (object type).
 * @p $-prefixed tokens are expanded via object_type_groups() before matching.
 * @param field  Rule object_type field.
 * @param ctx    Request context.
 * @return       True if any alternative matches.
 */
static bool match_object_type(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") {
      DRETURN(true);
   }
   const auto &groups = object_type_groups();
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") {
         DRETURN(true);
      }
      if (!tok.empty() && tok[0] == '$') {
         const auto it = groups.find(tok);
         if (it != groups.end()) {
            for (const auto &type : it->second) {
               if (ctx.object_type == type) {
                  DRETURN(true);
               }
            }
         }
      } else {
         if (fnmatch(tok.c_str(), ctx.object_type.c_str(), 0) == 0) {
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/**
 * Match characteristic 5 (object key).
 * The token "owner=$request_user" resolves to ctx.object_owner == ctx.request_user;
 * all other tokens are treated as fnmatch patterns against ctx.object_key.
 * @param field  Rule object_key field.
 * @param ctx    Request context.
 * @return       True if any alternative matches.
 */
static bool match_object_key(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") {
      DRETURN(true);
   }
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") {
         DRETURN(true);
      }
      if (tok == "owner=$request_user") {
         if (ctx.object_owner == ctx.request_user) {
            DRETURN(true);
         }
      } else {
         if (fnmatch(tok.c_str(), ctx.object_key.c_str(), 0) == 0) {
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/**
 * Match characteristic 6 (object value constraint).
 * '*' always matches. A request with no required constraints also always matches.
 * Otherwise all entries in ctx.required_value_constraints must appear in the
 * rule's '|'-separated grant set.
 * @param field  Rule value_constraint field.
 * @param ctx    Request context.
 * @return       True if the grant set covers all required constraints.
 */
static bool match_value_constraint(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") {
      DRETURN(true);
   }
   if (ctx.required_value_constraints.empty()) {
      DRETURN(true);
   }
   const auto granted = split_pipe(field);
   for (const auto &req : ctx.required_value_constraints) {
      bool found = false;
      for (const auto &g : granted) {
         if (req == g) {
            found = true;
            break;
         }
      }
      if (!found) {
         DRETURN(false);
      }
   }
   DRETURN(true);
}

/**
 * DFS reachability check: returns true if @a target is reachable from @a from
 * via parent_role_list links.
 * @param from      Starting role name.
 * @param target    Role name to search for.
 * @param role_list Master role list.
 * @param visited   Accumulates visited role names to avoid redundant traversal.
 * @return          True if @a target is reachable from @a from.
 */
static bool reachable(const char *from, const char *target, const lList *role_list,
                      std::unordered_set<std::string> &visited) {
   DENTER(TOP_LAYER);
   if (strcmp(from, target) == 0) {
      DRETURN(true);
   }
   if (!visited.insert(from).second) {
      DRETURN(false);
   }
   const lListElem *role = lGetElemStr(role_list, RL_name, from);
   if (role == nullptr) {
      DRETURN(false);
   }
   for_each_ep_lv(parent_ep, lGetList(role, RL_parent_role_list)) {
      const char *parent_name = lGetString(parent_ep, ST_name);
      if (reachable(parent_name, target, role_list, visited)) {
         DRETURN(true);
      }
   }
   DRETURN(false);
}

/**
 * DFS helper for collect_perm_rules(): appends rules from @a role_name and its
 * ancestors into @a rules in child-first DFS order.
 * @param role_name  Current role being processed.
 * @param role_list  Master role list.
 * @param rules      Accumulates parsed PermRule objects.
 * @param visited    Prevents re-processing roles reached via multiple paths.
 */
static void collect_recursive(const char *role_name, const lList *role_list, ocs::Role::PermRuleList &rules,
                               std::unordered_set<std::string> &visited) {
   DENTER(TOP_LAYER);
   if (role_name == nullptr || !visited.insert(role_name).second) {
      DRETURN_VOID;
   }
   const lListElem *role = lGetElemStr(role_list, RL_name, role_name);
   if (role == nullptr) {
      DRETURN_VOID;
   }

   // append this role's own rules first
   const char *perm_str = lGetString(role, RL_perm_list);
   if (perm_str != nullptr && strcmp(perm_str, "NONE") != 0) {
      lList *dummy_al = nullptr;
      ocs::Role::PermRuleList own_rules;
      ocs::Role::parse_perm_list(perm_str, own_rules, &dummy_al);
      lFreeList(&dummy_al);
      rules.insert(rules.end(), own_rules.begin(), own_rules.end());
   }

   // then recurse into parents in order
   for_each_ep_lv(parent_ep, lGetList(role, RL_parent_role_list)) {
      const char *parent_name = lGetString(parent_ep, ST_name);
      collect_recursive(parent_name, role_list, rules, visited);
   }
   DRETURN_VOID;
}

} // anonymous namespace

// -----------------------------------------------------------------------------

lListElem *
ocs::Role::create_template() {
   DENTER(TOP_LAYER);

   lListElem *role = lCreateElem(RL_Type);
   if (role == nullptr) {
      DRETURN(nullptr);
   }

   lSetString(role, RL_name, "template");
   lSetBool(role, RL_enabled, true);
   lSetString(role, RL_perm_list, "NONE");

   DRETURN(role);
}

bool
ocs::Role::match_rule(const PermRule &rule, const MatchContext &ctx) {
   DENTER(TOP_LAYER);
   // all six characteristics must match simultaneously
   DRETURN(match_source(rule.source, ctx)
        && match_origin(rule.origin, ctx)
        && match_operation(rule.operation, ctx)
        && match_object_type(rule.object_type, ctx)
        && match_object_key(rule.object_key, ctx)
        && match_value_constraint(rule.value_constraint, ctx));
}

bool
ocs::Role::would_create_cycle(const char *role_name, const char *candidate_parent, const lList *role_list) {
   DENTER(TOP_LAYER);
   // a cycle forms if role_name is already reachable from candidate_parent
   std::unordered_set<std::string> visited;
   DRETURN(reachable(candidate_parent, role_name, role_list, visited));
}

void
ocs::Role::collect_perm_rules(const char *role_name, const lList *role_list, PermRuleList &rules) {
   DENTER(TOP_LAYER);
   std::unordered_set<std::string> visited;
   collect_recursive(role_name, role_list, rules, visited);
   DRETURN_VOID;
}

bool
ocs::Role::is_authorized(const lList *role_list, const lList *userset_list, const MatchContext &ctx) {
   DENTER(TOP_LAYER);
   for_each_ep_lv(role_ep, role_list) {
      // skip disabled roles
      if (!lGetBool(role_ep, RL_enabled)) {
         continue;
      }
      // check whether the requesting user is a member of this role's user_list
      bool user_in_role = false;
      for_each_ep_lv(acl_ep, lGetList(role_ep, RL_user_list)) {
         const char *userset_name = lGetString(acl_ep, US_name);
         const lListElem *userset_ep = lGetElemStr(userset_list, US_name, userset_name);
         if (userset_ep == nullptr) {
            continue;
         }
         if (sge_contained_in_access_list(ctx.request_user.c_str(),
                                          ctx.request_group.c_str(),
                                          ctx.request_grp_list,
                                          userset_ep) == 1) {
            user_in_role = true;
            break;
         }
      }
      if (!user_in_role) {
         continue;
      }
      // user is in this role — collect effective rules and evaluate
      PermRuleList rules;
      collect_perm_rules(lGetString(role_ep, RL_name), role_list, rules);
      for (const auto &rule : rules) {
         if (match_rule(rule, ctx)) {
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}
