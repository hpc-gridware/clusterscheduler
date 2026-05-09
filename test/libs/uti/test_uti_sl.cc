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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <fnmatch.h>
#include <cstdio>
#include <cstdlib>

#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_sl.h"
#include "uti/sge_stdio.h"

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

/* following is used in test_mt_support() */
#define TEST_SL_MAX_THREADS 10

struct _test_sl_thread_t {
   pthread_mutex_t mutex;
   sge_sl_list_t *list;
   bool do_terminate;
};

typedef struct _test_sl_thread_t test_sl_thread_t;

/* used in test to check the destroy sequence */
dstring test_string = DSTRING_INIT;

bool
test_destroy_test(void **data) {
   sge_dstring_append(&test_string, *((char **) data));
   return true;
}

int
test_compare_ulong(const void *data1, const void *data2) {
   int ret = 0;
   uint32_t number1 = *(uint32_t *) data1;
   uint32_t number2 = *(uint32_t *) data2;

   if (number1 < number2) {
      ret = -1;
   } else if (number1 > number2) {
      ret = +1;
   } else {
      ret = 0;
   }
   return ret;
}

int
test_compare(const void *data1, const void *data2) {
   int ret = 0;

   if (data1 != nullptr && data2 != nullptr) {
      ret = strcmp(*(char **) data1, *(char **) data2);
   }
   return ret;
}

int
test_compare_first_char(const void *data1, const void *data2) {
   int ret = 0;

   if (data1 != nullptr && data2 != nullptr) {
      ret = fnmatch(*(char **) data1, *(char **) data2, 0);
   }
   return ret;
}

bool
test_sequence(sge_sl_list_t *list, bool forward, const char *expected,
              uint32_t elems, const char *function) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (ret) {
      sge_sl_elem_t *next;
      sge_sl_elem_t *current;

      ret &= sge_sl_lock(list);

      /* create string from stored characters */
      next = nullptr;
      ret &= sge_sl_elem_next(list, &next, forward ? SGE_SL_FORWARD : SGE_SL_BACKWARD);
      sge_dstring_sprintf(&test_string, "");
      while (ret && (current = next) != nullptr) {
         ret &= sge_sl_elem_next(list, &next, forward ? SGE_SL_FORWARD : SGE_SL_BACKWARD);
         sge_dstring_append(&test_string, (char *) sge_sl_elem_data(current));
      }

      /* test sequence of characters in string */
      if (strcmp(expected, sge_dstring_get_string(&test_string)) != 0) {
         fprintf(stderr, "Error: Expected %s-sequence \"%s\" "
                         "but it was \"%s\" in function %s()\n", forward ? "forward" : "backward",
                 expected, sge_dstring_get_string(&test_string), function);
         ret = false;
      }
      if (elems != sge_sl_get_elem_count(list)) {
         fprintf(stderr, "Error: Expected %d elements in list but got %d "
                         "in function %s()\n", (int) elems, (int) sge_sl_get_elem_count(list), function);
         ret = false;
      }

      sge_sl_unlock(list);
   }
   DRETURN(ret);
}

bool
test_search_sequence(sge_sl_list_t *list, bool forward, const char *key,
                     const char *expected, uint32_t elems, const char *function) {
   bool ret = true;

   DENTER(TOP_LAYER);
   if (ret) {
      sge_sl_elem_t *next;
      sge_sl_elem_t *current;

      ret &= sge_sl_lock(list);

      /* create string from stored characters */
      next = nullptr;
      ret &= sge_sl_elem_search(list, &next, (void *) key, test_compare_first_char,
                                forward ? SGE_SL_FORWARD : SGE_SL_BACKWARD);
      sge_dstring_sprintf(&test_string, "");
      while (ret && (current = next) != nullptr) {
         ret &= sge_sl_elem_search(list, &next, (void *) key, test_compare_first_char,
                                   forward ? SGE_SL_FORWARD : SGE_SL_BACKWARD);

         sge_dstring_append(&test_string, (char *) sge_sl_elem_data(current));
      }

      /* test sequence of characters in string */
      if (strcmp(expected, sge_dstring_get_string(&test_string)) != 0) {
         fprintf(stderr, "Error: Expected %s-sequence \"%s\" "
                         "but it was \"%s\" in function %s()\n", forward ? "forward" : "backward",
                 expected, sge_dstring_get_string(&test_string), function);
         ret = false;
      }
      if (elems != sge_sl_get_elem_count(list)) {
         fprintf(stderr, "Error: Expected %d elements in list but got %d "
                         "in function %s()\n", (int) elems, (int) sge_sl_get_elem_count(list), function);
         ret = false;
      }

      sge_sl_unlock(list);
   }
   DRETURN(ret);
}

