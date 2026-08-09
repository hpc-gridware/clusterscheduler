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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The commlib's thread wrapper
 */

#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sys/time.h>
#include <pthread.h>

#include "uti/ocs_cond.h"
#include "uti/sge_stdlib.h"

#include "comm/lists/cl_lists.h"


/** @def CL_DO_THREAD_DEBUG
 * @brief Log every wait, trigger and cancel test of every thread
 *
 * Off by default; the traffic it produces is only useful when chasing a
 * thread that will not wake up or will not die.
 */
#define CL_DO_THREAD_DEBUG 0


/* this global is used to set the thread configuration data for each thread */
static pthread_mutex_t global_thread_config_key_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t global_thread_config_key;
static int global_thread_config_key_done = 0;

static int cl_thread_set_default_cancel_method();

/** @brief Allocate a condition variable with its mutex and counter
 * @param condition receives the new condition; must point at a nullptr
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_create_thread_condition(cl_thread_condition_t **condition) {
   cl_thread_condition_t *new_condition = nullptr;
   int ret_val;
   if (condition == nullptr) {
      /* no condition pointer pointer */
      return CL_RETVAL_PARAMS;
   }
   if (*condition != nullptr) {
      /* pointer pointer is already initialized (not free) */
      return CL_RETVAL_PARAMS;
   }

   new_condition = (cl_thread_condition_t *) sge_malloc(sizeof(cl_thread_condition_t));
   if (new_condition == nullptr) {
      return CL_RETVAL_MALLOC;
   }

   new_condition->thread_mutex_lock = (pthread_mutex_t *) sge_malloc(sizeof(pthread_mutex_t));
   if (new_condition->thread_mutex_lock == nullptr) {
      free(new_condition);
      return CL_RETVAL_MALLOC;
   }

   new_condition->trigger_count_mutex = (pthread_mutex_t *) sge_malloc(sizeof(pthread_mutex_t));
   if (new_condition->trigger_count_mutex == nullptr) {
      free(new_condition->thread_mutex_lock);
      free(new_condition);
      return CL_RETVAL_MALLOC;
   }
   new_condition->trigger_count = 0;


   new_condition->thread_cond_var = (pthread_cond_t *) sge_malloc(sizeof(pthread_cond_t));
   if (new_condition->thread_cond_var == nullptr) {
      free(new_condition->trigger_count_mutex);
      free(new_condition->thread_mutex_lock);
      free(new_condition);
      return CL_RETVAL_MALLOC;
   }

   if (pthread_mutex_init(new_condition->thread_mutex_lock, nullptr) != 0) {
      free(new_condition->trigger_count_mutex);
      free(new_condition->thread_mutex_lock);
      free(new_condition->thread_cond_var);
      free(new_condition);
      return CL_RETVAL_MUTEX_ERROR;
   }

   if (pthread_mutex_init(new_condition->trigger_count_mutex, nullptr) != 0) {
      ret_val = pthread_mutex_destroy(new_condition->thread_mutex_lock);
      if (ret_val == EBUSY) {
         return CL_RETVAL_MUTEX_CLEANUP_ERROR;
      }
      free(new_condition->trigger_count_mutex);
      free(new_condition->thread_mutex_lock);
      free(new_condition->thread_cond_var);
      free(new_condition);
      return CL_RETVAL_MUTEX_ERROR;
   }

   if (ocs::uti::condition_initialize(new_condition->thread_cond_var) != 0) {
      ret_val = pthread_mutex_destroy(new_condition->thread_mutex_lock);
      if (ret_val == EBUSY) {
         return CL_RETVAL_MUTEX_CLEANUP_ERROR;
      }
      ret_val = pthread_mutex_destroy(new_condition->trigger_count_mutex);
      if (ret_val == EBUSY) {
         return CL_RETVAL_MUTEX_CLEANUP_ERROR;
      }
      free(new_condition->trigger_count_mutex);
      free(new_condition->thread_mutex_lock);
      free(new_condition->thread_cond_var);
      free(new_condition);
      return CL_RETVAL_CONDITION_ERROR;
   }

   *condition = new_condition;
   return CL_RETVAL_OK;
}

