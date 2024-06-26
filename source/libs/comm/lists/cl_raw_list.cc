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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/time.h>
#include <cstdlib>

#include "uti/sge_stdlib.h"

#include "comm/lists/cl_lists.h"

/* setup raw list

   list_p              -> address of a raw list pointer to setup 
   enable_list_locking -> if set the list will create mutex lock variables

   return values:

   int                 -> CL_RETVAL_xxxx error codes

   - if return value is not CL_RETVAL_OK NO memory has to be freed by caller, the
     list is NOT initialized. 

   - On CL_RETVAL_OK the list must be freed by calling the function cl_raw_list_cleanup()
*/
int cl_raw_list_setup(cl_raw_list_t **list_p, const char *list_name, int enable_list_locking) {  /* CR check */

   if (list_p == nullptr || list_name == nullptr) {
      /* don't accept nullptr pointer for list pointer */
      return CL_RETVAL_PARAMS;
   }

   if (*list_p != nullptr) {
      /* pointer to list pointer must be set to nullptr */
      return CL_RETVAL_PARAMS;
   }

   /* get memory for cl_raw_list_t list object */
   *list_p = (cl_raw_list_t *) sge_malloc(sizeof(cl_raw_list_t));
   if (*list_p == nullptr) {
      return CL_RETVAL_MALLOC;
   }
   memset(*list_p, 0, sizeof(cl_raw_list_t));

   (*list_p)->list_name = strdup(list_name);
   if ((*list_p)->list_name == nullptr) {
      free(*list_p);
      *list_p = nullptr;
   }

   if (enable_list_locking) {
      /* malloc pthread_mutex_t for the list */
      (*list_p)->list_mutex = (pthread_mutex_t *) sge_malloc(sizeof(pthread_mutex_t));
      if ((*list_p)->list_mutex == nullptr) {
         cl_raw_list_cleanup(list_p);
         return CL_RETVAL_MALLOC;
      }
      if (pthread_mutex_init((*list_p)->list_mutex, nullptr) != 0) {
         CL_LOG_STR(CL_LOG_ERROR, "raw list mutex init setup error for list:", (*list_p)->list_name);
         cl_raw_list_cleanup(list_p);
         return CL_RETVAL_MUTEX_ERROR;
      }
   }
#ifdef CL_DO_COMMLIB_DEBUG
   CL_LOG_STR(CL_LOG_DEBUG,"raw list setup complete for list:",(*list_p)->list_name);
#endif
   return CL_RETVAL_OK;
}


/* setup raw list

   list_p              -> address of a raw list pointer which was successfully
                          set up by a call to cl_raw_list_setup() 
   return values:

   CL_RETVAL_xxxx error codes:

   - CL_RETVAL_OK                  -> list is freed, no erros

   - CL_RETVAL_LIST_DATA_NOT_EMPTY -> list is not empty , try again

   - CL_RETVAL_MUTEX_CLEANUP_ERROR -> list is not freed , try again

   
*/
int cl_raw_list_cleanup(cl_raw_list_t **list_p) {  /* CR check */
   int ret_val;

   int do_log = 1;

   if (list_p == nullptr) {
      /* we expect an address of an pointer */
      return CL_RETVAL_PARAMS;
   }
   if (*list_p == nullptr) {
      /* we expect an initalized pointer */
      return CL_RETVAL_PARAMS;
   }

   if ((*list_p)->list_type == CL_LOG_LIST) {
      /* never try to log when cleaning up log list !!! */
      do_log = 0;
   }

   /* check if list is empty */
   if ((*list_p)->list_data != nullptr) {
      if (do_log) {
         CL_LOG_STR(CL_LOG_ERROR, "list_data is not empty for list:", (*list_p)->list_name);
      }
      return CL_RETVAL_LIST_DATA_NOT_EMPTY;
   }

   if ((*list_p)->first_elem != nullptr) {
      if (do_log) {
         CL_LOG_STR(CL_LOG_ERROR, "list is not empty listname is:", (*list_p)->list_name);
      }
      return CL_RETVAL_LIST_NOT_EMPTY;
   }

   /* destroy any mutex variables if set */
   if ((*list_p)->list_mutex != nullptr) {
      ret_val = pthread_mutex_destroy((*list_p)->list_mutex);
      if (ret_val == EBUSY) {
         if (do_log) {
            CL_LOG_STR(CL_LOG_ERROR, "raw list mutex cleanup error: EBUSY for list:", (*list_p)->list_name);
#ifdef CL_DO_COMMLIB_DEBUG
            CL_LOG_STR(CL_LOG_ERROR, "last logger:", (*list_p)->last_locker);
#endif
         }
         return CL_RETVAL_MUTEX_CLEANUP_ERROR;
      }
      free((*list_p)->list_mutex);
      (*list_p)->list_mutex = nullptr;
   }

#ifdef CL_DO_COMMLIB_DEBUG
   if (do_log) {
      CL_LOG_STR(CL_LOG_DEBUG,"raw list cleanup complete for list:",(*list_p)->list_name );
   }
   free((*list_p)->last_locker);
   (*list_p)->last_locker = nullptr;
#endif

   /* destroy list name */
   if ((*list_p)->list_name != nullptr) {
      free((*list_p)->list_name);
      (*list_p)->list_name = nullptr;
   }

   /* free list, set pointer to nullptr */
   free(*list_p);
   *list_p = nullptr;

   return CL_RETVAL_OK;
}

