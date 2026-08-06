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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Per-thread "last error" storage
 *
 * One @ref _sge_err_object_t per thread, held in thread-local storage and
 * created on first use. Only the most recent error survives — #sge_err_set
 * overwrites both the id and the message.
 */

#include <cstdio>
#include <cstring>

#include "uti/sge_dstring.h"
#include "uti/sge_err.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_stdlib.h"

#define ERR_LAYER TOP_LAYER ///< rmon layer this module logs under

#define SGE_ERR_MAX_MESSAGE_LENGTH 256 ///< message buffer size; longer texts are truncated

/// The error slot of one thread
struct _sge_err_object_t {
   sge_err_t id;                              ///< the recorded error, #SGE_ERR_SUCCESS when clear
   char message[SGE_ERR_MAX_MESSAGE_LENGTH];  ///< the formatted message, truncated to fit
};

/// The error slot of one thread; see @ref _sge_err_object_t
typedef struct _sge_err_object_t sge_err_object_t;

static pthread_once_t sge_err_once = PTHREAD_ONCE_INIT;
static pthread_key_t sge_err_key;

/* destructor function that will be called when a thread ends */
static void
sge_err_destroy(void *state) {
   sge_free(&state);
}

/* init function that initializes the key that holds the pointer
 * to thread local storrage for this module */
static void
sge_err_once_init() {
   pthread_key_create(&sge_err_key, sge_err_destroy);
}

/* initialization function used to initialize thread local storrage */
static void
sge_err_object_init(sge_err_object_t *object) {
   object->id = SGE_ERR_SUCCESS;
   object->message[0] = '\0';
}

/* function that returns thread local storrage for this module */
static bool
sge_err_get_object(sge_err_object_t **object) {
   bool ret = true;

   DENTER(ERR_LAYER);
   *object = (sge_err_object_t *)pthread_getspecific(sge_err_key);
   if (*object == nullptr) {
      sge_err_object_t *new_object = (sge_err_object_t *) sge_malloc(sizeof(sge_err_object_t));
      SGE_ASSERT(new_object != nullptr);
      int pthread_ret = pthread_setspecific(sge_err_key, (void *) new_object);

      if (pthread_ret == 0) {
         sge_err_object_init(new_object);
         *object = new_object;
      } else {
         ERROR("pthread_setspecific failed to initialize sge_err_object_t in %s\n", __func__);
         ocs::TerminationManager::trigger_abort();
      }
   }
   DRETURN(ret);
}

/* local function that sets the error id and the error message (format + variable arguments) */
static void
sge_err_vset(sge_err_t id, const char *format, va_list args) {
   sge_err_object_t *err_obj = nullptr;

   DENTER(ERR_LAYER);
   sge_err_get_object(&err_obj);
   err_obj->id = id;
   vsnprintf(err_obj->message, SGE_ERR_MAX_MESSAGE_LENGTH, format, args);
   DRETURN_VOID;
}

/* initialization function that has to be called before threads are spawned */
static void
sge_err_init() {
   DENTER(ERR_LAYER);
   pthread_once(&sge_err_once, sge_err_once_init);
   DRETURN_VOID;
}

/**
 * @brief Creates the thread-local key before `main()` runs
 *
 * Exists only for the side effect of its constructor. A single static instance
 * is defined below; do not remove it, and do not instantiate it anywhere else.
 */
class ErrorThreadInit {
public:
   /// Runs the one-time `pthread_key_create()` for this module
   ErrorThreadInit() {
      sge_err_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static ErrorThreadInit error_obj{};


/**
 * @brief Record an error for the calling thread
 *
 * Replaces whatever was in the thread's slot. A nullptr @p format leaves the
 * previous error untouched rather than clearing it.
 *
 * @param id the kind of error
 * @param format `printf` style format for the message, truncated to
 *        #SGE_ERR_MAX_MESSAGE_LENGTH
 * @param ... the format's arguments
 */
void
sge_err_set(sge_err_t id, const char *format, ...) {
   va_list args;

   DENTER(ERR_LAYER);
   if (format != nullptr) {
      va_start(args, format);
      sge_err_vset(id, format, args);
      va_end(args);
   }
   DRETURN_VOID;
}

/**
 * @brief Read back the calling thread's error
 *
 * Does nothing if @p id or @p message is nullptr, or @p size is 0. When no
 * error is recorded, @p id becomes #SGE_ERR_SUCCESS and @p message an empty
 * string.
 *
 * @param pos unused — only the most recent error is kept, so there is no
 *        position to select. Kept for source compatibility.
 * @param[out] id the recorded error kind
 * @param[out] message buffer receiving the message, truncated to @p size
 * @param size size of @p message in bytes
 */
void
sge_err_get(uint32_t pos, sge_err_t *id, char *message, size_t size) {
   DENTER(ERR_LAYER);
   if (id != nullptr && message != nullptr && size > 0) {
      sge_err_object_t *err_obj = nullptr;

      sge_err_get_object(&err_obj);
      if (err_obj->id != SGE_ERR_SUCCESS) {
         *id = err_obj->id;
         sge_strlcpy(message, err_obj->message, size);
      } else {
         *id = SGE_ERR_SUCCESS;
         message[0] = '\0';
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Has the calling thread recorded an error?
 *
 * @return true when the thread's slot holds anything other than
 *         #SGE_ERR_SUCCESS
 */
bool
sge_err_has_error() {
   sge_err_object_t *err_obj = nullptr;
   bool ret;

   DENTER(ERR_LAYER);
   sge_err_get_object(&err_obj);
   ret = (err_obj->id != SGE_ERR_SUCCESS) ? true : false;
   DRETURN(ret);
}

/**
 * @brief Clear the calling thread's error
 *
 * Resets the id to #SGE_ERR_SUCCESS. The message text is left in the buffer,
 * but #sge_err_get will not return it while the id is #SGE_ERR_SUCCESS.
 */
void
sge_err_clear() {
   sge_err_object_t *err_obj = nullptr;

   DENTER(ERR_LAYER);
   sge_err_get_object(&err_obj);
   err_obj->id = SGE_ERR_SUCCESS;
   DRETURN_VOID;
}