/** @brief Free a condition variable and set the pointer to nullptr
 * @param condition the condition to free
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_delete_thread_condition(cl_thread_condition_t **condition) {
   int ret_val;
   if (condition == nullptr) {
      return CL_RETVAL_PARAMS;
   }
   if (*condition == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   if ((*condition)->thread_mutex_lock != nullptr) {
      ret_val = pthread_mutex_destroy((*condition)->thread_mutex_lock);
      if (ret_val == EBUSY) {
         return CL_RETVAL_MUTEX_CLEANUP_ERROR;
      }
   }

   if ((*condition)->trigger_count_mutex != nullptr) {
      ret_val = pthread_mutex_destroy((*condition)->trigger_count_mutex);
      if (ret_val == EBUSY) {
         return CL_RETVAL_MUTEX_CLEANUP_ERROR;
      }
   }

   if ((*condition)->thread_cond_var != nullptr) {
      ret_val = pthread_cond_destroy((*condition)->thread_cond_var);
      if (ret_val == EBUSY) {
         return CL_RETVAL_CONDITION_CLEANUP_ERROR;
      }
   }

   if ((*condition)->thread_mutex_lock != nullptr) {
      free((*condition)->thread_mutex_lock);
   }

   if ((*condition)->trigger_count_mutex != nullptr) {
      free((*condition)->trigger_count_mutex);
   }

   if ((*condition)->thread_cond_var != nullptr) {
      free((*condition)->thread_cond_var);
   }

   free(*condition);
   *condition = nullptr;
   return CL_RETVAL_OK;
}

/** @brief Sleep on a condition until it is triggered or the time runs out
 *
 * Returns at once when triggers are already pending, so a wake-up that
 * arrives before the wait is not lost.
 *
 * @param condition the condition to sleep on
 * @param sec seconds to wait at most
 * @param micro_sec microseconds to add to that
 * @return #CL_RETVAL_OK when triggered, #CL_RETVAL_CONDITION_WAIT_TIMEOUT
 *         when the time ran out, else a `CL_RETVAL_*` code
 */
int cl_thread_wait_for_thread_condition(cl_thread_condition_t *condition, long sec, long micro_sec) {
   int ret_val = CL_RETVAL_OK;

   if (condition == nullptr) {
      CL_LOG(CL_LOG_ERROR, "thread condition is nullptr");
      return CL_RETVAL_PARAMS;
   }


   /* lock condition mutex */
   if (pthread_mutex_lock(condition->thread_mutex_lock) != 0) {
      return CL_RETVAL_MUTEX_LOCK_ERROR;
   }


   pthread_mutex_lock(condition->trigger_count_mutex);
#if CL_DO_THREAD_DEBUG
   CL_LOG_INT(CL_LOG_DEBUG,"Trigger count:", (int)condition->trigger_count );
#endif

   if (condition->trigger_count == 0) {
      /* trigger count is zero, wait for trigger */

      pthread_mutex_unlock(condition->trigger_count_mutex);

      if (sec <= 0 && micro_sec <= 0) {
         /* do a not timed wait */
         if (pthread_cond_wait(condition->thread_cond_var, condition->thread_mutex_lock) != 0) {
            ret_val = CL_RETVAL_CONDITION_ERROR;
            pthread_mutex_lock(condition->trigger_count_mutex);
            condition->trigger_count = 0;
            pthread_mutex_unlock(condition->trigger_count_mutex);
         } else {
            /* triggered, do not count the awake trigger */
            pthread_mutex_lock(condition->trigger_count_mutex);
            if (condition->trigger_count > 0) {
               condition->trigger_count = condition->trigger_count - 1;
            }
            pthread_mutex_unlock(condition->trigger_count_mutex);
         }
      } else {
         int retcode = ocs::uti::condition_timedwait(condition->thread_cond_var, condition->thread_mutex_lock, sec, micro_sec);

         if (retcode == ETIMEDOUT) {
            ret_val = CL_RETVAL_CONDITION_WAIT_TIMEOUT;   /* timeout */
         } else if (retcode != 0) {
            ret_val = CL_RETVAL_CONDITION_ERROR;
            pthread_mutex_lock(condition->trigger_count_mutex);
            condition->trigger_count = 0;
            pthread_mutex_unlock(condition->trigger_count_mutex);
         }

         if (ret_val != CL_RETVAL_CONDITION_WAIT_TIMEOUT && ret_val != CL_RETVAL_CONDITION_ERROR) {
            /* triggered, do not count the awake trigger */
            pthread_mutex_lock(condition->trigger_count_mutex);
            if (condition->trigger_count > 0) {
               condition->trigger_count = condition->trigger_count - 1;
            }
            pthread_mutex_unlock(condition->trigger_count_mutex);
         }
      }
   } else {
      /* trigger count is > zero, do not trigger */
#if CL_DO_THREAD_DEBUG
      CL_LOG(CL_LOG_DEBUG,"Thread was triggerd before wait - continue");
      CL_LOG_INT(CL_LOG_DEBUG,"Trigger count:", (int)condition->trigger_count );
#endif
      condition->trigger_count = condition->trigger_count - 1;
      pthread_mutex_unlock(condition->trigger_count_mutex);
   }


   /* unlock condition mutex */
   if (pthread_mutex_unlock(condition->thread_mutex_lock) != 0) {
      if (ret_val == CL_RETVAL_OK) {
         ret_val = CL_RETVAL_MUTEX_UNLOCK_ERROR;
      }
   }

   return ret_val;
}

