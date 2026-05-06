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

#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
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
// parse_perm_list  [T01–T06]
//
// parse_perm_list() converts a raw perm_list string into a vector of
// PermRule structs. Rules are comma-separated; each rule has exactly six
// colon-separated non-empty fields. "NONE" and nullptr are accepted as the
// empty rule set.
// ---------------------------------------------------------------------------

static void test_parse_perm_list() {
   printf("\n--- parse_perm_list ---\n");
   lList *al = nullptr;
   ocs::Role::PermRuleList rules;

   // T01: "NONE" is the canonical empty rule set sentinel
   CHECK(1, "NONE yields empty rules", ocs::Role::parse_perm_list("NONE", rules, &al) && rules.empty());
   lFreeList(&al);

   // T02: nullptr input is treated the same as NONE
   CHECK(2, "nullptr yields empty rules", ocs::Role::parse_perm_list(nullptr, rules, &al) && rules.empty());
   lFreeList(&al);

   // T03: a well-formed single rule with all six fields is parsed correctly
   rules.clear();
   bool ok = ocs::Role::parse_perm_list("*:*:ADD:JOB:*:*", rules, &al);
   CHECK(3, "valid single rule returns true",      ok);
   CHECK(3, "valid single rule produces 1 rule",   ok && rules.size() == 1);
   CHECK(3, "rule source field is *",              ok && rules[0].source == "*");
   CHECK(3, "rule operation field is ADD",         ok && rules[0].operation == "ADD");
   CHECK(3, "rule object_type field is JOB",       ok && rules[0].object_type == "JOB");
   lFreeList(&al);

   // T04: a rule with only five colon-separated fields is rejected
   rules.clear();
   {
      int saved = suppress_stderr();
      bool res = ocs::Role::parse_perm_list("*:*:ADD:JOB:*", rules, &al);
      restore_stderr(saved);
      CHECK(4, "five fields returns false", !res);
   }
   lFreeList(&al);

   // T05: a rule with an empty (zero-length) field is rejected
   rules.clear();
   {
      int saved = suppress_stderr();
      bool res = ocs::Role::parse_perm_list("*:*::JOB:*:*", rules, &al);
      restore_stderr(saved);
      CHECK(5, "empty field returns false", !res);
   }
   lFreeList(&al);

   // T06: two comma-separated rules are parsed into two independent PermRule structs
   rules.clear();
   ok = ocs::Role::parse_perm_list("*:*:ADD:JOB:*:*, *:*:DEL:JOB:*:*", rules, &al);
   CHECK(6, "two rules returns true",              ok);
   CHECK(6, "two rules produces 2 rules",          ok && rules.size() == 2);
   CHECK(6, "first rule operation is ADD",         ok && rules[0].operation == "ADD");
   CHECK(6, "second rule operation is DEL",        ok && rules[1].operation == "DEL");
   lFreeList(&al);
}

