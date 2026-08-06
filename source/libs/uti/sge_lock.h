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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Reader/writer locks used to guard the master data stores
 */

#include <cinttypes>
#include "uti/sge_rmon_macros.h"

#if 0
/// define to log how long each lock is held
#define SGE_DEBUG_LOCK_TIME 
#endif

#if 1
/// define to hand out the locks in request order, so writers cannot starve
#define SGE_USE_LOCK_FIFO
#endif

#if defined(LINUX)
#undef LOCK_READ
#undef LOCK_WRITE
#endif

/** @brief How a lock is taken */
typedef enum {
   LOCK_READ = 1,   ///< shared: any number of readers, no writer
   LOCK_WRITE = 2   ///< exclusive: one writer, no readers
} sge_lockmode_t;

/** @brief Identifies the holder of a lock, see #sge_locker_id */
typedef uint64_t sge_locker_t;

/** @brief Which of the qmaster data stores a lock guards
 *
 * @todo `LOCK_MASTER_CONF` should go away.
 */
typedef enum {
   LOCK_GLOBAL = 0,     ///< the main data store
   LOCK_SCHEDULER,      ///< the scheduler data store
   LOCK_LISTENER,       ///< read only snapshot holding just the auth data, for listener requests
   LOCK_READER,         ///< read only snapshot holding a full copy, for read only requests
   LOCK_MASTER_CONF,    ///< the master configuration

   NUM_OF_LOCK_TYPES    ///< number of lock types, not a lock itself
} sge_locktype_t;

void
sge_lock(sge_locktype_t aType, sge_lockmode_t aMode, const char *func, sge_locker_t anID);

bool
sge_try_lock(sge_locktype_t aType, sge_lockmode_t aMode, const char *func, sge_locker_t anID);

void
sge_unlock(sge_locktype_t aType, sge_lockmode_t aMode, const char *func, sge_locker_t anID);

sge_locker_t
sge_locker_id();

/// try to take a lock, reporting the calling function automatically
#define SGE_TRY_LOCK(type, mode) sge_try_lock(type, mode, __func__, sge_locker_id())
/// take a lock, reporting the calling function automatically
#define SGE_LOCK(type, mode) sge_lock(type, mode, __func__, sge_locker_id())
/// release a lock, reporting the calling function automatically
#define SGE_UNLOCK(type, mode) sge_unlock(type, mode, __func__, sge_locker_id())
