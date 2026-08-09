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
 * @brief Unit tests for hgroup cache in `daemons/qmaster`
 */

#include <cstdio>
#include <cstring>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_component.h"
#include "uti/sge_hostname.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_ClientBase.h"

#include "basis_types.h"

/*
 * CS-2451 stage 2, against a RUNNING qmaster.
 *
 * Stage 2 only MAINTAINS the resolved host list cache (HGRP_cached_hosts /
 * HGRP_cache_version); nothing reads it yet. That makes it invisible to every
 * existing test: a cache that is never used cannot break anything, and a cache
 * that is silently wrong cannot be noticed either -- until stage 3 starts
 * trusting it. This test closes that window by checking the cache directly.
 *
 * The invariant under test is the only one that matters:
 *
 *     for every host group G:  HGRP_cached_hosts(G) == resolve(G)
 *
 * where resolve() is the tree walk the cache replaces. It is verified for the
 * groups the cluster already has (built at startup) and for groups this test
 * creates and modifies (built incrementally in hgroup_success()), including the
 * case the incremental path is easiest to get wrong: modifying a nested group
 * must refresh the caches of the groups CONTAINING it, not just its own.
 *
 * Run as a manager -- the test creates and deletes its own host groups. They
 * are named "@test_cs2451_*" and are removed again at the end.
 *
 * Not wired into a plain "ctest" run: it needs a cluster. Registered DISABLED,
 * like test_qmaster_hgroup_reserved, and installed into testbin/ so a testsuite
 * check can drive it.
 */

#define GRP_LEAF "@test_cs2451_leaf"
#define GRP_MID  "@test_cs2451_mid"
#define GRP_TOP  "@test_cs2451_top"

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

/** @brief Fetch the whole host group list from the qmaster. */
static lList *
fetch_hgroup_list() {
   lList *lp = nullptr;
   lEnumeration *what = lWhat("%T(ALL)", HGRP_Type);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::GET,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, what);
   lFreeWhat(&what);
   lFreeList(&alp);
   return lp;
}

/** @brief Add or replace one host group. Returns true on success. */
static bool
write_hgroup(ocs::gdi::Command cmd, const char *name, const char *const members[], int num_members) {
   lList *lp = lCreateList("req", HGRP_Type);
   lListElem *ep = lAddElemHost(&lp, HGRP_name, name, HGRP_Type);
   lList *hosts = lCreateList("hosts", HR_Type);

   for (int i = 0; i < num_members; i++) {
      lAddElemHost(&hosts, HR_name, members[i], HR_Type);
   }
   lSetList(ep, HGRP_host_list, hosts);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, cmd,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
   bool ok = !answer_list_has_error(&alp);
   if (!ok) {
      answer_list_output(&alp);
   }
   lFreeList(&alp);
   lFreeList(&lp);
   return ok;
}

/** @brief Delete a host group, ignoring "does not exist". */
static void
drop_hgroup(const char *name) {
   lList *lp = lCreateList("del", HGRP_Type);
   lAddElemHost(&lp, HGRP_name, name, HGRP_Type);

   lList *alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::DEL,
                                          ocs::gdi::SubCommand::NONE, &lp, nullptr, nullptr);
   lFreeList(&alp);
   lFreeList(&lp);
}

/**
 * @brief Does the cache of one group match a fresh tree walk?
 *
 * Compared ELEMENT BY ELEMENT, in order -- not as sets. The cache has to
 * reproduce the walk exactly, quirks included, because stage 3 replaces the walk
 * with it and every caller must see what it saw before.
 *
 * One such quirk makes a set comparison actively wrong here:
 * href_list_find_all_references() merges its recursion result with a plain
 * lAddList() (sge_href.cc), while href_list_add() only suppresses duplicates
 * within one level. So @top containing both hostA and @sub, where @sub also
 * contains hostA, resolves to hostA TWICE -- today, without any cache involved.
 * A "cache must not contain duplicates" rule would fail on such a cluster and
 * blame the cache for the resolver's behaviour.
 */