/** @brief Throw away the pending triggers of a condition
 * @param condition the condition to clear
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_clear_triggered_conditions(cl_thread_condition_t *condition) {
   if (condition == nullptr) {
      return CL_RETVAL_PARAMS;
   }
   /* increase trigger count */
   if (pthread_mutex_lock(condition->trigger_count_mutex) == 0) {
      condition->trigger_count = 0;
#if CL_DO_THREAD_DEBUG
      CL_LOG(CL_LOG_DEBUG,"cleared trigger count");
#endif
      if (pthread_mutex_unlock(condition->trigger_count_mutex) != 0) {
         CL_LOG(CL_LOG_ERROR, "could not unlock trigger_count_mutex");
         return CL_RETVAL_MUTEX_UNLOCK_ERROR;
      }
   } else {
      CL_LOG(CL_LOG_ERROR, "could not lock trigger_count_mutex");
      return CL_RETVAL_MUTEX_LOCK_ERROR;
   }
   return CL_RETVAL_OK;
}

/** @brief Wake a thread sleeping on a condition
 * @param condition the condition to trigger
 * @param do_broadcast wake every waiter rather than one
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_trigger_thread_condition(cl_thread_condition_t *condition, int do_broadcast) {
   int ret_val = CL_RETVAL_OK;

   if (condition == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* first lock condition mutex */
   if (pthread_mutex_lock(condition->thread_mutex_lock) != 0) {
      return CL_RETVAL_MUTEX_LOCK_ERROR;
   }

   /* increase trigger count */
   if (pthread_mutex_lock(condition->trigger_count_mutex) == 0) {
      condition->trigger_count = condition->trigger_count + 1;
      if (pthread_mutex_unlock(condition->trigger_count_mutex) != 0) {
         CL_LOG(CL_LOG_ERROR, "could not unlock trigger_count_mutex");
      }
   } else {
      CL_LOG(CL_LOG_ERROR, "could not lock trigger_count_mutex");
   }

   if (do_broadcast != 0) {
      /* signal condition with broadcast */
      if (pthread_cond_broadcast(condition->thread_cond_var) != 0) {
         ret_val = CL_RETVAL_CONDITION_SIGNAL_ERROR;
      }
   } else {
      /* signal condition with signal */
      if (pthread_cond_signal(condition->thread_cond_var) != 0) {
         ret_val = CL_RETVAL_CONDITION_SIGNAL_ERROR;
      }
   }

   /* unlock condition mutex */
   if (pthread_mutex_unlock(condition->thread_mutex_lock) != 0) {
      if (ret_val == CL_RETVAL_OK) {
         ret_val = CL_RETVAL_MUTEX_UNLOCK_ERROR;
      }
   }
   return ret_val;
}

/** @brief Release the thread specific key that carries the thread settings
 *
 * Called once at process shutdown, after every thread is gone.
 */