bool
test_create_insert_destroy() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;

   DENTER(TOP_LAYER);

   /* create a list */
   ret = sge_sl_create(&list);

   /* insert elements */
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"j", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"i", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"h", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"g", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"f", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"e", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"d", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"c", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"b", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"a", SGE_SL_FORWARD);
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_sequence(list, true, "abcdefghij", 10, __func__);
      ret &= test_sequence(list, false, "jihgfedcba", 10, __func__);
   }

   /* cleanup: destroy function saves deletion sequence in test_string */
   if (ret) {
      sge_dstring_sprintf(&test_string, "");
      ret &= sge_sl_destroy(&list, test_destroy_test);
   }

   /* test delete sequence */
   if (ret) {
      if (strcmp("abcdefghij", sge_dstring_get_string(&test_string)) != 0) {
         fprintf(stderr, "Error: Expected sequence in %s() is \"%s\" "
                         "but it was \"%s\"\n", __func__, "abcdefghij",
                 sge_dstring_get_string(&test_string));
         ret = false;
      }
   }

   DRETURN(ret);
}

bool
test_create_append() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;

   DENTER(TOP_LAYER);

   /* create a list */
   ret = sge_sl_create(&list);

   /* insert elements */
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"a", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"b", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"c", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"d", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"e", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"f", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"g", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"h", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"i", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"j", SGE_SL_BACKWARD);
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_sequence(list, true, "abcdefghij", 10, __func__);
      ret &= test_sequence(list, false, "jihgfedcba", 10, __func__);
   }

   if (ret) {
      ret &= sge_sl_destroy(&list, test_destroy_test);
   }

   DRETURN(ret);
}

bool
test_create_insort() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;

   DENTER(TOP_LAYER);

   /* create a list */
   ret = sge_sl_create(&list);

   /* insert elements */
   if (ret) {

      ret &= sge_sl_insert_search(list, (void *)"h", test_compare);  /* first and last*/
      ret &= sge_sl_insert_search(list, (void *)"f", test_compare);  /* new first */
      ret &= sge_sl_insert_search(list, (void *)"d", test_compare);  /* new first */
      ret &= sge_sl_insert_search(list, (void *)"g", test_compare);  /* in between */
      ret &= sge_sl_insert_search(list, (void *)"e", test_compare);  /* in between */
      ret &= sge_sl_insert_search(list, (void *)"a", test_compare);  /* new first */
      ret &= sge_sl_insert_search(list, (void *)"i", test_compare);  /* new last */
      ret &= sge_sl_insert_search(list, (void *)"c", test_compare);  /* in between */
      ret &= sge_sl_insert_search(list, (void *)"b", test_compare);  /* in between */
      ret &= sge_sl_insert_search(list, (void *)"j", test_compare);  /* new last */
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_sequence(list, true, "abcdefghij", 10, __func__);
      ret &= test_sequence(list, false, "jihgfedcba", 10, __func__);
   }

   if (ret) {
      ret &= sge_sl_destroy(&list, test_destroy_test);
   }

   DRETURN(ret);
}

bool
test_create_insert_sort() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;

   DENTER(TOP_LAYER);

   /* create a list */
   ret = sge_sl_create(&list);

   /* insert elements */
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"h", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"f", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"d", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"g", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"e", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"a", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"i", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"c", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"b", SGE_SL_FORWARD);
      ret &= sge_sl_insert(list, (void *)"j", SGE_SL_FORWARD);
   }
   if (ret) {
      ret &= sge_sl_sort(list, test_compare);
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_sequence(list, true, "abcdefghij", 10, __func__);
      ret &= test_sequence(list, false, "jihgfedcba", 10, __func__);
   }

   if (ret) {
      ret &= sge_sl_destroy(&list, test_destroy_test);
   }

   DRETURN(ret);
}

