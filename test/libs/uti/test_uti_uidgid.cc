/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2024-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Unit tests for uidgid in `libs/uti`
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <vector>
#include <unistd.h>
#include <pwd.h>

#include "uti/sge_component.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_rmon_macros.h"

#include <sge_log.h>

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
 * @brief print the groups which ocs_get_groups() returns, in the format of id(1)
 *
 * Without a user it prints one line per variant of ocs_get_groups(): "getgroups: " with the
 * groups of the process and "getgrouplist: " with the groups of the user running the program.
 * With a user it prints only the "getgrouplist: " line for that user, as the groups of the
 * process are those of the user running the program.
 * The lines are written by ocs_id2dstring(), e.g.
 * "getgrouplist: uid=1000(joe) gid=1000(joe) groups=1000(joe), 27(sudo)".
 *
 * @param user the user, nullptr for the user running the program
 * @return EXIT_SUCCESS, or EXIT_FAILURE if the user or the groups could not be resolved
 */
static int print_groups(const char *user) {
   DENTER(TOP_LAYER);
   char user_buf[MAX_STRING_SIZE];
   uid_t uid;
   gid_t gid;
   if (user == nullptr) {
      uid = getuid();
      gid = getgid();
      if (sge_uid2user(uid, user_buf, sizeof(user_buf), MAX_NIS_RETRIES) != 0) {
         fprintf(stderr, "cannot resolve the name of uid " uid_t_fmt "\n", uid);
         DRETURN(EXIT_FAILURE);
      }
   } else {
      if (sge_user2uid(user, &uid, &gid, MAX_NIS_RETRIES) != 0) {
         fprintf(stderr, "cannot resolve user %s\n", user);
         DRETURN(EXIT_FAILURE);
      }
      sge_strlcpy(user_buf, user, sizeof(user_buf));
   }
   char group[MAX_STRING_SIZE];
   if (sge_gid2group(gid, group, sizeof(group), MAX_NIS_RETRIES) != 0) {
      snprintf(group, sizeof(group), gid_t_fmt, gid);
   }

   int ret = EXIT_SUCCESS;
   DSTRING_STATIC(dstr, MAX_STRING_SIZE);
   int amount;
   ocs_grp_elem_t *grp_array = nullptr;

   if (user == nullptr) {
      char error_str[MAX_STRING_SIZE];
      if (ocs_get_groups(&amount, &grp_array, error_str, sizeof(error_str))) {
         ocs_id2dstring(&dstr, uid, user_buf, gid, group, amount, grp_array);
         printf("getgroups: %s", sge_dstring_get_string(&dstr));
      } else {
         fprintf(stderr, "ocs_get_groups(1) failed: %s\n", error_str);
         ret = EXIT_FAILURE;
      }
      sge_free(&grp_array);
   }

   DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
   if (ocs_get_groups(user_buf, gid, &amount, &grp_array, &error_dstr)) {
      ocs_id2dstring(&dstr, uid, user_buf, gid, group, amount, grp_array);
      printf("getgrouplist: %s", sge_dstring_get_string(&dstr));
   } else {
      fprintf(stderr, "ocs_get_groups(2) failed: %s\n", sge_dstring_get_string(&error_dstr));
      ret = EXIT_FAILURE;
   }
   sge_free(&grp_array);

   DRETURN(ret);
}

