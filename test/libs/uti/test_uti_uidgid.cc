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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <pwd.h>

#include "uti/sge_component.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_rmon_macros.h"

#include <sge_log.h>

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

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_uidgid");
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
      char *group_name2 = nullptr;
      CHECK(id, "sge_gid2group (allocating overload) returns 0",
            sge_gid2group(gid, &last_gid, &group_name2, MAX_NIS_RETRIES) == 0); id++;
      CHECK(id, "sge_gid2group (allocating overload) yields same name as first overload",
            group_name2 != nullptr && strcmp(group_name2, groupname) == 0); id++;
      sge_free(&group_name2);
   }

   printf("\n--- sge_getpwnam_r ---\n");
   // T13: passwd lookup by username must succeed for the current user
   {
      int bufsize = get_pw_buffer_size();
      char *buf = static_cast<char *>(sge_malloc(bufsize));
      SGE_ASSERT(buf != nullptr);
      struct passwd pw{};
      struct passwd *res = sge_getpwnam_r(username, &pw, buf, bufsize);
      CHECK(id, "sge_getpwnam_r returns non-null for current user", res != nullptr); id++;
      sge_free(&buf);
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
      // T22: both variants must report the same number of groups
      CHECK(id, "both ocs_get_groups variants return the same group count",
            amount1 == amount2); id++;
      sge_free(&grp_array2);
   }

   printf("\n--- ocs_id2dstring ---\n");
   // T23/T24: identity string must be non-empty and contain the uid= prefix
   {
      DSTRING_STATIC(dstr, MAX_STRING_SIZE);
      ocs_id2dstring(&dstr, uid, username, gid, groupname, amount1, grp_array1);
      const char *idstr = sge_dstring_get_string(&dstr);
      CHECK(id, "ocs_id2dstring produces non-empty output",
            idstr != nullptr && strlen(idstr) > 0); id++;
      CHECK(id, "ocs_id2dstring output contains uid= prefix",
            idstr != nullptr && strstr(idstr, "uid=") != nullptr); id++;
   }
   // T25: with no supplementary groups the output contains "NONE"
   {
      DSTRING_STATIC(dstr, MAX_STRING_SIZE);
      ocs_id2dstring(&dstr, uid, username, gid, groupname, 0, nullptr);
      const char *idstr = sge_dstring_get_string(&dstr);
      CHECK(id, "ocs_id2dstring with 0 groups contains NONE",
            idstr != nullptr && strstr(idstr, "NONE") != nullptr); id++;
   }
   sge_free(&grp_array1);

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
