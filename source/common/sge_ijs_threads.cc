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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Thin wrappers around the commlib thread list, used by interactive job support
 */

#include "sge_ijs_threads.h"

/** @brief Create the thread list this module hangs its threads off
 *
 * @param thread_lib_handle receives the new thread list
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int thread_init_lib(THREAD_LIB_HANDLE **thread_lib_handle) {
   return cl_thread_list_setup(thread_lib_handle, "thread list");
}

/** @brief Destroy the thread list
 *
 * @param thread_lib_handle the list to destroy; set to `nullptr` afterwards
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int thread_cleanup_lib(THREAD_LIB_HANDLE **thread_lib_handle) {
   return cl_thread_list_cleanup(thread_lib_handle);
}

/** @brief Start a thread and add it to the thread list
 *
 * @param thread_lib_handle the thread list to add it to
 * @param thread receives the new thread
 * @param log_list where the thread writes its commlib log
 * @param thread_name the name the thread is known by in the list
 * @param thread_id the id the thread is known by in the list
 * @param thread_func the function the thread runs
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int create_thread(THREAD_LIB_HANDLE *thread_lib_handle,
                  THREAD_HANDLE **thread,
                  cl_raw_list_t *log_list,
                  const char *thread_name,
                  int thread_id,
                  void *thread_func(void *)) {
   return cl_thread_list_create_thread(thread_lib_handle, thread, log_list, 
                                       thread_name, thread_id,
                                       thread_func, nullptr, nullptr, CL_TT_IJS);
}

/** @brief Register the calling thread with the commlib
 *
 * @param log_list where the thread writes its commlib log
 * @param thread receives the thread settings
 * @param thread_name the name to register under
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int register_thread(cl_raw_list_t *log_list,
                    THREAD_HANDLE *thread,
                    const char *thread_name) {
   int ret;
   ret = cl_thread_setup(thread,
                         log_list, 
                         thread_name, 0, nullptr, nullptr, nullptr, CL_TT_IJS_REGISTER);
   if (ret == CL_RETVAL_OK) {
      ret = cl_thread_func_startup(thread);
   }
   return ret;
}

/** @brief Mark the thread as started, so a `thread_join()` can wait for it
 *
 * @param t_conf the thread settings of the calling thread
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int thread_func_startup(void *t_conf) {
   cl_thread_settings_t *thread_config;

   /* get pointer to cl_thread_settings_t struct */
   thread_config = (cl_thread_settings_t*)t_conf;
   return cl_thread_func_startup(thread_config);
}

/** @brief Mark the thread as finished
 *
 * @param t_conf the thread settings of the calling thread
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int thread_func_cleanup(void *t_conf) {
   cl_thread_settings_t *thread_config;

   /* get pointer to cl_thread_settings_t struct */
   thread_config = (cl_thread_settings_t*)t_conf;
   return cl_thread_func_cleanup(thread_config);
}


/** @brief Ask a thread to stop and wake it if it is waiting
 *
 * @param thread the thread to stop
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int thread_shutdown(THREAD_HANDLE *thread) {
   return cl_thread_shutdown(thread);
}

/** @brief Wake a thread that is in #thread_wait_for_event
 *
 * @param thread the thread to wake
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int thread_trigger_event(THREAD_HANDLE *thread) {
   return cl_thread_trigger_event(thread);
}

/** @brief Block until woken by #thread_trigger_event or until the timeout expires
 *
 * @param thread the calling thread
 * @param sec seconds to wait
 * @param msec additional milliseconds to wait
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int thread_wait_for_event(THREAD_HANDLE *thread, int sec, int msec) {
   return cl_thread_wait_for_event(thread, sec, msec);
}

/** @brief Wait for a thread to end
 *
 * @param thread the thread to wait for
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int thread_join(THREAD_HANDLE *thread) {
   return cl_thread_join(thread);
}


/** @brief Act on a pending cancellation request, if there is one
 *
 * @param t_conf the thread settings of the calling thread
 * @return `CL_RETVAL_OK` on success, a `CL_RETVAL_*` error otherwise
 */
int thread_testcancel(void *t_conf) {
   cl_thread_settings_t *thread_config;
   /* get pointer to cl_thread_settings_t struct */
   thread_config = (cl_thread_settings_t*)t_conf;
   return cl_thread_func_testcancel(thread_config);
}