/**
 * @brief run the module test, or print the groups
 *
 * Usage: test_uti_uidgid [-groups [user]]
 * Without arguments it runs the module test. With -groups it prints the groups which
 * ocs_get_groups() returns, see print_groups().
 *
 * @param argc number of arguments
 * @param argv the arguments
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_uidgid");
   if (argc > 1) {
      if (strcmp(argv[1], "-groups") != 0 || argc > 3) {
         fprintf(stderr, "usage: %s [-groups [user]]\n", argv[0]);
         return EXIT_FAILURE;
      }
      return print_groups(argc == 3 ? argv[2] : nullptr);
   }

   int id = 1;

   // suppress ERROR-level sge_log output when lookup functions fail
   component_set_daemonized(true);

   uid_t uid = getuid();
   gid_t gid = getgid();
   char username[MAX_STRING_SIZE] = {};
   char groupname[MAX_STRING_SIZE] = {};

   printf("\n--- buffer sizes ---\n");
   // T01/T02: buffers for getpwnam_r / getgrgid_r must be usable for real entries
   CHECK(id, "get_group_buffer_size() >= 100", get_group_buffer_size() >= 100); id++;
   CHECK(id, "get_pw_buffer_size() >= 100", get_pw_buffer_size() >= 100); id++;

   printf("\n--- uid/user round-trip ---\n");
   // T03: look up the username for the current process uid
   CHECK(id, "sge_uid2user returns 0 for current uid",
         sge_uid2user(uid, username, sizeof(username), MAX_NIS_RETRIES) == 0); id++;
   // T04: resolved name must be non-empty
   CHECK(id, "resolved username is non-empty", strlen(username) > 0); id++;
   // T05/T06: reverse lookup must yield the same uid
   {
      uid_t uid2 = 0;
      gid_t gid2 = 0;
      CHECK(id, "sge_user2uid returns 0 for resolved username",
            sge_user2uid(username, &uid2, &gid2, MAX_NIS_RETRIES) == 0); id++;
      CHECK(id, "sge_user2uid round-trip yields original uid", uid2 == uid); id++;
   }

   printf("\n--- gid/group round-trip ---\n");
   // T07: look up the group name for the current process gid (first overload: dst buffer)
   CHECK(id, "sge_gid2group (first overload) returns 0 for current gid",
         sge_gid2group(gid, groupname, sizeof(groupname), MAX_NIS_RETRIES) == 0); id++;
   // T08: resolved name must be non-empty
   CHECK(id, "resolved group name is non-empty", strlen(groupname) > 0); id++;
   // T09/T10: reverse lookup must yield the same gid
   {
      gid_t gid2 = 0;
      CHECK(id, "sge_group2gid returns 0 for resolved group name",
            sge_group2gid(groupname, &gid2, MAX_NIS_RETRIES) == 0); id++;
      CHECK(id, "sge_group2gid round-trip yields original gid", gid2 == gid); id++;
   }
   // T11/T12: second overload (allocating, cached) — result must match the first overload
   {
      gid_t last_gid = (gid_t)-1;
      std::optional<std::string> group_name2;
      CHECK(id, "sge_gid2group (allocating overload) returns 0",
            sge_gid2group(gid, &last_gid, group_name2, MAX_NIS_RETRIES) == 0); id++;
      CHECK(id, "sge_gid2group (allocating overload) yields same name as first overload",
            group_name2 && *group_name2 == groupname); id++;
   }

   printf("\n--- sge_getpwnam_r ---\n");
   // T13: passwd lookup by username must succeed for the current user
   {
      std::vector<char> buf(get_pw_buffer_size());
      struct passwd pw{};
      struct passwd *res = sge_getpwnam_r(username, &pw, buf.data(), buf.size());
      CHECK(id, "sge_getpwnam_r returns non-null for current user", res != nullptr); id++;
   }

   printf("\n--- sge_is_user_superuser / sge_is_start_user_superuser ---\n");
   // T14: root must always be recognised as superuser
   CHECK(id, "sge_is_user_superuser(\"root\") is true", sge_is_user_superuser("root")); id++;
   // T15: current user is superuser only when running as root
   CHECK(id, "sge_is_user_superuser(username) matches (uid == 0)",
         sge_is_user_superuser(username) == (uid == 0)); id++;
   // T16: sge_is_start_user_superuser() reflects whether the process uid is root
   CHECK(id, "sge_is_start_user_superuser() matches (uid == 0)",
         sge_is_start_user_superuser() == (uid == 0)); id++;

   printf("\n--- error handling ---\n");
   // T17: sge_uid2user with a non-existent uid must return non-zero (retries=0 avoids sleeping)
   {
      char buf[MAX_STRING_SIZE];
      CHECK(id, "sge_uid2user returns non-zero for non-existent uid",
            sge_uid2user((uid_t)999999999, buf, sizeof(buf), 0) != 0); id++;
   }
   // T18: sge_gid2group with a non-existent gid must return non-zero
   {
      char buf[MAX_STRING_SIZE];
      CHECK(id, "sge_gid2group returns non-zero for non-existent gid",
            sge_gid2group((gid_t)999999999, buf, sizeof(buf), 0) != 0); id++;
   }

   printf("\n--- supplementary groups ---\n");
   int amount1 = 0;
   ocs_grp_elem_t *grp_array1 = nullptr;
   {
      char error_str[MAX_STRING_SIZE];
      // T19: process-based group lookup must succeed
      CHECK(id, "ocs_get_groups(by-process) succeeds",
            ocs_get_groups(&amount1, &grp_array1, error_str, sizeof(error_str))); id++;
   }
   // T20: every process belongs to at least one group
   CHECK(id, "group count >= 1", amount1 >= 1); id++;
   {
      DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
      int amount2 = 0;
      ocs_grp_elem_t *grp_array2 = nullptr;
      // T21: name-based lookup must succeed
      CHECK(id, "ocs_get_groups(by-name) succeeds",
            ocs_get_groups(username, gid, &amount2, &grp_array2, &error_dstr)); id++;
      // NOTE: the two variants are intentionally NOT compared for equality. The
      // by-process variant (getgroups) returns the running process's supplementary
      // groups, snapshotted at session start, while the by-name variant
      // (getgrouplist) returns the user's configured groups. These legitimately
      // differ when a group membership changed after the session began, or under
      // su/sudo/containers - so an equality assertion would be environment-fragile.
      sge_free(&grp_array2);
   }

   printf("\n--- ocs_id2dstring ---\n");
   // T22/T23: identity string must be non-empty and contain the uid= prefix
   {
      DSTRING_STATIC(dstr, MAX_STRING_SIZE);
      ocs_id2dstring(&dstr, uid, username, gid, groupname, amount1, grp_array1);
      const char *idstr = sge_dstring_get_string(&dstr);
      CHECK(id, "ocs_id2dstring produces non-empty output",
            idstr != nullptr && strlen(idstr) > 0); id++;
      CHECK(id, "ocs_id2dstring output contains uid= prefix",
            idstr != nullptr && strstr(idstr, "uid=") != nullptr); id++;
   }
   // T24: with no supplementary groups the output contains "NONE"
   {
      DSTRING_STATIC(dstr, MAX_STRING_SIZE);
      ocs_id2dstring(&dstr, uid, username, gid, groupname, 0, nullptr);
      const char *idstr = sge_dstring_get_string(&dstr);
      CHECK(id, "ocs_id2dstring with 0 groups contains NONE",
            idstr != nullptr && strstr(idstr, "NONE") != nullptr); id++;
   }
   sge_free(&grp_array1);

   printf("\n--- buffer ownership ---\n");
   // T25: the cached overload of sge_gid2group keeps a name that is set for the same gid
   {
      gid_t last_gid = gid;
      std::optional<std::string> cached{"cached-marker"};
      CHECK(id, "sge_gid2group (allocating overload) reuses the cached name",
            sge_gid2group(gid, &last_gid, cached, MAX_NIS_RETRIES) == 0 && cached == "cached-marker"); id++;
   }
   // T26: sge_getgrgid_r sizes an empty buffer itself
   {
      std::vector<char> buf;
      struct group gr{};
      struct group *res = sge_getgrgid_r(gid, &gr, buf, MAX_NIS_RETRIES);
      CHECK(id, "sge_getgrgid_r with an empty buffer resolves the current gid",
            res != nullptr && !buf.empty() && strcmp(res->gr_name, groupname) == 0); id++;
   }
   // T27: sge_getgrgid_r grows a buffer that is too small (ERANGE comes as return value, not in errno)
   {
      std::vector<char> buf(1);
      struct group gr{};
      struct group *res = sge_getgrgid_r(gid, &gr, buf, 1);
      CHECK(id, "sge_getgrgid_r grows a one byte buffer and resolves the current gid",
            res != nullptr && buf.size() > 1 && strcmp(res->gr_name, groupname) == 0); id++;
   }

   // T28: ocs_getpwuid_r grows a buffer that is too small, as sge_uid2user relies on
   {
      std::vector<char> buf(1);
      struct passwd pw{};
      struct passwd *res = ocs_getpwuid_r(uid, &pw, buf, 0);
      CHECK(id, "ocs_getpwuid_r grows a one byte buffer and resolves the current uid",
            res != nullptr && buf.size() > 1 && strcmp(res->pw_name, username) == 0); id++;
   }
   // T29: ocs_getgrgid_r grows a buffer that is too small, as the cached sge_gid2group relies on
   {
      std::vector<char> buf(1);
      struct group gr{};
      struct group *res = ocs_getgrgid_r(gid, &gr, buf, 0);
      CHECK(id, "ocs_getgrgid_r grows a one byte buffer and resolves the current gid",
            res != nullptr && buf.size() > 1 && strcmp(res->gr_name, groupname) == 0); id++;
   }
   // T30: ocs_getgrgid_r reports a gid without entry as nullptr
   {
      std::vector<char> buf;
      struct group gr{};
      CHECK(id, "ocs_getgrgid_r returns nullptr for a non-existent gid",
            ocs_getgrgid_r((gid_t)999999999, &gr, buf, 0) == nullptr); id++;
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
