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

/*
 * Unit tests for qref_list_host_rejected() - CS-2450.
 *
 * qref_list_host_rejected() answers "does this -q / RQS host reference cover
 * that host?". The reference side may carry wildcards; the members of a host
 * group may not. Before CS-2450 the members were matched as expressions too,
 * so a host group whose *name* contained a pattern character resolved to a
 * different host set here than via hgroup_find_all_references(), which is what
 * "qconf -shgrp_resolved" uses.
 *
 * These scenarios need no running cluster - the function is pure over an
 * HGRP_Type list.
 */

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>

#include "basis_types.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "sgeobj/sge_qref.h"

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

static int s_fail = 0;

static char s_sge_root[128];

/*
 * qref_list_host_rejected() compares host names with sge_hostcmp(), which
 * consults ocs::Bootstrap for ignore_fqdn / default_domain. Bootstrap loads
 * $SGE_ROOT/$SGE_CELL/common/bootstrap on first access and calls sge_exit(1)
 * if it cannot, so a throwaway one is created here. This keeps the test
 * independent of an installed cluster - the values only have to parse, the
 * scenarios below use short host names that need no normalisation.
 */
static bool setup_bootstrap() {
   snprintf(s_sge_root, sizeof(s_sge_root), "/tmp/test_sgeobj_qref_%ld", (long) getpid());

   char path[256];
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

#define CHECK(id, label, expr) \
   do { \
      if (!(expr)) { \
         printf("FAIL  [T%02d] %s\n", (id), (label)); \
         ++s_fail; \
      } else { \
         printf("ok    [T%02d] %s\n", (id), (label)); \
      } \
   } while (0)

/* "rejected == false" reads badly in the scenarios below */
#define COVERS(href, host, list)     (!qref_list_host_rejected((href), (host), (list)))
#define EXCLUDES(href, host, list)   (qref_list_host_rejected((href), (host), (list)))

/*
 * Appends a host group to list. Name validation is switched off on purpose:
 * since CS-2450 a name like "@gpustar*" can no longer be added through GDI,
 * but it can still arrive from a spool file written by an older version, and
 * that is exactly the configuration these scenarios are about.
 */
static void add_hgroup(lList **list, const char *name, const char *m1, const char *m2) {
   lList *members = nullptr;

   if (m1 != nullptr) {
      href_list_add(&members, nullptr, m1);
   }
   if (m2 != nullptr) {
      href_list_add(&members, nullptr, m2);
   }
   if (*list == nullptr) {
      *list = lCreateList("hgroup_list", HGRP_Type);
   }
   lAppendElem(*list, hgroup_create(nullptr, name, members, false));
}

// ---------------------------------------------------------------------------
// qref_list_host_rejected  [T01-T14]
//
// Fixture, the reproduction from CS-2450:
//
//   group_name @gpustar*     hostlist v04706
//   group_name @gpustar1     hostlist v04707
//   group_name @cacheparent  hostlist @gpustar*
//   group_name @nested       hostlist @cacheparent
//
// "@cacheparent" references the host group literally named "@gpustar*". Its
// only member host is therefore v04706. v04707 belongs to "@gpustar1" and must
// stay out of reach - it is reachable only if the member entry "@gpustar*" is
// interpreted as a pattern.
//
// Scenarios T01-T03 cover direct host group references.
// Scenarios T04-T06 cover the member resolution that CS-2450 is about.
// Scenarios T07-T08 cover nesting one level deeper.
// Scenarios T09-T11 cover the reference side, which keeps wildcard semantics.
// Scenarios T12-T14 cover plain host references and unknown names.
// ---------------------------------------------------------------------------

static void test_qref_list_host_rejected() {
   printf("\n--- qref_list_host_rejected ---\n");

   lList *hgroups = nullptr;
   add_hgroup(&hgroups, "@gpustar*",    "v04706", nullptr);
   add_hgroup(&hgroups, "@gpustar1",    "v04707", nullptr);
   add_hgroup(&hgroups, "@cacheparent", "@gpustar*", nullptr);
   add_hgroup(&hgroups, "@nested",      "@cacheparent", nullptr);

   // T01: a host group covers its own member
   CHECK(1, "@gpustar* covers v04706", COVERS("@gpustar*", "v04706", hgroups));

   // T02: "@gpustar*" is passed as the *reference*, where wildcards are legal
   //      and intended. It is an expression, so it matches the group names
   //      "@gpustar*" and "@gpustar1" alike and reaches both their members.
   //      This asymmetry is the point: patterns apply to the reference, never
   //      to the members (contrast T05).
   CHECK(2, "reference @gpustar* covers v04707 (pattern on reference side)", COVERS("@gpustar*", "v04707", hgroups));

   // T03: a host that is in no group at all is excluded
   CHECK(3, "@gpustar* excludes v04708", EXCLUDES("@gpustar*", "v04708", hgroups));

   // T04: the member "@gpustar*" resolves to the group of that literal name
   CHECK(4, "@cacheparent covers v04706", COVERS("@cacheparent", "v04706", hgroups));

   // T05: CS-2450 - the member must NOT be matched as a pattern. Before the fix
   //      "@gpustar*" matched "@gpustar1" too and this returned "covers".
   CHECK(5, "@cacheparent excludes v04707", EXCLUDES("@cacheparent", "v04707", hgroups));

   // T06: control - an unrelated host stays excluded either way
   CHECK(6, "@cacheparent excludes v04708", EXCLUDES("@cacheparent", "v04708", hgroups));

   // T07: nesting is followed across two levels
   CHECK(7, "@nested covers v04706", COVERS("@nested", "v04706", hgroups));

   // T08: ... and the pattern member stays exact at depth
   CHECK(8, "@nested excludes v04707", EXCLUDES("@nested", "v04707", hgroups));

   // T09: the reference side keeps expression semantics - "@gpustar?" matches
   //      the group name "@gpustar1" and thus reaches its member
   CHECK(9, "reference @gpustar? covers v04707", COVERS("@gpustar?", "v04707", hgroups));

   // T10: the same reference also matches the literally named group "@gpustar*"
   CHECK(10, "reference @gpustar? covers v04706", COVERS("@gpustar?", "v04706", hgroups));

   // T11: an expression on the reference side that matches nothing
   CHECK(11, "reference @nomatch* excludes v04706", EXCLUDES("@nomatch*", "v04706", hgroups));

   // T12: a plain host reference matches the host itself
   CHECK(12, "host reference v04706 covers v04706", COVERS("v04706", "v04706", hgroups));

   // T13: a wildcard host reference keeps working
   CHECK(13, "host reference v0470* covers v04707", COVERS("v0470*", "v04707", hgroups));

   // T14: a host group that does not exist covers nothing
   CHECK(14, "unknown @doesnotexist excludes v04706", EXCLUDES("@doesnotexist", "v04706", hgroups));

   lFreeList(&hgroups);
}


// ---------------------------------------------------------------------------
// The cache must answer exactly what the walk answers  [T15-T31]  -- CS-2451
//
// Stage 3 replaces the member walk inside qref_hgroup_rejected() with a lookup
// in HGRP_cached_hosts. This is the check that the replacement is faithful.
//
// It deliberately does NOT restate the expected answers: it runs the same
// (reference, host) pairs against two identical fixtures, one cached and one
// not, and requires the two to AGREE. Restating expectations would let the two
// implementations drift apart while both tests still pass; agreement cannot.
//
// The pairs are those of T01-T14 above, so the CS-2450 trap is included: the
// fixture contains a group literally named "@gpustar*", and the cache must
// resolve the member entry "@gpustar*" to that group alone and never as a
// pattern. hgroup_find_all_references() uses hgroup_list_locate() for exactly
// that reason, but this is where it gets proven for the cached path.
// ---------------------------------------------------------------------------

static void build_fixture(lList **hgroups) {
   add_hgroup(hgroups, "@gpustar*",    "v04706", nullptr);
   add_hgroup(hgroups, "@gpustar1",    "v04707", nullptr);
   add_hgroup(hgroups, "@cacheparent", "@gpustar*", nullptr);
   add_hgroup(hgroups, "@nested",      "@cacheparent", nullptr);
}

static void test_qref_cache_agrees_with_walk() {
   printf("\n--- cached vs uncached agreement ---\n");

   struct { const char *reference; const char *host; } pairs[] = {
      {"@gpustar*", "v04706"}, {"@gpustar*", "v04707"}, {"@gpustar*", "v04708"},
      {"@cacheparent", "v04706"}, {"@cacheparent", "v04707"}, {"@cacheparent", "v04708"},
      {"@nested", "v04706"}, {"@nested", "v04707"}, {"@nested", "v04708"},
      {"@gpustar?", "v04707"}, {"@gpustar?", "v04706"}, {"@nomatch*", "v04706"},
      {"v04706", "v04706"}, {"v0470*", "v04707"}, {"@doesnotexist", "v04706"},
   };
   const int num_pairs = sizeof(pairs) / sizeof(pairs[0]);

   lList *walked = nullptr;
   lList *cached = nullptr;
   build_fixture(&walked);
   build_fixture(&cached);

   hgroup_list_update_caches(cached, nullptr);

   /* Without this the whole comparison could pass with no cache anywhere -- two
    * runs of the same walk always agree. */
   {
      const lListElem *hgroup;
      bool all = (lGetNumberOfElem(cached) > 0);

      for_each_ep(hgroup, cached) {
         all &= hgroup_has_host_cache(hgroup);
      }
      CHECK(15, "the cached fixture really carries caches", all);

      bool none = true;
      for_each_ep(hgroup, walked) {
         none &= !hgroup_has_host_cache(hgroup);
      }
      CHECK(16, "the uncached fixture really carries none", none);
   }

   for (int i = 0; i < num_pairs; i++) {
      const bool a = qref_list_host_rejected(pairs[i].reference, pairs[i].host, walked);
      const bool b = qref_list_host_rejected(pairs[i].reference, pairs[i].host, cached);
      char label[128];

      snprintf(label, sizeof(label), "%s vs %s: same answer (%s)",
               pairs[i].reference, pairs[i].host, a ? "excluded" : "covered");
      CHECK(17 + i, label, a == b);
   }

   lFreeList(&walked);
   lFreeList(&cached);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   if (!setup_bootstrap()) {
      printf("FAIL - cannot create the throwaway bootstrap environment\n");
      return 1;
   }

   test_qref_list_host_rejected();
   test_qref_cache_agrees_with_walk();

   teardown_bootstrap();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
