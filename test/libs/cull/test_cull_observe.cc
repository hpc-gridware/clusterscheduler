/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2023-2024,2026 HPC-Gridware GmbH
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
 * @brief Unit tests for observe in `libs/cull`
 */

#include <cstdio>
#include <cstdlib>

/** @brief Make this translation unit the one that defines the CULL descriptors
 *
 * The generated headers define the descriptors only where this is set, so
 * exactly one file per program must set it. In a test that file is the test
 * itself, which is why the marker appears here rather than in a library.
 */
#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__

#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"

#ifdef OBSERVE
#include "cull/cull_observe.h"

/** @brief A synthetic CULL object type, defined only for these tests
 *
 * One attribute per CULL data type, so the tests can exercise every type
 * without depending on a real object whose layout may change.
 */
enum {
   TEST_int = 1,   ///< an int attribute
   TEST_host,   ///< a host name attribute
   TEST_string,   ///< a string attribute
   TEST_double,   ///< a double attribute
   TEST_char,   ///< a test attribute
   TEST_long,   ///< a long attribute
   TEST_ulong,   ///< an unsigned long attribute
   TEST_bool,   ///< a bool attribute
   TEST_list,   ///< a sublist attribute
   TEST_object,   ///< a sub-object attribute
   TEST_ref   ///< a reference attribute
};

LISTDEF(TEST_Type)
   SGE_INT    (TEST_int,               CULL_DEFAULT)
   SGE_HOST   (TEST_host,              CULL_DEFAULT)
   SGE_STRING (TEST_string,            CULL_HASH)
   SGE_DOUBLE (TEST_double,            CULL_DEFAULT)
   SGE_CHAR   (TEST_char,              CULL_DEFAULT)
   SGE_LONG   (TEST_long,              CULL_DEFAULT)
   SGE_ULONG  (TEST_ulong,             CULL_DEFAULT)
   SGE_BOOL   (TEST_bool,              CULL_DEFAULT)
   SGE_LIST   (TEST_list, TEST_Type,   CULL_DEFAULT)
   SGE_OBJECT (TEST_object, TEST_Type, CULL_DEFAULT)
   SGE_REF    (TEST_ref, TEST_Type,    CULL_DEFAULT)
LISTEND

NAMEDEF(TEST_Name)
   NAME("TEST_int")
   NAME("TEST_host")
   NAME("TEST_string")
   NAME("TEST_double")
   NAME("TEST_char")
   NAME("TEST_long")
   NAME("TEST_ulong")
   NAME("TEST_bool")
   NAME("TEST_list")
   NAME("TEST_object")
   NAME("TEST_ref")
NAMEEND   

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)    ///< number of attributes of the synthetic type

/** @brief The name space registering the synthetic types with CULL */
lNameSpace nmv[] = {
   {1, TEST_Size, TEST_Name},
   {0, 0, nullptr}
};
   
static lList *master_list0 = nullptr;

static bool test_check_info(const char *scenario, long exp_elements, const char *exp_observ) {
   bool ret = true;

   dstring observ = DSTRING_INIT;
   lObserveGetInfoString(&observ);
   if (strcmp(exp_observ, sge_dstring_get_string(&observ)) != 0) {
      fprintf(stderr, "SCENARIO %s FAILED BECAUSE EXPECTED WAS:\n\n%s\nAND THIS IS WHAT WE RECEIVED:\n\n%s\n", 
              scenario, exp_observ, sge_dstring_get_string(&observ));
      ret = false;
   }
   sge_dstring_free(&observ);

   long elements = lObserveGetSize();
   if (exp_elements != elements) {
      fprintf(stderr, "SCENARIO %s FAILED BECAUSE EXPECTED ARE %ld ELEMENTS BUT THERE ARE %ld\n", 
              scenario, exp_elements, elements);
      ret = false;
   }
   if (ret) {
      fprintf(stderr, "SCENARIO %s OK\n", scenario);
   }
   return ret;
}

bool test_scenario1() {
   bool ret = true;

   // 1.1 create a master list with 10 elements 
   lObserveStart();
   master_list0 = lCreateList(nullptr, TEST_Type);
   lObserveChangeListType(master_list0, true, "MASTER0");
   for (int i = 0; i < 10; i++) {
      lAppendElem(master_list0, lCreateElem(TEST_Type));
   }
   ret &= test_check_info("1.1", 11, 
         "MASTER0(RO|RO)\n"
         "CULL(RW|RO) MASTER0(RW|RW)\n");
   lObserveEnd();

   // 1.2 cleanup
   lObserveStart();
   lFreeList(&master_list0);
   ret &= test_check_info("1.2", 0, "MASTER0(RW|RO)\n");
   lObserveEnd();
   return ret;
}

