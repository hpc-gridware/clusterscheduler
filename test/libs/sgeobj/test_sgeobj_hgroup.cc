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
 * @brief Unit tests for hgroup in `libs/sgeobj`
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <sys/stat.h>
#include <unistd.h>

#include "uti/ocs_Pattern.h"
#include "uti/sge_hostname.h"

#include "sgeobj/sge_answer.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
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
// hgroup_update_cache() / hgroup_list_update_caches()  -- CS-2451
// ---------------------------------------------------------------------------

/*
 * hgroup_find_all_references() compares host names with sge_hostcmp(), which
 * consults ocs::Bootstrap for ignore_fqdn / default_domain. Bootstrap loads
 * $SGE_ROOT/$SGE_CELL/common/bootstrap on first access and calls sge_exit(1) if
 * it cannot, so a throwaway one is created here -- the same trick
 * test_sgeobj_qref.cc uses. The values only have to parse.
 */
static char s_sge_root[128];

static bool setup_bootstrap() {
   char path[256];

   snprintf(s_sge_root, sizeof(s_sge_root), "/tmp/test_sgeobj_hgroup_%ld", (long) getpid());
   if (mkdir(s_sge_root, 0755) != 0) {
      return false;
   }
   snprintf(path, sizeof(path), "%s/default", s_sge_root);
   if (mkdir(path, 0755) != 0) {
      return false;
   }
   snprintf(path, sizeof(path), "%s/default/common", s_sge_root);
   if (mkdir(path, 0755) != 0) {
      return false;
   }
   snprintf(path, sizeof(path), "%s/default/common/bootstrap", s_sge_root);

   FILE *fp = fopen(path, "w");
   if (fp == nullptr) {
      return false;
   }
   fprintf(fp,
           "admin_user        none\n"
           "default_domain    none\n"
           "ignore_fqdn       true\n"
           "spooling_method   classic\n"
           "spooling_lib      none\n"
           "spooling_params   none\n"
           "binary_path       none\n"
           "qmaster_spool_dir none\n"
           "security_mode     none\n");
   fclose(fp);

   setenv("SGE_ROOT", s_sge_root, 1);
   setenv("SGE_CELL", "default", 1);
   return true;
}

static void teardown_bootstrap() {
   char path[256];

   snprintf(path, sizeof(path), "%s/default/common/bootstrap", s_sge_root);
   unlink(path);
   snprintf(path, sizeof(path), "%s/default/common", s_sge_root);
   rmdir(path);
   snprintf(path, sizeof(path), "%s/default", s_sge_root);
   rmdir(path);
   rmdir(s_sge_root);
}

/* Append a host group with the given members. Name validation off: these lists
 * are built by hand, not accepted from a client. */
static lListElem *
add_hgroup(lList **list, const char *name, std::initializer_list<const char *> members) {
   lList *hrefs = nullptr;

   for (const char *m : members) {
      href_list_add(&hrefs, nullptr, m);
   }

   /* hgroup_create() takes ownership of hrefs (lSetList, no copy) */
   lListElem *hgroup = hgroup_create(nullptr, name, hrefs, false);
   if (hgroup == nullptr) {
      lFreeList(&hrefs);
      return nullptr;
   }
   if (*list == nullptr) {
      *list = lCreateList("hgroups", HGRP_Type);
   }
   lAppendElem(*list, hgroup);
   return hgroup;
}

/* Does the cache equal a fresh walk, element by element and in order? The cache
 * must reproduce the walk exactly -- see the note on duplicates below. */
static bool
cache_equals_walk(const lListElem *hgroup, const lList *master_list) {
   lList *walked = nullptr;

   if (!hgroup_find_all_references(hgroup, nullptr, master_list, &walked, nullptr)) {
      lFreeList(&walked);
      return false;
   }

   const lList *cached = lGetList(hgroup, HGRP_cached_hosts);
   bool ok = lGetNumberOfElem(cached) == lGetNumberOfElem(walked);
   const lListElem *c = lFirst(cached);
   const lListElem *w = lFirst(walked);

   while (ok && c != nullptr && w != nullptr) {
      ok = (sge_hostcmp(lGetHost(c, HR_name), lGetHost(w, HR_name)) == 0);
      c = lNext(c);
      w = lNext(w);
   }
   lFreeList(&walked);
   return ok;
}