bool
test_dechain_before_after() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;
   sge_sl_elem_t *new_elem = nullptr;
   sge_sl_elem_t *elem = nullptr;

   DENTER(TOP_LAYER);

   /* create a list */
   ret = sge_sl_create(&list);

   /* insert elements */
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"c", SGE_SL_FORWARD);

      ret &= sge_sl_elem_search(list, &elem, (void *)"c", test_compare, SGE_SL_FORWARD);
   }

   /* insert before */
   if (ret) {
      ret &= sge_sl_elem_create(&new_elem, (void *)"a");
      ret &= sge_sl_insert_before(list, new_elem, elem);
      ret &= sge_sl_elem_create(&new_elem, (void *)"b");
      ret &= sge_sl_insert_before(list, new_elem, elem);
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_sequence(list, true, "abc", 3, __func__);
      ret &= test_sequence(list, false, "cba", 3, __func__);
   }

   /* append after */
   if (ret) {
      ret &= sge_sl_elem_create(&new_elem, (void *)"e");
      ret &= sge_sl_append_after(list, new_elem, elem);
      ret &= sge_sl_elem_create(&new_elem, (void *)"d");
      ret &= sge_sl_append_after(list, new_elem, elem);
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_sequence(list, true, "abcde", 5, __func__);
      ret &= test_sequence(list, false, "edcba", 5, __func__);
   }

   /* dechain */
   if (ret) {
      ret &= sge_sl_dechain(list, elem);
      sge_sl_elem_destroy(&elem, test_destroy_test);
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_sequence(list, true, "abde", 4, __func__);
      ret &= test_sequence(list, false, "edba", 4, __func__);
   }

   /* dechain first */
   if (ret) {
      elem = nullptr;
      ret &= sge_sl_elem_next(list, &elem, SGE_SL_FORWARD);
      ret &= sge_sl_dechain(list, elem);
      sge_sl_elem_destroy(&elem, test_destroy_test);
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_sequence(list, true, "bde", 3, __func__);
      ret &= test_sequence(list, false, "edb", 3, __func__);
   }

   /* dechain first */
   if (ret) {
      elem = nullptr;
      ret &= sge_sl_elem_next(list, &elem, SGE_SL_BACKWARD);
      ret &= sge_sl_dechain(list, elem);
      sge_sl_elem_destroy(&elem, test_destroy_test);
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_sequence(list, true, "bd", 2, __func__);
      ret &= test_sequence(list, false, "db", 2, __func__);
   }

   /* cleanup */
   if (ret) {
      ret &= sge_sl_destroy(&list, test_destroy_test);
   }

   DRETURN(ret);
}

bool
test_search_forward_backward() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;

   DENTER(TOP_LAYER);

   /* create a list */
   ret = sge_sl_create(&list);

   /* insert elements */
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"ax", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"bx", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xa", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xb", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"ex", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"fx", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xc", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"hx", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"ix", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xd", SGE_SL_BACKWARD);
   }

   if (ret) {
      char *data = nullptr;

      ret &= sge_sl_data_search(list, (void *)"X*", (void **) &data,
                                test_compare_first_char, SGE_SL_FORWARD);
      if (strcmp(data, "Xa") != 0) {
         fprintf(stderr, "Error: Expected %s as search result but got %s "
                         "in function %s()\n", "Xa", data, __func__);
         ret = false;
      }
   }

   /* test links forward and backward */
   if (ret) {
      ret &= test_search_sequence(list, true, "X*", "XaXbXcXd", 10, __func__);
      ret &= test_search_sequence(list, false, "X*", "XdXcXbXa", 10, __func__);
   }

   if (ret) {
      ret &= sge_sl_destroy(&list, test_destroy_test);
   }

   DRETURN(ret);
}

