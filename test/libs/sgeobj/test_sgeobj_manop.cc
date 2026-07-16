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

#include "uti/sge_stdlib.h"
#include "uti/sge_uidgid.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_userset.h"

#include "gdi/ocs_gdi_Packet.h"

/*
 * CS-2394: managers and operators are the members of the reserved "manager" and
 * "operator" usersets. manop_is_manager()/manop_is_operator() resolve that
 * membership from the SGE_TYPE_USERSET master list of the active data store.
 *
 * These tests drive the two functions directly: they fill the userset master
 * list, build a gdi::Packet describing the requesting user, and check the
 * resolved permission.
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
// Helpers
// ---------------------------------------------------------------------------

/** @brief Replace the userset master list with a list holding the given usersets. */
static void
set_master_usersets(std::initializer_list<lListElem *> usersets) {
   lList **master_userset_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_USERSET);
   lFreeList(master_userset_list);
   *master_userset_list = lCreateList("usersets", US_Type);
   for (lListElem *us : usersets) {
      lAppendElem(*master_userset_list, us);
   }
}

/** @brief Build a US_ACL userset with the given UE_name entries (user or @group). */
static lListElem *
make_userset(const char *name, std::initializer_list<const char *> members) {
   lListElem *us = lCreateElem(US_Type);
   lSetString(us, US_name, name);
   lSetUlong(us, US_type, US_ACL);
   for (const char *m : members) {
      lAddSubStr(us, UE_name, m, US_entries, UE_Type);
   }
   return us;
}

/**
 * @brief Build a packet describing the request creator.
 *
 * @param user       user name of the request creator
 * @param group      primary group of the request creator
 * @param sup_groups supplementary group names (may be empty)
 */
static ocs::gdi::Packet *
make_packet(const char *user, const char *group, std::initializer_list<const char *> sup_groups) {
   auto *packet = new ocs::gdi::Packet();

   snprintf(packet->user, sizeof(packet->user), "%s", user);
   snprintf(packet->group, sizeof(packet->group), "%s", group);

   packet->amount = static_cast<int>(sup_groups.size());
   if (packet->amount > 0) {
      packet->grp_array = reinterpret_cast<ocs_grp_elem_t *>(sge_malloc(sizeof(ocs_grp_elem_t) * packet->amount));
      int i = 0;
      for (const char *g : sup_groups) {
         packet->grp_array[i].id = static_cast<gid_t>(1000 + i);
         snprintf(packet->grp_array[i].name, sizeof(packet->grp_array[i].name), "%s", g);
         i++;
      }
   } else {
      packet->grp_array = nullptr;
   }
   return packet;
}

static void
free_packet(ocs::gdi::Packet **packet) {
   if (*packet != nullptr) {
      sge_free(&(*packet)->grp_array);
      delete *packet;
      *packet = nullptr;
   }
}

// ---------------------------------------------------------------------------
// manop_is_manager()  [T01–T08]
// ---------------------------------------------------------------------------

static void
test_manop_is_manager() {
   ocs::gdi::Packet *packet;

   // T01: user listed by name in the "manager" userset
   set_master_usersets({make_userset(MANAGER_USERSET, {"root", "alice"})});
   packet = make_packet("alice", "users", {});
   CHECK(1, "user in manager userset -> manager", manop_is_manager(packet));
   free_packet(&packet);

   // T02: user not listed
   packet = make_packet("bob", "users", {});
   CHECK(2, "user not in manager userset -> no manager", !manop_is_manager(packet));
   free_packet(&packet);

   // T03: root is a manager via the seeded entry
   packet = make_packet("root", "root", {});
   CHECK(3, "root in manager userset -> manager", manop_is_manager(packet));
   free_packet(&packet);

   // T04: the reserved userset does not exist -> nobody is a manager.
   // This is the case the legacy fallback used to cover; it is gone (CS-2394).
   set_master_usersets({make_userset("arusers", {"alice"})});
   packet = make_packet("alice", "users", {});
   CHECK(4, "no manager userset -> no manager", !manop_is_manager(packet));
   free_packet(&packet);

   // T05: an empty manager userset grants nothing
   set_master_usersets({make_userset(MANAGER_USERSET, {})});
   packet = make_packet("alice", "users", {});
   CHECK(5, "empty manager userset -> no manager", !manop_is_manager(packet));
   free_packet(&packet);

   // T06: membership via the primary group (@group), see CS-493
   set_master_usersets({make_userset(MANAGER_USERSET, {"root", "@admins"})});
   packet = make_packet("alice", "admins", {});
   CHECK(6, "primary group in manager userset -> manager", manop_is_manager(packet));
   free_packet(&packet);

   // T07: the group name only matches with the @ prefix
   set_master_usersets({make_userset(MANAGER_USERSET, {"admins"})});
   packet = make_packet("alice", "admins", {});
   CHECK(7, "group without @ prefix -> no manager", !manop_is_manager(packet));
   free_packet(&packet);

   // T08: a user name is not matched against @group entries
   set_master_usersets({make_userset(MANAGER_USERSET, {"@alice"})});
   packet = make_packet("alice", "users", {});
   CHECK(8, "@user entry does not match user name -> no manager", !manop_is_manager(packet));
   free_packet(&packet);
}

