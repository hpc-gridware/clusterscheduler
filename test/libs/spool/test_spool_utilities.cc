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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_dstring.h"

#include "cull/cull.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_object.h"

#include "spool/sge_spooling_utilities.h"

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
   DENTER_MAIN(TOP_LAYER, "test_spool_utilities");
   component_set_daemonized(true);
   lInit(nmv);

   int id = 1;
   lList *alp = nullptr;

   // --- spool_get_fields_to_spool ---
   printf("\n--- spool_get_fields_to_spool ---\n");

   // T01: valid call returns a non-null field array
   spooling_field *fields = spool_get_fields_to_spool(&alp, QU_Type, &spool_config_instr);
   lFreeList(&alp);
   CHECK(id, "spool_get_fields_to_spool(QU_Type) returns non-null", fields != nullptr); id++;

   // T02: the returned array contains at least one field
   int field_count = 0;
   if (fields != nullptr) {
      while (fields[field_count].nm != NoName) {
         field_count++;
      }
   }
   CHECK(id, "spool_get_fields_to_spool(QU_Type) returns at least one field", field_count > 0); id++;
   fields = spool_free_spooling_fields(fields);

   // T03: null descriptor is rejected, returns null
   spooling_field *null_fields = spool_get_fields_to_spool(&alp, nullptr, &spool_config_instr);
   lFreeList(&alp);
   CHECK(id, "spool_get_fields_to_spool(null descriptor) returns null", null_fields == nullptr); id++;

   // --- field-value round-trip ---
   printf("\n--- field-value round-trip ---\n");

   // T04: for every field in QU_Type, serialize to string and parse back gives identical string
   {
      const lDescr *descr = QU_Type;
      lListElem *elem = lCreateElem(descr);
      lListElem *copy = lCreateElem(descr);
      dstring elem_str = DSTRING_INIT;
      dstring copy_str = DSTRING_INIT;
      int bad_count = 0;

      for (int i = 0; mt_get_type(descr[i].mt) != lEndT; i++) {
         int nm = descr[i].nm;
         sge_dstring_clear(&elem_str);
         sge_dstring_clear(&copy_str);

         const char *value = object_append_field_to_dstring(elem, &alp, &elem_str, nm, '\0');
         lFreeList(&alp);
         const char *reread_value = nullptr;

         if (value != nullptr) {
            object_parse_field_from_string(copy, &alp, nm, value);
            lFreeList(&alp);
            reread_value = object_append_field_to_dstring(copy, &alp, &copy_str, nm, '\0');
            lFreeList(&alp);
         }

         if (sge_strnullcmp(value, reread_value) != 0) {
            printf("   mismatch for field %s: \"%s\" != \"%s\"\n", lNm2Str(nm),
                   value != nullptr ? value : "<null>",
                   reread_value != nullptr ? reread_value : "<null>");
            bad_count++;
         }
      }

      CHECK(id, "all QU_Type fields: serialize+parse round-trip is lossless on a default element",
            bad_count == 0); id++;

      lFreeElem(&elem);
      lFreeElem(&copy);
      sge_dstring_free(&elem_str);
      sge_dstring_free(&copy_str);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
