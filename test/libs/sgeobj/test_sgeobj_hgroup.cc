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

#include "uti/ocs_Bootstrap.h"
#include "uti/ocs_Pattern.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"

#include "sgeobj/sge_answer.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/ocs_Matcher.h"
#include "sgeobj/sge_utility.h"

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

/** @def CHECK
 * @brief Assert one condition and record the result
 *
 * Prints `PASS`/`FAIL` with the test's id and label and counts the failure, so
 * a run reports every problem rather than stopping at the first.
 *
 * @param id the test number, printed as `[Tnn]`
 * @param label what the check is about, printed on failure
 * @param expr the condition that must hold
 */
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
// classify_member() and its neighbours
// ---------------------------------------------------------------------------

/*
 * CS-2681: a host group member may describe a set of hosts instead of naming
 * one. The classification is what tells the three member classes apart, and it
 * lives in the core rather than behind the edition boundary because the host
 * name normaliser needs it and every host value of the system passes through
 * that. What the prefixes mean is interpreted elsewhere; here they are only
 * recognised.
 */
static void
test_classify_member() {
   printf("\n--- classify_member ---\n");

   // T46-T47: a leading '@' decides, and it decides first
   CHECK(46, "@gpu_nodes is a group reference",
         ocs::classify_member("@gpu_nodes") == ocs::MemberClass::GROUP_REFERENCE);
   CHECK(47, "@host:gpu* is a group reference, not a matcher",
         ocs::classify_member("@host:gpu*") == ocs::MemberClass::GROUP_REFERENCE);

   // T48-T50: the prefix comparison is not case-sensitive
   CHECK(48, "host:gpu* is a matcher",
         ocs::classify_member("host:gpu*") == ocs::MemberClass::MATCHER);
   CHECK(49, "HOST:gpu* is a matcher",
         ocs::classify_member("HOST:gpu*") == ocs::MemberClass::MATCHER);
   CHECK(50, "HoSt:gpu* is a matcher",
         ocs::classify_member("HoSt:gpu*") == ocs::MemberClass::MATCHER);

   // T51-T52: the reserved prefixes are matchers too, so that validation can
   // refuse them with a message of their own instead of reporting an unknown host
   CHECK(51, "ip:10.1.0.0-10.1.0.99 is a matcher",
         ocs::classify_member("ip:10.1.0.0-10.1.0.99") == ocs::MemberClass::MATCHER);
   CHECK(52, "ip6:fe80::/10 is a matcher",
         ocs::classify_member("ip6:fe80::/10") == ocs::MemberClass::MATCHER);
   CHECK(53, "ip6: is told apart from ip:",
         ocs::matcher_kind("ip6:fe80::/10") == ocs::MatcherKind::IP6);
   CHECK(54, "ip: is told apart from ip6:",
         ocs::matcher_kind("ip:10.1.0.5") == ocs::MatcherKind::IP);

   // T55-T59: everything else is a literal host name, unchanged
   CHECK(55, "node001 is a literal host name",
         ocs::classify_member("node001") == ocs::MemberClass::LITERAL_HOST);
   CHECK(56, "hostname01 is a literal host name - the prefix is host: and nothing else",
         ocs::classify_member("hostname01") == ocs::MemberClass::LITERAL_HOST);
   CHECK(57, "ip is a literal host name",
         ocs::classify_member("ip") == ocs::MemberClass::LITERAL_HOST);
   CHECK(58, "the empty name is a literal host name",
         ocs::classify_member("") == ocs::MemberClass::LITERAL_HOST);
   CHECK(59, "nullptr is a literal host name",
         ocs::classify_member(nullptr) == ocs::MemberClass::LITERAL_HOST);

   // T60-T63: the payload is the member without its prefix
   CHECK(60, "the payload of host:gpu* is gpu*",
         strcmp(ocs::matcher_payload("host:gpu*"), "gpu*") == 0);
   CHECK(61, "the payload of ip6:fe80::/10 is fe80::/10",
         strcmp(ocs::matcher_payload("ip6:fe80::/10"), "fe80::/10") == 0);
   CHECK(62, "the payload of HOST: is empty, not absent",
         ocs::matcher_payload("HOST:") != nullptr && ocs::matcher_payload("HOST:")[0] == '\0');
   CHECK(63, "a literal host name has no payload",
         ocs::matcher_payload("node001") == nullptr);
}

