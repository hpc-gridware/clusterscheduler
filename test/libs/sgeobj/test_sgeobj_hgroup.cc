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
#include <cstring>
#include <initializer_list>

#include "uti/ocs_Pattern.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"

/*
 * CS-2438: admin and submit hosts become the members of the reserved host groups
 * "@admin_hosts" and "@submit_hosts"; "@exec_hosts" mirrors the execution host
 * list and is maintained by the system.
 *
 * hgroup_is_reserved() decides which groups may not be deleted, and
 * hgroup_is_system_maintained() which of them no role may write at all. The two
 * are deliberately separate: all three resist deletion, only @exec_hosts resists
 * modification. A single predicate would have made @admin_hosts read-only or
 * @exec_hosts deletable.
 *
 * The tests drive both predicates directly. The guards that use them live in the
 * qmaster (hgroup_del(), hgroup_mod()) and need a data store plus a GDI packet,
 * so they are covered by the testsuite checks rather than here.
 */

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

// ---------------------------------------------------------------------------
// hgroup_is_reserved()
// ---------------------------------------------------------------------------

static void
test_is_reserved() {
   printf("\n--- hgroup_is_reserved ---\n");

   // T01-T03: the three reserved names
   CHECK(1, "@admin_hosts is reserved", hgroup_is_reserved(ADMIN_HOSTGROUP));
   CHECK(2, "@submit_hosts is reserved", hgroup_is_reserved(SUBMIT_HOSTGROUP));
   CHECK(3, "@exec_hosts is reserved", hgroup_is_reserved(EXEC_HOSTGROUP));

   // T04: an ordinary group is not
   CHECK(4, "@allhosts is not reserved", !hgroup_is_reserved("@allhosts"));

   // T05: nullptr must not crash and must not be reserved -- hgroup_del() calls
   // this with lGetHost(), which returns nullptr for an unset field
   CHECK(5, "nullptr is not reserved", !hgroup_is_reserved(nullptr));

   // T06-T08: the leading '@' is part of the name. ocs::is_hgroup_name() is
   // name[0] == '@', so a bare "admin_hosts" is not a host group name at all and
   // must not be mistaken for the reserved one.
   CHECK(6, "admin_hosts without @ is not reserved", !hgroup_is_reserved("admin_hosts"));
   CHECK(7, "submit_hosts without @ is not reserved", !hgroup_is_reserved("submit_hosts"));
   CHECK(8, "exec_hosts without @ is not reserved", !hgroup_is_reserved("exec_hosts"));

   // T09-T11: look-alikes. A cluster may legitimately own these, and treating
   // them as reserved would make an admin's own group undeletable.
   CHECK(9, "@admin_host (singular) is not reserved", !hgroup_is_reserved("@admin_host"));
   CHECK(10, "@admin_hosts2 is not reserved", !hgroup_is_reserved("@admin_hosts2"));
   CHECK(11, "@my_admin_hosts is not reserved", !hgroup_is_reserved("@my_admin_hosts"));

   // T12-T13: matching is case sensitive, like the reserved usersets in
   // sge_userset_qmaster.cc -- the names are literals the product writes itself
   CHECK(12, "@ADMIN_HOSTS is not reserved", !hgroup_is_reserved("@ADMIN_HOSTS"));
   CHECK(13, "@Exec_Hosts is not reserved", !hgroup_is_reserved("@Exec_Hosts"));

   // T14: empty string
   CHECK(14, "empty name is not reserved", !hgroup_is_reserved(""));
}

// ---------------------------------------------------------------------------
// hgroup_is_system_maintained()
// ---------------------------------------------------------------------------

static void
test_is_system_maintained() {
   printf("\n--- hgroup_is_system_maintained ---\n");

   // T15: only @exec_hosts is refused for every role including manager
   CHECK(15, "@exec_hosts is system maintained", hgroup_is_system_maintained(EXEC_HOSTGROUP));

   // T16-T17: the writable reserved groups. This is the distinction that makes
   // two predicates necessary -- both are reserved, neither is read-only.
   CHECK(16, "@admin_hosts is writable", !hgroup_is_system_maintained(ADMIN_HOSTGROUP));
   CHECK(17, "@submit_hosts is writable", !hgroup_is_system_maintained(SUBMIT_HOSTGROUP));

   // T18-T19: ordinary group and nullptr
   CHECK(18, "@allhosts is not system maintained", !hgroup_is_system_maintained("@allhosts"));
   CHECK(19, "nullptr is not system maintained", !hgroup_is_system_maintained(nullptr));

   // T20: every system maintained group is also reserved -- the write refusal
   // would be pointless if the group could simply be deleted instead
   CHECK(20, "system maintained implies reserved",
         !hgroup_is_system_maintained(EXEC_HOSTGROUP) || hgroup_is_reserved(EXEC_HOSTGROUP));
}

// ---------------------------------------------------------------------------
// The constants themselves
// ---------------------------------------------------------------------------

static void
test_constants() {
   printf("\n--- reserved names ---\n");

   // T21-T23: all three must be valid host group names, or hgroup_check_name()
   // would reject the very groups the qmaster seeds at startup
   CHECK(21, "@admin_hosts is a host group name", ocs::is_hgroup_name(ADMIN_HOSTGROUP));
   CHECK(22, "@submit_hosts is a host group name", ocs::is_hgroup_name(SUBMIT_HOSTGROUP));
   CHECK(23, "@exec_hosts is a host group name", ocs::is_hgroup_name(EXEC_HOSTGROUP));

   // T24: the three names are distinct
   CHECK(24, "the reserved names are distinct",
         strcmp(ADMIN_HOSTGROUP, SUBMIT_HOSTGROUP) != 0 &&
         strcmp(ADMIN_HOSTGROUP, EXEC_HOSTGROUP) != 0 &&
         strcmp(SUBMIT_HOSTGROUP, EXEC_HOSTGROUP) != 0);

   // T25: they match what the RBAC specification names as predefined host groups
   // (04_Logical_View.md, "Source of Request" / "Protected Object Keys"). A typo
   // here would silently disconnect the rule engine from the seeded groups.
   CHECK(25, "names match the specification",
         strcmp(ADMIN_HOSTGROUP, "@admin_hosts") == 0 &&
         strcmp(SUBMIT_HOSTGROUP, "@submit_hosts") == 0 &&
         strcmp(EXEC_HOSTGROUP, "@exec_hosts") == 0);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_is_reserved();
   test_is_system_maintained();
   test_constants();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
