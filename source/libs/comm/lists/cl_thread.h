#pragma once
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

/** @file
 * @brief The commlib's thread wrapper
 *
 * A thin layer over pthreads that gives every thread a name, an id, a state
 * and an event condition it can sleep on, so that the commlib and the
 * qmaster can list their threads and wake one of them without knowing what
 * it does.
 *
 * ## Writing a thread function
 *
 * A thread implementation receives a #cl_thread_settings_t pointer and must
 * bracket itself:
 *
 * @code
 * void *timeout_thread_main(void *t_conf) {
 *    auto *thread_config = (cl_thread_settings_t *)t_conf;
 *    pthread_cleanup_push((void *)cl_thread_default_cleanup_function, (void *)thread_config);
 *
 *    // ... initialisation
 *
 *    cl_thread_func_startup(thread_config);
 *
 *    // ... thread main
 *
 *    cl_thread_func_cleanup(thread_config);
 *    pthread_cleanup_pop(0);
 *    return nullptr;
 * }
 * @endcode
 *
 * #cl_thread_func_startup releases the creator, which is waiting on the
 * startup condition; without it #cl_thread_setup times out. The
 * `pthread_cleanup_push` is what keeps the state correct when the thread is
 * cancelled rather than returning.
 */

/** @name Thread states
 *
 * The value of #cl_thread_settings_type::thread_state.
 * @{
 */
#define CL_THREAD_STARTING 1   ///< Created, has not reached #cl_thread_func_startup yet
#define CL_THREAD_RUNNING  2   ///< Doing work
#define CL_THREAD_WAITING  3   ///< Sleeping in #cl_thread_wait_for_event
#define CL_THREAD_EXIT     4   ///< Left its main function
#define CL_THREAD_CANCELED 5   ///< Cancelled rather than returned
#define CL_THREAD_CREATOR  6   ///< Not a created thread at all: the one that called #cl_thread_setup
/** @} */




/** @brief Called when a thread ends, however it ends
 *
 * @param thread_config the thread's settings
 */
typedef void  (*cl_thread_cleanup_func_t)(cl_thread_settings_t *thread_config);

/** @brief What kind of thread this is
 *
 * The commlib does not act on most of these; they exist so that a thread can
 * be identified in a listing, and so that the qmaster's threads can be told
 * apart in `qping -info`. Only #CL_TT_COMMLIB has behaviour attached, and it
 * must not be given to a thread created outside the commlib.
 */
typedef enum cl_thread_type_def {
   CL_TT_UNDEFINED = 0,  ///< Unset
   CL_TT_CREATOR,        ///< The `main()` thread, which created the others
   CL_TT_COMMLIB,        ///< A commlib thread - never set this on a thread created elsewhere
   CL_TT_LISTENER,       ///< qmaster: accepts GDI requests
   CL_TT_WORKER,         ///< qmaster: carries them out
   CL_TT_READER,         ///< qmaster: serves read-only requests
   CL_TT_EVENT_MASTER,   ///< qmaster: distributes events
   CL_TT_EVENT_MIRROR,   ///< qmaster: keeps a mirrored copy of the master lists
   CL_TT_SCHEDULER,      ///< qmaster: the scheduler
   CL_TT_SIGNALER,       ///< qmaster: turns signals into events
   CL_TT_UNUSED,         ///< qmaster: unused
   CL_TT_TIMER,          ///< qmaster: recurring tasks
   CL_TT_IJS,            ///< Interactive job support
   CL_TT_IJS_REGISTER,   ///< Interactive job support, registration
   CL_TT_USER1,          ///< Free for an application
   CL_TT_USER2,          ///< Free for an application
   CL_TT_USER3,          ///< Free for an application
   CL_TT_USER4,          ///< Free for an application
   CL_TT_USER5           ///< Free for an application
} cl_thread_type_t;


/** @brief Everything the commlib knows about one thread
 *
 * Each thread holds a pointer to its own, reachable from inside the thread
 * with #cl_thread_get_thread_config.
 */
struct cl_thread_settings_type {
#ifdef CL_DO_COMMLIB_DEBUG
   struct timeval           thread_last_cancel_test_time;   ///< When the thread last called #cl_thread_func_testcancel - a thread that stops testing can no longer be cancelled
#endif
   char *thread_name;             ///< Name of the thread, as it appears in logs and in `qping -info`
   int thread_id;                 ///< Id of the thread
   int thread_state;              ///< One of the `CL_THREAD_*` states
   unsigned long thread_event_count;   ///< How often #cl_thread_wait_for_event has returned
   cl_raw_list_t *thread_log_list;     ///< The log list to append to, may be nullptr
   pthread_t *thread_pointer;          ///< The pthread itself
   cl_thread_condition_t *thread_event_condition;     ///< What the thread sleeps on and is woken through
   cl_thread_condition_t *thread_startup_condition;   ///< What the creator waits on until #cl_thread_func_startup signals it
   cl_thread_cleanup_func_t thread_cleanup_func;      ///< Called when the thread ends
   cl_thread_type_t thread_type;       ///< What kind of thread this is
   void *thread_user_data;             ///< Free for the thread's own use
};

/** @brief A condition variable with its mutex and a trigger counter
 *
 * The counter is what makes a wake-up that arrives before the wait not get
 * lost: #cl_thread_wait_for_thread_condition returns at once when triggers
 * are pending rather than sleeping through them.
 */
struct cl_thread_condition_type {
   pthread_mutex_t *thread_mutex_lock;   ///< Guards the condition variable
   pthread_cond_t *thread_cond_var;      ///< What a waiting thread sleeps on

   pthread_mutex_t *trigger_count_mutex; ///< Guards `trigger_count`
   unsigned long trigger_count;          ///< Pending #cl_thread_trigger_thread_condition calls
};


int cl_thread_setup(cl_thread_settings_t *thread_config,
                    cl_raw_list_t *log_list,
                    const char *name,
                    int id,
                    void *(*start_routine)(void *),
                    cl_thread_cleanup_func_t cleanup_func,
                    void *user_data,
                    cl_thread_type_t thread_type);

int cl_thread_cleanup(cl_thread_settings_t *thread_config);

cl_thread_settings_t *cl_thread_get_thread_config();

int cl_thread_set_thread_config(cl_thread_settings_t *thread_config);

int cl_thread_unset_thread_config();

int cl_thread_shutdown(cl_thread_settings_t *thread_config);

const char *cl_thread_get_state(cl_thread_settings_t *thread_config);

const char *cl_thread_convert_state_id(int thread_state);

int cl_thread_join(cl_thread_settings_t *thread_config);

void cl_thread_cleanup_global_thread_config_key();

int cl_thread_wait_for_event(cl_thread_settings_t *thread_config, long sec, long micro_sec);

int cl_thread_trigger_event(cl_thread_settings_t *thread_config);

int cl_thread_clear_events(cl_thread_settings_t *thread_config);


int cl_thread_create_thread_condition(cl_thread_condition_t **condition);

int cl_thread_delete_thread_condition(cl_thread_condition_t **condition);

int cl_thread_wait_for_thread_condition(cl_thread_condition_t *condition, long sec, long micro_sec);

int cl_thread_trigger_thread_condition(cl_thread_condition_t *condition, int do_broadcast);

int cl_thread_clear_triggered_conditions(cl_thread_condition_t *condition);


int cl_thread_func_startup(cl_thread_settings_t *thread_config);

int cl_thread_func_testcancel(cl_thread_settings_t *thread_config);

int cl_thread_func_cleanup(cl_thread_settings_t *thread_config);

void cl_thread_default_cleanup_function(cl_thread_settings_t *thread_config);






