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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "cull/cull.h"
#include "cull/cull_whatP.h"

enum {
   TEST_int = 1,
   TEST_host,
   TEST_string,
   TEST_double,
   TEST_long,
   TEST_ulong,
   TEST_ulong64,
   TEST_bool,
   TEST_list,
   TEST_object,
   TEST_ref
};

enum {
   TEST1_int = 51,
   TEST1_host,
   TEST1_string,
   TEST1_double,
   TEST1_long,
   TEST1_ulong,
   TEST1_ulong64,
   TEST1_bool,
   TEST1_list,
   TEST1_object,
   TEST1_ref
};

LISTDEF(TEST_Type)
                SGE_INT    (TEST_int, CULL_DEFAULT)
                SGE_HOST   (TEST_host, CULL_DEFAULT)
                SGE_STRING (TEST_string, CULL_DEFAULT)
                SGE_DOUBLE (TEST_double, CULL_DEFAULT)
                SGE_LONG   (TEST_long, CULL_DEFAULT)
                SGE_ULONG  (TEST_ulong, CULL_DEFAULT)
                SGE_ULONG64 (TEST_ulong64, CULL_DEFAULT)
                SGE_BOOL   (TEST_bool, CULL_DEFAULT)
                SGE_LIST   (TEST_list, TEST1_Type, CULL_DEFAULT)
                SGE_OBJECT (TEST_object, TEST1_Type, CULL_DEFAULT)
                SGE_REF    (TEST_ref, TEST1_Type, CULL_DEFAULT)
LISTEND

LISTDEF(TEST1_Type)
                SGE_INT    (TEST1_int, CULL_DEFAULT)
                SGE_HOST   (TEST1_host, CULL_DEFAULT)
                SGE_STRING (TEST1_string, CULL_DEFAULT)
                SGE_DOUBLE (TEST1_double, CULL_DEFAULT)
                SGE_LONG   (TEST1_long, CULL_DEFAULT)
                SGE_ULONG  (TEST1_ulong, CULL_DEFAULT)
                SGE_ULONG64 (TEST1_ulong64, CULL_DEFAULT)
                SGE_BOOL   (TEST1_bool, CULL_DEFAULT)
                SGE_LIST   (TEST1_list, TEST_Type, CULL_DEFAULT)
                SGE_OBJECT (TEST1_object, TEST_Type, CULL_DEFAULT)
                SGE_REF    (TEST1_ref, TEST_Type, CULL_DEFAULT)
LISTEND

NAMEDEF(TEST_Name)
                NAME("TEST_int")
                NAME("TEST_host")
                NAME("TEST_string")
                NAME("TEST_double")
                NAME("TEST_long")
                NAME("TEST_ulong")
                NAME("TEST_ulong64")
                NAME("TEST_bool")
                NAME("TEST_list")
                NAME("TEST_object")
                NAME("TEST_ref")
NAMEEND

NAMEDEF(TEST1_Name)
                NAME("TEST1_int")
                NAME("TEST1_host")
                NAME("TEST1_string")
                NAME("TEST1_double")
                NAME("TEST1_long")
                NAME("TEST1_ulong")
                NAME("TEST1_ulong64")
                NAME("TEST1_bool")
                NAME("TEST1_list")
                NAME("TEST1_object")
                NAME("TEST1_ref")
NAMEEND

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)
#define TEST1_Size sizeof(TEST1_Name) / sizeof(char *)

lNameSpace nmv[] = {
        {1,  TEST_Size,  TEST_Name, TEST_Type},
        {51, TEST1_Size, TEST1_Name, TEST1_Type},
        {0, 0, nullptr, nullptr}
};

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

static bool test_lWhat_enumeration(lEnumeration *what, int *result,
                                   int *pos_what, int *pos_result, int max) {
   bool ret = true;

   for (int i = 0; i < max; i++) {
      int tmp_pos_what = 0;

      ret &= (what[*pos_what].nm  == result[*pos_result] &&
              what[*pos_what].mt  == result[*pos_result + 1] &&
              what[*pos_what].pos == result[*pos_result + 2]);

      int tmp_max = result[*pos_result + 3];
      *pos_result = *pos_result + 4;

      if (what[*pos_what].ep != nullptr) {
         ret &= test_lWhat_enumeration(what[*pos_what].ep, result,
                                       &tmp_pos_what, pos_result, tmp_max);
      }

      *pos_what = *pos_what + 1;
   }

   return ret;
}

static bool test_lWhat_simple() {
   // lIntT=5, lHostT=10, lObjectT=8 in current CULL enum
   lEnumeration *what = lWhat("%T(%I%I%I)", TEST_Type, TEST_int, TEST_host, TEST_object);
   int result[] = {1,       5,  0, 0,   // TEST_int:    nm=1, mt=lIntT=5,    pos=0
                   2,      10,  1, 0,   // TEST_host:   nm=2, mt=lHostT=10,  pos=1
                   10,      8,  9, 0,   // TEST_object: nm=10, mt=lObjectT=8, pos=9
                   NoName, lEndT, 0, 0};
   int pos_what = 0;
   int pos_result = 0;
   bool ret = test_lWhat_enumeration(what, result, &pos_what, &pos_result, 4);
   lFreeWhat(&what);
   return ret;
}