/* add element

   list_p              -> pointer to list which was successfully
                          set up by a call to cl_raw_list_setup() 
  
   data                -> pointer to list data to insert into list

   return values:
 
   pointer to a new list element (cl_raw_list_elem_t*) or nullptr

*/
cl_raw_list_elem_t *cl_raw_list_append_elem(cl_raw_list_t *list_p, void *data) {

   cl_raw_list_elem_t *new_elem = nullptr;

   if (data == nullptr || list_p == nullptr) {
      return nullptr;
   }

   /* malloc memory for new cl_raw_list_elem_t */
   new_elem = (cl_raw_list_elem_t *) sge_malloc(sizeof(cl_raw_list_elem_t));
   if (new_elem == nullptr) {
      return nullptr;
   }


   /* initialize new list element with data */
   new_elem->data = data;

   cl_raw_list_append_dechained_elem(list_p, new_elem);

#ifdef CL_DO_COMMLIB_DEBUG
   /* ENABLE THIS ONLY FOR LIST DEBUGING */
   if ( list_p->list_type != CL_LOG_LIST ) {
      CL_LOG_STR(CL_LOG_DEBUG, "list:", list_p->list_name);
      CL_LOG_INT(CL_LOG_DEBUG,"elements in list:", (int)list_p->elem_count); 
   }
#endif
   return new_elem;
}


int cl_raw_list_append_dechained_elem(cl_raw_list_t *list_p, cl_raw_list_elem_t *dechain_elem) {
   if (dechain_elem == nullptr || list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }
   dechain_elem->next = nullptr;
   dechain_elem->last = nullptr;

   /* append new element into list */
   if (list_p->first_elem == nullptr) {
      /* we have an empty list */
      list_p->first_elem = dechain_elem;
      list_p->last_elem = dechain_elem;
   } else {
      /* we append at the end */
      list_p->last_elem->next = dechain_elem;
      dechain_elem->last = list_p->last_elem;
      list_p->last_elem = dechain_elem;
   }

   /* increase list element count */
   list_p->elem_count = list_p->elem_count + 1;
   return CL_RETVAL_OK;
}

