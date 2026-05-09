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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstring>

#include "uti/sge_err.h"
#include "uti/sge_rmon_macros.h"

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
   DENTER_MAIN(TOP_LAYER, "test_uti_err");
   int id = 1;
   sge_err_t err_id;
   char message[1024];

   printf("\n--- initial state ---\n");
   // T01: no error set on entry
   CHECK(id, "initially no error", !sge_err_has_error()); id++;

   printf("\n--- set and get ---\n");
   // T02: has_error turns true after sge_err_set
   sge_err_set(SGE_ERR_PARAMETER, "example error");
   CHECK(id, "has_error true after set", sge_err_has_error()); id++;
   // T03: get returns the correct error id
   sge_err_get(0, &err_id, message, sizeof(message));
   CHECK(id, "get returns correct error id", err_id == SGE_ERR_PARAMETER); id++;
   // T04: get returns the correct error message
   CHECK(id, "get returns correct error message", strcmp(message, "example error") == 0); id++;
   sge_err_clear();

   // T05: set accepts printf-style format arguments
   sge_err_set(SGE_ERR_PARAMETER, "value=%d name=%s", 42, "foo");
   sge_err_get(0, &err_id, message, sizeof(message));
   CHECK(id, "set with format args produces correct message", strcmp(message, "value=42 name=foo") == 0); id++;
   sge_err_clear();

   printf("\n--- all error codes ---\n");
   // T06: SGE_ERR_MEMORY is stored and retrieved correctly
   sge_err_set(SGE_ERR_MEMORY, "out of memory");
   sge_err_get(0, &err_id, message, sizeof(message));
   CHECK(id, "SGE_ERR_MEMORY id is preserved", err_id == SGE_ERR_MEMORY); id++;
   sge_err_clear();
   // T07: SGE_ERR_FILE_EXIST is stored and retrieved correctly
   sge_err_set(SGE_ERR_FILE_EXIST, "file already exists");
   sge_err_get(0, &err_id, message, sizeof(message));
   CHECK(id, "SGE_ERR_FILE_EXIST id is preserved", err_id == SGE_ERR_FILE_EXIST); id++;
   sge_err_clear();

   printf("\n--- overwrite and clear ---\n");
   // T08/T09: second set overwrites the first (only one error slot)
   sge_err_set(SGE_ERR_MEMORY, "first error");
   sge_err_set(SGE_ERR_PARAMETER, "second error");
   sge_err_get(0, &err_id, message, sizeof(message));
   CHECK(id, "second set overwrites first (id)", err_id == SGE_ERR_PARAMETER); id++;
   CHECK(id, "second set overwrites first (message)", strcmp(message, "second error") == 0); id++;
   // T10: has_error returns false after clear
   sge_err_clear();
   CHECK(id, "has_error false after clear", !sge_err_has_error()); id++;
   // T11: get returns SGE_ERR_SUCCESS after clear
   sge_err_get(0, &err_id, message, sizeof(message));
   CHECK(id, "get returns SGE_ERR_SUCCESS after clear", err_id == SGE_ERR_SUCCESS); id++;
   // T12: get returns empty message after clear
   CHECK(id, "get returns empty message after clear", strcmp(message, "") == 0); id++;

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