static bool
cache_matches_walk(const lListElem *hgroup, const lList *master_hgroup_list) {
   lList *walked = nullptr;
   lList *answer_list = nullptr;

   if (!hgroup_find_all_references(hgroup, &answer_list, master_hgroup_list, &walked, nullptr)) {
      answer_list_output(&answer_list);
      lFreeList(&walked);
      return false;
   }
   lFreeList(&answer_list);

   const lList *cached = lGetList(hgroup, HGRP_cached_hosts);
   bool ok = lGetNumberOfElem(cached) == lGetNumberOfElem(walked);

   if (ok) {
      const lListElem *c = lFirst(cached);
      const lListElem *w = lFirst(walked);

      while (c != nullptr && w != nullptr) {
         if (sge_hostcmp(lGetHost(c, HR_name), lGetHost(w, HR_name)) != 0) {
            ok = false;
            break;
         }
         c = lNext(c);
         w = lNext(w);
      }
   }
   if (!ok) {
      printf("      cache/walk mismatch for %s: %d cached, %d walked\n",
             lGetHost(hgroup, HGRP_name), lGetNumberOfElem(cached), lGetNumberOfElem(walked));
   }
   lFreeList(&walked);
   return ok;
}

/** @brief Is host a member of the group's cache? */
static bool
cache_contains(const lList *master_hgroup_list, const char *name, const char *host) {
   const lListElem *hgroup = lGetElemHost(master_hgroup_list, HGRP_name, name);
   return hgroup != nullptr && href_list_locate(lGetList(hgroup, HGRP_cached_hosts), host) != nullptr;
}

// ---------------------------------------------------------------------------