void cl_thread_cleanup_global_thread_config_key() {
   pthread_mutex_lock(&global_thread_config_key_mutex);
   if (global_thread_config_key_done == 1) {
      pthread_key_delete(global_thread_config_key);
      global_thread_config_key_done = 0;
   }
   pthread_mutex_unlock(&global_thread_config_key_mutex);
}


/* if no start_routine is given (=nullptr) the cl_thread_settings_t struct is
   filled, but no thread is started */
/** @brief Create a thread and wait until it has started
 *
 * Fills `thread_config`, creates the pthread and blocks on the startup
 * condition until the new thread calls #cl_thread_func_startup - so a
 * successful return means the thread is really running, not merely created.
 *
 * @param thread_config receives the settings; allocated by the caller
 * @param log_list the log list the thread appends to, may be nullptr
 * @param name name of the thread
 * @param id id of the thread
 * @param start_routine the thread's main function
 * @param cleanup_func called when the thread ends, may be nullptr
 * @param user_data free for the thread's own use
 * @param thread_type what kind of thread this is
 * @return #CL_RETVAL_OK when the thread is up,
 *         #CL_RETVAL_THREAD_START_TIMEOUT when it never signalled, else a
 *         `CL_RETVAL_*` code
 */
int cl_thread_setup(cl_thread_settings_t *thread_config,
                    cl_raw_list_t *log_list,
                    const char *name,
                    int id,
                    void *(*start_routine)(void *),
                    cl_thread_cleanup_func_t cleanup_func,
                    void *user_data,
                    cl_thread_type_t thread_type) {

   int retry = 0;
   int ret_val;

   if (thread_config == nullptr || name == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   /* clean thread structure */
   memset(thread_config, 0, sizeof(cl_thread_settings_t));

   thread_config->thread_name = strdup(name);    /* malloc */
   if (thread_config->thread_name == nullptr) {
      return CL_RETVAL_MALLOC;
   }

   thread_config->thread_log_list = log_list;

   thread_config->thread_id = id;

   thread_config->thread_type = thread_type;

   ret_val = cl_thread_create_thread_condition(&(thread_config->thread_event_condition));
   if (ret_val != CL_RETVAL_OK) {
      return ret_val;
   }

   thread_config->thread_state = CL_THREAD_STARTING;

   thread_config->thread_cleanup_func = cleanup_func;
   thread_config->thread_user_data = user_data;

   if (start_routine != nullptr) {
      thread_config->thread_pointer = (pthread_t *) sge_malloc(sizeof(pthread_t));
      if (thread_config->thread_pointer == nullptr) {
         return CL_RETVAL_MALLOC;
      }
   } else {
      thread_config->thread_pointer = nullptr;
   }


   ret_val = cl_thread_create_thread_condition(&(thread_config->thread_startup_condition));
   if (ret_val != CL_RETVAL_OK) {
      return ret_val;
   }

   pthread_mutex_lock(&global_thread_config_key_mutex);
   if (global_thread_config_key_done == 0) {
      pthread_key_create(&global_thread_config_key, nullptr);
      global_thread_config_key_done = 1;
   }
   pthread_mutex_unlock(&global_thread_config_key_mutex);

   if (start_routine != nullptr) {
      /* startup thread */
      if (pthread_create(thread_config->thread_pointer, nullptr, start_routine, thread_config) != 0) {
         return CL_RETVAL_THREAD_CREATE_ERROR;
      }


      /* wait for thread startup condition variable set */
      while (thread_config->thread_state == CL_THREAD_STARTING) {
         cl_thread_wait_for_thread_condition(thread_config->thread_startup_condition, 0, 100 * 1000);
         retry++;
         if (retry > 60) {
            return CL_RETVAL_THREAD_START_TIMEOUT;
         }
      }
   } else {
      /* this is creator thread setting */
      thread_config->thread_state = CL_THREAD_CREATOR;
      if (cl_thread_set_thread_config(thread_config) != CL_RETVAL_OK) {
         CL_LOG_STR(CL_LOG_ERROR, "cl_thread_set_thre_config() error for thread ->", thread_config->thread_name);
      }
   }

   CL_LOG_STR(CL_LOG_INFO, "setup complete for thread ->", thread_config->thread_name);
   return CL_RETVAL_OK;
}

/** @brief Wait for a thread to end
 * @param thread_config the thread's settings
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_join(cl_thread_settings_t *thread_config) {

   if (thread_config == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   cl_thread_trigger_event(thread_config);

   CL_LOG(CL_LOG_DEBUG, "cl_thread_join() waiting for thread ...");

   /* wait for thread's end of life */
   if (pthread_join(*(thread_config->thread_pointer), nullptr) != 0) {
      return CL_RETVAL_THREAD_JOIN_ERROR;
   }
   CL_LOG(CL_LOG_DEBUG, "cl_thread_join() done");
   return CL_RETVAL_OK;
}

