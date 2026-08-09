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

#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <pthread.h>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_stdlib.h"

#include "spool/berkeleydb/msg_spoollib_berkeleydb.h"
#include "spool/berkeleydb/sge_bdb_types.h"

#include <cinttypes>

/** @brief Everything one spooling rule needs to reach its Berkeley DB
 *
 * One of these per rule, reached through the opaque #bdb_info handle. The
 * environment and the database handles live here and are shared between all
 * threads; only the transaction handle is per thread, and it is kept in
 * `bdb_connection` under #_bdb_info::key.
 */
struct _bdb_info {
   pthread_mutex_t   mtx;                 ///< Guards this object; taken by #bdb_lock_info
   pthread_key_t     key;                 ///< Thread specific key under which each thread finds its connection

   const char *      path;                ///< The database path

   DB_ENV *          env;                 ///< Global database environment
   DB **             db;                  ///< Global database handles, one per #bdb_database

   uint64_t            next_clear;          ///< When the transaction log is next trimmed, see #BERKELEYDB_CLEAR_INTERVAL
   uint64_t            next_checkpoint;     ///< When the cache is next written to disk, see #BERKELEYDB_CHECKPOINT_INTERVAL
   bool              recover;             ///< Run recovery when the environment is opened
};

/** @brief The handles that belong to one thread
 *
 * Stored under the thread specific key of #_bdb_info and created on first use
 * by `bdb_init_connection()`, so that a thread never has to be told to
 * initialise itself.
 *
 * @warning Only `txn` is live. `env` and `db` are set to nullptr on creation
 *          and freed on destruction and are never read or assigned a real
 *          handle in between - #bdb_get_env and #bdb_get_db both return the
 *          shared ones from #_bdb_info. They are what is left of Berkeley
 *          DB's RPC mode, which needed a per thread environment; the doc
 *          comments on those accessors still described it. The array
 *          `bdb_init_connection()` allocates for `db` is therefore one
 *          allocation per thread that nothing ever uses.
 */
typedef struct bdb_connection {
   DB_ENV *    env;                 ///< Dead - see the warning above
   DB **        db;                 ///< Dead - see the warning above
   DB_TXN *    txn;                 ///< The running transaction of this thread
} bdb_connection;

static void
bdb_init_connection(bdb_connection *con);

static void 
bdb_destroy_connection(void *connection);

/**
 * @brief Create Berkeley DB specific data structures
 *
 * Creates and initializes an object describing the connection to a
 * Berkeley DB database and holding database and transaction handles.
 * Transaction handles are thread specific.
 * Path is
 * - the absolute path to a Database in case of local spooling
 *
 * @param path path to the database
 *
 * @return pointer to a newly created and initialized structure
 *
 * @note MT-NOTE: bdb_create() is MT safe
 */
bdb_info
bdb_create(const char *path)
{
   int ret, i;
   bdb_info info = (bdb_info) sge_malloc(sizeof(struct _bdb_info));

   pthread_mutex_init(&(info->mtx), nullptr);
   ret = pthread_key_create(&(info->key), bdb_destroy_connection);
   if (ret != 0) {
      fprintf(stderr, "can't initialize key for thread local storage: %s\n", strerror(ret));
   }
   info->path   = path;
   info->env    = nullptr;

   info->db     = (DB **)sge_malloc(BDB_ALL_DBS * sizeof(DB *));
   for (i = 0; i < BDB_ALL_DBS; i++) {
      info->db[i] = nullptr;
   }

   info->next_clear = 0;
   info->next_checkpoint = 0;
   info->recover = false;

   return info;
}

/**
 * @brief Release the state of one spooling rule
 *
 * Releases the per-rule bdb_info allocated in bdb_create: deletes the
 * pthread key, frees the strdup'd path and the DB handle array, destroys
 * the mutex, and frees the struct. Worker threads that still hold a
 * per-thread bdb_connection must have had their pthread-key destructor
 * fired first; ordinarily that happens at thread exit.
 *
 * @param info the handle, set to nullptr on return
 */