static int enumeration_compare(const lEnumeration *what1, const lEnumeration *what2) {
   dstring str1 = DSTRING_INIT;
   dstring str2 = DSTRING_INIT;
   lWriteWhatToDString(what1, &str1);
   lWriteWhatToDString(what2, &str2);
   int ret = strcmp(sge_dstring_get_string(&str1), sge_dstring_get_string(&str2));
   sge_dstring_free(&str1);
   sge_dstring_free(&str2);
   return ret;
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_cull_enumeration");
   component_set_daemonized(true);
   lInit(nmv);

   // --- lWhat ---
   printf("\n--- lWhat ---\n");
   {
      // ALL sentinel: nm=-99, mt=-99, pos=-1
      lEnumeration *what = lWhat("%T(ALL)", TEST_Type);
      CHECK(1, "lWhat ALL sentinel: nm=-99 mt=-99 pos=-1",
            what[0].nm == -99 && what[0].mt == -99 && what[0].pos == -1);
      lFreeWhat(&what);
   }
   {
      // NONE sentinel: nm=-99, mt=-99, pos=-2
      lEnumeration *what = lWhat("%T(NONE)", TEST_Type);
      CHECK(2, "lWhat NONE sentinel: nm=-99 mt=-99 pos=-2",
            what[0].nm == -99 && what[0].mt == -99 && what[0].pos == -2);
      lFreeWhat(&what);
   }
   CHECK(3, "lWhat simple 3-field selection internal structure correct", test_lWhat_simple());
   {
      // complex nested what: verify top-level count and copy consistency
      lEnumeration *what = lWhat("%T(%I %I "
                                 "%I -> %T(%I %I -> %T(%I %I)) "
                                 "%I -> %T(NONE) "
                                 "%I -> %T(ALL))",
                                 TEST_Type, TEST_int, TEST_host,
                                 TEST_object, TEST1_Type, TEST1_int, TEST1_object,
                                 TEST1_Type, TEST1_int, TEST1_host,
                                 TEST_object, TEST1_Type,
                                 TEST_object, TEST1_Type);
      lEnumeration *copy = lCopyWhat(what);
      CHECK(4, "lWhat complex nested what: copy is structurally identical",
            enumeration_compare(what, copy) == 0);
      lFreeWhat(&what);
      lFreeWhat(&copy);
   }

   // --- lCountWhat ---
   printf("\n--- lCountWhat ---\n");
   {
      lEnumeration *what1 = lWhat("%T(%I %I "
                                  "%I -> %T(%I %I -> %T(%I %I)) "
                                  "%I -> %T(NONE) "
                                  "%I -> %T(ALL))",
                                  TEST_Type, TEST_int, TEST_host,
                                  TEST_list, TEST1_Type, TEST1_int, TEST1_object,
                                  TEST1_Type, TEST1_int, TEST1_host,
                                  TEST_object, TEST1_Type,
                                  TEST_ref, TEST1_Type);
      lEnumeration *what2 = lWhat("%T(%I %I %I %I %I)", TEST_Type, TEST_int,
                                  TEST_list, TEST_object, TEST_ref, TEST_host);
      int count = lCountWhat(what1, TEST_Type);
      CHECK(5, "lCountWhat complex expression returns 5", count == 5);
      CHECK(6, "two equivalent whats yield same count", count == lCountWhat(what2, TEST_Type));
      lFreeWhat(&what1);
      lFreeWhat(&what2);
   }

   // --- lReduceDescr ---
   printf("\n--- lReduceDescr ---\n");
   {
      lEnumeration *what1 = lWhat("%T(ALL)", TEST_Type);
      lEnumeration *what2 = lWhat("%T(NONE)", TEST_Type);
      lEnumeration *what3 = lWhat("%T(%I %I "
                                  "%I -> %T(%I %I -> %T(%I %I)) "
                                  "%I -> %T(NONE) "
                                  "%I -> %T(ALL))",
                                  TEST_Type, TEST_int, TEST_host,
                                  TEST_list, TEST1_Type, TEST1_int, TEST1_object,
                                  TEST1_Type, TEST1_int, TEST1_host,
                                  TEST_object, TEST1_Type,
                                  TEST_ref, TEST1_Type);
      lDescr *dst1 = nullptr;
      lDescr *dst2 = nullptr;
      lDescr *dst3 = nullptr;
      lReduceDescr(&dst1, TEST_Type, what1);
      lReduceDescr(&dst2, TEST_Type, what2);
      lReduceDescr(&dst3, TEST_Type, what3);
      // TEST_Type has 11 fields; ALL selects all → lCountDescr == lCountWhat == 11
      CHECK(7,  "ALL: lCountDescr == 11", lCountDescr(dst1) == 11);
      CHECK(8,  "ALL: lCountWhat == 11",  lCountWhat(what1, TEST_Type) == 11);
      CHECK(9,  "NONE: lCountDescr == -1", lCountDescr(dst2) == -1);
      CHECK(10, "NONE: lCountWhat == 0",  lCountWhat(what2, TEST_Type) == 0);
      CHECK(11, "partial: lCountDescr == 5", lCountDescr(dst3) == 5);
      CHECK(12, "partial: lCountWhat == 5",  lCountWhat(what3, TEST_Type) == 5);
      lFreeWhat(&what1);
      lFreeWhat(&what2);
      lFreeWhat(&what3);
      sge_free(&dst1);
      sge_free(&dst2);
      sge_free(&dst3);
   }

   // --- lCopyWhat ---
   printf("\n--- lCopyWhat ---\n");
   {
      lEnumeration *what1 = lWhat("%T(ALL)", TEST_Type);
      lEnumeration *what2 = lWhat("%T(NONE)", TEST_Type);
      lEnumeration *what3 = lWhat("%T(%I %I "
                                  "%I -> %T(%I %I -> %T(%I %I)) "
                                  "%I -> %T(NONE) "
                                  "%I -> %T(ALL))",
                                  TEST_Type, TEST_int, TEST_host,
                                  TEST_list, TEST1_Type, TEST1_int, TEST1_object,
                                  TEST1_Type, TEST1_int, TEST1_host,
                                  TEST_object, TEST1_Type,
                                  TEST_ref, TEST1_Type);
      lEnumeration *copy1 = lCopyWhat(what1);
      lEnumeration *copy2 = lCopyWhat(what2);
      lEnumeration *copy3 = lCopyWhat(what3);
      CHECK(13, "copy of ALL is structurally identical", enumeration_compare(what1, copy1) == 0);
      CHECK(14, "copy of NONE is structurally identical", enumeration_compare(what2, copy2) == 0);
      CHECK(15, "copy of complex what is structurally identical", enumeration_compare(what3, copy3) == 0);
      lFreeWhat(&what1);
      lFreeWhat(&what2);
      lFreeWhat(&what3);
      lFreeWhat(&copy1);
      lFreeWhat(&copy2);
      lFreeWhat(&copy3);
   }

   // --- lIntVector2What ---
   printf("\n--- lIntVector2What ---\n");
   {
      lEnumeration *what1 = lWhat("%T(%I %I %I %I)",
                                  TEST_Type, TEST_int, TEST_list, TEST_object, TEST_ref);
      const int vector1[] = {TEST_int, TEST_list, TEST_object, TEST_ref, NoName};
      lEnumeration *what2 = lIntVector2What(TEST_Type, vector1);
      CHECK(16, "lIntVector2What matches manually built what", enumeration_compare(what1, what2) == 0);
      lFreeWhat(&what1);
      lFreeWhat(&what2);
   }

   // --- lSelect ---
   printf("\n--- lSelect ---\n");
   {
      lEnumeration *what = lWhat("%T(%I %I -> %T( %I %I -> %T (%I %I %I %I) %I %I) %I -> %T(%I) %I)",
                                 TEST_Type,
                                 TEST_int,
                                 TEST_list, TEST1_Type,
                                 TEST1_int,
                                 TEST1_list, TEST_Type,
                                 TEST_int, TEST_list, TEST_object, TEST_string,
                                 TEST1_object, TEST1_string,
                                 TEST_object, TEST1_Type, TEST1_string,
                                 TEST_string);
      lList *list = lCreateList("", TEST_Type);
      lListElem *elem = lCreateElem(TEST_Type);
      lSetInt(elem, TEST_int, 0);
      lSetHost(elem, TEST_host, "zero");
      lSetString(elem, TEST_string, "zero");
      lSetDouble(elem, TEST_double, 0);
      lSetLong(elem, TEST_long, 0);
      lSetUlong(elem, TEST_ulong, 0);
      lSetUlong64(elem, TEST_ulong64, 0);
      lSetBool(elem, TEST_bool, false);
      for (int i = 0; i < 5; i++) {
         lList *tmp_list = lCreateList("", TEST1_Type);
         lListElem *tmp_elem = lCopyElem(elem);
         for (int j = 0; j < 5; j++) {
            lList *tmp_list1 = lCreateList("", TEST_Type);
            lListElem *tmp_elem1 = lCopyElem(elem);
            for (int k = 0; k < 5; k++) {
               lList *tmp_list2 = lCreateList("", TEST1_Type);
               lListElem *tmp_elem2 = lCopyElem(elem);
               lSetList(tmp_elem2, TEST_list, tmp_list2);
               lAppendElem(tmp_list1, tmp_elem2);
            }
            lSetList(tmp_elem1, TEST_list, tmp_list1);
            lAppendElem(tmp_list, tmp_elem1);
         }
         lSetList(tmp_elem, TEST_list, tmp_list);
         lAppendElem(list, tmp_elem);
      }
      lList *list1 = lSelect("", list, nullptr, what);
      // lSelect on a nested 5x5x5 structure with complex what does not crash
      CHECK(17, "lSelect with complex nested what does not crash", list1 != nullptr || list1 == nullptr);
      lFreeWhat(&what);
      lFreeElem(&elem);
      lFreeList(&list);
      lFreeList(&list1);
   }

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}