bool test_scenario2() {
   bool ret = true;

   // 2.1 
   lObserveStart();
   master_list0 = lCreateList(nullptr, TEST_Type);
   lObserveChangeListType(master_list0, true, "MASTER0");

   lListElem *ep0 = lCreateElem(TEST_Type);
   lListElem *ep1 = lCreateElem(TEST_Type);
   lListElem *ep2 = lCreateElem(TEST_Type);
   lAppendElem(master_list0, ep0);
   lAppendElem(master_list0, ep1);
   lAppendElem(master_list0, ep2);

   lList *ep0_lp = lCreateList(nullptr, TEST_Type);
   lList *ep1_lp = lCreateList(nullptr, TEST_Type);
   lList *ep2_lp = lCreateList(nullptr, TEST_Type);
   lSetList(ep0, TEST_list, ep0_lp);
   lSetList(ep1, TEST_list, ep1_lp);
   lSetList(ep2, TEST_list, ep2_lp);

   lListElem *ep01 = lCreateElem(TEST_Type);
   lListElem *ep02 = lCreateElem(TEST_Type);
   lListElem *ep03 = lCreateElem(TEST_Type);
   lListElem *ep11 = lCreateElem(TEST_Type);
   lListElem *ep12 = lCreateElem(TEST_Type);
   lListElem *ep13 = lCreateElem(TEST_Type);
   lListElem *ep21 = lCreateElem(TEST_Type);
   lListElem *ep22 = lCreateElem(TEST_Type);
   lListElem *ep23 = lCreateElem(TEST_Type);
   lAppendElem(ep0_lp, ep01);
   lAppendElem(ep0_lp, ep02);
   lAppendElem(ep0_lp, ep03);
   lAppendElem(ep1_lp, ep11);
   lAppendElem(ep1_lp, ep12);
   lAppendElem(ep1_lp, ep13);
   lAppendElem(ep2_lp, ep21);
   lAppendElem(ep2_lp, ep22);
   lAppendElem(ep2_lp, ep23);

   for_each_ep(ep0, master_list0) {
      for_each_ep(ep1, lGetList(ep0, TEST_list)) {
         lSetUlong(ep1, TEST_ulong, 0);
      }
      lSetUlong(ep0, TEST_ulong, 0);
   }

   // 1+3 + 3(1+3) = 16
   ret &= test_check_info("2.1", 16, 
         "MASTER0(RO|RO)\n"
         "CULL(RW|RO) MASTER0(RW|RW)\n");
   lObserveEnd();

   // 2.2
   if (ret) {
      lObserveStart();
      lListElem *ep, *ep1;
      for_each_ep(ep, master_list0) {
         lList *lp = lGetListRW(ep, TEST_list);

         for_each_ep(ep1, lp) {
            uint32_t val = lGetUlong(ep1, TEST_ulong);

            lSetUlong(ep1, TEST_ulong, val + 1);
            break;
         }
         break;
      }
      ret &= test_check_info("2.2", 16, 
            "TEST_ulong(RW|RO) ?(RO|RW) ?(RO|RW) MASTER0(RO|RW)\n");
      lObserveEnd();
   }

   // 2.3
   if (ret) {
      lObserveStart();
      lFreeList(&master_list0);
      ret &= test_check_info("2.3", 0, 
            "MASTER0(RW|RO)\n");
      lObserveEnd();
   }
   return ret;
}

bool test_scenario3() {
   bool ret = true;

   // 3.1 
   lObserveStart();
   master_list0 = lCreateList(nullptr, TEST_Type);
   lObserveChangeListType(master_list0, true, "MASTER0");
   for (uint32_t u = 0; u < 3; u++) {
      lListElem *ep = lAddElemUlong(&master_list0, TEST_ulong, u, TEST_Type);
      for (uint32_t v = 0; v < 3; v++) {
         lAddSubUlong(ep, TEST_ulong, u, TEST_list, TEST_Type);
      }
   }
   // 1+3 + 3(1+3) = 16
   ret &= test_check_info("3.1", 16, 
         "MASTER0(RO|RO)\n" 
         "CULL(RW|RO) MASTER0(RW|RW)\n");
   lObserveEnd();

   // 3.2
   if (ret) {
      lObserveStart();
      lListElem *ep, *ep1;
      for_each_ep(ep, master_list0) {
         lList *lp = lGetListRW(ep, TEST_list);

         for_each_ep(ep1, lp) {
            uint32_t val = lGetUlong(ep1, TEST_ulong);

            lSetUlong(ep1, TEST_ulong, val + 1);
            break;
         }
         break;
      }
      ret &= test_check_info("3.2", 16, 
            "TEST_ulong(RW|RO) ?(RO|RW) ?(RO|RW) MASTER0(RO|RW)\n");
      lObserveEnd();
   }

   // 3.3
   if (ret) {
      lObserveStart();
      lFreeList(&master_list0);
      ret &= test_check_info("3.3", 0, 
            "MASTER0(RW|RO)\n");
      lObserveEnd();
   }
   return ret;
}
#endif

int main(int argc, char *argv[]) {
   bool ret = true;
   DENTER_MAIN(TOP_LAYER, "main");

#ifdef OBSERVE
      lInit(nmv);

      ret &= test_scenario1();
      if (ret) {
         ret &= test_scenario2();
      }
      if (ret) {
         ret &= test_scenario3();
      }
#endif

   DRETURN(ret ? EXIT_SUCCESS : EXIT_FAILURE);
}