int
main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_qmaster_hgroup_cache");
   lList *alp = nullptr;

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   /*
    * Enrolled as QSTAT, not QCONF, on purpose. get_gdi_executor_ds() sends a GET
    * from qconf-run-by-a-manager to the GLOBAL data store; every other GET goes
    * to the READER store, which the mirror feeds from events. Reading through
    * the mirror is what makes this a test of stage 2: a cache recomputed in
    * hgroup_success() but never shipped as an event would still look perfect
    * from the GLOBAL store, and be missing everywhere a reader thread answers.
    *
    * Writes are unaffected -- manager privileges are checked per user, and a
    * packet containing a non-GET task is routed to a worker regardless of the
    * component name.
    *
    * Automatic sessions make this deterministic: after our own writes the GET
    * waits in ReaderWaitingRequestQueue until the mirror has caught up, so there
    * is nothing to poll for and no sleep to tune.
    */
   if (ocs::gdi::ClientBase::setup_and_enroll(QSTAT, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      printf("\nSKIP - no cluster reachable (this test needs a running qmaster)\n");
      DRETURN(99);
   }

   /* two real, resolvable host names -- the qmaster host is always one of them,
    * the second is any other host of an existing group (a made-up name would be
    * rejected by href_list_resolve_hostnames() before the cache is involved) */
   const char *host_a = component_get_qualified_hostname();
   char host_b[CL_MAXHOSTNAMELEN] = "";

   /* ---- what the startup rebuild produced -------------------------------- */
   {
      lList *hgroup_list = fetch_hgroup_list();
      const lListElem *hgroup;
      int checked = 0;
      bool all_versioned = true;
      bool all_match = true;

      for_each_ep(hgroup, hgroup_list) {
         const lListElem *href;

         all_versioned &= (lGetUlong(hgroup, HGRP_cache_version) != 0);
         all_match &= cache_matches_walk(hgroup, hgroup_list);
         checked++;

         for_each_ep(href, lGetList(hgroup, HGRP_host_list)) {
            const char *h = lGetHost(href, HR_name);

            if (host_b[0] == '\0' && h[0] != '@' && strcmp(h, host_a) != 0) {
               snprintf(host_b, sizeof(host_b), "%s", h);
            }
         }
      }
      CHECK(1, "the cluster has host groups to check", checked > 0);
      CHECK(2, "every group got a cache version at startup", all_versioned);
      CHECK(3, "every startup cache matches a fresh tree walk", all_match);
      lFreeList(&hgroup_list);
   }

   /* ---- a group created now gets a cache -------------------------------- */
   drop_hgroup(GRP_TOP);   /* leftovers from an aborted earlier run */
   drop_hgroup(GRP_MID);
   drop_hgroup(GRP_LEAF);

   {
      const char *leaf_members[] = {host_a};
      CHECK(4, "created " GRP_LEAF, write_hgroup(ocs::gdi::Command::ADD, GRP_LEAF, leaf_members, 1));

      lList *hgroup_list = fetch_hgroup_list();
      const lListElem *leaf = lGetElemHost(hgroup_list, HGRP_name, GRP_LEAF);

      CHECK(5, GRP_LEAF " has a cache version after ADD",
            leaf != nullptr && lGetUlong(leaf, HGRP_cache_version) != 0);
      CHECK(6, GRP_LEAF " caches its member",
            cache_contains(hgroup_list, GRP_LEAF, host_a));
      lFreeList(&hgroup_list);
   }

   /* ---- nesting: the cache of a container resolves through -------------- */
   {
      const char *mid_members[] = {GRP_LEAF};
      const char *top_members[] = {GRP_MID};
      CHECK(7, "created " GRP_MID " containing " GRP_LEAF,
            write_hgroup(ocs::gdi::Command::ADD, GRP_MID, mid_members, 1));
      CHECK(8, "created " GRP_TOP " containing " GRP_MID,
            write_hgroup(ocs::gdi::Command::ADD, GRP_TOP, top_members, 1));

      lList *hgroup_list = fetch_hgroup_list();
      CHECK(9, GRP_TOP " resolves two levels down to the leaf host",
            cache_contains(hgroup_list, GRP_TOP, host_a));

      const lListElem *top = lGetElemHost(hgroup_list, HGRP_name, GRP_TOP);
      CHECK(10, GRP_TOP " caches no group references, only hosts",
            top != nullptr && cache_matches_walk(top, hgroup_list));
      lFreeList(&hgroup_list);
   }

   /* ---- the part the incremental path is easiest to get wrong ------------
    * modifying the LEAF must refresh the caches of MID and TOP as well */
   if (host_b[0] == '\0') {
      printf("ok    [T11] skipped - only one host name available in this cluster\n");
      printf("ok    [T12] skipped - only one host name available in this cluster\n");
      printf("ok    [T13] skipped - only one host name available in this cluster\n");
   } else {
      const char *leaf_members[] = {host_a, host_b};
      CHECK(11, "added a second host to " GRP_LEAF,
            write_hgroup(ocs::gdi::Command::MOD, GRP_LEAF, leaf_members, 2));

      lList *hgroup_list = fetch_hgroup_list();
      CHECK(12, "the new host reached the cache of the direct referencee " GRP_MID,
            cache_contains(hgroup_list, GRP_MID, host_b));
      CHECK(13, "the new host reached the cache of the transitive referencee " GRP_TOP,
            cache_contains(hgroup_list, GRP_TOP, host_b));
      lFreeList(&hgroup_list);
   }

   /* ---- and the whole list is still consistent afterwards ---------------- */
   {
      lList *hgroup_list = fetch_hgroup_list();
      const lListElem *hgroup;
      bool all_match = true;

      for_each_ep(hgroup, hgroup_list) {
         all_match &= cache_matches_walk(hgroup, hgroup_list);
      }
      CHECK(14, "every cache still matches a fresh tree walk", all_match);
      lFreeList(&hgroup_list);
   }

   /* ---- cleanup: top down, a referenced group cannot be deleted ---------- */
   drop_hgroup(GRP_TOP);
   drop_hgroup(GRP_MID);
   drop_hgroup(GRP_LEAF);
   {
      lList *hgroup_list = fetch_hgroup_list();
      CHECK(15, "test host groups removed again",
            lGetElemHost(hgroup_list, HGRP_name, GRP_LEAF) == nullptr &&
            lGetElemHost(hgroup_list, HGRP_name, GRP_MID) == nullptr &&
            lGetElemHost(hgroup_list, HGRP_name, GRP_TOP) == nullptr);
      lFreeList(&hgroup_list);
   }

   ocs::gdi::ClientBase::shutdown();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
