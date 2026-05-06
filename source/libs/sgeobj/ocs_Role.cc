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

// Split s on '|' into non-empty tokens
static std::vector<std::string> split_pipe(const std::string &s) {
   DENTER(TOP_LAYER);
   std::vector<std::string> tokens;
   size_t pos = 0;
   while (true) {
      const size_t bar = s.find('|', pos);
      const std::string tok = s.substr(pos, bar == std::string::npos ? std::string::npos : bar - pos);
      if (!tok.empty()) tokens.push_back(tok);
      if (bar == std::string::npos) break;
      pos = bar + 1;
   }
   DRETURN(tokens);
}

// Predefined origin variable expansions (spec table in §Origin of Request)
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

// Predefined object type group expansions (spec table in §Object Types)
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

// characteristic 1: source of request
// @-prefixed tokens are matched (via fnmatch) against ctx.source_hostgroups;
// plain tokens are matched via fnmatch against ctx.source (hostname).
static bool match_source(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") DRETURN(true);
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") DRETURN(true);
      if (!tok.empty() && tok[0] == '@') {
         for (const auto &hg : ctx.source_hostgroups) {
            if (fnmatch(tok.c_str(), hg.c_str(), 0) == 0) DRETURN(true);
         }
      } else {
         if (fnmatch(tok.c_str(), ctx.source.c_str(), 0) == 0) DRETURN(true);
      }
   }
   DRETURN(false);
}

// characteristic 2: origin of request
// $-prefixed tokens are expanded via origin_vars(); others are fnmatch patterns.
static bool match_origin(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") DRETURN(true);
   const auto &vars = origin_vars();
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") DRETURN(true);
      if (!tok.empty() && tok[0] == '$') {
         const auto it = vars.find(tok);
         if (it != vars.end()) {
            for (const auto &client : it->second) {
               if (ctx.origin == client) DRETURN(true);
            }
         }
      } else {
         if (fnmatch(tok.c_str(), ctx.origin.c_str(), 0) == 0) DRETURN(true);
      }
   }
   DRETURN(false);
}

// characteristic 3: operation type
// supports fnmatch patterns (e.g. MO* matches MOD and MOD_STATE) and | alternatives.
static bool match_operation(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") DRETURN(true);
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") DRETURN(true);
      if (fnmatch(tok.c_str(), ctx.operation.c_str(), 0) == 0) DRETURN(true);
   }
   DRETURN(false);
}

// characteristic 4: object type
// $-prefixed tokens are expanded via object_type_groups(); others are fnmatch patterns.
static bool match_object_type(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") DRETURN(true);
   const auto &groups = object_type_groups();
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") DRETURN(true);
      if (!tok.empty() && tok[0] == '$') {
         const auto it = groups.find(tok);
         if (it != groups.end()) {
            for (const auto &type : it->second) {
               if (ctx.object_type == type) DRETURN(true);
            }
         }
      } else {
         if (fnmatch(tok.c_str(), ctx.object_type.c_str(), 0) == 0) DRETURN(true);
      }
   }
   DRETURN(false);
}

// characteristic 5: object key
// "owner=$request_user" is the only phase-1 dynamic group; others are fnmatch patterns.
static bool match_object_key(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") DRETURN(true);
   for (const auto &tok : split_pipe(field)) {
      if (tok == "*") DRETURN(true);
      if (tok == "owner=$request_user") {
         if (ctx.object_owner == ctx.request_user) DRETURN(true);
      } else {
         if (fnmatch(tok.c_str(), ctx.object_key.c_str(), 0) == 0) DRETURN(true);
      }
   }
   DRETURN(false);
}

// characteristic 6: object value constraint
// '*' in the rule grants any elevated capability.
// Otherwise the rule's |-separated grant set must cover all of ctx.required_value_constraints.
// A request with no required constraints always matches.
static bool match_value_constraint(const std::string &field, const ocs::Role::MatchContext &ctx) {
   DENTER(TOP_LAYER);
   if (field == "*") DRETURN(true);
   if (ctx.required_value_constraints.empty()) DRETURN(true);
   const auto granted = split_pipe(field);
   for (const auto &req : ctx.required_value_constraints) {
      bool found = false;
      for (const auto &g : granted) {
         if (req == g) { found = true; break; }
      }
      if (!found) DRETURN(false);
   }
   DRETURN(true);
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