static bool
cached_has(const lList *master_list, const char *group, const char *host) {
   const lListElem *hgroup = lGetElemHost(master_list, HGRP_name, group);
   return hgroup != nullptr && href_list_locate(lGetList(hgroup, HGRP_cached_hosts), host) != nullptr;
}

static void
test_update_cache() {
   printf("\n--- hgroup_update_cache ---\n");

   lList *master_list = nullptr;

   /* @leaf -> two hosts;  @mid -> @leaf;  @top -> @mid + a host of its own */
   add_hgroup(&master_list, "@leaf", {"hosta", "hostb"});
   add_hgroup(&master_list, "@mid", {"@leaf"});
   add_hgroup(&master_list, "@top", {"@mid", "hostc"});

   CHECK(26, "nothing is cached before the first update",
         lGetUlong(lGetElemHost(master_list, HGRP_name, "@leaf"), HGRP_cache_version) == 0);

   CHECK(27, "hgroup_list_update_caches succeeds",
         hgroup_list_update_caches(master_list, nullptr));

   CHECK(28, "@leaf caches its own hosts",
         cached_has(master_list, "@leaf", "hosta") && cached_has(master_list, "@leaf", "hostb"));
   CHECK(29, "@mid resolves one level down",
         cached_has(master_list, "@mid", "hosta") && cached_has(master_list, "@mid", "hostb"));
   CHECK(30, "@top resolves two levels down and keeps its own host",
         cached_has(master_list, "@top", "hosta") && cached_has(master_list, "@top", "hostc"));
   CHECK(31, "no group reference leaks into a cache",
         !cached_has(master_list, "@top", "@mid") && !cached_has(master_list, "@mid", "@leaf"));

   {
      const lListElem *hgroup;
      bool all = true;

      for_each_ep(hgroup, master_list) {
         all &= (lGetUlong(hgroup, HGRP_cache_version) != 0) && cache_equals_walk(hgroup, master_list);
      }
      CHECK(32, "every cache equals a fresh walk and carries a version", all);
   }

   /* the nested case: change @leaf, and the containers must follow */
   {
      lListElem *leaf = lGetElemHostRW(master_list, HGRP_name, "@leaf");
      lList *new_hosts = nullptr;

      href_list_add(&new_hosts, nullptr, "hosta");
      href_list_add(&new_hosts, nullptr, "hostd");
      lSetList(leaf, HGRP_host_list, new_hosts);

      /* what the qmaster does in hgroup_success(): the group plus its referencees */
      lList *referencees = nullptr;
      CHECK(33, "the referencees of @leaf are found transitively",
            hgroup_find_all_referencees(leaf, nullptr, master_list, &referencees) &&
            lGetElemHost(referencees, HR_name, "@mid") != nullptr &&
            lGetElemHost(referencees, HR_name, "@top") != nullptr);

      hgroup_update_cache(leaf, nullptr, master_list);
      const lListElem *href;
      for_each_ep(href, referencees) {
         hgroup_update_cache(lGetElemHostRW(master_list, HGRP_name, lGetHost(href, HR_name)),
                             nullptr, master_list);
      }
      lFreeList(&referencees);

      CHECK(34, "the removed host is gone from @top",
            !cached_has(master_list, "@top", "hostb"));
      CHECK(35, "the added host reached @top",
            cached_has(master_list, "@top", "hostd"));
      CHECK(36, "@top still has its own host",
            cached_has(master_list, "@top", "hostc"));
   }

   /*
    * The empty result -- this is what HGRP_cache_version is FOR.
    *
    * cull represents an empty list as nullptr, so "resolved to no hosts" and
    * "never computed" are the same value in HGRP_cached_hosts. Only the version
    * tells them apart, and a consumer that checked the list alone would fall
    * back to the walk forever on a group that legitimately resolves to nothing.
    *
    * Two ways to get there, both exercised: a group with no members at all, and
    * a group whose only member is a group that does not exist. The second is not
    * an error: href_list_find_references() skips a reference it cannot locate
    * and still reports success. Through GDI it cannot happen -- hgroup_mod()
    * rejects unknown references via hgroup_list_exists() -- but a spool file
    * written by hand can produce it, and it must not poison the cache.
    */
   {
      lListElem *empty = add_hgroup(&master_list, "@empty", {});
      lListElem *dangling = add_hgroup(&master_list, "@dangling", {"@does_not_exist"});

      CHECK(37, "a group with no members caches successfully",
            hgroup_update_cache(empty, nullptr, master_list));
      CHECK(38, "a dangling group reference is skipped, not an error",
            hgroup_update_cache(dangling, nullptr, master_list));
      CHECK(39, "both resolve to no hosts at all",
            lGetList(empty, HGRP_cached_hosts) == nullptr &&
            lGetList(dangling, HGRP_cached_hosts) == nullptr);
      CHECK(40, "and both are still marked computed -- empty is not uncomputed",
            lGetUlong(empty, HGRP_cache_version) != 0 &&
            lGetUlong(dangling, HGRP_cache_version) != 0);
   }

   lFreeList(&master_list);
}


