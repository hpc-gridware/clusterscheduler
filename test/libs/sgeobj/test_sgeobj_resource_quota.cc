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

#include "uti/sge_rmon_macros.h"
#include "uti/sge_component.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_resource_quota.h"

typedef struct {
   const char* users;
   const char* group;
   const char* projects;
   const char* pes;
   const char* hosts;
   const char* queues;
} filter_t;

typedef struct {
   filter_t    rule;
   filter_t    query;
   const char* label;
} filter_test_t;

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

// Build an RQR_Type rule element from the users/projects/pes/hosts/queues fields.
static lListElem *build_rule(const filter_t &f) {
   lListElem *filter = nullptr;
   lListElem *rule = lCreateElem(RQR_Type);
   if (rqs_parse_filter_from_string(&filter, f.users, nullptr)) {
      lSetObject(rule, RQR_filter_users, filter);
   }
   if (rqs_parse_filter_from_string(&filter, f.projects, nullptr)) {
      lSetObject(rule, RQR_filter_projects, filter);
   }
   if (rqs_parse_filter_from_string(&filter, f.pes, nullptr)) {
      lSetObject(rule, RQR_filter_pes, filter);
   }
   if (rqs_parse_filter_from_string(&filter, f.hosts, nullptr)) {
      lSetObject(rule, RQR_filter_hosts, filter);
   }
   if (rqs_parse_filter_from_string(&filter, f.queues, nullptr)) {
      lSetObject(rule, RQR_filter_queues, filter);
   }
   return rule;
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sge_resource_quota");

   component_set_daemonized(true);
   lInit(nmv);

   // test fixtures: @hgrp1→{host1}, @hgrp2→{host2}; userset1→{user1}, userset2→{@staff}
   lList *hgroup_list = lCreateList("", HGRP_Type);
   lListElem *hgroup = lCreateElem(HGRP_Type);
   lSetHost(hgroup, HGRP_name, "@hgrp1");
   lAddSubHost(hgroup, HR_name, "host1", HGRP_host_list, HR_Type);
   lAppendElem(hgroup_list, hgroup);
   hgroup = lCreateElem(HGRP_Type);
   lSetHost(hgroup, HGRP_name, "@hgrp2");
   lAddSubHost(hgroup, HR_name, "host2", HGRP_host_list, HR_Type);
   lAppendElem(hgroup_list, hgroup);

   lList *userset_list = lCreateList("", US_Type);
   lListElem *userset = lCreateElem(US_Type);
   lSetString(userset, US_name, "userset1");
   lAddSubStr(userset, UE_name, "user1", US_entries, UE_Type);
   lAppendElem(userset_list, userset);
   userset = lCreateElem(US_Type);
   lSetString(userset, US_name, "userset2");
   lAddSubStr(userset, UE_name, "@staff", US_entries, UE_Type);
   lAppendElem(userset_list, userset);

   // positive filter matching — T01–T48
   filter_test_t positive_tests[] = {
   // simple search
      {{"user1,user2,user3", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user3", "staff", "*", "*", "*", "*"},
       "simple: user3 in user list"},
      {{nullptr, nullptr, "project1,project2,project3", nullptr, nullptr, nullptr},
       {"*", "staff", "project2", "*", "*", "*"},
       "simple: project2 in project list"},
      {{nullptr, nullptr, nullptr, "pe1,pe2,pe3", nullptr, nullptr},
       {"*", "staff", "*", "pe3", "*", "*"},
       "simple: pe3 in pe list"},
      {{nullptr, nullptr, nullptr, nullptr, "h1,h2,h3", nullptr},
       {"*", "staff", "*", "*", "h3", "*"},
       "simple: h3 in host list"},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue1,queue2,queue3"},
       {"*", "staff", "*", "*", "*", "queue1"},
       "simple: queue1 in queue list"},
   // wildcard query
      {{"user1,user2,user3", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user*", "*", "staff", "*", "*", "*"},
       "wildcard query: user* matches user list"},
      {{nullptr, nullptr, "project1,project2,project3", nullptr, nullptr, nullptr},
       {"*", "staff", "project*", "*", "*", "*"},
       "wildcard query: project* matches project list"},
      {{nullptr, nullptr, nullptr, "pe1,pe2,pe3", nullptr, nullptr},
       {"*", "staff", "*", "pe*", "*", "*"},
       "wildcard query: pe* matches pe list"},
      {{nullptr, nullptr, nullptr, nullptr, "h1,h2,h3", nullptr},
       {"*", "staff", "*", "*", "h*", "*"},
       "wildcard query: h* matches host list"},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue1,queue2,queue3"},
       {"*", "staff", "*", "*", "*", "que*"},
       "wildcard query: que* matches queue list"},
   // wildcard rule definition
      {{"user*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user3", "staff", "*", "*", "*", "*"},
       "wildcard rule: user* matches user3"},
      {{nullptr, nullptr, "project*", nullptr, nullptr, nullptr},
       {"*", "staff", "project2", "*", "*", "*"},
       "wildcard rule: project* matches project2"},
      {{nullptr, nullptr, nullptr, "pe*", nullptr, nullptr},
       {"*", "staff", "*", "pe3", "*", "*"},
       "wildcard rule: pe* matches pe3"},
      {{nullptr, nullptr, nullptr, nullptr, "h*", nullptr},
       {"*", "staff", "*", "*", "h1", "*"},
       "wildcard rule: h* matches h1"},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue*"},
       {"*", "staff", "*", "*", "*", "queue1"},
       "wildcard rule: queue* matches queue1"},
   // wildcard rule definition + wildcard query
      {{"user*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"u*", "staff", "*", "*", "*", "*"},
       "wildcard both: user*/u*"},
      {{nullptr, nullptr, "project*", nullptr, nullptr, nullptr},
       {"*", "staff", "pro*", "*", "*", "*"},
       "wildcard both: project*/pro*"},
      {{nullptr, nullptr, nullptr, "pe*", nullptr, nullptr},
       {"*", "staff", "*", "p*", "*", "*"},
       "wildcard both: pe*/p*"},
      {{nullptr, nullptr, nullptr, nullptr, "host*", nullptr},
       {"*", "staff", "*", "*", "h*", "*"},
       "wildcard both: host*/h*"},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue*"},
       {"*", "staff", "*", "*", "*", "qu*"},
       "wildcard both: queue*/qu*"},
   // hostgroup rule definition
      {{nullptr, nullptr, nullptr, nullptr, "@hgrp1", nullptr},
       {"*", "staff", "*", "*", "host1", "*"},
       "@hgrp1 rule matches host1 query"},
      {{nullptr, nullptr, nullptr, nullptr, "@hgrp1", nullptr},
       {"*", "staff", "*", "*", "ho*", "*"},
       "@hgrp1 rule matches ho* query"},
      {{nullptr, nullptr, nullptr, nullptr, "@hgr*", nullptr},
       {"*", "staff", "*", "*", "host1", "*"},
       "@hgr* rule matches host1 query"},
      {{nullptr, nullptr, nullptr, nullptr, "@hgr*", nullptr},
       {"*", "staff", "*", "*", "hos*", "*"},
       "@hgr* rule matches hos* query"},
      {{nullptr, nullptr, nullptr, nullptr, "@hgrp1", nullptr},
       {"*", "staff", "*", "*", "@hgrp1", "*"},
       "@hgrp1 rule matches @hgrp1 query"},
      {{nullptr, nullptr, nullptr, nullptr, "host1", nullptr},
       {"*", "staff", "*", "*", "@hgrp1", "*"},
       "host1 rule matches @hgrp1 query"},
      {{nullptr, nullptr, nullptr, nullptr, "ho*", nullptr},
       {"*", "staff", "*", "*", "@hgrp1", "*"},
       "ho* rule matches @hgrp1 query"},
      {{nullptr, nullptr, nullptr, nullptr, "host1", nullptr},
       {"*", "staff", "*", "*", "@hgrp*", "*"},
       "host1 rule matches @hgrp* query"},
      {{nullptr, nullptr, nullptr, nullptr, "ho*", nullptr},
       {"*", "staff", "*", "*", "@hgrp*", "*"},
       "ho* rule matches @hgrp* query"},
      {{nullptr, nullptr, nullptr, nullptr, "ho*", nullptr},
       {"*", "staff", "*", "*", "*", "*"},
       "ho* rule matches * query"},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgrp1", nullptr},
       {"*", "staff", "*", "*", "*", "*"},
       "!@hgrp1 rule matches * query"},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgrp1", nullptr},
       {"*", "staff", "*", "*", "@hgrp*", "*"},
       "!@hgrp1 rule matches @hgrp* query (covers non-excluded groups)"},
      {{nullptr, nullptr, nullptr, nullptr, "@hgrp2", nullptr},
       {"*", "staff", "*", "*", "host2", "*"},
       "@hgrp2 rule matches host2 query"},
   // userset rule definition
      {{"@userset1", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", "staff", "*", "*", "*", "*"},
       "@userset1 rule matches user1 query"},
      {{"@userset1", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"use*", "staff", "*", "*", "*", "*"},
       "@userset1 rule matches use* query"},
      {{"@users*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", "staff", "*", "*", "*", "*"},
       "@users* rule matches user1 query"},
      {{"@users*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user*", "staff", "*", "*", "*", "*"},
       "@users* rule matches user* query"},
      {{"user1", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"@userset1", "staff", "*", "*", "*", "*"},
       "user1 rule matches @userset1 query"},
      {{"us*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"@userset1", "staff", "*", "*", "*", "*"},
       "us* rule matches @userset1 query"},
      {{"user1", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"@use*", "staff", "*", "*", "*", "*"},
       "user1 rule matches @use* query"},
      {{"use*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"@use*", "staff", "*", "*", "*", "*"},
       "use* rule matches @use* query"},
      {{"@user*2", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", "staff", "*", "*", "*", "*"},
       "@user*2 rule matches user1 with group staff"},
      {{"@user*2", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", "*", "*", "*", "*", "*"},
       "@user*2 rule matches user1 with group *"},
      {{"!@user*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"*", "*", "*", "*", "*", "*"},
       "!@user* rule matches * with any group"},
      {{"@userset2", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", "staff", "*", "*", "*", "*"},
       "@userset2 exact name: user1 matches via primary group staff"},
   // project definition
      {{nullptr, nullptr, "!*", nullptr, nullptr, nullptr},
       {"*", "staff", nullptr, "*", "*", "*"},
       "project !* rule matches null project query"},
   // multi-dimension: users + hosts both constrained
      {{"user1", nullptr, nullptr, nullptr, "host1", nullptr},
       {"user1", "staff", "*", "*", "host1", "*"},
       "multi-dim: user1 and host1 both match"},
   // empty rule matches everything
      {{nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
       {"*", "staff", "*", "*", "*", "*"},
       "empty rule matches any query"},
   };

   // negative filter matching — T49–T80
   filter_test_t negative_tests[] = {
   // simple search fails
      {{"*,!user3", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user3", "staff", "*", "*", "*", "*"},
       "*,!user3 rule excludes user3"},
      {{"user1,user2", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user3", "staff", "*", "*", "*", "*"},
       "user1,user2 rule excludes user3"},
      {{nullptr, nullptr, "*,!project2", nullptr, nullptr, nullptr},
       {"*", "staff", "project2", "*", "*", "*"},
       "*,!project2 rule excludes project2"},
      {{nullptr, nullptr, "project1,project3", nullptr, nullptr, nullptr},
       {"*", "staff", "project2", "*", "*", "*"},
       "project1,project3 rule excludes project2"},
      {{nullptr, nullptr, nullptr, "*,!pe3", nullptr, nullptr},
       {"*", "staff", "*", "pe3", "*", "*"},
       "*,!pe3 rule excludes pe3"},
      {{nullptr, nullptr, nullptr, "pe1,pe2", nullptr, nullptr},
       {"*", "staff", "*", "pe3", "*", "*"},
       "pe1,pe2 rule excludes pe3"},
      {{nullptr, nullptr, nullptr, nullptr, "*,!h3", nullptr},
       {"*", "staff", "*", "*", "h3", "*"},
       "*,!h3 rule excludes h3"},
      {{nullptr, nullptr, nullptr, nullptr, "h1,h2", nullptr},
       {"*", "staff", "*", "*", "h3", "*"},
       "h1,h2 rule excludes h3"},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "*,!queue1"},
       {"*", "staff", "*", "*", "*", "queue1"},
       "*,!queue1 rule excludes queue1"},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "queue2,queue3"},
       {"*", "staff", "*", "*", "*", "queue1"},
       "queue2,queue3 rule excludes queue1"},
   // negated wildcard fails
      {{"!us*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user*", "staff", "*", "*", "*", "*"},
       "!us* rule excludes user* query"},
      {{nullptr, nullptr, "!pro*", nullptr, nullptr, nullptr},
       {"*", "staff", "project*", "*", "*", "*"},
       "!pro* rule excludes project* query"},
      {{nullptr, nullptr, nullptr, "!p*", nullptr, nullptr},
       {"*", "staff", "*", "pe*", "*", "*"},
       "!p* rule excludes pe* query"},
      {{nullptr, nullptr, nullptr, nullptr, "!h*", nullptr},
       {"*", "staff", "*", "*", "hos*", "*"},
       "!h* rule excludes hos* query"},
      {{nullptr, nullptr, nullptr, nullptr, nullptr, "!qu*"},
       {"*", "staff", "*", "*", "*", "que*"},
       "!qu* rule excludes que* query"},
   // negated hostgroup fails
      {{nullptr, nullptr, nullptr, nullptr, "!@hgrp1", nullptr},
       {"*", "staff", "*", "*", "host1", "*"},
       "!@hgrp1 rule excludes host1"},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgrp1", nullptr},
       {"*", "staff", "*", "*", "ho*", "*"},
       "!@hgrp1 rule excludes ho* query"},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgr*", nullptr},
       {"*", "staff", "*", "*", "host1", "*"},
       "!@hgr* rule excludes host1"},
      {{nullptr, nullptr, nullptr, nullptr, "!@hgr*", nullptr},
       {"*", "staff", "*", "*", "hos*", "*"},
       "!@hgr* rule excludes hos* query"},
      {{nullptr, nullptr, nullptr, nullptr, "!hgrp1", nullptr},
       {"*", "staff", "*", "*", "hgrp1", "*"},
       "!hgrp1 rule excludes hgrp1"},
      {{nullptr, nullptr, nullptr, nullptr, "!hgrp*", nullptr},
       {"*", "staff", "*", "*", "hgrp*", "*"},
       "!hgrp* rule excludes hgrp*"},
   // negated userset fails
      {{"!@userset1", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", "staff", "*", "*", "*", "*"},
       "!@userset1 rule excludes user1"},
      {{"!@userset1", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"use*", "staff", "*", "*", "*", "*"},
       "!@userset1 rule excludes use* query"},
      {{"!@users*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", "staff", "*", "*", "*", "*"},
       "!@users* rule excludes user1"},
      {{"!@users*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user*", "staff", "*", "*", "*", "*"},
       "!@users* rule excludes user* query"},
      {{"!@userset2", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", "staff", "*", "*", "*", "*"},
       "!@userset2 rule excludes user1 whose group staff is in userset2"},
      {{"@userset1,!@userset2", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", "staff", "*", "*", "*", "*"},
       "userset1+!userset2: user1 in userset1 but excluded via userset2"},
      {{"@user*2", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"user1", nullptr, "*", "*", "*", "*"},
       "@user*2 rule excludes user1 with null group"},
   // project definition fails
      {{nullptr, nullptr, "*", nullptr, nullptr, nullptr},
       {"*", "staff", nullptr, "*", "*", "*"},
       "* project rule excludes null project query"},
      {{nullptr, nullptr, "!*", nullptr, nullptr, nullptr},
       {"*", "staff", "project1", "*", "*", "*"},
       "!* project rule excludes non-null project"},
   // multi-dimension: user matches but host does not
      {{"user1", nullptr, nullptr, nullptr, "host1", nullptr},
       {"user1", "staff", "*", "*", "host2", "*"},
       "multi-dim: user1 matches but host2 not in host1 rule"},
   // !* user rule matches nothing
      {{"!*", nullptr, nullptr, nullptr, nullptr, nullptr},
       {"*", "staff", "*", "*", "*", "*"},
       "!* user rule matches nothing"},
   };

   // run positive tests
   printf("\n--- positive filter matching ---\n");
   int id = 1;
   for (int i = 0; i < (int)(sizeof(positive_tests) / sizeof(positive_tests[0])); i++) {
      lListElem *rule = build_rule(positive_tests[i].rule);
      const filter_t &q = positive_tests[i].query;
      CHECK(id++, positive_tests[i].label,
            rqs_is_matching_rule(rule, q.users, q.group, nullptr, q.projects,
                                 q.pes, q.hosts, q.queues, userset_list, hgroup_list));
      lFreeElem(&rule);
   }

   // run negative tests
   printf("\n--- negative filter matching ---\n");
   for (int i = 0; i < (int)(sizeof(negative_tests) / sizeof(negative_tests[0])); i++) {
      lListElem *rule = build_rule(negative_tests[i].rule);
      const filter_t &q = negative_tests[i].query;
      CHECK(id++, negative_tests[i].label,
            !rqs_is_matching_rule(rule, q.users, q.group, nullptr, q.projects,
                                  q.pes, q.hosts, q.queues, userset_list, hgroup_list));
      lFreeElem(&rule);
   }

   // supplementary-group userset matching — T81–T83 (GCS only; skipped if WITH_EXTENSIONS not built)
   printf("\n--- supplementary-group matching ---\n");
   mconf_set_enable_sup_grp_eval(true);
   if (!mconf_get_enable_sup_grp_eval()) {
      printf("skipped — WITH_EXTENSIONS not built\n");
   } else {
      // userset2 has entry "@staff"; "nobody" is not in any userset by name or primary group
      filter_t userset2_rule = {"@userset2", nullptr, nullptr, nullptr, nullptr, nullptr};

      lList *sup_staff = lCreateList("", ST_Type);
      lAddElemStr(&sup_staff, ST_name, "staff", ST_Type);

      lList *sup_other = lCreateList("", ST_Type);
      lAddElemStr(&sup_other, ST_name, "other", ST_Type);

      // T81: match via supplementary group
      lListElem *rule = build_rule(userset2_rule);
      CHECK(id++, "sup-grp: @userset2 matches nobody via supplementary group staff",
            rqs_is_matching_rule(rule, "nobody", nullptr, sup_staff, nullptr,
                                 "*", "*", "*", userset_list, hgroup_list));
      lFreeElem(&rule);

      // T82: no match when supplementary group is not in userset2
      rule = build_rule(userset2_rule);
      CHECK(id++, "sup-grp: @userset2 no match when supplementary group is other",
            !rqs_is_matching_rule(rule, "nobody", nullptr, sup_other, nullptr,
                                  "*", "*", "*", userset_list, hgroup_list));
      lFreeElem(&rule);

      // T83: same data but flag disabled — supplementary group path is skipped
      mconf_set_enable_sup_grp_eval(false);
      rule = build_rule(userset2_rule);
      CHECK(id++, "sup-grp: @userset2 no match with flag disabled",
            !rqs_is_matching_rule(rule, "nobody", nullptr, sup_staff, nullptr,
                                  "*", "*", "*", userset_list, hgroup_list));
      lFreeElem(&rule);

      lFreeList(&sup_staff);
      lFreeList(&sup_other);
   }

   lFreeList(&hgroup_list);
   lFreeList(&userset_list);

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