int cl_raw_list_dechain_elem(cl_raw_list_t *list_p, cl_raw_list_elem_t *dechain_elem) {

   if (dechain_elem == nullptr || list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   if (dechain_elem == list_p->first_elem) {
      if (dechain_elem == list_p->last_elem) {
         list_p->last_elem = nullptr;
         list_p->first_elem = nullptr;
      } else {
         list_p->first_elem = dechain_elem->next;
         list_p->first_elem->last = nullptr;
      }
   } else {
      if (dechain_elem == list_p->last_elem) {
         /* dechain at the end */
         list_p->last_elem = dechain_elem->last;
         list_p->last_elem->next = nullptr;
      } else {
         /* dechain in the middle */
         dechain_elem->last->next = dechain_elem->next;
         dechain_elem->next->last = dechain_elem->last;
      }
   }
   dechain_elem->last = nullptr;
   dechain_elem->next = nullptr;
   /* decrease the list element counter */
   list_p->elem_count = list_p->elem_count - 1;
   return CL_RETVAL_OK;
}


/* remove element from list

  list_p              -> address of a raw list pointer which was successfully
                          set up by a call to cl_raw_list_setup() 
  delete_elem         -> element pointer of element to remove from list 

  return values:

  pointer to void: data element (caller must free the memory for the data */
void *cl_raw_list_remove_elem(cl_raw_list_t *list_p, cl_raw_list_elem_t *delete_elem) {       /* CR check */
   void *old_data = nullptr;

   if (delete_elem == nullptr || list_p == nullptr) {
      /* parameter errors */
      return nullptr;
   }

   old_data = delete_elem->data;

   /* now dechain the list element from list */
   cl_raw_list_dechain_elem(list_p, delete_elem);

   /* now delete the dechained element */
   free(delete_elem);

#ifdef CL_DO_COMMLIB_DEBUG
   if ( list_p->list_type != CL_LOG_LIST ) {
      CL_LOG_STR(CL_LOG_DEBUG, "list:", list_p->list_name);
      CL_LOG_INT(CL_LOG_DEBUG,"elements in list:", (int)list_p->elem_count); 
   }
#endif
   return old_data;
}

unsigned long cl_raw_list_get_elem_count(cl_raw_list_t *list_p) {   /* CR check */
   if (list_p) {
      return list_p->elem_count;
   }
   return 0;
}

int cl_raw_list_lock(cl_raw_list_t *list_p) {             /* CR check */
   if (list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }
   if (list_p->list_mutex != nullptr) {
#ifdef CL_DO_COMMLIB_DEBUG
      /* ENABLE THIS ONLY FOR LOCK DEBUGING (1 of 2) */
      if ( list_p->list_type != CL_LOG_LIST ) {
        CL_LOG_STR(CL_LOG_INFO, "try locking list:", list_p->list_name); 
        if (list_p->last_locker != nullptr) {
           CL_LOG_STR(CL_LOG_INFO, "last locker thread:", list_p->last_locker);
        }
      }
#endif
      if (pthread_mutex_lock(list_p->list_mutex) != 0) {
         if (list_p->list_type != CL_LOG_LIST) {
            CL_LOG_STR(CL_LOG_ERROR, "mutex lock error for list:", list_p->list_name);
         }
         return CL_RETVAL_MUTEX_LOCK_ERROR;
      }
#ifdef CL_DO_COMMLIB_DEBUG
      if ( list_p->list_type != CL_LOG_LIST ) {
         cl_thread_settings_t* thread_config_p = nullptr;
         if (list_p->last_locker != nullptr) {
            free(list_p->last_locker);
            list_p->last_locker = nullptr;
         }
         thread_config_p = cl_thread_get_thread_config();
         if (thread_config_p == nullptr) {
            list_p->last_locker = strdup("unknown");
         } else {
            list_p->last_locker = strdup(thread_config_p->thread_name);
         }
         
         CL_LOG_STR(CL_LOG_INFO, "got lock:", list_p->list_name); 
         if (list_p->unlock_count != list_p->lock_count) {
            CL_LOG_STR(CL_LOG_ERROR, "unlock count doesn't match lock count: ", list_p->list_name); 
            printf("abort due thread lock error\n");
            exit(1);
         }
         list_p->lock_count = list_p->lock_count + 1;
         CL_LOG_INT(CL_LOG_INFO, "lock_count is", list_p->lock_count ); 
      }
#endif
   }
   return CL_RETVAL_OK;
}

int cl_raw_list_unlock(cl_raw_list_t *list_p) {
   if (list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }
   if (list_p->list_mutex != nullptr) {
#ifdef CL_DO_COMMLIB_DEBUG
      /* ENABLE THIS ONLY FOR LOCK DEBUGING (2 of 2) */
      if ( list_p->list_type != CL_LOG_LIST ) {
         CL_LOG_STR(CL_LOG_INFO, "unlocking list:",list_p->list_name); 
         list_p->unlock_count = list_p->unlock_count + 1;
         if (list_p->unlock_count != list_p->lock_count) {
            CL_LOG_STR(CL_LOG_ERROR, "unlock count doesn't match lock count: ", list_p->list_name); 
            printf("abort due thread lock error\n");
            exit(1);
         }
      }
#endif
      if (pthread_mutex_unlock(list_p->list_mutex) != 0) {
         if (list_p->list_type != CL_LOG_LIST) {
            CL_LOG_STR(CL_LOG_ERROR, "mutex unlock error for list:", list_p->list_name);
         }
         return CL_RETVAL_MUTEX_UNLOCK_ERROR;
      }
   }
   return CL_RETVAL_OK;
}

cl_raw_list_elem_t *cl_raw_list_get_first_elem(cl_raw_list_t *list_p) {   /* CR check */
   cl_raw_list_elem_t *elem = nullptr;

   if (list_p != nullptr) {
      elem = list_p->first_elem;
   }
   return elem;
}

cl_raw_list_elem_t *cl_raw_list_get_least_elem(cl_raw_list_t *list_p) {   /* CR check */
   cl_raw_list_elem_t *elem = nullptr;
   if (list_p != nullptr) {
      elem = list_p->last_elem;
   }
   return elem;
}

cl_raw_list_elem_t *cl_raw_list_search_elem(cl_raw_list_t *list_p, void *data) {  /* CR check */
   cl_raw_list_elem_t *elem = nullptr;

   if (list_p != nullptr) {
      elem = list_p->first_elem;
      while (elem && elem->data != data) {
         elem = elem->next;
      }
   }
   if (elem == nullptr) {
      if (list_p->list_type != CL_LOG_LIST) {
         CL_LOG_STR(CL_LOG_DEBUG, "element not found in list:", list_p->list_name);
      }
   }
   return elem;
}

cl_raw_list_elem_t *cl_raw_list_get_next_elem(cl_raw_list_elem_t *elem) {       /* CR check */
   if (elem) {
      return elem->next;
   }
   return nullptr;
}

cl_raw_list_elem_t *cl_raw_list_get_last_elem(cl_raw_list_elem_t *elem) {       /* CR check */
   if (elem) {
      return elem->last;
   }
   return nullptr;
}


