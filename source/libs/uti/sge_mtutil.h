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
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2003 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2024-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Mutex and condition variable helpers with logging
 */

#include <pthread.h>
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_stdlib.h"

/**
 * @brief Fetch, and lazily create, the thread local data of a module
 *
 * Declares `_variable` as a `type *` holding the calling thread's value of the pthread key `_key`. When the
 * thread has no value yet, the data is made with `ocs::uti::create<type>()` (value-initialised, so a plain
 * struct starts zeroed), `init_func(_variable)` fills in the defaults and the pointer is stored with
 * `pthread_setspecific()`. A failing `pthread_setspecific()` aborts the process.
 *
 * The destructor registered with `pthread_key_create()` for `_key` must release the data with
 * `ocs::uti::destroy()`, never with `free()` or sge_free().
 *
 * @param type type of the thread local data
 * @param _variable name of the variable that is declared and receives the pointer
 * @param init_func function `void init_func(type *)` that initialises newly made data
 * @param _key the `pthread_key_t` of the module
 */
#define GET_SPECIFIC(type, _variable, init_func, _key) \
   auto _variable = static_cast<type *>(pthread_getspecific((pthread_key_t)(_key))); \
   if ((_variable) == nullptr) { \
      _variable = ocs::uti::create<type>(); \
      init_func(_variable); \
      int _ret = pthread_setspecific(_key, (void*)_variable); \
      if (_ret != 0) { \
         fprintf(stderr, "pthread_setspecific(%s) failed: %s\n", __func__, strerror(_ret)); \
         ocs::TerminationManager::trigger_abort(); \
      } \
   } \
   void()

extern void sge_mutex_lock(const char *, const char *, int, pthread_mutex_t *);

extern void sge_mutex_unlock(const char *, const char *, int, pthread_mutex_t *);

extern void sge_relative_timespec(signed long, struct timespec *);