/** @brief The settings of the calling thread
 *
 * Read from the thread specific key, so a thread can find itself without
 * being passed anything.
 *
 * @return the settings, or nullptr when called from a thread that was not
 *         set up through this module
 */
cl_thread_settings_t *cl_thread_get_thread_config() {
   /* cl_thread_setup  will set the thread specific data */
   cl_thread_settings_t *settings = nullptr;
   pthread_mutex_lock(&global_thread_config_key_mutex);
   if (global_thread_config_key_done != 0) {
      settings = (cl_thread_settings_t *) pthread_getspecific(global_thread_config_key);
   }
   pthread_mutex_unlock(&global_thread_config_key_mutex);
   return settings;
}

/** @brief Release what #cl_thread_setup allocated
 * @param thread_config the thread's settings
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_cleanup(cl_thread_settings_t *thread_config) {
   /* free all malloc()'ed pointers in cl_thread_settings_t structure */
   int ret_val;

   /* on CL_RETVAL_MUTEX_CLEANUP_ERROR or CL_RETVAL_CONDITION_CLEANUP_ERROR 
      the struct is NOT freed  !!! */

   if (thread_config == nullptr) {
      return CL_RETVAL_PARAMS;
   }


   if (thread_config->thread_event_condition != nullptr) {
      ret_val = cl_thread_delete_thread_condition(&(thread_config->thread_event_condition));
      if (ret_val != CL_RETVAL_OK) {
         return ret_val;
      }
   }

   if (thread_config->thread_startup_condition != nullptr) {
      ret_val = cl_thread_delete_thread_condition(&(thread_config->thread_startup_condition));
      if (ret_val != CL_RETVAL_OK) {
         return ret_val;
      }
   }

   if (thread_config->thread_name) {
      CL_LOG_STR(CL_LOG_DEBUG, "cleanup for thread ->", thread_config->thread_name);
   }

   /* destroy thread name */
   if (thread_config->thread_name) {
      free(thread_config->thread_name);
      thread_config->thread_name = nullptr;
   }

   /* destroy thread_pointer */
   if (thread_config->thread_pointer) {
      free(thread_config->thread_pointer);
      thread_config->thread_pointer = nullptr;
   }

   return CL_RETVAL_OK;
}

/** @brief Ask a thread to end
 *
 * Cancels the thread and wakes it, so that a thread sleeping in
 * #cl_thread_wait_for_event notices at its next cancel test.
 *
 * @param thread_config the thread's settings
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_shutdown(cl_thread_settings_t *thread_config) {
   int ret_val;

   if (thread_config == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   ret_val = pthread_cancel(*(thread_config->thread_pointer));

   switch (ret_val) {
      case 0:
         return CL_RETVAL_OK;
      case ESRCH:
         return CL_RETVAL_THREAD_NOT_FOUND;
      default:
         return CL_RETVAL_UNKNOWN;
   }
}

/** @brief Sleep until this thread is triggered or the time runs out
 * @param thread_config the thread's settings
 * @param sec seconds to wait at most
 * @param micro_sec microseconds to add to that
 * @return #CL_RETVAL_OK when triggered, #CL_RETVAL_CONDITION_WAIT_TIMEOUT
 *         when the time ran out, else a `CL_RETVAL_*` code
 */
