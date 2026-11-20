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
 * @brief Unit tests for hgroup reserved in `daemons/qmaster`
 */

#include <cstdio>
#include <cstring>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_href.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_HR.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_ClientBase.h"

#include "basis_types.h"
#include "cull/cull_test_api.h"

/*
 * CS-2438 chunk 1, against a RUNNING qmaster.
 *
 * The unit test test_sgeobj_hgroup covers the two predicates in isolation. What
 * it cannot cover is the qmaster side: hgroup_del()'s refusal, the @exec_hosts
 * write rejection and the end-state check keeping the qmaster host in
 * @admin_hosts. Those need a data store, a GDI packet and the spooling context,
 * i.e. a live cluster -- so they are exercised here through real GDI requests,
 * the same way qconf would issue them.
 *
 * Every write attempted here is expected to FAIL. The test passes when the
 * qmaster refuses, and -- just as important -- when the object is unchanged
 * afterwards: a guard that reports an error but has already modified the master
 * list would be worse than no guard.
 *
 * Run as a manager (the testsuite user is one). The refusals under test are not
 * permission checks; a manager must be refused too.
 *
 * Not wired into a plain "ctest" run: it needs a cluster. Registered DISABLED,
 * like test_mir_basic and test_spool, and installed into testbin/ so a testsuite
 * check can drive it.
 */

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

/**
 * @brief Fetch one host group from the qmaster
 * @param name the group name
 * @return a copy of the group, or an empty handle if it is absent
 */
static ocs::tom::ElementPtr<ocs::tom::HGRP>
fetch_hgroup(const char *name) {
   ocs::tom::ListPtr<ocs::tom::HGRP> lp;
   const auto where = ocs::tom::Where<ocs::tom::HGRP>::from_typed(lWhere("%T(%I==%s)", ocs::tom::HGRP::descriptor.e,
                                                                         HGRP_name, name));

   auto alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::GET,
                                        ocs::gdi::SubCommand::NONE, lp, where, ocs::tom::what_all<ocs::tom::HGRP>());
   alp.reset();

   /* select by name rather than lFirst(): a GET may return more than asked for */
   ocs::tom::ConstElement<ocs::tom::HGRP> found = ocs::tom::find_first<HGRP_name>(*lp, name);
   return found ? found.clone() : ocs::tom::ElementPtr<ocs::tom::HGRP>();
}

/**
 * @brief Try to delete a host group
 * @param name the group name
 * @return true if the qmaster REFUSED
 */
static bool
delete_refused(const char *name) {
   auto lp = ocs::tom::ListPtr<ocs::tom::HGRP>::create("del");
   lp->emplace_back<HGRP_name>(ocs::tom::Host(name));

   auto alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::DEL,
                                        ocs::gdi::SubCommand::NONE, lp);
   bool refused = answer_list_has_error(&alp);
   alp.reset();
   return refused;
}

/**
 * @brief Try to replace a host group's host list
 * @param name the group name
 * @param new_host_list the new host list, taken over
 * @return true if the qmaster REFUSED
 */
static bool
modify_refused(const char *name, ocs::tom::ListPtr<ocs::tom::HR> &&new_host_list) {
   auto hosts = std::move(new_host_list);
   auto lp = ocs::tom::ListPtr<ocs::tom::HGRP>::create("mod");
   ocs::tom::Element<ocs::tom::HGRP> ep = lp->emplace_back<HGRP_name>(ocs::tom::Host(name));
   ocs::tom::set<HGRP_host_list>(ep, std::move(hosts));

   auto alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::HGRP_LIST, ocs::gdi::Command::MOD,
                                        ocs::gdi::SubCommand::NONE, lp);
   bool refused = answer_list_has_error(&alp);
   alp.reset();
   return refused;
}

/**
 * @brief Number of direct entries of a group
 * @param name the group name
 * @return the number of entries, -1 if the group is absent
 */
static int
member_count(const char *name) {
   auto hgrp = fetch_hgroup(name);
   if (hgrp == nullptr) {
      return -1;
   }
   return lGetNumberOfElem(ocs::tom::get<HGRP_host_list>(hgrp));
}

// ---------------------------------------------------------------------------

