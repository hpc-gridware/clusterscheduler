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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>

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

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_uti_uidgid");
   int ret = check_get_buffer_size();
   if (ret == EXIT_SUCCESS) {
      ret = check_supplementary_groups();
   }
   return ret;
}
