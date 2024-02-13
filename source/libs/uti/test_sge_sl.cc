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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <fnmatch.h>

#include "uti/sge_dstring.h"
#include "uti/sge_err.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_sl.h"
#include "uti/sge_stdio.h"

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
   u_long32 number1 = *(u_long32 *) data1;
   u_long32 number2 = *(u_long32 *) data2;

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
              u_long32 elems, const char *function) {
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
                     const char *expected, u_long32 elems, const char *function) {
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
test_create_insert_destroy(void) {
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
test_create_append(void) {
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
test_create_insort(void) {
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
test_create_insert_sort(void) {
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
test_dechain_before_after(void) {
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
test_search_forward_backward(void) {
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
test_delete_forward_backward(void) {
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
test_delete_search(void) {
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
test_for_each_ep(void) {
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

      DPRINTF(("action = %d\n", action));
      if (action == 3 || action == 6) {
         sge_sl_lock(global->list); /* unlock will be sone below in action 3 */
         DPRINTF(("search_action = %d\n", search_action));
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
test_mt_support(void) {
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

int main(int argc, char *argv[]) {
   bool ret = true;

   DENTER_MAIN(TOP_LAYER, "test_sl");
   ret &= test_create_insert_destroy();
   ret &= test_create_append();
   ret &= test_create_insort();
   ret &= test_create_insert_sort();
   ret &= test_search_forward_backward();
   ret &= test_delete_forward_backward();
   ret &= test_delete_search();
   ret &= test_dechain_before_after();
   ret &= test_for_each_ep();
   ret &= test_mt_support();
   DRETURN(ret == true ? 0 : 1);
}

