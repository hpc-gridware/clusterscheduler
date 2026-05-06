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

#include <cstdio>
#include <initializer_list>
#include <fcntl.h>
#include <unistd.h>

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_Role.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

#define CHECK(label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  %s\n", (label)); \
         ++s_fail; \
      } else { \
         printf("ok    %s\n", (label)); \
      } \
   } while (0)

// Redirect stderr to /dev/null and return a saved copy of the original fd.
static int suppress_stderr() {
   int saved = dup(STDERR_FILENO);
   int null_fd = open("/dev/null", O_WRONLY);
   dup2(null_fd, STDERR_FILENO);
   close(null_fd);
   return saved;
}

// Restore stderr from the saved fd returned by suppress_stderr().
static void restore_stderr(int saved) {
   dup2(saved, STDERR_FILENO);
   close(saved);
}

// ---------------------------------------------------------------------------
// CULL builder helpers
// ---------------------------------------------------------------------------

static lListElem *make_userset(const char *name, std::initializer_list<const char *> members) {
   lListElem *us = lCreateElem(US_Type);
   lSetString(us, US_name, name);
   for (const char *m : members) {
      lAddSubStr(us, UE_name, m, US_entries, UE_Type);
   }
   return us;
}

static lListElem *make_role(const char *name, bool enabled, const char *perm_list,
                             std::initializer_list<const char *> parents,
                             std::initializer_list<const char *> usersets) {
   lListElem *role = lCreateElem(RL_Type);
   lSetString(role, RL_name, name);
   lSetBool(role, RL_enabled, enabled);
   lSetString(role, RL_perm_list, perm_list);
   for (const char *p : parents) {
      lAddSubStr(role, ST_name, p, RL_parent_role_list, ST_Type);
   }
   for (const char *u : usersets) {
      lAddSubStr(role, US_name, u, RL_user_list, US_Type);
   }
   return role;
}

// ---------------------------------------------------------------------------
// parse_perm_list
// ---------------------------------------------------------------------------