// ---------------------------------------------------------------------------
// sge_hostmatch_pattern()
// ---------------------------------------------------------------------------

/*
 * CS-2681: the comparison of a stored pattern against a host name. It differs
 * from sge_hostmatch() in that only the host side is normalised - the pattern
 * was brought into canonical form when it was written - and in that both sides
 * are lowered, as host names compare everywhere else in the system.
 *
 * Runs under the throwaway bootstrap environment of this file, that is with
 * ignore_fqdn set and no default domain. The domain rules cannot be varied
 * within one process: ocs::Bootstrap keeps its setters private and reads its
 * configuration once.
 */
static void
test_hostmatch_pattern() {
   printf("\n--- sge_hostmatch_pattern ---\n");

   CHECK(64, "the environment of this group has ignore_fqdn set",
         ocs::Bootstrap::get_ignore_fqdn() && !ocs::Bootstrap::has_default_domain());

   // T65-T71: the fnmatch metacharacters, negation among them
   CHECK(65, "node* matches node001", sge_hostmatch_pattern("node*", "node001") == 0);
   CHECK(66, "node* does not match gpu001", sge_hostmatch_pattern("node*", "gpu001") != 0);
   CHECK(67, "node00? matches node001", sge_hostmatch_pattern("node00?", "node001") == 0);
   CHECK(68, "node[0-9]* matches node001", sge_hostmatch_pattern("node[0-9]*", "node001") == 0);
   CHECK(69, "node[!0-9]* does not match node001",
         sge_hostmatch_pattern("node[!0-9]*", "node001") != 0);
   CHECK(70, "node[!0-9]* matches nodeX01",
         sge_hostmatch_pattern("node[!0-9]*", "nodeX01") == 0);
   CHECK(71, "a missing argument is an error, not a match",
         sge_hostmatch_pattern(nullptr, "node001") == -1 &&
         sge_hostmatch_pattern("node*", nullptr) == -1);

   // T72-T73: case-insensitive on both sides
   CHECK(72, "Node* matches node001", sge_hostmatch_pattern("Node*", "node001") == 0);
   CHECK(73, "node* matches NODE001", sge_hostmatch_pattern("node*", "NODE001") == 0);

   // T74: the candidate is normalised
   CHECK(74, "with ignore_fqdn, node* matches node001.example.com",
         sge_hostmatch_pattern("node*", "node001.example.com") == 0);

   // T75-T76: the pattern is not. sge_hostmatch() sends both sides through
   // sge_hostcpy(), which looks for a dot without knowing what a bracket
   // expression is and cuts this pattern down to "node[0" - the reason this
   // function exists is that the stored pattern must survive the comparison.
   CHECK(75, "with ignore_fqdn, a dot inside a bracket expression survives",
         sge_hostmatch_pattern("node[0.9]x", "node0x") == 0);
   CHECK(76, "sge_hostmatch would have cut the same pattern",
         sge_hostmatch("node[0.9]x", "node0x") != 0);

   // T77: a pattern whose domain no longer matches the domain rules in force
   // fails; it does not widen
   CHECK(77, "with ignore_fqdn, node*.example.com admits nothing",
         sge_hostmatch_pattern("node*.example.com", "node001") != 0);
}

// ---------------------------------------------------------------------------
// The three member classes on the write path
// ---------------------------------------------------------------------------

/*
 * CS-2682. Everything here is core code and answers the same in both editions -
 * the branch structure is identical, only what lies behind ocs::Matcher differs.
 * Whether a matcher can be *stored* is therefore not tested here but in the
 * extension's own test, where the implementation exists.
 */