int cl_thread_wait_for_event(cl_thread_settings_t *thread_config, long sec, long micro_sec) {

   int ret = CL_RETVAL_OK;

   if (thread_config == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   thread_config->thread_event_count = thread_config->thread_event_count + 1;
   thread_config->thread_state = CL_THREAD_WAITING;
#if CL_DO_THREAD_DEBUG
   CL_LOG(CL_LOG_DEBUG, "cl_thread_wait_for_event() start waiting ...");
#endif


   ret = cl_thread_wait_for_thread_condition(thread_config->thread_event_condition, sec, micro_sec);

   thread_config->thread_state = CL_THREAD_RUNNING;
#if CL_DO_THREAD_DEBUG
   CL_LOG(CL_LOG_DEBUG, "cl_thread_wait_for_event() wake up");
#endif


   return ret;
}

/** @brief The one letter state of a thread
 * @param thread_config the thread's settings
 * @return the letter, see #cl_thread_convert_state_id, or a message when
 *         `thread_config` is nullptr
 */
const char *cl_thread_get_state(cl_thread_settings_t *thread_config) {

   if (thread_config == nullptr) {
      return "got no thread config";
   }

   return cl_thread_convert_state_id(thread_config->thread_state);
}

/** @brief Turn a thread state into the letter that is shown for it
 *
 * These letters are what `qping -info` prints in its thread listing.
 *
 * @param thread_state one of the `CL_THREAD_*` states
 * @return `"r"` running, `"w"` waiting, `"d"` exited, `"s"` starting,
 *         `"c"` cancelled, `"m"` the main thread, `"?"` anything else
 */
const char *cl_thread_convert_state_id(int thread_state) {


   switch (thread_state) {
      case CL_THREAD_RUNNING:
         return "r";
      case CL_THREAD_WAITING:
         return "w";
      case CL_THREAD_EXIT:
         return "d";
      case CL_THREAD_STARTING:
         return "s";
      case CL_THREAD_CANCELED:
         return "c";
      case CL_THREAD_CREATOR:
         return "m"; /* m for Main thread */
      default:
         return "?";
   }
}

/** @brief Throw away the events queued for this thread
 * @param thread_config the thread's settings
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_clear_events(cl_thread_settings_t *thread_config) {
   if (thread_config == nullptr) {
      return CL_RETVAL_PARAMS;
   }
   return cl_thread_clear_triggered_conditions(thread_config->thread_event_condition);
}

/** @brief Wake this thread out of #cl_thread_wait_for_event
 * @param thread_config the thread's settings
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_trigger_event(cl_thread_settings_t *thread_config) {
   int ret_val;
   if (thread_config == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   ret_val = cl_thread_trigger_thread_condition(thread_config->thread_event_condition, 0);
#if CL_DO_THREAD_DEBUG
   CL_LOG(CL_LOG_DEBUG, "cl_thread_trigger_event() called");
#endif
   return ret_val;
}

/** @brief Let the thread be cancelled here, if a cancel is pending
 *
 * A thread that never calls this cannot be shut down: #cl_thread_shutdown
 * only marks it, and the mark takes effect at the next cancellation point.
 *
 * @param thread_config the thread's settings
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_func_testcancel(cl_thread_settings_t *thread_config) {
   if (thread_config == nullptr) {
      return CL_RETVAL_THREAD_CANCELSTATE_ERROR;
   }

#ifdef CL_DO_COMMLIB_DEBUG
   gettimeofday(&(thread_config->thread_last_cancel_test_time), nullptr);
#endif

   // pthread_cleanup_push() and pthread_cleanup_pop() must be used in the same { ... } context

   pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
   if (thread_config->thread_cleanup_func != nullptr) {
      constexpr int execute_pop = 0;
      // push user cleanup function
      pthread_cleanup_push(reinterpret_cast<void (*)(void *)>(thread_config->thread_cleanup_func), thread_config) ;
      // push default cleanup function
      pthread_cleanup_push(reinterpret_cast<void (*)(void *)>(cl_thread_default_cleanup_function), thread_config) ;
      // check if the thread was cancelled - if yes, pthread_testcancel() will not return
      pthread_testcancel();
      pthread_cleanup_pop(execute_pop);
      pthread_cleanup_pop(execute_pop);
   } else {
      constexpr int execute_pop = 0;
      // push default cleanup function
      pthread_cleanup_push(reinterpret_cast<void (*)(void *)>(cl_thread_default_cleanup_function), thread_config) ;
      // check if the thread was cancelled - if yes, pthread_testcancel() will not return
      pthread_testcancel();
      pthread_cleanup_pop(execute_pop);
   }
   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

   return CL_RETVAL_OK;
}

static int cl_thread_set_default_cancel_method() {
   /*
    * Setting thread cancel state and type to default values.
    * Commlib threads have a cancelation point:
    * The threads * have to call cl_thread_func_testcancel() in their mainloop!
    */
   pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);
   pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, nullptr);
   return CL_RETVAL_OK;
}