static void test_parse_perm_list() {
   printf("\n--- parse_perm_list ---\n");
   lList *al = nullptr;
   ocs::Role::PermRuleList rules;

   // "NONE" → empty rule set
   CHECK("NONE yields empty rules", ocs::Role::parse_perm_list("NONE", rules, &al) && rules.empty());
   lFreeList(&al);

   // nullptr → treated same as NONE
   CHECK("nullptr yields empty rules", ocs::Role::parse_perm_list(nullptr, rules, &al) && rules.empty());
   lFreeList(&al);

   // valid single rule with six colon-separated fields
   rules.clear();
   bool ok = ocs::Role::parse_perm_list("*:*:ADD:JOB:*:*", rules, &al);
   CHECK("valid single rule returns true",      ok);
   CHECK("valid single rule produces 1 rule",   ok && rules.size() == 1);
   CHECK("rule source field is *",              ok && rules[0].source == "*");
   CHECK("rule operation field is ADD",         ok && rules[0].operation == "ADD");
   CHECK("rule object_type field is JOB",       ok && rules[0].object_type == "JOB");
   lFreeList(&al);

   // five fields → error
   rules.clear();
   {
      int saved = suppress_stderr();
      bool res = ocs::Role::parse_perm_list("*:*:ADD:JOB:*", rules, &al);
      restore_stderr(saved);
      CHECK("five fields returns false", !res);
   }
   lFreeList(&al);

   // empty third field → error
   rules.clear();
   {
      int saved = suppress_stderr();
      bool res = ocs::Role::parse_perm_list("*:*::JOB:*:*", rules, &al);
      restore_stderr(saved);
      CHECK("empty field returns false", !res);
   }
   lFreeList(&al);

   // two comma-separated rules
   rules.clear();
   ok = ocs::Role::parse_perm_list("*:*:ADD:JOB:*:*, *:*:DEL:JOB:*:*", rules, &al);
   CHECK("two rules returns true",              ok);
   CHECK("two rules produces 2 rules",          ok && rules.size() == 2);
   CHECK("first rule operation is ADD",         ok && rules[0].operation == "ADD");
   CHECK("second rule operation is DEL",        ok && rules[1].operation == "DEL");
   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// match_rule
// ---------------------------------------------------------------------------

static void test_match_rule() {
   printf("\n--- match_rule ---\n");

   ocs::Role::PermRule rule;
   ocs::Role::MatchContext ctx;
   ctx.source       = "host1.example.com";
   ctx.origin       = "qsub";
   ctx.operation    = "ADD";
   ctx.object_type  = "JOB";
   ctx.object_key   = "42";
   ctx.request_user = "alice";
   ctx.object_owner = "alice";

   // all-wildcard rule always matches
   rule = {"*", "*", "*", "*", "*", "*"};
   CHECK("all-wildcard matches", ocs::Role::match_rule(rule, ctx));

   // single characteristic mismatch blocks the rule
   rule = {"*", "*", "DEL", "*", "*", "*"};
   CHECK("operation DEL vs ADD → false", !ocs::Role::match_rule(rule, ctx));

   // pipe alternation: ADD appears in the set
   rule = {"*", "*", "GET|ADD|DEL", "*", "*", "*"};
   CHECK("|-alternation: ADD in set → true", ocs::Role::match_rule(rule, ctx));

   // pipe alternation: ADD not in the set
   rule = {"*", "*", "GET|DEL", "*", "*", "*"};
   CHECK("|-alternation: ADD not in set → false", !ocs::Role::match_rule(rule, ctx));

   // fnmatch pattern on operation
   rule = {"*", "*", "MO*", "*", "*", "*"};
   ctx.operation = "MOD";
   CHECK("fnmatch MO* matches MOD",        ocs::Role::match_rule(rule, ctx));
   ctx.operation = "MOD_STATE";
   CHECK("fnmatch MO* matches MOD_STATE",  ocs::Role::match_rule(rule, ctx));
   ctx.operation = "GET";
   CHECK("fnmatch MO* does not match GET", !ocs::Role::match_rule(rule, ctx));
   ctx.operation = "ADD";

   // origin variable expansion: $job_cmd covers qsub
   rule = {"*", "$job_cmd", "*", "*", "*", "*"};
   ctx.origin = "qsub";
   CHECK("$job_cmd covers qsub",       ocs::Role::match_rule(rule, ctx));
   ctx.origin = "qconf";
   CHECK("$job_cmd does not cover qconf", !ocs::Role::match_rule(rule, ctx));

   // $admin_cmd covers qconf
   rule = {"*", "$admin_cmd", "*", "*", "*", "*"};
   ctx.origin = "qconf";
   CHECK("$admin_cmd covers qconf", ocs::Role::match_rule(rule, ctx));
   ctx.origin = "qsub";

   // $system_service covers sge_qmaster
   rule = {"*", "$system_service", "*", "*", "*", "*"};
   ctx.origin = "sge_qmaster";
   CHECK("$system_service covers sge_qmaster", ocs::Role::match_rule(rule, ctx));
   ctx.origin = "qsub";

   // object type group expansion: $submit_objects covers JOB and AR
   rule = {"*", "*", "*", "$submit_objects", "*", "*"};
   ctx.object_type = "JOB";
   CHECK("$submit_objects covers JOB",            ocs::Role::match_rule(rule, ctx));
   ctx.object_type = "AR";
   CHECK("$submit_objects covers AR",             ocs::Role::match_rule(rule, ctx));
   ctx.object_type = "CQUEUE";
   CHECK("$submit_objects does not cover CQUEUE", !ocs::Role::match_rule(rule, ctx));
   ctx.object_type = "JOB";

   // source: plain hostname matched via fnmatch
   rule = {"submit*.example.com", "*", "*", "*", "*", "*"};
   ctx.source = "submit1.example.com";
   CHECK("source fnmatch: submit1 matches",       ocs::Role::match_rule(rule, ctx));
   ctx.source = "admin1.example.com";
   CHECK("source fnmatch: admin1 does not match", !ocs::Role::match_rule(rule, ctx));
   ctx.source = "host1.example.com";

   // source: @-prefixed token matched against source_hostgroups
   rule = {"@submit_hosts", "*", "*", "*", "*", "*"};
   ctx.source_hostgroups = {"@submit_hosts", "@other"};
   CHECK("@submit_hosts matched in source_hostgroups",      ocs::Role::match_rule(rule, ctx));
   ctx.source_hostgroups = {"@exec_hosts"};
   CHECK("@submit_hosts not in source_hostgroups → false",  !ocs::Role::match_rule(rule, ctx));
   ctx.source_hostgroups = {};

   // object key: owner=$request_user special token
   rule = {"*", "*", "*", "*", "owner=$request_user", "*"};
   ctx.request_user = "alice";
   ctx.object_owner = "alice";
   CHECK("owner=$request_user: owner matches → true",           ocs::Role::match_rule(rule, ctx));
   ctx.object_owner = "bob";
   CHECK("owner=$request_user: different owner → false",        !ocs::Role::match_rule(rule, ctx));
   ctx.object_owner = "alice";

   // object key: fnmatch pattern
   rule = {"*", "*", "*", "*", "job_*", "*"};
   ctx.object_key = "job_42";
   CHECK("object_key fnmatch: job_42 matches",     ocs::Role::match_rule(rule, ctx));
   ctx.object_key = "ar_5";
   CHECK("object_key fnmatch: ar_5 does not match", !ocs::Role::match_rule(rule, ctx));
   ctx.object_key = "42";

   // value constraint: * always matches even with required constraints
   rule = {"*", "*", "*", "*", "*", "*"};
   ctx.required_value_constraints = {"EXCLUSIVE"};
   CHECK("value_constraint=* always matches", ocs::Role::match_rule(rule, ctx));

   // no required constraints → any grant set matches
   rule = {"*", "*", "*", "*", "*", "EXCLUSIVE"};
   ctx.required_value_constraints = {};
   CHECK("no required constraints → matches any grant set", ocs::Role::match_rule(rule, ctx));

   // required constraint covered by grant set
   rule = {"*", "*", "*", "*", "*", "EXCLUSIVE|PRIORITY"};
   ctx.required_value_constraints = {"EXCLUSIVE"};
   CHECK("required EXCLUSIVE covered → true", ocs::Role::match_rule(rule, ctx));

   // all required constraints covered
   ctx.required_value_constraints = {"EXCLUSIVE", "PRIORITY"};
   CHECK("both required constraints covered → true", ocs::Role::match_rule(rule, ctx));

   // one required constraint missing from grant set
   rule = {"*", "*", "*", "*", "*", "EXCLUSIVE"};
   ctx.required_value_constraints = {"EXCLUSIVE", "PRIORITY"};
   CHECK("PRIORITY not in grant set → false", !ocs::Role::match_rule(rule, ctx));
   ctx.required_value_constraints = {};
}

// ---------------------------------------------------------------------------
// collect_perm_rules
// ---------------------------------------------------------------------------

static void test_collect_perm_rules() {
   printf("\n--- collect_perm_rules ---\n");

   lList *role_list = lCreateList("roles", RL_Type);
   ocs::Role::PermRuleList rules;

   // single role with no parents: only its own rules are collected
   lAppendElem(role_list, make_role("base", true, "*:*:ADD:JOB:*:*", {}, {}));
   ocs::Role::collect_perm_rules("base", role_list, rules);
   CHECK("no-parent role: 1 rule collected",      rules.size() == 1);
   CHECK("no-parent role: correct operation ADD",  !rules.empty() && rules[0].operation == "ADD");

   // linear chain A → B → C: child-first DFS order
   lAppendElem(role_list, make_role("C", true, "*:*:GET:*:*:*", {},    {}));
   lAppendElem(role_list, make_role("B", true, "*:*:DEL:*:*:*", {"C"}, {}));
   lAppendElem(role_list, make_role("A", true, "*:*:ADD:*:*:*", {"B"}, {}));

   rules.clear();
   ocs::Role::collect_perm_rules("A", role_list, rules);
   CHECK("chain A→B→C: 3 rules collected",      rules.size() == 3);
   CHECK("chain: A's rule first (ADD)",          rules.size() > 0 && rules[0].operation == "ADD");
   CHECK("chain: B's rule second (DEL)",         rules.size() > 1 && rules[1].operation == "DEL");
   CHECK("chain: C's rule third (GET)",          rules.size() > 2 && rules[2].operation == "GET");

   // diamond DAG: A2 → B2 → D2 and A2 → C2 → D2; D2's rule must appear exactly once
   lAppendElem(role_list, make_role("D2", true, "*:*:MOD:*:*:*", {},         {}));
   lAppendElem(role_list, make_role("B2", true, "*:*:DEL:*:*:*", {"D2"},     {}));
   lAppendElem(role_list, make_role("C2", true, "*:*:GET:*:*:*", {"D2"},     {}));
   lAppendElem(role_list, make_role("A2", true, "*:*:ADD:*:*:*", {"B2","C2"},{}));

   rules.clear();
   ocs::Role::collect_perm_rules("A2", role_list, rules);
   CHECK("diamond DAG: 4 rules total (D2 visited once)", rules.size() == 4);
   int mod_count = 0;
   for (const auto &r : rules) {
      if (r.operation == "MOD") {
         ++mod_count;
      }
   }
   CHECK("diamond DAG: D2's MOD rule appears exactly once", mod_count == 1);

   lFreeList(&role_list);
}

// ---------------------------------------------------------------------------
// is_authorized
// ---------------------------------------------------------------------------

static void test_is_authorized() {
   printf("\n--- is_authorized ---\n");

   // devs contains alice; ops contains bob
   lList *userset_list = lCreateList("usersets", US_Type);
   lAppendElem(userset_list, make_userset("devs", {"alice"}));
   lAppendElem(userset_list, make_userset("ops",  {"bob"}));

   ocs::Role::MatchContext ctx;
   ctx.origin       = "qsub";
   ctx.operation    = "ADD";
   ctx.object_type  = "JOB";
   ctx.object_key   = "*";
   ctx.source       = "host1";
   ctx.request_user = "alice";
   ctx.request_group = "users";

   // no roles at all → default-deny
   lList *role_list = lCreateList("roles", RL_Type);
   CHECK("empty role list → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));

   // role assigned to devs with NONE perm_list → still denied
   lAppendElem(role_list, make_role("r_none", true, "NONE", {}, {"devs"}));
   CHECK("NONE perm_list → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));

   // role with a rule that matches the request → allowed
   lAppendElem(role_list, make_role("r_submit", true, "*:qsub:ADD:JOB:*:*", {}, {"devs"}));
   CHECK("matching rule → true", ocs::Role::is_authorized(role_list, userset_list, ctx));

   // user not assigned to any role → denied
   ctx.request_user  = "charlie";
   ctx.request_group = "staff";
   CHECK("user in no role → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));
   ctx.request_user  = "alice";
   ctx.request_group = "users";

   // disabled role is skipped entirely
   lFreeList(&role_list);
   role_list = lCreateList("roles", RL_Type);
   lAppendElem(role_list, make_role("r_disabled", false, "*:*:*:*:*:*", {}, {"devs"}));
   CHECK("disabled role skipped → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));

   // rule inherited from parent role authorizes the request
   lFreeList(&role_list);
   role_list = lCreateList("roles", RL_Type);
   lAppendElem(role_list, make_role("r_parent", true, "*:qsub:ADD:JOB:*:*", {}, {}));
   lAppendElem(role_list, make_role("r_child",  true, "NONE", {"r_parent"}, {"devs"}));
   CHECK("inherited rule from parent → true", ocs::Role::is_authorized(role_list, userset_list, ctx));

   // bob's role covers only qconf MOD CQUEUE, not qsub ADD JOB → denied
   ctx.request_user  = "bob";
   ctx.request_group = "ops";
   lFreeList(&role_list);
   role_list = lCreateList("roles", RL_Type);
   lAppendElem(role_list, make_role("r_ops", true, "*:qconf:MOD:CQUEUE:*:*", {}, {"ops"}));
   CHECK("non-matching rule → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));

   lFreeList(&role_list);
   lFreeList(&userset_list);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_parse_perm_list();
   test_match_rule();
   test_collect_perm_rules();
   test_is_authorized();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