// ---------------------------------------------------------------------------
// match_rule  [T07–T24]
//
// match_rule() evaluates a single PermRule against a MatchContext. All six
// characteristics must match simultaneously for the function to return true.
// Each field supports wildcards (*), pipe-separated alternation (A|B|C),
// fnmatch patterns, variable expansions ($job_cmd, $admin_cmd, etc.), and
// special tokens (owner=$request_user, @hostgroup).
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

   // T07: an all-wildcard rule matches any context
   rule = {"*", "*", "*", "*", "*", "*"};
   CHECK(7, "all-wildcard matches", ocs::Role::match_rule(rule, ctx));

   // T08: a single mismatched characteristic causes the whole rule to fail
   rule = {"*", "*", "DEL", "*", "*", "*"};
   CHECK(8, "operation DEL vs ADD → false", !ocs::Role::match_rule(rule, ctx));

   // T09: pipe alternation — the context value appears in the allowed set
   rule = {"*", "*", "GET|ADD|DEL", "*", "*", "*"};
   CHECK(9, "|-alternation: ADD in set → true", ocs::Role::match_rule(rule, ctx));

   // T10: pipe alternation — the context value is absent from the allowed set
   rule = {"*", "*", "GET|DEL", "*", "*", "*"};
   CHECK(10, "|-alternation: ADD not in set → false", !ocs::Role::match_rule(rule, ctx));

   // T11: fnmatch pattern on the operation field
   rule = {"*", "*", "MO*", "*", "*", "*"};
   ctx.operation = "MOD";
   CHECK(11, "fnmatch MO* matches MOD",        ocs::Role::match_rule(rule, ctx));
   ctx.operation = "MOD_STATE";
   CHECK(11, "fnmatch MO* matches MOD_STATE",  ocs::Role::match_rule(rule, ctx));
   ctx.operation = "GET";
   CHECK(11, "fnmatch MO* does not match GET", !ocs::Role::match_rule(rule, ctx));
   ctx.operation = "ADD";

   // T12: $job_cmd expands to the set of job-submission client names (qsub, qrsh, ...)
   rule = {"*", "$job_cmd", "*", "*", "*", "*"};
   ctx.origin = "qsub";
   CHECK(12, "$job_cmd covers qsub",       ocs::Role::match_rule(rule, ctx));
   ctx.origin = "qconf";
   CHECK(12, "$job_cmd does not cover qconf", !ocs::Role::match_rule(rule, ctx));

   // T13: $admin_cmd expands to administrative client names (qconf, ...)
   rule = {"*", "$admin_cmd", "*", "*", "*", "*"};
   ctx.origin = "qconf";
   CHECK(13, "$admin_cmd covers qconf", ocs::Role::match_rule(rule, ctx));
   ctx.origin = "qsub";

   // T14: $system_service expands to internal daemon names (sge_qmaster, ...)
   rule = {"*", "$system_service", "*", "*", "*", "*"};
   ctx.origin = "sge_qmaster";
   CHECK(14, "$system_service covers sge_qmaster", ocs::Role::match_rule(rule, ctx));
   ctx.origin = "qsub";

   // T15: $submit_objects expands to user-submittable object types (JOB, AR)
   rule = {"*", "*", "*", "$submit_objects", "*", "*"};
   ctx.object_type = "JOB";
   CHECK(15, "$submit_objects covers JOB",            ocs::Role::match_rule(rule, ctx));
   ctx.object_type = "AR";
   CHECK(15, "$submit_objects covers AR",             ocs::Role::match_rule(rule, ctx));
   ctx.object_type = "CQUEUE";
   CHECK(15, "$submit_objects does not cover CQUEUE", !ocs::Role::match_rule(rule, ctx));
   ctx.object_type = "JOB";

   // T16: source field matched as an fnmatch pattern against the FQDN
   rule = {"submit*.example.com", "*", "*", "*", "*", "*"};
   ctx.source = "submit1.example.com";
   CHECK(16, "source fnmatch: submit1 matches",       ocs::Role::match_rule(rule, ctx));
   ctx.source = "admin1.example.com";
   CHECK(16, "source fnmatch: admin1 does not match", !ocs::Role::match_rule(rule, ctx));
   ctx.source = "host1.example.com";

   // T17: @-prefixed source token is matched against the source_hostgroups vector
   rule = {"@submit_hosts", "*", "*", "*", "*", "*"};
   ctx.source_hostgroups = {"@submit_hosts", "@other"};
   CHECK(17, "@submit_hosts matched in source_hostgroups",      ocs::Role::match_rule(rule, ctx));
   ctx.source_hostgroups = {"@exec_hosts"};
   CHECK(17, "@submit_hosts not in source_hostgroups → false",  !ocs::Role::match_rule(rule, ctx));
   ctx.source_hostgroups = {};

   // T18: owner=$request_user resolves to the requesting user's name
   rule = {"*", "*", "*", "*", "owner=$request_user", "*"};
   ctx.request_user = "alice";
   ctx.object_owner = "alice";
   CHECK(18, "owner=$request_user: owner matches → true",           ocs::Role::match_rule(rule, ctx));
   ctx.object_owner = "bob";
   CHECK(18, "owner=$request_user: different owner → false",        !ocs::Role::match_rule(rule, ctx));
   ctx.object_owner = "alice";

   // T19: object_key field supports fnmatch patterns
   rule = {"*", "*", "*", "*", "job_*", "*"};
   ctx.object_key = "job_42";
   CHECK(19, "object_key fnmatch: job_42 matches",      ocs::Role::match_rule(rule, ctx));
   ctx.object_key = "ar_5";
   CHECK(19, "object_key fnmatch: ar_5 does not match", !ocs::Role::match_rule(rule, ctx));
   ctx.object_key = "42";

   // T20: value_constraint=* always matches, regardless of required constraints
   rule = {"*", "*", "*", "*", "*", "*"};
   ctx.required_value_constraints = {"EXCLUSIVE"};
   CHECK(20, "value_constraint=* always matches", ocs::Role::match_rule(rule, ctx));

   // T21: when no elevated permissions are required, any grant set is acceptable
   rule = {"*", "*", "*", "*", "*", "EXCLUSIVE"};
   ctx.required_value_constraints = {};
   CHECK(21, "no required constraints → matches any grant set", ocs::Role::match_rule(rule, ctx));

   // T22: required constraint is satisfied when it appears in the grant set
   rule = {"*", "*", "*", "*", "*", "EXCLUSIVE|PRIORITY"};
   ctx.required_value_constraints = {"EXCLUSIVE"};
   CHECK(22, "required EXCLUSIVE covered → true", ocs::Role::match_rule(rule, ctx));

   // T23: all required constraints must appear in the grant set for the rule to match
   ctx.required_value_constraints = {"EXCLUSIVE", "PRIORITY"};
   CHECK(23, "both required constraints covered → true", ocs::Role::match_rule(rule, ctx));

   // T24: a required constraint absent from the grant set blocks the match
   rule = {"*", "*", "*", "*", "*", "EXCLUSIVE"};
   ctx.required_value_constraints = {"EXCLUSIVE", "PRIORITY"};
   CHECK(24, "PRIORITY not in grant set → false", !ocs::Role::match_rule(rule, ctx));
   ctx.required_value_constraints = {};
}