static void
test_member_classes() {
   printf("\n--- member classes on the write path ---\n");

   // A matcher must reach neither output of the difference. The host side of it
   // is what missing execution host objects are created from, so a matcher
   // landing there would bring an execution host named after it into being -
   // and through @exec_hosts that host would join the candidate set matchers
   // are resolved against.
   {
      lList *before = nullptr;
      lList *after = nullptr;
      lList *add_hosts = nullptr;
      lList *add_groups = nullptr;
      lList *answer_list = nullptr;

      lAddElemHost(&before, HR_name, "node001", HR_Type);
      lAddElemHost(&after, HR_name, "node001", HR_Type);
      lAddElemHost(&after, HR_name, "node002", HR_Type);
      lAddElemHost(&after, HR_name, "@other", HR_Type);
      lAddElemHost(&after, HR_name, "host:gpu*", HR_Type);

      CHECK(78, "the difference of two member lists succeeds",
            href_list_compare(after, &answer_list, before, &add_hosts, &add_groups, nullptr, nullptr));
      CHECK(79, "the added host is found",
            href_list_locate(add_hosts, "node002") != nullptr);
      CHECK(80, "the added group is found",
            href_list_locate(add_groups, "@other") != nullptr);
      CHECK(81, "the matcher is not among the hosts -- no execution host is made of it",
            href_list_locate(add_hosts, "host:gpu*") == nullptr);
      CHECK(82, "and not among the groups either",
            href_list_locate(add_groups, "host:gpu*") == nullptr);

      lFreeList(&answer_list);
      lFreeList(&add_hosts);
      lFreeList(&add_groups);
      lFreeList(&before);
      lFreeList(&after);
   }

   // Two members denoting the same set are collapsed, and the collapse is
   // reported. Dropping one silently is what a security relevant object cannot
   // afford; the report carries STATUS_OK so it cannot fail the client.
   {
      lList *members = nullptr;
      lList *answer_list = nullptr;

      lAddElemHost(&members, HR_name, "host:gpu*", HR_Type);
      lAddElemHost(&members, HR_name, "node001", HR_Type);
      lAddElemHost(&members, HR_name, "HOST:GPU*", HR_Type);

      href_list_make_uniq(members, &answer_list);

      CHECK(83, "the duplicate matcher is gone, case notwithstanding",
            lGetNumberOfElem(members) == 2);
      CHECK(84, "the collapse is reported",
            lGetNumberOfElem(answer_list) == 1);
      CHECK(85, "and the report cannot fail the client",
            lGetUlong(lFirst(answer_list), AN_status) == STATUS_OK);

      lFreeList(&answer_list);
      lFreeList(&members);
   }

   // Two matchers that differ are two members. They would collide if the host
   // name normaliser truncated them at their first dot, as it does a host name.
   {
      lList *members = nullptr;
      lList *answer_list = nullptr;

      lAddElemHost(&members, HR_name, "host:gpu*.a.example.com", HR_Type);
      lAddElemHost(&members, HR_name, "host:gpu*.b.example.com", HR_Type);

      href_list_make_uniq(members, &answer_list);

      CHECK(86, "two different matchers stay two members",
            lGetNumberOfElem(members) == 2);
      CHECK(87, "and each is found under its own name",
            href_list_locate(members, "host:gpu*.a.example.com") != nullptr &&
            href_list_locate(members, "host:gpu*.b.example.com") != nullptr);

      lFreeList(&answer_list);
      lFreeList(&members);
   }

   // A host group member may be a matcher; the name of a host never may.
   {
      lList *answer_list = nullptr;

      CHECK(88, "a host may not be named after a matcher",
            !verify_host_name(&answer_list, "host:gpu*"));
      lFreeList(&answer_list);

      CHECK(89, "an ordinary host name is still accepted",
            verify_host_name(&answer_list, "node001.example.com"));
      lFreeList(&answer_list);
   }
}

// ---------------------------------------------------------------------------
// Enumeration: the three-class walk and its third output
// ---------------------------------------------------------------------------

/*
 * CS-2683. The walk is the second enumeration path -- the one that deliberately
 * does not use the cache. It has to know the third member class itself, expand
 * matchers over the candidate set, and hand the matchers back separately so an
 * interface can show them.
 *
 * Whether a matcher captures anything depends on the edition: the open source
 * build compiles the same branches but its expansion contributes no hosts. Both
 * are asserted, so the boundary is evidenced rather than assumed.
 */

/** @brief Add a host group with the given members to a list
 *
 * @param list the host group list, created if empty
 * @param name the group name
 * @param members the member names, nullptr-terminated
 * @return the new element
 */
static lListElem *
add_group(lList **list, const char *name, const char *const *members) {
   lListElem *hgroup = lAddElemHost(list, HGRP_name, name, HGRP_Type);
   lList *member_list = nullptr;

   for (int i = 0; members[i] != nullptr; i++) {
      lAddElemHost(&member_list, HR_name, members[i], HR_Type);
   }
   lSetList(hgroup, HGRP_host_list, member_list);
   return hgroup;
}

