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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <sys/time.h>

#include "uti/sge_stdlib.h"

#include "comm/lists/cl_lists.h"

/* this functions must lock / unlock the raw list manually */
static int cl_thread_list_add_thread(cl_raw_list_t *list_p, cl_thread_settings_t *thread_config);  /* CR check */
static int cl_thread_list_del_thread(cl_raw_list_t *list_p, cl_thread_settings_t *thread_config);  /* CR check */
#if 0
static cl_thread_list_elem_t* cl_thread_list_get_last_elem(cl_raw_list_t* list_p, cl_thread_list_elem_t* elem);
#endif


static int cl_thread_list_add_thread(cl_raw_list_t *list_p, cl_thread_settings_t *thread_config) { /* CR check */
   cl_thread_list_elem_t *new_elem = nullptr;

   if (thread_config == nullptr || list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* create new cl_thread_list_elem_t element */
   new_elem = (cl_thread_list_elem_t *) sge_malloc(sizeof(cl_thread_list_elem_t));
   if (new_elem == nullptr) {
      return CL_RETVAL_MALLOC;
   }

   new_elem->thread_config = thread_config;

   /* append elem and set elem pointer in new element */
   new_elem->raw_elem = cl_raw_list_append_elem(list_p, (void *) new_elem);

   if (new_elem->raw_elem == nullptr) {
      free(new_elem);
      return CL_RETVAL_MALLOC;
   }

   return CL_RETVAL_OK;
}

static int cl_thread_list_del_thread(cl_raw_list_t *list_p, cl_thread_settings_t *thread_config) {
   cl_thread_list_elem_t *elem = nullptr;

   /* search for element */
   elem = cl_thread_list_get_first_elem(list_p);
   while (elem != nullptr && elem->thread_config != thread_config) {
      elem = cl_thread_list_get_next_elem(elem);
   }

   /* remove elem from list and delete elem */
   if (elem) {
      cl_raw_list_remove_elem(list_p, elem->raw_elem);
      free(elem);
      return CL_RETVAL_OK;
   }
   return CL_RETVAL_THREAD_NOT_FOUND;
}

int cl_thread_list_setup(cl_raw_list_t **list_p, const char *list_name) {        /* CR check */
   return cl_raw_list_setup(list_p, list_name, 1); /* enable list locking */
}

int cl_thread_list_cleanup(cl_raw_list_t **list_p) {    /* CR check */
   return cl_raw_list_cleanup(list_p);
}

int cl_thread_list_create_thread(cl_raw_list_t *list_p,
                                 cl_thread_settings_t **new_thread_p,
                                 cl_raw_list_t *log_list,
                                 const char *name,
                                 int id,
                                 void *(*start_routine)(void *),
                                 cl_thread_cleanup_func_t cleanup_func,
                                 void *user_data,
                                 cl_thread_type_t thread_type) {
   cl_thread_settings_t *thread_p = nullptr;
   int ret_val;

   /* log_list can be nullptr */
   if (start_routine == nullptr || name == nullptr || list_p == nullptr || new_thread_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* malloc memory for thread settings, freed in cl_thread_list_delete_thread() */
   thread_p = (cl_thread_settings_t *) sge_malloc(sizeof(cl_thread_settings_t));
   if (thread_p == nullptr) {
      return CL_RETVAL_MALLOC;
   }

   *new_thread_p = thread_p;

   /* start the new thread */
   if ((ret_val = cl_thread_setup(thread_p, log_list, name, id, start_routine, cleanup_func, user_data, thread_type)) !=
       CL_RETVAL_OK) {
      cl_thread_shutdown(thread_p);
      cl_thread_join(thread_p);
      cl_thread_cleanup(thread_p);
      free(thread_p);
      return ret_val;
   }

   /* lock the thread list */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      cl_thread_shutdown(thread_p);
      cl_thread_join(thread_p);
      cl_thread_cleanup(thread_p);
      free(thread_p);
      return ret_val;
   }

   /* add new thread to thread list */
   if ((ret_val = cl_thread_list_add_thread(list_p, thread_p)) != CL_RETVAL_OK) {
      cl_raw_list_unlock(list_p);
      cl_thread_shutdown(thread_p);
      cl_thread_join(thread_p);
      cl_thread_cleanup(thread_p);
      free(thread_p);
      return ret_val;
   }

   /* unlock the thread list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   return CL_RETVAL_OK;
}

int cl_thread_list_delete_thread_by_id(cl_raw_list_t *list_p, int id) {   /* CR check */
   cl_thread_settings_t *thread = nullptr;
   int ret_val = CL_RETVAL_OK;

   /* lock thread list */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   /* get thread by id */
   if ((thread = cl_thread_list_get_thread_by_id(list_p, id)) == nullptr) {
      cl_raw_list_unlock(list_p);
      return CL_RETVAL_PARAMS;
   }

   /* remove thread from list */
   if ((ret_val = cl_thread_list_del_thread(list_p, thread)) != CL_RETVAL_OK) {
      cl_raw_list_unlock(list_p);
      return ret_val;
   }

   /* unlock thread list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      cl_thread_shutdown(thread);
      cl_thread_join(thread);
      cl_thread_cleanup(thread);
      free(thread);
      return ret_val;
   }

   /* trigger thread shutdwon */
   if ((ret_val = cl_thread_shutdown(thread)) != CL_RETVAL_OK) {
      cl_thread_join(thread);
      cl_thread_cleanup(thread);
      free(thread);
      return ret_val;
   }


   /* wait for thread's end */
   if ((ret_val = cl_thread_join(thread)) != CL_RETVAL_OK) {
      cl_thread_cleanup(thread);
      free(thread);
      return ret_val;
   }

   /* cleanup stuff */
   ret_val = cl_thread_cleanup(thread);
   free(thread);

   return ret_val;
}

int cl_thread_list_delete_thread(cl_raw_list_t *list_p, cl_thread_settings_t *thread) {
   int ret_val = CL_RETVAL_OK;

   if (thread == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* lock thread list */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   /* remove thread from list */
   if ((ret_val = cl_thread_list_del_thread(list_p, thread)) != CL_RETVAL_OK) {
      cl_raw_list_unlock(list_p);
      return ret_val;
   }

   /* unlock thread list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      cl_thread_shutdown(thread);
      cl_thread_join(thread);
      cl_thread_cleanup(thread);
      free(thread);
      return ret_val;
   }

   /* trigger thread shutdwon */
   if ((ret_val = cl_thread_shutdown(thread)) != CL_RETVAL_OK) {
      cl_thread_join(thread);
      cl_thread_cleanup(thread);
      free(thread);
      return ret_val;
   }


   /* wait for thread's end */
   if ((ret_val = cl_thread_join(thread)) != CL_RETVAL_OK) {
      cl_thread_cleanup(thread);
      free(thread);
      return ret_val;
   }

   /* cleanup stuff */
   ret_val = cl_thread_cleanup(thread);
   free(thread);

   return ret_val;
}

int cl_thread_list_delete_thread_without_join(cl_raw_list_t *list_p, cl_thread_settings_t *thread) {
   int ret_val = CL_RETVAL_OK;

   if (thread == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* lock thread list */
   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   /* remove thread from list */
   if ((ret_val = cl_thread_list_del_thread(list_p, thread)) != CL_RETVAL_OK) {
      cl_raw_list_unlock(list_p);
      return ret_val;
   }

   /* unlock thread list */
   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      cl_thread_shutdown(thread);
      cl_thread_join(thread);
      cl_thread_cleanup(thread);
      free(thread);
      return ret_val;
   }

   /* cleanup stuff */
   ret_val = cl_thread_cleanup(thread);
   free(thread);

   return ret_val;
}

int cl_thread_list_delete_thread_from_list(cl_raw_list_t *list_p, cl_thread_settings_t *thread) {
   /*
    * TODO: Cleanup this function, provide a framework for shutting down threads in a 2 step 
    *       functionality. Sometimes a thread should only be triggered for shutdown and then
    *       removed from the list and cleaned up.
    * This is a workaround to be able to remove a thread from the thread list without calling
    * cl_thread_cleanup(). The thread list MUST be locked before calling this function.
    * Also cl_thread_cleanup() MUST be called for the thread after removing it from the list 
    */

   if (thread == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* remove thread from list */
   return cl_thread_list_del_thread(list_p, thread);
}

cl_thread_settings_t *cl_thread_list_get_thread_by_id(cl_raw_list_t *list_p, int thread_id) {  /* CR check */
   cl_thread_list_elem_t *elem = nullptr;
   cl_thread_settings_t *thread_config = nullptr;

   for (elem = cl_thread_list_get_first_elem(list_p); elem != nullptr; elem = cl_thread_list_get_next_elem(elem)) {
      if (elem->thread_config->thread_id == thread_id) {
         thread_config = elem->thread_config;
         return thread_config;
      }
   }
   return thread_config;
}

cl_thread_settings_t *cl_thread_list_get_thread_by_self(cl_raw_list_t *list_p, pthread_t *thread) {  /* CR check */
   cl_thread_list_elem_t *elem = nullptr;
   cl_thread_settings_t *thread_config = nullptr;

   for (elem = cl_thread_list_get_first_elem(list_p); elem != nullptr; elem = cl_thread_list_get_next_elem(elem)) {
      if (pthread_equal(*(elem->thread_config->thread_pointer), *thread) == 0) {
         thread_config = elem->thread_config;
         return thread_config;
      }
   }
   return thread_config;
}

cl_thread_settings_t *cl_thread_list_get_thread_by_name(cl_raw_list_t *list_p, char *thread_name) {  /* CR check */
   cl_thread_list_elem_t *elem = nullptr;
   cl_thread_settings_t *thread_config = nullptr;

   if (thread_name == nullptr) {
      return nullptr;
   }

   for (elem = cl_thread_list_get_first_elem(list_p); elem != nullptr; elem = cl_thread_list_get_next_elem(elem)) {
      if (strcmp(elem->thread_config->thread_name, thread_name) == 0) {
         thread_config = elem->thread_config;
         return thread_config;
      }
   }
   return thread_config;
}

cl_thread_settings_t *cl_thread_list_get_first_thread(cl_raw_list_t *list_p) {  /* CR check */

   cl_thread_settings_t *thread_config = nullptr;
   cl_thread_list_elem_t *elem = nullptr;

   elem = cl_thread_list_get_first_elem(list_p);
   if (elem) {
      thread_config = elem->thread_config;
   }
   return thread_config;
}

cl_thread_list_elem_t *cl_thread_list_get_first_elem(cl_raw_list_t *list_p) {  /* CR check */
   cl_raw_list_elem_t *raw_elem = cl_raw_list_get_first_elem(list_p);
   if (raw_elem) {
      return (cl_thread_list_elem_t *) raw_elem->data;
   }
   return nullptr;
}

cl_thread_list_elem_t *cl_thread_list_get_next_elem(cl_thread_list_elem_t *elem) {  /* CR check */
   cl_raw_list_elem_t *next_raw_elem = nullptr;
   cl_raw_list_elem_t *raw_elem = nullptr;

   if (elem) {
      raw_elem = elem->raw_elem;
      next_raw_elem = cl_raw_list_get_next_elem(raw_elem);
      if (next_raw_elem) {
         return (cl_thread_list_elem_t *) next_raw_elem->data;
      }
   }
   return nullptr;
}

#if 0
static cl_thread_list_elem_t* cl_thread_list_get_last_elem(cl_raw_list_t* list_p, cl_thread_list_elem_t* elem) {
   cl_raw_list_elem_t* last_raw_elem = nullptr;
 
   if (elem != nullptr) {
      cl_raw_list_elem_t* raw_elem = elem->raw_elem;
      last_raw_elem = cl_raw_list_get_last_elem(raw_elem);
      if (last_raw_elem) {
         return (cl_thread_list_elem_t*) last_raw_elem->data;
      }
   }
   return nullptr;
}
#endif
