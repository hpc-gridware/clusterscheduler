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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"
#include "uti/sge_rmon_macros.h"

#if 0
#define SGE_DEBUG_LOCK_TIME 
#endif

#if 1
#define SGE_USE_LOCK_FIFO
#endif

#if defined(LINUX)
#undef LOCK_READ
#undef LOCK_WRITE
#endif

typedef enum {
   LOCK_READ = 1, /* shared  */
   LOCK_WRITE = 2  /* exclusive */
} sge_lockmode_t;

typedef u_long64 sge_locker_t;

// locks to secure qmaster data stores
typedef enum {
   LOCK_GLOBAL = 0,     // master lock for the main DS
   LOCK_SCHEDULER,      // lock for the scheduler data store
   LOCK_READER,         // lock for the full read only snapshot providing a full copy (ro-requests)
   LOCK_LISTENER,       // lock for read only snapshot containing only auth data (listener-requests)
   LOCK_MASTER_CONF,    // TODO: we should get rid of this.

   NUM_OF_LOCK_TYPES    // Total number of locks
} sge_locktype_t;

void
sge_lock(sge_locktype_t aType, sge_lockmode_t aMode, const char *func, sge_locker_t anID);

bool
sge_try_lock(sge_locktype_t aType, sge_lockmode_t aMode, const char *func, sge_locker_t anID);

void
sge_unlock(sge_locktype_t aType, sge_lockmode_t aMode, const char *func, sge_locker_t anID);

sge_locker_t
sge_locker_id();

#define SGE_TRY_LOCK(type, mode) sge_try_lock(type, mode, __func__, sge_locker_id())
#define SGE_LOCK(type, mode) sge_lock(type, mode, __func__, sge_locker_id())
#define SGE_UNLOCK(type, mode) sge_unlock(type, mode, __func__, sge_locker_id())
