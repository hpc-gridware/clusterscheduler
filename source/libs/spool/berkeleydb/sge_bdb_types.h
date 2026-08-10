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
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/       

/** @file
 * @brief The Berkeley DB handles of one spooling rule
 */

#include <ctime>
#include <pthread.h>

#include <db.h>

#include "uti/sge_dstring.h"

/** @name How often the housekeeping runs
 *
 * #spool_berkeleydb_trigger uses these to decide what is due and to tell the
 * framework when to call it again.
 *
 * @todo (JG) These should be parameters of the Berkeley DB spooling rather
 *       than compiled in.
 * @{
 */
#define BERKELEYDB_CLEAR_INTERVAL 300   ///< Seconds between two trims of the transaction log
#define BERKELEYDB_CHECKPOINT_INTERVAL 60   ///< Seconds between two checkpoints, i.e. cache writes to disk
#define BERKELEYDB_MIN_INTERVAL BERKELEYDB_CHECKPOINT_INTERVAL   ///< The shorter of the two, and therefore how often the trigger has to run at all
/** @} */

/** @brief Which of the two databases an operation addresses
 *
 * Jobs are kept apart from everything else because they are written far more
 * often, so that job spooling cannot slow the configuration down or be held
 * up by it.
 */
typedef enum {
   BDB_CONFIG_DB = 0,   ///< Everything but jobs
   BDB_JOB_DB,          ///< Jobs and their tasks

   BDB_ALL_DBS          ///< Not a database: the number of them, for sizing arrays and for loops over both
} bdb_database;

/** @brief Opaque handle to the Berkeley DB state of one spooling rule
 *
 * The struct behind it is private to `sge_bdb_types.cc`; everything goes
 * through the accessors below. Environment and database handles are shared
 * between threads for a database on a local filesystem, while the
 * transaction handle is always thread specific - which is why
 * #bdb_get_txn and #bdb_set_txn reach into thread specific data while
 * #bdb_get_path does not.
 */
typedef struct _bdb_info *bdb_info;

bdb_info
bdb_create(const char *path);

void
bdb_destroy(bdb_info *info);

const char *
bdb_get_path(bdb_info info);

DB_ENV *
bdb_get_env(bdb_info info);

DB *
bdb_get_db(bdb_info info, const bdb_database database);

DB_TXN *
bdb_get_txn(bdb_info info);

uint64_t
bdb_get_next_clear(bdb_info info);

uint64_t
bdb_get_next_checkpoint(bdb_info info);

bool 
bdb_get_recover(bdb_info info);

void
bdb_set_env(bdb_info info, DB_ENV *env);

void
bdb_set_db(bdb_info info, DB *db, const bdb_database database);

void
bdb_set_txn(bdb_info info, DB_TXN *txn);

void
bdb_set_next_clear(bdb_info info, const uint64_t next);

void
bdb_set_next_checkpoint(bdb_info info, const uint64_t next);

void 
bdb_set_recover(bdb_info info, bool recover);

const char *
bdb_get_dbname(bdb_info info, dstring *buffer);

void
bdb_lock_info(bdb_info info);

void
bdb_unlock_info(bdb_info info);

const char *
bdb_get_database_name(const bdb_database database);