int
main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_qmaster_hgroup_reserved");
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QCONF, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      printf("\nSKIP - no cluster reachable (this test needs a running qmaster)\n");
      DRETURN(99);
   }

   /* ---- seeding: setup_qmaster.cc must have created all three ------------ */
   CHECK(1, "@admin_hosts exists after startup", member_count(ADMIN_HOSTGROUP) >= 0);
   CHECK(2, "@submit_hosts exists after startup", member_count(SUBMIT_HOSTGROUP) >= 0);
   CHECK(3, "@exec_hosts exists after startup", member_count(EXEC_HOSTGROUP) >= 0);

   /* the qmaster host is an admin host by definition */
   {
      auto hgrp = fetch_hgroup(ADMIN_HOSTGROUP);
      const char *qmaster_host = component_get_qualified_hostname();
      CHECK(4, "qmaster host is a direct member of @admin_hosts",
            hgrp != nullptr &&
            href_list_locate(ocs::tom::get<HGRP_host_list>(hgrp), qmaster_host) != nullptr);
      hgrp.reset();
   }

   /* ---- none of the reserved groups may be deleted ----------------------- */
   {
      const int before_admin = member_count(ADMIN_HOSTGROUP);
      const int before_submit = member_count(SUBMIT_HOSTGROUP);
      const int before_exec = member_count(EXEC_HOSTGROUP);

      CHECK(5, "delete of @admin_hosts refused", delete_refused(ADMIN_HOSTGROUP));
      CHECK(6, "delete of @submit_hosts refused", delete_refused(SUBMIT_HOSTGROUP));
      CHECK(7, "delete of @exec_hosts refused", delete_refused(EXEC_HOSTGROUP));

      /* the refusal must leave the objects untouched */
      CHECK(8, "@admin_hosts survived the delete attempt", member_count(ADMIN_HOSTGROUP) == before_admin);
      CHECK(9, "@submit_hosts survived the delete attempt", member_count(SUBMIT_HOSTGROUP) == before_submit);
      CHECK(10, "@exec_hosts survived the delete attempt", member_count(EXEC_HOSTGROUP) == before_exec);
   }

   /* ---- @exec_hosts is read-only, even for a manager --------------------- */
   {
      const int before = member_count(EXEC_HOSTGROUP);
      auto hosts = ocs::tom::ListPtr<ocs::tom::HR>::create("hosts");
      hosts->emplace_back<HR_name>(ocs::tom::Host(component_get_qualified_hostname()));

      CHECK(11, "modify of @exec_hosts refused", modify_refused(EXEC_HOSTGROUP, std::move(hosts)));
      CHECK(12, "@exec_hosts unchanged after the modify attempt", member_count(EXEC_HOSTGROUP) == before);
   }

   /* ---- the qmaster host may not leave @admin_hosts ---------------------- */
   {
      const char *qmaster_host = component_get_qualified_hostname();
      const int before = member_count(ADMIN_HOSTGROUP);

      /* an empty host list would drop the qmaster host -> end-state check bites */
      auto empty = ocs::tom::ListPtr<ocs::tom::HR>::create("hosts");
      CHECK(13, "emptying @admin_hosts refused", modify_refused(ADMIN_HOSTGROUP, std::move(empty)));

      /* a list of other hosts, still without the qmaster host, must fail too --
       * the guard is about the end state, not about the list being empty */
      auto others = ocs::tom::ListPtr<ocs::tom::HR>::create("hosts");
      others->emplace_back<HR_name>(ocs::tom::Host("an-unrelated-host"));
      CHECK(14, "@admin_hosts without the qmaster host refused",
            modify_refused(ADMIN_HOSTGROUP, std::move(others)));

      CHECK(15, "@admin_hosts unchanged after both attempts", member_count(ADMIN_HOSTGROUP) == before);

      auto hgrp = fetch_hgroup(ADMIN_HOSTGROUP);
      CHECK(16, "qmaster host still a member",
            hgrp != nullptr &&
            href_list_locate(ocs::tom::get<HGRP_host_list>(hgrp), qmaster_host) != nullptr);
      hgrp.reset();
   }

   /* ---- an ordinary group is NOT protected ------------------------------- */
   {
      /* @allhosts is created by every installation; deleting it is refused only
       * because queues reference it, not because it is reserved. Assert the
       * distinction the other way round: it must not be reported as reserved. */
      CHECK(17, "@allhosts is not treated as reserved", !hgroup_is_reserved("@allhosts"));
   }

   ocs::gdi::ClientBase::shutdown();

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