// ---------------------------------------------------------------------------
// host_is_referenced() and the reserved groups -- CS-2438
// ---------------------------------------------------------------------------

/*
 * host_is_referenced() decides whether "qconf -de" may delete an exec host. It
 * walks the host groups, and before CS-2438 the admin and submit host lists were
 * NOT host groups, so being an admin host never blocked a deletion.
 *
 * Seeding @admin_hosts with the qmaster host turned that into a deadlock: the
 * deletion is refused because the host is in @admin_hosts, and hgroup_mod()
 * refuses to take the qmaster host out of @admin_hosts, so no command sequence
 * gets the user out of it. Caught by system_tests/clients/qconf, qconf_de_check.
 */
static void
test_host_is_referenced_reserved() {
   printf("\n--- host_is_referenced and reserved groups ---\n");

   lListElem *host = lCreateElem(EH_Type);
   lSetHost(host, EH_name, "hosta");

   /* one group at a time, so a passing case cannot be masked by another group */
   for (int i = 0; i < 3; i++) {
      const char *reserved[] = {ADMIN_HOSTGROUP, SUBMIT_HOSTGROUP, EXEC_HOSTGROUP};
      lList *hgrp_list = nullptr;
      lList *answer_list = nullptr;

      add_hgroup(&hgrp_list, reserved[i], {"hosta"});
      bool referenced = host_is_referenced(host, &answer_list, nullptr, hgrp_list);

      CHECK(41 + i, reserved[i], !referenced);
      lFreeList(&answer_list);
      lFreeList(&hgrp_list);
   }

   /* the skip must not swallow real references */
   {
      lList *hgrp_list = nullptr;
      lList *answer_list = nullptr;

      add_hgroup(&hgrp_list, "@userowned", {"hosta"});
      CHECK(44, "a user group still counts as a reference",
            host_is_referenced(host, &answer_list, nullptr, hgrp_list));
      lFreeList(&answer_list);
      lFreeList(&hgrp_list);
   }

   /* the deliberate boundary: reserved groups are skipped as GROUPS, not as a
    * source of hosts. A user group that pulls in @admin_hosts is still that
    * user's own configuration naming this host, and they can edit it -- so it
    * counts, and there is no deadlock. */
   {
      lList *hgrp_list = nullptr;
      lList *answer_list = nullptr;

      add_hgroup(&hgrp_list, ADMIN_HOSTGROUP, {"hosta"});
      add_hgroup(&hgrp_list, "@wraps_admin", {ADMIN_HOSTGROUP});
      CHECK(45, "a user group referencing a reserved group still counts",
            host_is_referenced(host, &answer_list, nullptr, hgrp_list));
      lFreeList(&answer_list);
      lFreeList(&hgrp_list);
   }

   lFreeElem(&host);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_is_reserved();
   test_is_system_maintained();
   test_constants();

   if (!setup_bootstrap()) {
      printf("FAIL - cannot create the throwaway bootstrap environment\n");
      return 1;
   }
   test_update_cache();
   test_host_is_referenced_reserved();
   teardown_bootstrap();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