void
bdb_destroy(bdb_info *info)
{
   if (info == nullptr || *info == nullptr) {
      return;
   }

   pthread_key_delete((*info)->key);
   pthread_mutex_destroy(&((*info)->mtx));

   if ((*info)->path != nullptr) {
      /* path was strdup'd by spool_berkeleydb_create_context and handed
       * to bdb_create; the struct stores it typed as const for its
       * external getters, so cast const away for the free.
       */
      sge_free((void *)&((*info)->path));
   }

   if ((*info)->db != nullptr) {
      sge_free(&((*info)->db));
   }

   sge_free(info);
}

/*
* initialize thread local storage for a connection.
*  NOTES
*     MT-NOTE: bdb_init_connection() is MT safe 
*/
static void
bdb_init_connection(bdb_connection *con)
{
   int i;

   con->env = nullptr;

   con->db     = (DB **)sge_malloc(BDB_ALL_DBS * sizeof(DB *));
   for (i = 0; i < BDB_ALL_DBS; i++) {
      con->db[i] = nullptr;
   }

   con->txn = nullptr;
}

/*
* destroy the thread local storage for a connection
*  NOTES
*     MT-NOTE: bdb_destroy_connection() is MT safe 
*/
static void
bdb_destroy_connection(void *connection)
{
   /* Nothing to do here in principle.
    * Transactions and database connections shall be closed by calling the 
    * shutdown function.
    * But we can generate an error, if there is still something open.
    */
   bdb_connection *con = (bdb_connection *)connection;

   DENTER(TOP_LAYER);

   if (con->txn != nullptr) {
      /* error */
   }

   if (con->db != nullptr) {
      sge_free(&(con->db));
      /* error */
   }

   if (con->env != nullptr) {
      /* error */
   }
   
   DRETURN_VOID;
}

/**
 * @brief Get the database path
 *
 * Returns the path to a Berkeley DB database.
 * If the RPC mechanism is used, this is the last component of the path.
 *
 * @param info the database object
 *
 * @return path to the database.
 *
 * @note MT-NOTE: bdb_get_path() is MT safe
 */
const char *
bdb_get_path(bdb_info info)
{
   return info->path;
}

/**
 * @brief Get Berkeley DB database environment
 *
 * Returns the Berkeley DB database environment set earlier using
 * bdb_set_env().
 * The environment is shared between all threads.
 *
 * @param info the database object
 *
 * @return the database environment
 *
 * @note MT-NOTE: bdb_get_env() is MT safe
 *
 * @see #bdb_set_env
 */
DB_ENV *
bdb_get_env(bdb_info info)
{
   DB_ENV *env = nullptr;

   env = info->env;

   return env;
}

/**
 * @brief Get Berkeley DB database handle
 *
 * Returns the handle stored earlier with #bdb_set_db.
 *
 * @param info the database object
 * @param database which of the two databases
 *
 * @return the database handle, or nullptr if it is not open
 *
 * @note MT-NOTE: bdb_get_db() is MT safe
 *
 * @see #bdb_set_db
 */
DB *
bdb_get_db(bdb_info info, const bdb_database database)
{
   DB *db = nullptr;

   db = info->db[database];

   return db;
}

/**
 * @brief Get a transaction handle
 *
 * Returns a transaction handle set earlier with bdb_set_txn().
 * Each thread can have one transaction open.
 *
 * @param info the database object
 *
 * @return a transaction handle
 *
 * @note MT-NOTE: bdb_get_txn() is MT safe
 *
 * @see #bdb_set_txn
 */
DB_TXN *
bdb_get_txn(bdb_info info)
{
   GET_SPECIFIC(bdb_connection, con, bdb_init_connection, info->key);
   return con->txn;
}

/** @brief When the transaction log is next due to be trimmed
 * @param info the handle
 * @return the time, as an absolute timestamp
 */
uint64_t
bdb_get_next_clear(bdb_info info)
{
   return info->next_clear;
}

/** @brief When the database is next due to be checkpointed
 * @param info the handle
 * @return the time, as an absolute timestamp
 */