/** @brief Announce that the thread is up
 *
 * Signals the startup condition #cl_thread_setup is blocked on. Must be the
 * first thing a thread main function does after its initialisation.
 *
 * @param thread_config the thread's settings
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_func_startup(cl_thread_settings_t *thread_config) {
   int ret_val = CL_RETVAL_OK;
   if (thread_config == nullptr) {
      return CL_RETVAL_PARAMS;
   }

   cl_thread_set_default_cancel_method();

   /* set thread config data */
   if (cl_thread_set_thread_config(thread_config) != CL_RETVAL_OK) {
      printf("cl_thread_set_thread_config() error\n");
   }
   thread_config->thread_event_count = 0;

   ret_val = cl_thread_trigger_thread_condition(thread_config->thread_startup_condition, 0);

   if (ret_val == CL_RETVAL_OK) {
      thread_config->thread_state = CL_THREAD_RUNNING;
   }
   CL_LOG(CL_LOG_DEBUG, "cl_thread_func_startup() done");
   return ret_val;
}

/** @brief Make these settings the calling thread's own
 * @param thread_config the settings to store
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_set_thread_config(cl_thread_settings_t *thread_config) {

   cl_thread_set_default_cancel_method();

   pthread_mutex_lock(&global_thread_config_key_mutex);
   if (global_thread_config_key_done != 0) {
      if (pthread_setspecific(global_thread_config_key, thread_config) != 0) {
         pthread_mutex_unlock(&global_thread_config_key_mutex);
         return CL_RETVAL_THREAD_SETSPECIFIC_ERROR;
      }
      pthread_mutex_unlock(&global_thread_config_key_mutex);
      return CL_RETVAL_OK;
   }
   pthread_mutex_unlock(&global_thread_config_key_mutex);
   return CL_RETVAL_NOT_THREAD_SPECIFIC_INIT;
}

/** @brief Forget the calling thread's settings
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_unset_thread_config() {

   pthread_mutex_lock(&global_thread_config_key_mutex);
   if (global_thread_config_key_done != 0) {
      if (pthread_setspecific(global_thread_config_key, nullptr) != 0) {
         pthread_mutex_unlock(&global_thread_config_key_mutex);
         return CL_RETVAL_THREAD_SETSPECIFIC_ERROR;
      }
      pthread_mutex_unlock(&global_thread_config_key_mutex);
      return CL_RETVAL_OK;
   }
   pthread_mutex_unlock(&global_thread_config_key_mutex);
   return CL_RETVAL_NOT_THREAD_SPECIFIC_INIT;
}

/** @brief Announce that the thread is about to end
 * @param thread_config the thread's settings
 * @return #CL_RETVAL_OK on success, else a `CL_RETVAL_*` code
 */
int cl_thread_func_cleanup(cl_thread_settings_t *thread_config) {
   if (thread_config == nullptr) {
      return CL_RETVAL_PARAMS;
   }
   thread_config->thread_state = CL_THREAD_EXIT;
   CL_LOG(CL_LOG_DEBUG, "cl_thread_func_cleanup() called");
   cl_thread_unset_thread_config();
   return CL_RETVAL_OK;
}

/** @brief The cleanup handler a thread main function pushes
 *
 * Runs whether the thread returns or is cancelled, which is what keeps the
 * state correct in the cancelled case.
 *
 * @param thread_config the thread's settings
 */
void cl_thread_default_cleanup_function(cl_thread_settings_t *thread_config) {
   if (thread_config != nullptr) {
      thread_config->thread_state = CL_THREAD_CANCELED;
      CL_LOG(CL_LOG_INFO, "cl_thread_default_cleanup_function() called");
      /*  There is no need to unset thread config - This can result in
       *  unexpected cl_log_list - logging output.
       */
#if 0
      cl_thread_unset_thread_config();
#endif
   }
}


