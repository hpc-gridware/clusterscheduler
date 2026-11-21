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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Per-thread state of the cull library
 */

#include "tom/detail/ocs_State.h"

#include <cstring>
#include <cerrno>
#include <pthread.h>

#include "uti/sge_stdlib.h"
#include "uti/ocs_TerminationManager.h"



/**
 * @brief All per-thread state of the cull library
 *
 * Held in thread-local storage. #global_sort_order is here because `qsort()`
 * takes no user argument, so #lSortCompareUsingGlobal has no other way to
 * reach the order it should apply.
 */
typedef struct {
   int lerrno;                          ///< last cull error, one of the `LE*` codes
} cull_state_t;

static pthread_once_t cull_once = PTHREAD_ONCE_INIT;

static void cull_once_init();

// the file-local part of the thread state; the functions are defined below with their qualified names
namespace ocs::tom::detail {
   static pthread_key_t state_key; ///< the key of the per-thread state

   static void state_destroy(void *theState);

   static cull_state_t *state_getspecific(pthread_key_t aKey);

   static void state_init(cull_state_t *theState);
} // namespace ocs::tom::detail


/**
 * @brief The last cull error recorded on this thread
 *
 * @return one of the `LE*` codes, or 0 when none was recorded
 */
int ocs::tom::detail::state_get_lerrno() {
   cull_state_t *cull_state = nullptr;

   pthread_once(&cull_once, cull_once_init);

   cull_state = ocs::tom::detail::state_getspecific(ocs::tom::detail::state_key);

   return cull_state->lerrno;
}

/**
 * @brief Record a cull error on this thread
 *
 * @param i one of the `LE*` codes
 */
void ocs::tom::detail::state_set_lerrno(int i) {
   cull_state_t *cull_state = nullptr;

   pthread_once(&cull_once, cull_once_init);

   cull_state = ocs::tom::detail::state_getspecific(ocs::tom::detail::state_key);

   cull_state->lerrno = i;

   return;
}

/**
 * @brief One-time CULL initialization
 *
 * Create access key for thread local storage. Register cleanup function.
 *
 * This function must be called exactly once.
 *
 * @param void none
 *
 * @return none
 *
 * @note MT-NOTE: cull_once_init() is MT safe.
 */
static void cull_once_init() {
   pthread_key_create(&ocs::tom::detail::state_key, ocs::tom::detail::state_destroy);
   return;
} /* cull_once_init() */

/**
 * @brief Free thread local storage
 *
 * Free thread local storage.
 *
 * @param theState Pointer to memory which should be freed.
 *
 * @return none
 *
 * @note MT-NOTE: ocs::tom::detail::state_destroy() is MT safe.
 */
void ocs::tom::detail::state_destroy(void *theState) {
   sge_free(&theState);
}

/**
 * @brief Get thread local cull state
 *
 * Return thread local cull state.
 *
 * If a given thread does call this function for the first time, no thread
 * local cull state is available for this particular thread. In this case the
 * thread local cull state is allocated and set.
 *
 * @param aKey Key for thread local cull state
 *
 * @return Pointer to thread local cull state
 *
 * @note MT-NOTE: ocs::tom::detail::state_getspecific() is MT safe
 */
cull_state_t *ocs::tom::detail::state_getspecific(pthread_key_t aKey) {
   cull_state_t *cull_state = (cull_state_t *) pthread_getspecific(aKey);

   if (cull_state == nullptr) {
      int res = EINVAL;

      cull_state = (cull_state_t *) sge_malloc(sizeof(cull_state_t));
      ocs::tom::detail::state_init(cull_state);
      res = pthread_setspecific(ocs::tom::detail::state_key, (const void *) cull_state);

      if (0 != res) {
         fprintf(stderr, "pthread_set_specific(%s) failed: %s\n", "ocs::tom::detail::state_getspecific",
                 strerror(res));
         ocs::TerminationManager::trigger_abort();
      }
   }

   return cull_state;
} /* ocs::tom::detail::state_getspecific() */

/**
 * @brief Initialize CULL state
 *
 * Initialize CULL state.
 *
 * @param theState Pointer to CULL state structure.
 *
 * @return none
 *
 * @note MT-NOTE: ocs::tom::detail::state_init() is MT safe.
 */
void ocs::tom::detail::state_init(cull_state_t *theState) {
   theState->lerrno = 0;
}
