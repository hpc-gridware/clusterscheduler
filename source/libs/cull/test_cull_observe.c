#include <stdio.h>
#include <stdlib.h>

#define __SGE_GDI_LIBRARY_HOME_OBJECT_FILE__
#include "uti/sge_rmon.h"

#include "cull/cull.h"
#include "cull/cull_list.h"

#ifdef OBSERVE
#include "cull/cull_observe.h"

enum {
   TEST_int = 1,
   TEST_host,
   TEST_string,
   TEST_float,
   TEST_double,
   TEST_char,
   TEST_long,
   TEST_ulong,
   TEST_bool,
   TEST_list,
   TEST_object,
   TEST_ref
};

LISTDEF(TEST_Type)
   SGE_INT    (TEST_int,               CULL_DEFAULT)
   SGE_HOST   (TEST_host,              CULL_DEFAULT)
   SGE_STRING (TEST_string,            CULL_HASH)
   SGE_FLOAT  (TEST_float,             CULL_DEFAULT)
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
   NAME("TEST_float")
   NAME("TEST_double")
   NAME("TEST_char")
   NAME("TEST_long")
   NAME("TEST_ulong")
   NAME("TEST_bool")
   NAME("TEST_list")
   NAME("TEST_object")
   NAME("TEST_ref")
NAMEEND   

#define TEST_Size sizeof(TEST_Name) / sizeof(char *)

lNameSpace nmv[] = {
   {1, TEST_Size, TEST_Name},
   {0, 0, NULL}
};
   
static lList *master_list0 = NULL;

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

bool test_scenario1(void) {
   bool ret = true;

   // 1.1 create a master list with 10 elements 
   lObserveStart();
   master_list0 = lCreateList(NULL, TEST_Type);
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

bool test_scenario2(void) {
   bool ret = true;

   // 2.1 
   lObserveStart();
   master_list0 = lCreateList(NULL, TEST_Type);
   lObserveChangeListType(master_list0, true, "MASTER0");

   lListElem *ep0 = lCreateElem(TEST_Type);
   lListElem *ep1 = lCreateElem(TEST_Type);
   lListElem *ep2 = lCreateElem(TEST_Type);
   lAppendElem(master_list0, ep0);
   lAppendElem(master_list0, ep1);
   lAppendElem(master_list0, ep2);

   lList *ep0_lp = lCreateList(NULL, TEST_Type);
   lList *ep1_lp = lCreateList(NULL, TEST_Type);
   lList *ep2_lp = lCreateList(NULL, TEST_Type);
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

   for_each(ep0, master_list0) {
      for_each(ep1, lGetList(ep0, TEST_list)) {
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
      for_each(ep, master_list0) {
         lList *lp = lGetListRW(ep, TEST_list);

         for_each(ep1, lp) {
            u_long32 val = lGetUlong(ep1, TEST_ulong);

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

bool test_scenario3(void) {
   bool ret = true;

   // 3.1 
   lObserveStart();
   master_list0 = lCreateList(NULL, TEST_Type);
   lObserveChangeListType(master_list0, true, "MASTER0");
   for (u_long32 u = 0; u < 3; u++) {
      lListElem *ep = lAddElemUlong(&master_list0, TEST_ulong, u, TEST_Type);
      for (u_long32 v = 0; v < 3; v++) {
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
      for_each(ep, master_list0) {
         lList *lp = lGetListRW(ep, TEST_list);

         for_each(ep1, lp) {
            u_long32 val = lGetUlong(ep1, TEST_ulong);

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

int main(int argc, char *argv[])
{
   bool ret = true;
   DENTER_MAIN(TOP_LAYER, "main")

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


