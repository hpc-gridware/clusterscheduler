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
 *  Portions of this software are Copyright (c) 2024-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <unistd.h>

#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"

#include <sge_rmon_macros.h>

int check_get_buffer_size() {
   int ret = EXIT_SUCCESS;
   int size;

   size = get_group_buffer_size();
   printf("get_group_buffer_size: %d\n", size);
   if (size < 100) {
      ret = EXIT_FAILURE;
   }

   size = get_pw_buffer_size();
   printf("get_pw_buffer_size: %d\n", size);
   if (size < 100) {
      ret = EXIT_FAILURE;
   }

   return ret;
}

int check_supplementary_groups() {
   printf("\n");
   int ret = EXIT_SUCCESS;
   int amount1;
   ocs_grp_elem_t *grp_array;
   char error_str[MAX_STRING_SIZE];
   if (!ocs_get_groups(&amount1, &grp_array, error_str, sizeof(error_str))) {
      fprintf(stderr, "ocs_get_groups(1) failed: %s\n", error_str);
      ret = EXIT_FAILURE;
   } else {
      for (int i = 0; i < amount1; i++) {
         printf("Group %d:\t" gid_t_fmt "\t%s\n", i, grp_array[i].id, grp_array[i].name);
      }
   }
   sge_free(&grp_array);

   printf("\n");
   DSTRING_STATIC(error_dstr, MAX_STRING_SIZE);
   uid_t uid = getuid();
   gid_t gid = getgid();
   char user[MAX_STRING_SIZE];
   sge_uid2user(uid, user, sizeof(user), MAX_NIS_RETRIES);
   int amount2;
   if (!ocs_get_groups(user, gid, &amount2, &grp_array, &error_dstr)) {
      fprintf(stderr, "ocs_get_groups(2) failed: %s\n", sge_dstring_get_string(&error_dstr));
      ret = EXIT_FAILURE;
   } else {
      if (amount1 != amount2) {
         fprintf(stderr, "ocs_get_groups() variants reported different amount of groups: %d vs. %d\n", amount1, amount2);
         ret = EXIT_FAILURE;
      } else {
         for (int i = 0; i < amount2; i++) {
            printf("Group %d:\t" gid_t_fmt "\t%s\n", i, grp_array[i].id, grp_array[i].name);
         }
      }
   }
   sge_free(&grp_array);

   return ret;
}

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

   int ret = check_get_buffer_size();
   if (ret == EXIT_SUCCESS) {
      ret = check_supplementary_groups();
   }
   return ret;
}
