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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The list types the commlib is built out of
 *
 * Everything the commlib keeps a collection of - threads, log messages,
 * connections, endpoints, host aliases - is a #cl_raw_list_t. The raw list
 * provides the double linked chain, the element count and the mutex; each
 * user of it defines its own element struct that holds a
 * #cl_raw_list_elem_t back pointer plus whatever it needs, and its own
 * `*_data_t` for list wide state.
 *
 * That is why the element structs below all look alike and all carry a
 * `raw_elem` member: it is how an element finds its place in the chain.
 */

#include <sys/time.h>
#include <pthread.h>

#include <cinttypes>

/** @name The list types that need special handling
 *
 * Stored in #cl_raw_list_t::list_type. Only these two are distinguished,
 * because only they may be touched while the log list is being flushed;
 * every other list leaves the field at zero.
 * @{
 */
#define CL_THREAD_LIST 1   ///< A list of threads
#define CL_LOG_LIST    2   ///< The log message list
/** @} */

typedef struct cl_thread_settings_type cl_thread_settings_t;      ///< Forward declaration, see `cl_thread.h`
typedef struct cl_thread_condition_type cl_thread_condition_t;    ///< Forward declaration, see `cl_thread.h`
/***********************************************************************/
/* RAW_LIST */
/***********************************************************************/
typedef struct cl_raw_list_elem_type cl_raw_list_elem_t;   ///< One link in a #cl_raw_list_t

/** @brief One link in a #cl_raw_list_t
 *
 * `data` points at the caller's own element struct, which in turn points
 * back here through its `raw_elem` member.
 */
struct cl_raw_list_elem_type {
   void *data;                 ///< The caller's element
   cl_raw_list_elem_t *next;   ///< The next link, or nullptr at the end
   cl_raw_list_elem_t *last;   ///< The previous link, or nullptr at the start
};


/** @brief The double linked, mutex guarded list every commlib list is made of
 *
 * A caller never walks `first_elem` directly without holding the lock -
 * `cl_raw_list_lock()` and `cl_raw_list_unlock()` are part of the interface,
 * not an implementation detail, because the commlib is multi threaded
 * throughout.
 */
typedef struct cl_raw_list_type {
   char *list_name;                ///< Name of the list, used in log messages
   int list_type;                  ///< #CL_THREAD_LIST, #CL_LOG_LIST, or 0 for anything else
   pthread_mutex_t *list_mutex;    ///< Guards every member below
   unsigned long elem_count;       ///< Number of entries
   void *list_data;                ///< The user's list wide data, e.g. #cl_log_list_data_t

#ifdef CL_DO_COMMLIB_DEBUG
   char* last_locker;              ///< Name of the function that took the lock last
   unsigned long         lock_count;     ///< How often the list was locked
   unsigned long         unlock_count;   ///< How often it was unlocked; a difference means a leaked lock
#endif
   cl_raw_list_elem_t *first_elem;   ///< Head of the chain
   cl_raw_list_elem_t *last_elem;    ///< Tail of the chain
} cl_raw_list_t;

/***********************************************************************/


/***********************************************************************/
/* THREAD_LIST */
/***********************************************************************/
typedef struct cl_thread_list_elem_type cl_thread_list_elem_t;   ///< One thread in a thread list

/** @brief One thread in a thread list */
struct cl_thread_list_elem_type {
   cl_thread_settings_t *thread_config;   ///< The thread's settings
   cl_raw_list_elem_t *raw_elem;          ///< Back pointer into the raw list
};


/***********************************************************************/

/** @brief How much the commlib logs
 *
 * Each level includes the ones above it, so #CL_LOG_DEBUG produces everything.
 */
typedef enum cl_log_type {
   CL_LOG_OFF = 0,   ///< Nothing is logged
   CL_LOG_ERROR,     ///< Errors only
   CL_LOG_WARNING,   ///< Errors and warnings
   CL_LOG_INFO,      ///< Plus progress information
   CL_LOG_DEBUG      ///< Everything, including the message traffic
} cl_log_t;

/***********************************************************************/
/* LOG_LIST */
/***********************************************************************/
typedef struct cl_log_list_elem_type cl_log_list_elem_t;   ///< One queued log message

/** @brief One queued log message
 *
 * Logging does not write; it appends one of these and returns, so that a
 * thread holding a connection lock never blocks on the log file. The
 * flushing thread turns them into output later.
 */
struct cl_log_list_elem_type {
   char *log_parameter;      ///< Additional text appended to the message
   char *log_message;        ///< The message itself
   char *log_thread_name;    ///< Name of the thread that logged it
   int log_thread_id;        ///< Id of that thread
   int log_thread_state;     ///< The thread's state at the time
   cl_log_t log_type;        ///< The level it was logged at
   char *log_module_name;    ///< Name of the module it came from
   cl_raw_list_elem_t *raw_elem;   ///< Back pointer into the raw list
};

/** @brief One error the commlib wants the application to know about
 *
 * The commlib cannot print to the application's log, so it queues errors here
 * and the application drains the list. `cl_already_logged` is what keeps a
 * connection that fails once a second from filling the log.
 */
typedef struct cl_application_error_list_elem_t {
   cl_raw_list_elem_t *raw_elem;   ///< Back pointer into the raw list
   int cl_error;                   ///< The commlib code, resolve with #cl_get_error_text
   char *cl_info;                  ///< Additional error information
   struct timeval cl_log_time;     ///< When the message was added
   bool cl_already_logged;         ///< Set when this error was already logged within the last `CL_DEFINE_MESSAGE_DUP_LOG_TIMEOUT` seconds
   cl_log_t cl_err_type;           ///< The level to report it at
} cl_application_error_list_elem_t;

/** @brief When queued log messages are written out */
typedef enum cl_log_list_flush_method_type {
   CL_LOG_FLUSHED,    ///< Only when the application calls `cl_log_list_flush()`
   CL_LOG_IMMEDIATE   ///< On every log call, which gives up the point of queueing
} cl_log_list_flush_method_t;


/** @name The hooks an application hands to the commlib
 *
 * All optional. Each lets the application take over something the commlib
 * cannot do on its own, because it knows nothing about the application's log
 * file, its message tags or its state.
 * @{
 */
typedef int               (*cl_log_func_t)(cl_raw_list_t *log_list);   ///< Write out the queued log messages
typedef unsigned long     (*cl_app_status_func_t)(char **info_message);   ///< Report the application's state, answered to a `qping -info`
typedef void              (*cl_error_func_t)(const cl_application_error_list_elem_t *commlib_error);   ///< Take one queued commlib error
typedef const char *(*cl_tag_name_func_t)(unsigned long tag);   ///< Turn a numeric message tag into a name, for readable traces
typedef void              (*cl_app_debug_client_func_t)(int cl_connected, int debug_level);   ///< Told when a debug client attaches or detaches, and at which level
/** @} */


/** @brief The list wide state of a log list, kept in #cl_raw_list_t::list_data */
typedef struct cl_log_list_data_type {
   cl_log_t current_log_level;             ///< Messages above this level are dropped rather than queued
   cl_log_list_flush_method_t flush_type;  ///< When the queue is written out
   cl_log_func_t flush_function;           ///< The application's writer
   cl_thread_settings_t *list_creator_settings;   ///< Settings of the thread that created the list
} cl_log_list_data_t;