static void
test_walk_three_classes() {
   printf("\n--- the three-class walk ---\n");

   const char *const exec_members[] = {"gpu001", "gpu002", "node001", nullptr};
   const char *const sub_members[] = {"login01", nullptr};
   const char *const top_members[] = {"host:gpu*", "node001", "@sub", nullptr};

   lList *hgroup_list = nullptr;
   add_group(&hgroup_list, EXEC_HOSTGROUP, exec_members);
   add_group(&hgroup_list, "@sub", sub_members);
   lListElem *top = add_group(&hgroup_list, "@top", top_members);

   lList *hosts = nullptr;
   lList *groups = nullptr;
   lList *matchers = nullptr;
   lList *answer_list = nullptr;

   CHECK(90, "the walk succeeds",
         hgroup_find_all_references(top, &answer_list, hgroup_list, &hosts, &groups, &matchers));

   // T91-T92: the classes stay apart -- this is what keeps an execution host
   // from ever being created out of a pattern
   CHECK(91, "the matcher comes back separately",
         lGetNumberOfElem(matchers) == 1 && href_list_locate(matchers, "host:gpu*") != nullptr);
   CHECK(92, "and never as a host or a group",
         href_list_locate(hosts, "host:gpu*") == nullptr &&
         href_list_locate(groups, "host:gpu*") == nullptr);

   // T93: the literal member and the one reached through the reference
   CHECK(93, "the literal members are found, directly and through the reference",
         href_list_locate(hosts, "node001") != nullptr &&
         href_list_locate(hosts, "login01") != nullptr);

   // T94: the expansion, and the edition boundary
#ifdef WITH_EXTENSIONS
   CHECK(94, "the matcher captures the hosts of the candidate set that fit it",
         href_list_locate(hosts, "gpu001") != nullptr &&
         href_list_locate(hosts, "gpu002") != nullptr);
#else
   CHECK(94, "without the extensions the matcher captures nothing",
         href_list_locate(hosts, "gpu001") == nullptr &&
         href_list_locate(hosts, "gpu002") == nullptr);
#endif

   // T95: the walk can be asked for the hosts a configuration *names*, without
   // what its matchers describe. That distinction is what keeps a host a matcher
   // happens to capture from becoming undeletable: a matcher names nothing, and
   // a host leaving the set it describes is what the set is for.
   {
      lList *named = nullptr;

      hgroup_find_all_references(top, &answer_list, hgroup_list, &named, nullptr, nullptr, false);
      CHECK(95, "without expansion only the named hosts come back",
            href_list_locate(named, "node001") != nullptr &&
            href_list_locate(named, "login01") != nullptr &&
            href_list_locate(named, "gpu001") == nullptr &&
            href_list_locate(named, "gpu002") == nullptr);
      lFreeList(&named);
   }

   lFreeList(&answer_list);
   lFreeList(&hosts);
   lFreeList(&groups);
   lFreeList(&matchers);
   lFreeList(&hgroup_list);
}

// ---------------------------------------------------------------------------
// Enumeration: generated trees against an independent oracle
// ---------------------------------------------------------------------------

/*
 * N-W-1 demands that both enumeration paths yield the same set. Today they are
 * the same function, so that equality is structural -- which is exactly why it
 * is worth pinning: the note on the walk explains under what circumstances
 * somebody might be tempted to make the cache the faster of the two.
 *
 * The oracle makes the test more than a tautology. It flattens each tree by a
 * second, obvious implementation written here, so a wrong answer that both
 * paths share is caught as well.
 */

/// Deterministic, so a failure can be reproduced from the test name alone
static unsigned int s_seed = 20260913u;

/** @brief A small deterministic pseudo random number generator
 * @param bound exclusive upper bound
 * @return a value in [0, bound)
 */
static unsigned int
next_random(const unsigned int bound) {
   s_seed = s_seed * 1103515245u + 12345u;
   return (s_seed >> 16) % bound;
}

/** @brief Flatten a group the obvious way, independently of the product
 *
 * @param hgroup_list the groups
 * @param name the group to flatten
 * @param candidates the hosts a matcher is resolved against
 * @param[out] out receives the host names
 * @param depth guards against a cycle the generator should not produce
 */