bool
test_delete_forward_backward() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;

   DENTER(TOP_LAYER);

   /* create a list */
   ret = sge_sl_create(&list);

   /* insert elements */
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"ax", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"bx", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xa", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xb", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"ex", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"fx", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xc", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"hx", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"ix", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xd", SGE_SL_BACKWARD);
   }

   /* delete some from beginning */
   if (ret) {
      ret &= sge_sl_delete(list, test_destroy_test, SGE_SL_FORWARD);
      ret &= sge_sl_delete(list, test_destroy_test, SGE_SL_FORWARD);
      ret &= sge_sl_delete(list, test_destroy_test, SGE_SL_FORWARD);
   }

   /* test */
   if (ret) {
      ret &= test_sequence(list, true, "XbexfxXchxixXd", 7, __func__);
   }

   /* delete some from end */
   if (ret) {
      ret &= sge_sl_delete(list, test_destroy_test, SGE_SL_BACKWARD);
      ret &= sge_sl_delete(list, test_destroy_test, SGE_SL_BACKWARD);
      ret &= sge_sl_delete(list, test_destroy_test, SGE_SL_BACKWARD);
   }

   /* test */
   if (ret) {
      ret &= test_sequence(list, true, "XbexfxXc", 4, __func__);
   }

   if (ret) {
      ret &= sge_sl_destroy(&list, test_destroy_test);
   }

   DRETURN(ret);
}

bool
test_delete_search() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;

   DENTER(TOP_LAYER);

   /* create a list */
   ret = sge_sl_create(&list);

   /* insert elements */
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"ax", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"bx", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xa", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xb", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"ex", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"fx", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xc", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"hx", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"ix", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"Xd", SGE_SL_BACKWARD);
   }

   /* delete some from beginning */
   if (ret) {
      ret &= sge_sl_delete_search(list, (void *)"X*", test_destroy_test,
                                  test_compare_first_char, SGE_SL_FORWARD);
      ret &= sge_sl_delete_search(list, (void *)"X*", test_destroy_test,
                                  test_compare_first_char, SGE_SL_FORWARD);
      ret &= sge_sl_delete_search(list, (void *)"X*", test_destroy_test,
                                  test_compare_first_char, SGE_SL_FORWARD);
      ret &= sge_sl_delete_search(list, (void *)"X*", test_destroy_test,
                                  test_compare_first_char, SGE_SL_FORWARD);
   }

   /* test */
   if (ret) {
      ret &= test_sequence(list, true, "axbxexfxhxix", 6, __func__);
   }

   if (ret) {
      ret &= sge_sl_destroy(&list, test_destroy_test);
   }

   DRETURN(ret);
}

bool
test_for_each_ep() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;
   int sum = 0;

   DENTER(TOP_LAYER);

   /* create a list */
   ret = sge_sl_create(&list);

   /* insert elements */
   if (ret) {
      ret &= sge_sl_insert(list, (void *) "1", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *) "2", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *) "3", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *) "4", SGE_SL_BACKWARD);
   }

   if (ret) {
      sge_sl_elem_t *elem;

      for_each_sl_locked (elem, list) {
         sum += atoi((char *)sge_sl_elem_data(elem));
      }
   }
   if (sum != 10) {
      fprintf(stderr, "Error: Expected a sum if %d but got %d "
                      "in function %s()\n", 10, sum, __func__);
      ret = false;
   }

   ret &= sge_sl_destroy(&list, nullptr);
   DRETURN(ret);
}