// ---------------------------------------------------------------------------
// collect_perm_rules  [T25–T27]
//
// collect_perm_rules() transitively collects all PermRules reachable from a
// given role via its parent_role_list, using a DFS traversal. Each role's own
// rules are appended before recursing into its parents (left-to-right). A
// visited set prevents duplicate collection when multiple inheritance paths
// converge on the same ancestor (diamond DAG pattern).
// ---------------------------------------------------------------------------

static void test_collect_perm_rules() {
   printf("\n--- collect_perm_rules ---\n");

   lList *role_list = lCreateList("roles", RL_Type);
   ocs::Role::PermRuleList rules;

   // T25: a role with no parents yields only its own rules
   lAppendElem(role_list, make_role("base", true, "*:*:ADD:JOB:*:*", {}, {}));
   ocs::Role::collect_perm_rules("base", role_list, rules);
   CHECK(25, "no-parent role: 1 rule collected",       rules.size() == 1);
   CHECK(25, "no-parent role: correct operation ADD",  !rules.empty() && rules[0].operation == "ADD");

   // T26: a linear chain A → B → C yields rules in child-first DFS order (A, B, C)
   lAppendElem(role_list, make_role("C", true, "*:*:GET:*:*:*", {},    {}));
   lAppendElem(role_list, make_role("B", true, "*:*:DEL:*:*:*", {"C"}, {}));
   lAppendElem(role_list, make_role("A", true, "*:*:ADD:*:*:*", {"B"}, {}));

   rules.clear();
   ocs::Role::collect_perm_rules("A", role_list, rules);
   CHECK(26, "chain A→B→C: 3 rules collected",      rules.size() == 3);
   CHECK(26, "chain: A's rule first (ADD)",          rules.size() > 0 && rules[0].operation == "ADD");
   CHECK(26, "chain: B's rule second (DEL)",         rules.size() > 1 && rules[1].operation == "DEL");
   CHECK(26, "chain: C's rule third (GET)",          rules.size() > 2 && rules[2].operation == "GET");

   // T27: a diamond DAG (A→B→D and A→C→D) yields D's rules exactly once
   lAppendElem(role_list, make_role("D2", true, "*:*:MOD:*:*:*", {},          {}));
   lAppendElem(role_list, make_role("B2", true, "*:*:DEL:*:*:*", {"D2"},      {}));
   lAppendElem(role_list, make_role("C2", true, "*:*:GET:*:*:*", {"D2"},      {}));
   lAppendElem(role_list, make_role("A2", true, "*:*:ADD:*:*:*", {"B2","C2"}, {}));

   rules.clear();
   ocs::Role::collect_perm_rules("A2", role_list, rules);
   CHECK(27, "diamond DAG: 4 rules total (D2 visited once)", rules.size() == 4);
   int mod_count = 0;
   for (const auto &r : rules) {
      if (r.operation == "MOD") { ++mod_count; }
   }
   CHECK(27, "diamond DAG: D2's MOD rule appears exactly once", mod_count == 1);

   lFreeList(&role_list);
}

