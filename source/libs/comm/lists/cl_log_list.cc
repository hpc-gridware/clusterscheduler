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
#include <unistd.h>
#include <libgen.h>

#include "uti/sge_stdlib.h"

#include "comm/lists/cl_lists.h"
#include "comm/lists/cl_util.h"

static pthread_mutex_t global_cl_log_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static cl_raw_list_t *global_cl_log_list = nullptr;


/* this functions must lock / unlock the raw list manually */
static int cl_log_list_add_log(cl_raw_list_t *list_p, const char *thread_name, int line, const char *function_name,
                               const char *module_name, int thread_id, int thread_state, cl_log_t log_type,
                               const char *message, const char *parameter); /* CR check */

#if 0
/* this functions are not needed */
static cl_log_list_elem_t* cl_log_list_get_next_elem(cl_raw_list_t* list_p, cl_log_list_elem_t* elem);
static cl_log_list_elem_t* cl_log_list_get_last_elem(cl_raw_list_t* list_p, cl_log_list_elem_t* elem);
#endif

static int cl_log_list_add_log(cl_raw_list_t *list_p, const char *thread_name, int line, const char *function_name,
                               const char *module_name, int thread_id, int thread_state, cl_log_t log_type,
                               const char *message, const char *parameter) { /* CR check */
   cl_log_list_elem_t *new_elem = nullptr;
   int module_length = 0;
   char *mod_name_start1 = nullptr;
   char *mod_name_start = nullptr;


   if (list_p == nullptr || thread_name == nullptr || function_name == nullptr || module_name == nullptr || message == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* create new cl_log_list_elem_t element */
   new_elem = (cl_log_list_elem_t *) sge_malloc(sizeof(cl_log_list_elem_t));
   if (new_elem == nullptr) {
      return CL_RETVAL_MALLOC;
   }

   new_elem->log_parameter = nullptr;

   if (parameter != nullptr) {
      new_elem->log_parameter = strdup(parameter);
      if (new_elem->log_parameter == nullptr) {
         free(new_elem);
         return CL_RETVAL_MALLOC;
      }
   }

   new_elem->log_message = strdup(message);    /* malloc */
   if (new_elem->log_message == nullptr) {
      if (new_elem->log_parameter != nullptr) {
         free(new_elem->log_parameter);
      }
      free(new_elem);
      return CL_RETVAL_MALLOC;
   }

   new_elem->log_thread_name = strdup(thread_name); /* malloc */
   if (new_elem->log_thread_name == nullptr) {
      free(new_elem->log_message);
      if (new_elem->log_parameter != nullptr) {
         free(new_elem->log_parameter);
      }
      free(new_elem);
      return CL_RETVAL_MALLOC;
   }

   /* this is   b a s e n a m e ( )  */
   mod_name_start1 = strrchr((char *)module_name, '/');
   if (mod_name_start1 != nullptr) {
      mod_name_start = mod_name_start1;
      mod_name_start++;
   }
   if (mod_name_start == nullptr) {
      mod_name_start = (char *) module_name;
   }

   module_length = strlen(function_name) + strlen(mod_name_start) + cl_util_get_int_number_length(line) + 1 + 4;
   new_elem->log_module_name = sge_malloc(sizeof(char) * module_length);
   if (new_elem->log_module_name == nullptr) {
      free(new_elem->log_message);
      free(new_elem->log_thread_name);
      if (new_elem->log_parameter != nullptr) {
         free(new_elem->log_parameter);
      }
      free(new_elem);
      return CL_RETVAL_MALLOC;
   }
   snprintf(new_elem->log_module_name, module_length, "%s [%s/%d]", function_name, mod_name_start, line);

   new_elem->log_thread_id = thread_id;
   new_elem->log_thread_state = thread_state;
   new_elem->log_type = log_type;

   /* append elem and set elem pointer in new element */
   new_elem->raw_elem = cl_raw_list_append_elem(list_p, (void *) new_elem);

   if (new_elem->raw_elem == nullptr) {
      free(new_elem->log_message);
      free(new_elem->log_thread_name);
      if (new_elem->log_parameter != nullptr) {
         free(new_elem->log_parameter);
      }
      free(new_elem->log_module_name);
      free(new_elem);
      return CL_RETVAL_MALLOC;
   }
   return CL_RETVAL_OK;
}

const char *cl_log_list_convert_type_id(cl_log_t id) {  /* CR check */

   switch (id) {
      case CL_LOG_OFF:
         return "-";
      case CL_LOG_ERROR:
         return "E";
      case CL_LOG_WARNING:
         return "W";
      case CL_LOG_INFO:
         return "I";
      case CL_LOG_DEBUG:
         return "D";
      default:
         return "?";
   }
}

int cl_log_list_set_log_level(cl_raw_list_t *list_p, cl_log_t new_log_level) {  /* CR check */
   cl_log_list_data_t *ldata = nullptr;
   cl_log_t log_level = CL_LOG_OFF;
   char *env_sge_commlib_debug = nullptr;
   if (list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* check for environment variable SGE_COMMLIB_DEBUG */
   log_level = new_log_level;
   env_sge_commlib_debug = getenv("SGE_COMMLIB_DEBUG");
   if (env_sge_commlib_debug != nullptr) {
      log_level = (cl_log_t) cl_util_get_ulong_value(env_sge_commlib_debug);
   }

   if (log_level < CL_LOG_OFF || log_level > CL_LOG_DEBUG) {
      CL_LOG(CL_LOG_ERROR, "undefined log level");
      return CL_RETVAL_PARAMS;
   }

   ldata = (cl_log_list_data_t *)list_p->list_data;
   if (ldata != nullptr) {
      CL_LOG_STR(CL_LOG_INFO, "setting loglevel to", cl_log_list_convert_type_id(log_level));
      ldata->current_log_level = log_level;
      return CL_RETVAL_OK;
   }

   return CL_RETVAL_LIST_DATA_IS_NULL;
}

int cl_log_list_del_log(cl_raw_list_t *list_p) {  /* CR check */
   cl_log_list_elem_t *elem = nullptr;

   /* search for element */
   elem = cl_log_list_get_first_elem(list_p);

   /* remove elem from list and delete elem */
   if (elem) {
      cl_raw_list_remove_elem(list_p, elem->raw_elem);
      free(elem->log_parameter);
      free(elem->log_message);
      free(elem->log_thread_name);
      free(elem->log_module_name);
      free(elem);
      return CL_RETVAL_OK;
   }
   return CL_RETVAL_THREAD_NOT_FOUND;
}

cl_log_list_elem_t *cl_log_list_get_first_elem(cl_raw_list_t *list_p) {   /* CR check */
   cl_raw_list_elem_t *raw_elem = cl_raw_list_get_first_elem(list_p);
   if (raw_elem) {
      return (cl_log_list_elem_t *) raw_elem->data;
   }
   return nullptr;
}

#if 0
static cl_log_list_elem_t* cl_log_list_get_next_elem(cl_raw_list_t* list_p, cl_log_list_elem_t* elem) {
   cl_raw_list_elem_t* next_raw_elem = nullptr;
 
   if (elem != nullptr) {
      cl_raw_list_elem_t* raw_elem = elem->raw_elem;
      next_raw_elem = cl_raw_list_get_next_elem(raw_elem);
      if (next_raw_elem) {
         return (cl_log_list_elem_t*) next_raw_elem->data;
      }
   }
   return nullptr;
}

static cl_log_list_elem_t* cl_log_list_get_last_elem(cl_raw_list_t* list_p, cl_log_list_elem_t* elem) {
   cl_raw_list_elem_t* last_raw_elem = nullptr;
   
   if (elem != nullptr) {
      cl_raw_list_elem_t* raw_elem = elem->raw_elem;
      last_raw_elem = cl_raw_list_get_last_elem(raw_elem);
      if (last_raw_elem) {
         return (cl_log_list_elem_t*) last_raw_elem->data;
      }
   }
   return nullptr;
}
#endif


/* functions from header file */



/* setup log list 

   cl_raw_list_t** list_p       -> address to raw list pointer to setup 
   const char*     creator_name -> name of the creator thread (e.g. "application")
   int id                       -> creator id

   return values

      CL_RETVAL_OK   - list initalized, to cleanup call cl_log_list_cleanup()
      CL_RETVAL_XXX  - error code   

   CL_RETVAL_XXX integer
*/
int cl_log_list_setup(cl_raw_list_t **list_p, const char *creator_name, int creator_id,
                      cl_log_list_flush_method_t flush_type, cl_log_func_t flush_func) {
   int ret_val;
   char *env_sge_commlib_debug = nullptr;

   cl_log_list_data_t *ldata = nullptr;
   cl_thread_settings_t *creator_settings = nullptr;

   if (list_p == nullptr || creator_name == nullptr) {
      /* parameter error */
      return CL_RETVAL_PARAMS;
   }

   if (*list_p != nullptr) {
      /* the list is already initialized */
      return CL_RETVAL_PARAMS;
   }

   /* malloc creator and list data structures */
   creator_settings = (cl_thread_settings_t *) sge_malloc(sizeof(cl_thread_settings_t));
   if (creator_settings == nullptr) {
      return CL_RETVAL_MALLOC;
   }

   ldata = (cl_log_list_data_t *) sge_malloc(sizeof(cl_log_list_data_t));
   if (ldata == nullptr) {
      free(creator_settings);
      return CL_RETVAL_MALLOC;
   }
   ldata->list_creator_settings = nullptr;  /* init the list data to nullptr */

   /* all CL_LOG() macro function calls in startup phase of the log list
      are lost, because the list is not completey set up !!! */

   /* create a raw list */
   ret_val = cl_raw_list_setup(list_p, "log list", 1);/* enable list locking */
   if (ret_val != CL_RETVAL_OK) {
      free(creator_settings);
      free(ldata);
      return ret_val;
   }

   /* set list_data pointer to nullptr to disable logging */
   (*list_p)->list_data = nullptr;        /* do not log in setup phase */
   (*list_p)->list_type = CL_LOG_LIST;


   /* setup creator thread information */
   if ((ret_val = cl_thread_setup(creator_settings, *list_p, creator_name, creator_id, nullptr, nullptr, nullptr,
                                  CL_TT_CREATOR)) != CL_RETVAL_OK) {
      cl_thread_cleanup(creator_settings);
      free(creator_settings);
      free(ldata);
      cl_log_list_cleanup(list_p);
      return ret_val;
   }

   /* initialization done, now set list_data to enable logging */
   (*list_p)->list_data = ldata;
   ldata->list_creator_settings = creator_settings;
   ldata->current_log_level = CL_LOG_WARNING;  /* initial loglevel is CL_LOG_WARNING */
   ldata->flush_type = flush_type;
   if (flush_func != nullptr) {
      ldata->flush_function = *flush_func;
   } else {
      ldata->flush_function = cl_log_list_flush_list;
   }

   /* check for environment variable SGE_COMMLIB_DEBUG */
   env_sge_commlib_debug = getenv("SGE_COMMLIB_DEBUG");
   if (env_sge_commlib_debug != nullptr) {
      ldata->current_log_level = (cl_log_t) cl_util_get_ulong_value(env_sge_commlib_debug);
   }

   CL_LOG(CL_LOG_INFO, "cl_log_list_setup() complete");

   switch (ldata->flush_type) {
      case CL_LOG_FLUSHED:
         CL_LOG(CL_LOG_INFO, "log entries are flushed by application");
         break;
      case CL_LOG_IMMEDIATE:
         CL_LOG(CL_LOG_INFO, "log entires are flushed immediate");
         break;
   }

   pthread_mutex_lock(&global_cl_log_list_mutex);
   global_cl_log_list = *list_p;
   pthread_mutex_unlock(&global_cl_log_list_mutex);

   return CL_RETVAL_OK;
}

int cl_log_list_cleanup(cl_raw_list_t **list_p) {          /* CR check */
   int ret_val;
   int ret_val2;
   cl_log_list_data_t *ldata = nullptr;
   cl_thread_settings_t *creator_settings = nullptr;

   if (list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }
   if (*list_p == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   pthread_mutex_lock(&global_cl_log_list_mutex);
   global_cl_log_list = nullptr;
   pthread_mutex_unlock(&global_cl_log_list_mutex);

   /* set ldata and creator_settings */
   ldata = (cl_log_list_data_t *) (*list_p)->list_data;
   if (ldata != nullptr) {
      creator_settings = ldata->list_creator_settings;
   }

   /* cleanup creator thread */
   ret_val = cl_thread_cleanup(creator_settings);

   /* flush all list content to get list empty*/
   cl_log_list_flush_list(*list_p);

   /* free list data */
   free(ldata);
   (*list_p)->list_data = nullptr;

   /* free creator_settings */
   free(creator_settings);

   ret_val2 = cl_raw_list_cleanup(list_p);

   if (ret_val != CL_RETVAL_OK) {
      return ret_val;
   }
   return ret_val2;
}

#if 0

cl_thread_settings_t* cl_log_list_get_creator_thread(cl_thread_settings_t* thread_config) {  /* CR check */
   cl_raw_list_t*         log_list = nullptr;
   cl_thread_settings_t*  creator_thread = nullptr;
   cl_log_list_data_t*    ldata = nullptr;

   if (thread_config == nullptr) {
      return nullptr;
   }

   log_list = thread_config->thread_log_list;   
   if (log_list != nullptr) {
      ldata = log_list->list_data;
      if (ldata != nullptr) {
         creator_thread = ldata->list_creator_settings;
      }
   }
   return creator_thread;
}
#endif

int
cl_log_list_log(cl_log_t log_type, int line, const char *function_name, const char *module_name, const char *log_text,
                const char *log_param) {

   int ret_val;
   int ret_val2;
   cl_thread_settings_t *thread_config = nullptr;
   cl_log_list_data_t *ldata = nullptr;
   char help[64];

   if (log_text == nullptr || module_name == nullptr || function_name == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* get the thread configuration for the calling thread */
   thread_config = cl_thread_get_thread_config();

#if 0
   /* This is to get all debug messages at startup of log list, only enable this
      for debugging the log list code */
   if (thread_config == nullptr) {
      printf("cl_log_list_log(): cl_thread_get_thread_config() returns nullptr\n");
      if (log_text && module_name && log_param) { printf("%s:%s %s\n",function_name , log_text, log_param);}
   }
#endif


   if (thread_config != nullptr) {
      if (thread_config->thread_log_list == nullptr) {
         return CL_RETVAL_LOG_NO_LOGLIST;
      }


      /* check current log level */
      ldata = (cl_log_list_data_t *)thread_config->thread_log_list->list_data;
      if (ldata != nullptr) {
         if (ldata->current_log_level < log_type || ldata->current_log_level == CL_LOG_OFF) {
            return CL_RETVAL_OK;  /* message log doesn't match current log level or is switched off */
         }
      } else {
         return CL_RETVAL_OK;  /* never try logging without list data ( this happens on setting up the log list ) */
      }

      if ((ret_val = cl_raw_list_lock(thread_config->thread_log_list)) != CL_RETVAL_OK) {
         return ret_val;
      }

      snprintf(help, 64, "%s (t@%ld/pid=%ld)", thread_config->thread_name, (unsigned long) pthread_self(),
               (unsigned long) getpid());
      ret_val2 = cl_log_list_add_log(thread_config->thread_log_list,
                                     help,
                                     line,
                                     function_name,
                                     module_name,
                                     thread_config->thread_id,
                                     thread_config->thread_state,
                                     log_type,
                                     log_text,
                                     log_param);

      if ((ret_val = cl_raw_list_unlock(thread_config->thread_log_list)) != CL_RETVAL_OK) {
         return ret_val;
      }
      if (ldata != nullptr) {
         if (ldata->flush_type == CL_LOG_IMMEDIATE) {
            cl_log_list_flush();
         }
      }
      return ret_val2;
   } else {
      /* TODO 2 of 2 :
       *  This happens to threads of application which have not started 
       *  commlib setup function. A thread config should be provided for these
       *  threads, or the application should use commlib threads ( lists/thread module ) 
       */
      pthread_mutex_lock(&global_cl_log_list_mutex);
      /* This must be an application thread */
      if (global_cl_log_list != nullptr) {
         ldata = (cl_log_list_data_t *)global_cl_log_list->list_data;
         if (ldata != nullptr) {
            if (ldata->current_log_level < log_type || ldata->current_log_level == CL_LOG_OFF) {
               pthread_mutex_unlock(&global_cl_log_list_mutex);
               return CL_RETVAL_OK;  /* message log doesn't match current log level or is switched off */
            }
         } else {
            pthread_mutex_unlock(&global_cl_log_list_mutex);
            return CL_RETVAL_OK;  /* never try logging without list data ( this happens on setting up the log list ) */
         }
         if ((ret_val = cl_raw_list_lock(global_cl_log_list)) != CL_RETVAL_OK) {
            pthread_mutex_unlock(&global_cl_log_list_mutex);
            return ret_val;
         }

         snprintf(help, 64, "unknown (t@%ld/pid=%ld)", (unsigned long) pthread_self(), (unsigned long) getpid());
         ret_val2 = cl_log_list_add_log(global_cl_log_list,
                                        help,
                                        line,
                                        function_name,
                                        module_name,
                                        -1,
                                        -1,
                                        log_type,
                                        log_text,
                                        log_param);

         if ((ret_val = cl_raw_list_unlock(global_cl_log_list)) != CL_RETVAL_OK) {
            pthread_mutex_unlock(&global_cl_log_list_mutex);
            return ret_val;
         }
         if (ldata != nullptr) {
            if (ldata->flush_type == CL_LOG_IMMEDIATE) {
               cl_log_list_flush();
            }
         }
         pthread_mutex_unlock(&global_cl_log_list_mutex);
         return ret_val2;
      }
      pthread_mutex_unlock(&global_cl_log_list_mutex);
      return CL_RETVAL_LOG_NO_LOGLIST;
   }
}

int cl_log_list_log_ssi(cl_log_t log_type, int line, const char *function_name, const char *module_name,
                        const char *log_text,
                        const char *log_1, const char *log_2, int log_3) {
   int ret_val;
   char my_buffer[512];
   cl_thread_settings_t *thread_config = nullptr;
   cl_log_list_data_t *ldata = nullptr;
   const char *help_null = "nullptr";
   const char *log_param1 = nullptr;
   const char *log_param2 = nullptr;

   /* get the thread configuration for the calling thread */
   thread_config = cl_thread_get_thread_config();
   if (thread_config != nullptr) {
      if (thread_config->thread_log_list == nullptr) {
         return CL_RETVAL_LOG_NO_LOGLIST;
      }
      ldata = (cl_log_list_data_t *)thread_config->thread_log_list->list_data;
   } else {
      /* TODO 1 of 2 :
       *  This happens to threads of application which have not started 
       *  commlib setup function. A thread config should be provided for these
       *  threads, or the application should use commlib threads ( lists/thread module ) 
       */
      pthread_mutex_lock(&global_cl_log_list_mutex);
      if (global_cl_log_list != nullptr) {
         ldata = (cl_log_list_data_t *)global_cl_log_list->list_data;
      }
      pthread_mutex_unlock(&global_cl_log_list_mutex);
   }

   if (ldata != nullptr) {
      if (ldata->current_log_level < log_type || ldata->current_log_level == CL_LOG_OFF) {
         return CL_RETVAL_OK;  /* message log doesn't match current log level or is switched off */
      }
   } else {
      return CL_RETVAL_OK;  /* never try logging without list data ( this happens on setting up the log list ) */
   }
   if (log_1 == nullptr) {
      log_param1 = help_null;
   } else {
      log_param1 = log_1;
   }

   if (log_2 == nullptr) {
      log_param2 = help_null;
   } else {
      log_param2 = log_2;
   }
   snprintf(my_buffer, 512, "\"%s/%s/%d\"", log_param1, log_param2, log_3);
   ret_val = cl_log_list_log(log_type, line, function_name, module_name, log_text, my_buffer);
   return ret_val;
}

int cl_log_list_log_int(cl_log_t log_type, int line, const char *function_name, const char *module_name,
                        const char *log_text, int param) {
   int ret_val;
   char my_int_buffer[512];
   cl_thread_settings_t *thread_config = nullptr;
   cl_log_list_data_t *ldata = nullptr;

   /* get the thread configuration for the calling thread */
   thread_config = cl_thread_get_thread_config();
   if (thread_config != nullptr) {
      if (thread_config->thread_log_list == nullptr) {
         return CL_RETVAL_LOG_NO_LOGLIST;
      }
      ldata = (cl_log_list_data_t *)thread_config->thread_log_list->list_data;
   } else {
      /* TODO 1 of 2 :
       *  This happens to threads of application which have not started 
       *  commlib setup function. A thread config should be provided for these
       *  threads, or the application should use commlib threads ( lists/thread module ) 
       */
      pthread_mutex_lock(&global_cl_log_list_mutex);
      if (global_cl_log_list != nullptr) {
         ldata = (cl_log_list_data_t *)global_cl_log_list->list_data;
      }
      pthread_mutex_unlock(&global_cl_log_list_mutex);
   }

   if (ldata != nullptr) {
      if (ldata->current_log_level < log_type || ldata->current_log_level == CL_LOG_OFF) {
         return CL_RETVAL_OK;  /* message log doesn't match current log level or is switched off */
      }
   } else {
      return CL_RETVAL_OK;  /* never try logging without list data ( this happens on setting up the log list ) */
   }

   snprintf(my_int_buffer, 512, "%d", param);
   ret_val = cl_log_list_log(log_type, line, function_name, module_name, log_text, my_int_buffer);
   return ret_val;
}

int cl_log_list_flush() {        /* CR check */
   cl_raw_list_t *list_p = nullptr;
   cl_thread_settings_t *thread_config = nullptr;
   cl_log_list_data_t *ldata = nullptr;

   /* get the thread configuration for the calling thread */
   thread_config = cl_thread_get_thread_config();
   if (thread_config == nullptr) {
      /* This thread has had no setup by commlib, it must be an
       * application thread
       */
      list_p = global_cl_log_list;
   } else {
      /* This is a commlib internal thread use thread config */
      list_p = thread_config->thread_log_list;
   }

   if (list_p == nullptr) {
      return CL_RETVAL_LOG_NO_LOGLIST;
   }

   /* here we have a log list pointer, check if
    * there is a log function definition */
   if (list_p->list_data != nullptr) {
      ldata = (cl_log_list_data_t *) list_p->list_data;
      if (ldata->flush_function != nullptr) {
         return ldata->flush_function(list_p);
      }
   }
   return cl_log_list_flush_list(list_p);
}

int cl_log_list_flush_list(cl_raw_list_t *list_p) {        /* CR check */
   int ret_val;
   cl_log_list_elem_t *elem = nullptr;
   struct timeval now;


   if (list_p == nullptr) {
      return CL_RETVAL_LOG_NO_LOGLIST;
   }

   if ((ret_val = cl_raw_list_lock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }

   while ((elem = cl_log_list_get_first_elem(list_p)) != nullptr) {
      /* TODO: rework logging output (log to file? call foreign log function, got by function pointer ?) */

      gettimeofday(&now, nullptr);

      printf("%-76s|", elem->log_module_name);
      if (elem->log_parameter == nullptr) {
         printf("%10ld.%-6ld|%35s|%s|%s| %s\n",
                (long) now.tv_sec,
                (long) now.tv_usec,
                elem->log_thread_name,
                cl_thread_convert_state_id(elem->log_thread_state),
                cl_log_list_convert_type_id(elem->log_type),
                elem->log_message);
      } else {
         printf("%10ld.%-6ld|%35s|%s|%s| %s %s\n",
                (long) now.tv_sec,
                (long) now.tv_usec,
                elem->log_thread_name,
                cl_thread_convert_state_id(elem->log_thread_state),
                cl_log_list_convert_type_id(elem->log_type),
                elem->log_message,
                elem->log_parameter);
      }
      cl_log_list_del_log(list_p);
      fflush(stdout);
   }

   if ((ret_val = cl_raw_list_unlock(list_p)) != CL_RETVAL_OK) {
      return ret_val;
   }
   return CL_RETVAL_OK;
}