void *
test_thread1_main(void *arg) {
   void *ret = nullptr;
   test_sl_thread_t *global = (test_sl_thread_t *) arg;

   DENTER(TOP_LAYER);
   while (global->do_terminate != true) {
      int max_actions = 10;
      int action = random() % max_actions;
      int search_action = random() % 3;
      sge_sl_elem_t *next;
      sge_sl_elem_t *elem;
      int i;

      DPRINTF("action = %d\n", action);
      if (action == 3 || action == 6) {
         sge_sl_lock(global->list); /* unlock will be sone below in action 3 */
         DPRINTF("search_action = %d\n", search_action);
         switch (search_action) {
            case 0:
               /* first element */
               elem = nullptr;
               sge_sl_elem_next(global->list, &elem, SGE_SL_FORWARD);
               break;
            case 1:
               /* last element */
               elem = nullptr;
               sge_sl_elem_next(global->list, &elem, SGE_SL_BACKWARD);
               break;
            case 2:
               /* somewhere in middle */
               i = sge_sl_get_elem_count(global->list) / 2;
               elem = nullptr;
               next = nullptr;
               sge_sl_elem_next(global->list, &next, SGE_SL_BACKWARD);
               while ((elem = next) != nullptr && i > 0) {
                  sge_sl_elem_next(global->list, &next, SGE_SL_BACKWARD);
                  i--;
               }
               break;
            default:
               elem = nullptr;
               break;
         }
      }

      switch (action) {
         case 0:
            /* append at and */
            sge_sl_insert(global->list, (void *) "0", SGE_SL_BACKWARD);
            break;
         case 1:
            /* insert at beginning */
            sge_sl_insert(global->list, (void *) "1", SGE_SL_FORWARD);
            break;
         case 2:
            /* sort and insert sorted */
            sge_sl_lock(global->list);
            sge_sl_sort(global->list, test_compare);
            sge_sl_insert_search(global->list, (void *) "2", test_compare_ulong);
            sge_sl_unlock(global->list);
            break;
         case 3:
            /* search and insert before or append after */
            if (elem != nullptr) {
               sge_sl_elem_t *new_elem = nullptr;
               int location = random() % 2;

               sge_sl_elem_create(&new_elem, (void *) "3");
               switch (location) {
                  case 0:
                     sge_sl_insert_before(global->list, new_elem, elem);
                     break;
                  case 1:
                     sge_sl_append_after(global->list, new_elem, elem);
                     break;
                  default:
                     break;
               }
            }
            sge_sl_unlock(global->list); /* lock has been done above during serach */
            break;
         case 4:
            sge_sl_delete(global->list, nullptr, SGE_SL_FORWARD);
            break;
         case 5:
            sge_sl_delete(global->list, nullptr, SGE_SL_BACKWARD);
            break;
         case 6:
            if (elem != nullptr) {
               sge_sl_delete_search(global->list, elem->data, nullptr,
                                    test_compare,
                                    random() % 2 == 0 ? SGE_SL_FORWARD : SGE_SL_BACKWARD);
            }
            sge_sl_unlock(global->list); /* lock has been done above during serach */
            break;
         case 9:
            /* terminate all threads when there are enough elements in list */
            if (sge_sl_get_elem_count(global->list) >= 10000) {
               pthread_mutex_lock(&global->mutex);
               global->do_terminate = true;
               pthread_mutex_unlock(&global->mutex);
            }
            break;
         default:
            break;
      }
   }
   DRETURN(ret);
}

bool
test_mt_support() {
   bool ret = true;
   test_sl_thread_t global;

   DENTER(TOP_LAYER);

   /* create a list */
   memset(&global, 0, sizeof(test_sl_thread_t));
   global.do_terminate = false;
   pthread_mutex_init(&global.mutex, nullptr);
   ret = sge_sl_create(&global.list);

   /* spawn threads */
   if (ret) {
      pthread_t thread[TEST_SL_MAX_THREADS];
      int i;

      for (i = 0; i < TEST_SL_MAX_THREADS; i++) {
         pthread_create(&(thread[i]), nullptr, test_thread1_main, &global);
      }
      for (i = 0; i < TEST_SL_MAX_THREADS; i++) {
         pthread_join(thread[i], nullptr);
      }
   }

   /* cleanup */
   pthread_mutex_destroy(&global.mutex);
   ret &= sge_sl_destroy(&global.list, nullptr);
   DRETURN(ret);
}

static bool
test_sge_sl_data() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;
   void *data = nullptr;

   DENTER(TOP_LAYER);

   // sge_sl_data on populated list: forward → first, backward → last
   ret = sge_sl_create(&list);
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"a", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"b", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"c", SGE_SL_BACKWARD);
   }
   if (ret) {
      sge_sl_data(list, &data, SGE_SL_FORWARD);
      if (strcmp((char *)data, "a") != 0) {
         fprintf(stderr, "Error: sge_sl_data(FORWARD) expected \"a\" but got \"%s\"\n", (char *)data);
         ret = false;
      }
      sge_sl_data(list, &data, SGE_SL_BACKWARD);
      if (strcmp((char *)data, "c") != 0) {
         fprintf(stderr, "Error: sge_sl_data(BACKWARD) expected \"c\" but got \"%s\"\n", (char *)data);
         ret = false;
      }
   }
   sge_sl_destroy(&list, nullptr);
   DRETURN(ret);
}