// ---------------------------------------------------------------------------
// manop_is_manager() with supplementary groups  [T09–T11]
//
// Supplementary group evaluation is only compiled in with the extensions and is
// additionally switched by the enable_sup_grp_eval configuration parameter.
// ---------------------------------------------------------------------------

static void
test_manop_sup_groups() {
   ocs::gdi::Packet *packet;

   set_master_usersets({make_userset(MANAGER_USERSET, {"root", "@staff"})});

   // T09: supplementary group evaluation switched off -> no match
   mconf_set_enable_sup_grp_eval(false);
   packet = make_packet("alice", "users", {"staff", "devs"});
   CHECK(9, "sup grp eval off -> no manager", !manop_is_manager(packet));
   free_packet(&packet);

   mconf_set_enable_sup_grp_eval(true);
#if defined(WITH_EXTENSIONS)
   // T10: supplementary group in the manager userset -> manager
   packet = make_packet("alice", "users", {"staff", "devs"});
   CHECK(10, "sup grp in manager userset -> manager", manop_is_manager(packet));
   free_packet(&packet);

   // T11: none of the supplementary groups is referenced -> no manager
   packet = make_packet("alice", "users", {"devs", "guests"});
   CHECK(11, "sup grp not in manager userset -> no manager", !manop_is_manager(packet));
   free_packet(&packet);
#else
   // Without the extensions mconf_get_enable_sup_grp_eval() is hardcoded to false,
   // so supplementary groups never grant manager permissions.
   packet = make_packet("alice", "users", {"staff", "devs"});
   CHECK(10, "no extensions -> sup grp never grants manager", !manop_is_manager(packet));
   free_packet(&packet);
#endif
   mconf_set_enable_sup_grp_eval(false);
}

// ---------------------------------------------------------------------------
// manop_is_operator()  [T12–T18]
// ---------------------------------------------------------------------------

static void
test_manop_is_operator() {
   ocs::gdi::Packet *packet;

   // T12: user listed in the "operator" userset
   set_master_usersets({make_userset(MANAGER_USERSET, {"root"}),
                        make_userset(OPERATOR_USERSET, {"root", "olivia"})});
   packet = make_packet("olivia", "users", {});
   CHECK(12, "user in operator userset -> operator", manop_is_operator(packet));
   free_packet(&packet);

   // T13: an operator is not a manager
   packet = make_packet("olivia", "users", {});
   CHECK(13, "operator is no manager", !manop_is_manager(packet));
   free_packet(&packet);

   // T14: managers are automatically operators, without being listed there
   packet = make_packet("root", "root", {});
   CHECK(14, "manager is automatically operator", manop_is_operator(packet));
   free_packet(&packet);

   // T15: unrelated user is neither
   packet = make_packet("bob", "users", {});
   CHECK(15, "unrelated user -> no operator", !manop_is_operator(packet));
   free_packet(&packet);

   // T16: no operator userset, but the user is a manager -> still an operator
   set_master_usersets({make_userset(MANAGER_USERSET, {"alice"})});
   packet = make_packet("alice", "users", {});
   CHECK(16, "no operator userset, manager -> operator", manop_is_operator(packet));
   free_packet(&packet);

   // T17: neither userset exists -> nobody is an operator
   set_master_usersets({});
   packet = make_packet("alice", "users", {});
   CHECK(17, "no usersets at all -> no operator", !manop_is_operator(packet));
   free_packet(&packet);

   // T18: membership via primary group in the operator userset
   set_master_usersets({make_userset(MANAGER_USERSET, {"root"}),
                        make_userset(OPERATOR_USERSET, {"@ops"})});
   packet = make_packet("olivia", "ops", {});
   CHECK(18, "primary group in operator userset -> operator", manop_is_operator(packet));
   free_packet(&packet);
}

// ---------------------------------------------------------------------------
// Reserved names are not confused with similar usersets  [T19–T20]
// ---------------------------------------------------------------------------

static void
test_reserved_names() {
   ocs::gdi::Packet *packet;

   // T19: a userset with a similar name does not grant manager permissions
   set_master_usersets({make_userset("managers", {"alice"}),
                        make_userset("mymanager", {"alice"})});
   packet = make_packet("alice", "users", {});
   CHECK(19, "look-alike userset names -> no manager", !manop_is_manager(packet));
   free_packet(&packet);

   // T20: the userset name is matched case sensitively
   set_master_usersets({make_userset("MANAGER", {"alice"})});
   packet = make_packet("alice", "users", {});
   CHECK(20, "userset name is case sensitive -> no manager", !manop_is_manager(packet));
   free_packet(&packet);
}

// ---------------------------------------------------------------------------

int main(int /*argc*/, char * /*argv*/[]) {
   lInit(nmv);

   test_manop_is_manager();
   test_manop_sup_groups();
   test_manop_is_operator();
   test_reserved_names();

   ocs::DataStore::free_master_list(SGE_TYPE_USERSET);

   printf("\n%s — %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   return s_fail == 0 ? 0 : 1;
}