uint64_t
bdb_get_next_checkpoint(bdb_info info)
{
   return info->next_checkpoint;
}

/** @brief Whether recovery runs when the environment is opened
 * @param info the handle
 * @return true if recovery is requested
 */
bool
bdb_get_recover(bdb_info info) 
{
   return info->recover;
}

/**
 * @brief Set the Berkeley DB environment
 *
 * Sets the Berkeley DB environment. It is shared between all threads.
 *
 * @param info the database object
 * @param env the environment handle to set
 *
 * @note MT-NOTE: bdb_set_env() is MT safe
 *
 * @see #bdb_get_env
 */
void
bdb_set_env(bdb_info info, DB_ENV *env)
{
   info->env  = env;
}

/**
 * @brief Set a Berkeley DB database handle
 *
 * Sets the Berkeley DB database handle. It is shared between all threads.
 *
 * @param info the database object
 * @param db the database handle to store
 * @param database which of the two databases it is
 *
 * @note MT-NOTE: bdb_set_db() is MT safe
 *
 * @see #bdb_get_db
 */
void
bdb_set_db(bdb_info info, DB *db, const bdb_database database)
{
   info->db[database]  = db;
}

/**
 * @brief Store a transaction handle
 *
 * Stores a Berkeley DB transaction handle.
 * It is always stored in thread local storage.
 *
 * @param info the database object
 * @param txn the transaction handle to store
 *
 * @note MT-NOTE: bdb_set_txn() is MT safe
 *
 * @see #bdb_get_txn
 */
void
bdb_set_txn(bdb_info info, DB_TXN *txn)
{
   GET_SPECIFIC(bdb_connection, con, bdb_init_connection, info->key);
   con->txn = txn;
}

/** @brief Set when the transaction log is next trimmed
 * @param info the handle
 * @param next the time, as an absolute timestamp
 */
void
bdb_set_next_clear(bdb_info info, const uint64_t next)
{
   info->next_clear = next;
}

/** @brief Set when the database is next checkpointed
 * @param info the handle
 * @param next the time, as an absolute timestamp
 */
void
bdb_set_next_checkpoint(bdb_info info, const uint64_t next)
{
   info->next_checkpoint = next;
}

void
bdb_set_recover(bdb_info info, bool recover)
{
   info->recover = recover;
}

/**
 * @brief Get a meaningfull database name
 *
 * Return a meaningfull name for a database connection.
 * It contains the database path.
 * A dstring buffer has to be provided by the caller.
 *
 * @param info the database object
 * @param buffer buffer to hold the database name
 *
 * @return the database name
 *
 * @note MT-NOTE: bdb_get_dbname() is MT safe
 */
const char *
bdb_get_dbname(bdb_info info, dstring *buffer)
{
   const char *ret;
   const char *path   = bdb_get_path(info);

   if (path == nullptr) {
      ret = sge_dstring_copy_string(buffer, MSG_BERKELEY_DBNOTINITIALIZED);
   } else {
      ret = sge_dstring_copy_string(buffer, path);
   }

   return ret;
}

/** @brief Take the lock guarding a handle
 * @param info the handle
 */
void
bdb_lock_info(bdb_info info)
{
   sge_mutex_lock("bdb mutex", "bdb_lock_info", __LINE__, &(info->mtx));
}

/** @brief Release the lock guarding a handle
 * @param info the handle
 */
void
bdb_unlock_info(bdb_info info)
{
   sge_mutex_unlock("bdb mutex", "bdb_unlock_info", __LINE__, &(info->mtx));
}

/** @brief The name of one of the two databases, for messages and filenames
 * @param database which database
 * @return the name
 */
const char *
bdb_get_database_name(const bdb_database database)
{
   const char *ret;

   switch (database) {
      case BDB_CONFIG_DB:
         ret = "sge";
         break;
      case BDB_JOB_DB:
         ret = "sge_job";
         break;
      default:
         ret = nullptr;
         break;
   };

   return ret;
}