static void
oracle_flatten(const lList *hgroup_list, const char *name, const lList *candidates,
               lList **out, int depth) {
   if (depth > 32) {
      return;
   }
   const lListElem *hgroup = lGetElemHost(hgroup_list, HGRP_name, name);

   if (hgroup == nullptr) {
      return;
   }
   for_each_ep_lv(member, lGetList(hgroup, HGRP_host_list)) {
      const char *member_name = lGetHost(member, HR_name);

      switch (ocs::classify_member(member_name)) {
         case ocs::MemberClass::GROUP_REFERENCE:
            oracle_flatten(hgroup_list, member_name, candidates, out, depth + 1);
            break;
         case ocs::MemberClass::MATCHER:
#ifdef WITH_EXTENSIONS
            for_each_ep_lv(candidate, candidates) {
               const char *host = lGetHost(candidate, HR_name);

               if (sge_hostmatch_pattern(ocs::matcher_payload(member_name), host) == 0) {
                  href_list_add(out, nullptr, host);
               }
            }
#endif
            break;
         case ocs::MemberClass::LITERAL_HOST:
            href_list_add(out, nullptr, member_name);
            break;
      }
   }
}

/** @brief Do two host reference lists hold the same set of names?
 *
 * @param a one list
 * @param b the other
 * @return true if each holds exactly the names of the other
 */
static bool
same_host_set(const lList *a, const lList *b) {
   if (lGetNumberOfElem(a) != lGetNumberOfElem(b)) {
      return false;
   }
   for_each_ep_lv(href, a) {
      if (href_list_locate(b, lGetHost(href, HR_name)) == nullptr) {
         return false;
      }
   }
   return true;
}

/** @brief Print the first disagreement, so a failure names its own case
 *
 * @param hgroup_list the generated tree
 * @param name the group that disagreed
 * @param expected what the oracle says
 * @param got what the product says
 * @param what which path produced it
 */
static void
dump_mismatch(const lList *hgroup_list, const char *name, const lList *expected,
              const lList *got, const char *what) {
   printf("      mismatch in %s for %s\n", what, name);
   for_each_ep_lv(hgroup, hgroup_list) {
      dstring members = DSTRING_INIT;

      href_list_append_to_dstring(lGetList(hgroup, HGRP_host_list), &members);
      printf("        %-14s = %s\n", lGetHost(hgroup, HGRP_name), sge_dstring_get_string(&members));
      sge_dstring_free(&members);
   }
   dstring a = DSTRING_INIT;
   dstring b = DSTRING_INIT;
   href_list_append_to_dstring(expected, &a);
   href_list_append_to_dstring(got, &b);
   printf("        expected = %s\n        got      = %s\n",
          sge_dstring_get_string(&a), sge_dstring_get_string(&b));
   sge_dstring_free(&a);
   sge_dstring_free(&b);
}