static bool
test_sge_sl_data_empty() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;
   void *data = (void *)"sentinel";

   DENTER(TOP_LAYER);

   // sge_sl_data on empty list sets *data to nullptr
   ret = sge_sl_create(&list);
   if (ret) {
      sge_sl_data(list, &data, SGE_SL_FORWARD);
      if (data != nullptr) {
         fprintf(stderr, "Error: sge_sl_data(FORWARD) on empty list should set data to nullptr\n");
         ret = false;
      }
      data = (void *)"sentinel";
      sge_sl_data(list, &data, SGE_SL_BACKWARD);
      if (data != nullptr) {
         fprintf(stderr, "Error: sge_sl_data(BACKWARD) on empty list should set data to nullptr\n");
         ret = false;
      }
   }
   sge_sl_destroy(&list, nullptr);
   DRETURN(ret);
}

static bool
test_for_each_sl_unlocked() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;
   int sum = 0;

   DENTER(TOP_LAYER);

   // for_each_sl (unlocked) iterates all elements exactly once
   ret = sge_sl_create(&list);
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"1", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"2", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"3", SGE_SL_BACKWARD);
      ret &= sge_sl_insert(list, (void *)"4", SGE_SL_BACKWARD);
   }
   if (ret) {
      sge_sl_elem_t *elem;
      for_each_sl(elem, list) {
         sum += atoi((char *)sge_sl_elem_data(elem));
      }
      if (sum != 10) {
         fprintf(stderr, "Error: for_each_sl expected sum 10 but got %d\n", sum);
         ret = false;
      }
   }
   ret &= sge_sl_destroy(&list, nullptr);
   DRETURN(ret);
}

static bool
test_empty_list_edge_cases() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;

   DENTER(TOP_LAYER);

   ret = sge_sl_create(&list);

   // count of freshly created list is 0
   if (ret && sge_sl_get_elem_count(list) != 0) {
      fprintf(stderr, "Error: fresh list should have count 0, got %u\n", sge_sl_get_elem_count(list));
      ret = false;
   }

   // elem_next on empty list: *elem stays nullptr, returns true
   if (ret) {
      sge_sl_elem_t *elem = nullptr;
      bool next_ret = sge_sl_elem_next(list, &elem, SGE_SL_FORWARD);
      if (!next_ret || elem != nullptr) {
         fprintf(stderr, "Error: sge_sl_elem_next on empty list should return true with elem=nullptr\n");
         ret = false;
      }
   }

   // delete on empty list is a silent no-op and returns true
   if (ret) {
      bool del_ret = sge_sl_delete(list, nullptr, SGE_SL_FORWARD);
      if (!del_ret || sge_sl_get_elem_count(list) != 0) {
         fprintf(stderr, "Error: sge_sl_delete on empty list should return true with count still 0\n");
         ret = false;
      }
   }

   sge_sl_destroy(&list, nullptr);
   DRETURN(ret);
}

static bool
test_sge_sl_get_mutex() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;

   DENTER(TOP_LAYER);

   ret = sge_sl_create(&list);
   if (ret && sge_sl_get_mutex(list) == nullptr) {
      fprintf(stderr, "Error: sge_sl_get_mutex should return non-null\n");
      ret = false;
   }
   sge_sl_destroy(&list, nullptr);
   DRETURN(ret);
}

static bool
test_dechain_sole_element() {
   bool ret = true;
   sge_sl_list_t *list = nullptr;
   sge_sl_elem_t *elem = nullptr;
   void *data = (void *)"sentinel";

   DENTER(TOP_LAYER);

   // insert one element, dechain it → list becomes empty
   ret = sge_sl_create(&list);
   if (ret) {
      ret &= sge_sl_insert(list, (void *)"x", SGE_SL_FORWARD);
   }
   if (ret) {
      ret &= sge_sl_elem_next(list, &elem, SGE_SL_FORWARD);
      ret &= sge_sl_dechain(list, elem);
      sge_sl_elem_destroy(&elem, nullptr);
   }
   if (ret && sge_sl_get_elem_count(list) != 0) {
      fprintf(stderr, "Error: count after dechain of sole element should be 0, got %u\n",
              sge_sl_get_elem_count(list));
      ret = false;
   }
   // sge_sl_data on now-empty list must return nullptr
   if (ret) {
      sge_sl_data(list, &data, SGE_SL_FORWARD);
      if (data != nullptr) {
         fprintf(stderr, "Error: sge_sl_data after dechain of sole element should be nullptr\n");
         ret = false;
      }
   }
   sge_sl_destroy(&list, nullptr);
   DRETURN(ret);
}

