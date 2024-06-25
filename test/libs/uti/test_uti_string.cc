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
#include <cstring>

#include "sge_string.h"

bool test_sge_str_move_left() {
   bool ret = true;

   //const char *sge_str_move_left(char *start, char *substr);
   char buffer[] = "123 456 789";
   if (sge_str_move_left(nullptr, nullptr) != nullptr ||
       sge_str_move_left(buffer, nullptr) != nullptr ||
       sge_str_move_left(nullptr, buffer) != nullptr) {
      fprintf(stderr, "sge_str_move_left: should return nullptr on nullptr argument\n");
      ret = false;
   }

   if (sge_str_move_left(buffer, buffer -5) != nullptr) {
      fprintf(stderr, "sge_str_move_left: should return nullptr on substr < start\n");
      ret = false;
   }

   const char *result = sge_str_move_left(buffer, buffer);
   if (result != buffer) {
      fprintf(stderr, "sge_str_move_left: should return buffer on substr == start == buffer\n");
      ret = false;
   }
   if (strcmp(result, "123 456 789") != 0) {
      fprintf(stderr, "sge_str_move_left: substr == start == buffer didn't return unmodified buffer but \"%s\"\n", result);
      ret = false;
   }

   result = sge_str_move_left(buffer, buffer + 4);
   if (result != buffer ||
       strcmp(buffer, "456 789") != 0) {
      fprintf(stderr, "sge_str_move_left: expected \"456 789\" but got \"%s\"\n", result);
      ret = false;
   }

   result = sge_str_move_left(buffer + 2, buffer + 4);
   if (result != buffer + 2) {
      fprintf(stderr, "sge_str_move_left: result should be buffer + 2: %p, but is %p\n",
              (void *)(buffer + 2), (void *)result);
      ret = false;
   }
   if (strcmp(buffer, "45789") != 0) {
      fprintf(stderr, "sge_str_move_left: expected \"45789\" but got \"%s\"\n", result);
      ret = false;
   }

   if (ret) {
      printf("sge_str_move_left: succeeded");
   } else {
      fprintf(stderr, "sge_str_move_left: failed");
   }

   return ret;
}

int main(int argc, char *argv[]) {
   bool ret = true;

   char buffer[10];
   size_t len;

   sge_strlcpy(buffer, "12345678901234567890", sizeof(buffer));
   printf("%2d %s\n", (int) strlen(buffer), buffer);
   if (strlen(buffer) != 9) {
      fprintf(stderr, "strlen after sge_strlcpy should be 9\n");
      ret = false;
   }

   sge_strlcpy(buffer, "1234", sizeof(buffer));
   len = sge_strlcat(buffer, "1234", sizeof(buffer));
   printf("%2zu %s\n", len, buffer);
   if (len != 9 || strcmp(buffer, "12341234") != 0) {
      fprintf(stderr, "sge_strlcat(1) failed\n");
      ret = false;
   }

   len = sge_strlcat(buffer, "1234", sizeof(buffer));
   printf("%2zu %s\n", len, buffer);
   if (len != 13 || strcmp(buffer, "123412341") != 0) {
      fprintf(stderr, "sge_strlcat(1) failed\n");
      ret = false;
   }

   len = sge_strlcat(buffer, "1234", sizeof(buffer));
   printf("%2zu %s\n", len, buffer);
   if (len != 14 || strcmp(buffer, "123412341") != 0) {
      fprintf(stderr, "sge_strlcat(1) failed\n");
      ret = false;
   }

   ret = test_sge_str_move_left();

   return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