static void
test_generated_trees() {
   printf("\n--- generated group trees ---\n");

   constexpr int TREES = 40;
   constexpr int GROUPS = 8;
   constexpr int CANDIDATES = 12;
   const char *const patterns[] = {"host:h0*", "host:h1*", "host:h?1", "host:x*", "host:*"};

   int cache_mismatches = 0;
   int walk_mismatches = 0;
   int invariant_breaks = 0;
   int unfindable = 0;

   for (int tree = 0; tree < TREES; tree++) {
      lList *hgroup_list = nullptr;
      lList *exec_members = nullptr;
      char buffer[64];

      for (int h = 0; h < CANDIDATES; h++) {
         snprintf(buffer, sizeof(buffer), "h%02d", h);
         lAddElemHost(&exec_members, HR_name, buffer, HR_Type);
      }
      lListElem *exec_group = lAddElemHost(&hgroup_list, HGRP_name, EXEC_HOSTGROUP, HGRP_Type);
      lSetList(exec_group, HGRP_host_list, exec_members);

      /* a group may only reference one already built, so no cycle can arise */
      for (int g = 0; g < GROUPS; g++) {
         lList *members = nullptr;
         const int count = 1 + next_random(4);

         for (int m = 0; m < count; m++) {
            switch (next_random(4)) {
               case 0:
                  if (g > 0) {
                     snprintf(buffer, sizeof(buffer), "@g%d", next_random(g));
                     break;
                  }
                  [[fallthrough]];
               case 1:
                  snprintf(buffer, sizeof(buffer), "%s", patterns[next_random(5)]);
                  break;
               case 2:
                  snprintf(buffer, sizeof(buffer), "h%02d", next_random(CANDIDATES));
                  break;
               default:
                  /* a host that is not an execution host, so it is in the group
                   * but never in the candidate set */
                  snprintf(buffer, sizeof(buffer), "outside%02d", next_random(5));
                  break;
            }
            lAddElemHost(&members, HR_name, buffer, HR_Type);
         }
         snprintf(buffer, sizeof(buffer), "@g%d", g);
         lListElem *hgroup = lAddElemHost(&hgroup_list, HGRP_name, buffer, HGRP_Type);
         lSetList(hgroup, HGRP_host_list, members);
      }

      lList *answer_list = nullptr;
      hgroup_list_update_caches(hgroup_list, &answer_list);
      lFreeList(&answer_list);

      for (int g = 0; g < GROUPS; g++) {
         snprintf(buffer, sizeof(buffer), "@g%d", g);
         const lListElem *hgroup = lGetElemHost(hgroup_list, HGRP_name, buffer);
         lList *expected = nullptr;
         lList *walked = nullptr;

         oracle_flatten(hgroup_list, buffer, lGetList(exec_group, HGRP_host_list), &expected, 0);
         hgroup_find_all_references(hgroup, nullptr, hgroup_list, &walked, nullptr, nullptr);

         if (!same_host_set(lGetList(hgroup, HGRP_cached_hosts), expected)) {
            if (cache_mismatches == 0) {
               dump_mismatch(hgroup_list, buffer, expected, lGetList(hgroup, HGRP_cached_hosts), "cache");
            }
            cache_mismatches++;
         }
         if (!same_host_set(walked, expected)) {
            if (walk_mismatches == 0) {
               dump_mismatch(hgroup_list, buffer, expected, walked, "walk");
            }
            walk_mismatches++;
         }

         /*
          * CS-2684, N-S-3. The two layers answer different questions and must
          * not contradict each other. Over the candidate set, where both can
          * speak about the same host, the agreement has to be exact:
          *
          *   admitted  => resolved   a host a matcher fits is in the candidate
          *                           set here, so the expansion must have taken
          *                           it as well
          *   resolved & not named
          *             => admitted   it got in through a matcher, so the point
          *                           question has to say so too
          *
          * Both directions hold in an open source build as well, where nothing
          * is admitted and nothing is expanded.
          */
         /*
          * Every entry of the cache has to be findable in it. The key is unique
          * and hashed, so a list that ever held two entries under one key cannot
          * be repaired by removing one -- that drops the key from the hash and
          * leaves the survivor present but unfindable, which on this list means
          * the membership test missing a host that is plainly in it. A linear
          * scan would not notice; this is the check that does.
          */
         for_each_ep_lv(cached, lGetList(hgroup, HGRP_cached_hosts)) {
            if (href_list_locate(lGetList(hgroup, HGRP_cached_hosts),
                                 lGetHost(cached, HR_name)) == nullptr) {
               unfindable++;
            }
         }

         lList *named = nullptr;
         hgroup_find_all_references(hgroup, nullptr, hgroup_list, &named, nullptr, nullptr, false);

         for_each_ep_lv(candidate, lGetList(exec_group, HGRP_host_list)) {
            const char *host = lGetHost(candidate, HR_name);
            const bool admitted = ocs::Matcher::admits(hgroup, host, hgroup_list);
            const bool resolved = href_list_locate(lGetList(hgroup, HGRP_cached_hosts), host) != nullptr;
            const bool is_named = href_list_locate(named, host) != nullptr;

            if (admitted && !resolved) {
               invariant_breaks++;
            }
            if (resolved && !is_named && !admitted) {
               invariant_breaks++;
            }
         }
         lFreeList(&named);

         lFreeList(&expected);
         lFreeList(&walked);
      }
      lFreeList(&hgroup_list);
   }

   CHECK(97, "every cache of every generated tree matches the oracle", cache_mismatches == 0);
   CHECK(98, "and so does a fresh walk -- the two paths agree", walk_mismatches == 0);
   CHECK(99, "enumeration and the membership test never contradict each other",
         invariant_breaks == 0);
   CHECK(100, "every host in a cache can be found in it, not merely be present",
         unfindable == 0);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_is_reserved();
   test_is_system_maintained();
   test_constants();
   test_classify_member();

   if (!setup_bootstrap()) {
      printf("FAIL - cannot create the throwaway bootstrap environment\n");
      return 1;
   }
   test_update_cache();
   test_host_is_referenced_reserved();
   test_hostmatch_pattern();
   test_member_classes();
   test_walk_three_classes();
   test_generated_trees();
   teardown_bootstrap();

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