int main(int /*argc*/, char * /*argv*/[]) {
   DENTER_MAIN(TOP_LAYER, "test_sl");
   int id = 1;

   printf("\n--- insert and traverse ---\n");
   // T01: prepend elements forward and verify forward/backward order and destroy sequence
   CHECK(id, "insert forward: sequence and destroy order", test_create_insert_destroy()); id++;
   // T02: append elements backward and verify forward/backward order
   CHECK(id, "insert backward: sequence correct", test_create_append()); id++;
   // T03: sorted insert keeps list in order
   CHECK(id, "insert_search: sorted insert produces ordered list", test_create_insort()); id++;
   // T04: bulk insert then sort produces ordered list
   CHECK(id, "insert then sort: forward/backward order correct", test_create_insert_sort()); id++;

   printf("\n--- search ---\n");
   // T05: data_search and elem_search forward/backward with fnmatch comparator
   CHECK(id, "search forward/backward with pattern comparator", test_search_forward_backward()); id++;

   printf("\n--- delete ---\n");
   // T06: sge_sl_delete from front and back leaves expected remainder
   CHECK(id, "delete from front and back: remaining sequence correct", test_delete_forward_backward()); id++;
   // T07: sge_sl_delete_search removes all elements matching pattern
   CHECK(id, "delete_search by pattern: remaining sequence correct", test_delete_search()); id++;

   printf("\n--- insert_before / append_after / dechain ---\n");
   // T08: insert_before, append_after, dechain first/last/middle all update links correctly
   CHECK(id, "insert_before / append_after / dechain: links correct", test_dechain_before_after()); id++;

   printf("\n--- for_each ---\n");
   // T09: for_each_sl_locked iterates all elements exactly once
   CHECK(id, "for_each_sl_locked: sum of all elements correct", test_for_each_ep()); id++;
   // T10: for_each_sl (unlocked) iterates all elements exactly once
   CHECK(id, "for_each_sl (unlocked): sum of all elements correct", test_for_each_sl_unlocked()); id++;

   printf("\n--- sge_sl_data ---\n");
   // T11: sge_sl_data returns first and last data pointers from a populated list
   CHECK(id, "sge_sl_data: forward=first, backward=last", test_sge_sl_data()); id++;
   // T12: sge_sl_data on empty list sets *data to nullptr for both directions
   CHECK(id, "sge_sl_data on empty list: yields nullptr", test_sge_sl_data_empty()); id++;

   printf("\n--- empty list edge cases ---\n");
   // T13: fresh list count=0; elem_next returns true with nullptr; delete is silent no-op
   CHECK(id, "empty list: count=0, elem_next=true/nullptr, delete=true no-op", test_empty_list_edge_cases()); id++;

   printf("\n--- sge_sl_get_mutex ---\n");
   // T14: sge_sl_get_mutex returns non-null pointer
   CHECK(id, "sge_sl_get_mutex returns non-null", test_sge_sl_get_mutex()); id++;

   printf("\n--- sole-element dechain ---\n");
   // T15: dechain the only element leaves count=0 and sge_sl_data returns nullptr
   CHECK(id, "dechain sole element: count=0, data=nullptr", test_dechain_sole_element()); id++;

   printf("\n--- multi-threaded concurrent operations ---\n");
   // T16: 10 threads concurrently insert/delete/search without corruption
   CHECK(id, "concurrent insert/delete/search: no corruption", test_mt_support()); id++;

   printf("\n%s - %d failure(s)\n", s_fail == 0 ? "PASS" : "FAIL", s_fail);
   DRETURN(s_fail == 0 ? 0 : 1);
}