// ---------------------------------------------------------------------------
// is_authorized  [T28–T34]
//
// is_authorized() evaluates whether a request context is permitted under the
// current role configuration. It iterates all enabled roles, checks whether
// the requesting user belongs to each role's user_list (via userset lookup),
// collects the effective rule set (including inherited rules), and returns
// true on the first matching rule. The default is deny.
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

   // T28: with no roles configured the system is default-deny
   lList *role_list = lCreateList("roles", RL_Type);
   CHECK(28, "empty role list → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));

   // T29: a role whose perm_list is NONE grants no permissions even when the user is a member
   lAppendElem(role_list, make_role("r_none", true, "NONE", {}, {"devs"}));
   CHECK(29, "NONE perm_list → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));

   // T30: a role with a matching rule authorizes the request
   lAppendElem(role_list, make_role("r_submit", true, "*:qsub:ADD:JOB:*:*", {}, {"devs"}));
   CHECK(30, "matching rule → true", ocs::Role::is_authorized(role_list, userset_list, ctx));

   // T31: a user who belongs to no role's user_list is denied
   ctx.request_user  = "charlie";
   ctx.request_group = "staff";
   CHECK(31, "user in no role → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));
   ctx.request_user  = "alice";
   ctx.request_group = "users";

   // T32: a disabled role is skipped entirely; its rules have no effect
   lFreeList(&role_list);
   role_list = lCreateList("roles", RL_Type);
   lAppendElem(role_list, make_role("r_disabled", false, "*:*:*:*:*:*", {}, {"devs"}));
   CHECK(32, "disabled role skipped → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));

   // T33: a rule inherited from a parent role is sufficient to authorize the request
   lFreeList(&role_list);
   role_list = lCreateList("roles", RL_Type);
   lAppendElem(role_list, make_role("r_parent", true, "*:qsub:ADD:JOB:*:*", {}, {}));
   lAppendElem(role_list, make_role("r_child",  true, "NONE", {"r_parent"}, {"devs"}));
   CHECK(33, "inherited rule from parent → true", ocs::Role::is_authorized(role_list, userset_list, ctx));

   // T34: a role whose rules cover a different operation/object type does not authorize the request
   ctx.request_user  = "bob";
   ctx.request_group = "ops";
   lFreeList(&role_list);
   role_list = lCreateList("roles", RL_Type);
   lAppendElem(role_list, make_role("r_ops", true, "*:qconf:MOD:CQUEUE:*:*", {}, {"ops"}));
   CHECK(34, "non-matching rule → false", !ocs::Role::is_authorized(role_list, userset_list, ctx));

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
