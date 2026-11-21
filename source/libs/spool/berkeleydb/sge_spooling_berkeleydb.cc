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
 * @brief The berkeleydb spooling backend
 */

/** @def NO_SGE_COMPILE_DEBUG
 * @brief Compiles the rmon tracing out of this whole file
 */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

/** @def BDB_LAYER
 * @brief The rmon layer this module would trace on
 */
#define BDB_LAYER BASIS_LAYER

#include <cstring>
#include <ctime>
#include <utility>

#include "uti/sge_rmon_macros.h"
#include "uti/config_file.h"
#include "uti/sge_dstring.h"
#include "uti/sge_profiling.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_stdlib.h"
#include "uti/sge.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_pe_task.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_suser.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "tom/ocs_Raw.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PET.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_STU.h"
#include "sgeobj/tom/ocs_tom_SU.h"

#include "spool/sge_spooling_utilities.h"
#include "spool/msg_spoollib.h"

#include "spool/berkeleydb/msg_spoollib_berkeleydb.h"
#include "spool/berkeleydb/sge_bdb.h"
#include "spool/berkeleydb/sge_spooling_berkeleydb.h"

static const char *spooling_method = "berkeleydb";

/** @brief Report `"berkeleydb"` as this library's spooling method
 *
 * @return the string `"berkeleydb"`
 */
#ifdef SPOOLING_berkeleydb
const char *get_spooling_method()
#else
const char *get_berkeleydb_spooling_method()
#endif
{
   return spooling_method;
}

static bool
spool_berkeleydb_option_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::Element<ocs::tom::SPR> rule,
                             const char *option);

/**
 * @brief Create a berkeleydb spooling context
 *
 * Create a spooling context for the berkeleydb spooling.
 * Argv has as first (and currently only) parameter the Database URL.
 * It currently consists of the path to the database directory.
 *
 * The return type is fixed by #spooling_create_context_func (the dynamic
 * loader), so the context is handed over raw.
 *
 * @param answer_list to return error messages
 * @param args arguments to the spooling method, see above.
 *
 * @return on success, the new spooling context (SPC), else nullptr
 */
lListElem *
spool_berkeleydb_create_context(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *args) {
   DENTER(BDB_LAYER);

   ocs::tom::ElementPtr<ocs::tom::SPC> context;

   /* check input parameter */
   if (args != nullptr) {
      bdb_info info;
      char *path   = nullptr;

      /* create spooling context */
      context = ocs_spool_create_context(answer_list, "berkeleydb spooling");

      /* create rule and type for all objects spooled in the spool dir */
      const auto rule = spool_context_create_rule(answer_list, context.get(),
                                       "default rule",
                                       args,
                                       spool_berkeleydb_option_func,
                                       spool_berkeleydb_default_startup_func,
                                       spool_berkeleydb_default_shutdown_func,
                                       spool_berkeleydb_default_maintenance_func,
                                       spool_berkeleydb_trigger_func,
                                       spool_berkeleydb_transaction_func,
                                       spool_berkeleydb_default_list_func,
                                       spool_berkeleydb_default_read_func,
                                       spool_berkeleydb_default_read_keys_func,
                                       spool_berkeleydb_default_write_func,
                                       spool_berkeleydb_default_delete_func,
                                       spool_default_validate_func,
                                       spool_default_validate_list_func);

      /* parse arguments */
      path = strdup(args);

      DPRINTF("using database path %s\n", path);

      info = bdb_create(path);
      rule.set<SPR_clientdata>(info);
      const auto type = spool_context_create_type(answer_list, context.get(), SGE_TYPE_ALL);
      spool_type_add_rule(answer_list, type, rule, true);
   }

   DRETURN(context.release());
}

/**
 * @brief Setup
 *
 * @param answer_list to return error messages
 * @param rule the rule containing data necessary for the startup (e.g. path to the spool directory)
 * @param check check the spooling database
 *
 * @return true, if the startup succeeded, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 *       MT-NOTE: spool_berkeleydb_default_startup_func() is MT safe
 *
 * @see `spool_startup_context()`
 */
bool spool_berkeleydb_default_startup_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                           const ocs::tom::ConstElement<ocs::tom::SPR> rule, bool check) {
   DENTER(BDB_LAYER);

   bool ret = true;
   bdb_info info;

   info = (bdb_info)rule.get<SPR_clientdata>();

   ret = spool_berkeleydb_check_version(answer_list);

   if (ret) {
      ret = spool_berkeleydb_create_environment(answer_list, info);
   }

   /* we only open database, if check = true */
   if (ret && check) {
      ret = spool_berkeleydb_open_database(answer_list, info, false);
   }

   DRETURN(ret);
}

/**
 * @brief Shutdown spooling context
 *
 * Shuts down the context, e.g. the database connection.
 *
 * @param answer_list to return error messages
 * @param rule the rule containing data necessary for the shutdown (e.g. path to the spool directory)
 *
 * @return true, if the shutdown succeeded, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 *       MT-NOTE: spool_berkeleydb_default_shutdown_func() is MT safe
 *
 * @see `spool_shutdown_context()`
 */
bool spool_berkeleydb_default_shutdown_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                            const ocs::tom::ConstElement<ocs::tom::SPR> rule) {
   DENTER(BDB_LAYER);

   bool ret = true;

   bdb_info info;

   info = (bdb_info)rule.get<SPR_clientdata>();

   if (info == nullptr) {
      /* Already shut down (e.g., spool_free_context after an explicit
       * spool_shutdown_context) — nothing to do.
       */
      DRETURN(true);
   }

   ret = spool_berkeleydb_close_database(answer_list, info);
   bdb_destroy(&info);
   // the framework hands the rule over read-only, but the shutdown resets its client data, as it did before
   ocs::tom::Element<ocs::tom::SPR>::from_typed(const_cast<lListElem *>(rule.raw())).set<SPR_clientdata>(nullptr);

   DRETURN(ret);
}

/**
 * @brief Maintain database
 *
 * Maintains the database:
 *    - initialization
 *    - ...
 *
 * @param answer_list to return error messages
 * @param rule the rule containing data necessary for the maintenance (e.g. path to the spool directory)
 * @param cmd the command to execute
 * @param args arguments to the maintenance command
 *
 * @return true, if the maintenance succeeded, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 *       MT-NOTE: spool_berkeleydb_default_maintenance_func() is MT safe
 *
 * @see `spool_maintain_context()`
 */
bool spool_berkeleydb_default_maintenance_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                               const ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                               const spooling_maintenance_command cmd,
                                               const char *args) {
   DENTER(BDB_LAYER);

   bool ret = true;

   bdb_info info;

   info = (bdb_info)rule.get<SPR_clientdata>();

   switch (cmd) {
      case SPM_init:
         ret = spool_berkeleydb_open_database(answer_list, info, true);
         break;
      default:
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                 ANSWER_QUALITY_WARNING,
                                 "unknown maintenance command %d\n", cmd);
         ret = false;
         break;

   }

   DRETURN(ret);
}

/**
 * @brief Do recurring tasks
 *
 * Does recurring tasks for the Berkeley DB.
 * In case of spooling to a local filesystem
 * - regular checkpointing is done
 * - a cleanup of the transaction log is done
 *
 * @param answer_list used to return error messages
 * @param rule the spooling rule
 * @param trigger time when this trigger was due
 * @param next_trigger time when trigger shall be called again
 *
 * @return true on success, else false
 *
 * @note MT-NOTE: spool_berkeleydb_trigger_func() is MT safe
 */
bool spool_berkeleydb_trigger_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                   const ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                   uint64_t trigger, uint64_t *next_trigger) {
   DENTER(BDB_LAYER);

   bool ret = true;
   bdb_info info;

   info = (bdb_info)rule.get<SPR_clientdata>();
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_WARNING,
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              rule.get<SPR_url>());
      ret = false;

      /* nothing can be done - but set new trigger!! */
      *next_trigger = trigger + sge_gmt32_to_gmt64(BERKELEYDB_MIN_INTERVAL);
   }

   if (ret) {
      ret = spool_berkeleydb_check_reopen_database(answer_list, info);
   }

   if (ret) {
      ret = spool_berkeleydb_trigger(answer_list, info, trigger, next_trigger);
   }

   DRETURN(ret);
}

/**
 * @brief Start or end a transaction
 *
 * Starts or ends a transaction (depending on cmd).
 * Each thread of a process can have one open transaction.
 *
 * @param answer_list to return error messages
 * @param rule spooling rule
 * @param cmd the transaction command
 *
 * @return true on success, else false
 *
 * @note MT-NOTE: spool_berkeleydb_transaction_func() is MT safe
 */
bool spool_berkeleydb_transaction_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                       const ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                       spooling_transaction_command cmd) {
   DENTER(BDB_LAYER);

   bool ret = true;

   bdb_info info;

   info = (bdb_info)rule.get<SPR_clientdata>();
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_ERROR,
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              rule.get<SPR_url>());
      ret = false;
   }

   if (ret) {
      spool_berkeleydb_check_reopen_database(answer_list, info);
   }

   if (ret) {
      switch (cmd) {
         case STC_begin:
            ret = spool_berkeleydb_start_transaction(answer_list, info);
            break;
         case STC_commit:
            ret = spool_berkeleydb_end_transaction(answer_list, info, true);
            break;
         case STC_rollback:
            ret = spool_berkeleydb_end_transaction(answer_list, info, false);
            break;
         default:
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_ERROR,
                                    MSG_BERKELEY_TRANSACTIONEINVAL);
            ret = false;
            break;
      }
   }

   DRETURN(ret);
}

/**
 * @brief Read lists through berkeleydb spooling
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to be used
 * @param list target list
 * @param object_type object type
 *
 * @return true, on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 *       MT-NOTE: spool_berkeleydb_default_list_func() is MT safe
 *
 *       The caller has to make sure, that locking for the list
 *       parameter is done correctly!
 *
 * @see `spool_read_list()`
 */
bool spool_berkeleydb_default_list_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                        const ocs::tom::ConstElement<ocs::tom::SPT> type,
                                        const ocs::tom::ConstElement<ocs::tom::SPR> rule, lList **list,
                                        const sge_object_type object_type) {
   DENTER(BDB_LAYER);

   bool ret = true;

   const lDescr *descr;
   const char *table_name;
   bdb_info info;
   const auto master_suser_list = ocs::DataStore::master_list_rw<SGE_TYPE_SUSER>();

   info = (bdb_info)rule.get<SPR_clientdata>();
   descr = object_type_get_descr(object_type);
   table_name = object_type_get_name(object_type);

   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_WARNING,
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              rule.get<SPR_url>());
      ret = false;
   }

   if (descr == nullptr || list == nullptr ||
              table_name == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_WARNING,
                              MSG_SPOOL_SPOOLINGOFXNOTSUPPORTED_S,
                              object_type_get_name(object_type));
      ret = false;
   }

   if (ret) {
      ret = spool_berkeleydb_check_reopen_database(answer_list, info);
   }

   if (ret) {
      /* if no transaction was opened from outside, open a new one */
      dstring key_dstring;
      char key_buffer[MAX_STRING_SIZE];
      const char *key;

      sge_dstring_init(&key_dstring, key_buffer, sizeof(key_buffer));
      key = sge_dstring_sprintf(&key_dstring, "%s:", table_name);

      if (ret) {
         // the callback signature hands the list over as lList **: the reader owns it while it reads and gives it
         // back, so that the slot holds the list again during the tree walk below
         const auto read_into_list = [&](const bdb_database source) {
            ocs::tom::RawListPtr objects(std::exchange(*list, nullptr));
            const bool ok = spool_berkeleydb_read_list(answer_list, info, source, objects, descr, key);
            *list = objects.release();
            return ok;
         };
         switch (object_type) {
            case SGE_TYPE_QINSTANCE:
               break;
            case SGE_TYPE_CQUEUE:
               /* read all cluster queues */
               ret = read_into_list(BDB_CONFIG_DB);
               if (ret) {
                  const char *qinstance_table;
                  /* for all cluster queues: read queue instances */
                  qinstance_table = object_type_get_name(SGE_TYPE_QINSTANCE);
                  // the database holds cluster queues (CQ) under this key, so the list is taken without a check
                  for (const ocs::tom::Element<ocs::tom::CQ> queue : ocs::tom::List<ocs::tom::CQ>::from_typed(*list)) {
                     const char *qname = queue.get<CQ_name>();

                     key = sge_dstring_sprintf(&key_dstring, "%s:%s/",
                                               qinstance_table,
                                               qname);
                     // queue instances under this key; a list left behind by a failed read is freed
                     auto [read_ok, qinstances] =
                        ocs_spool_berkeleydb_read_list<ocs::tom::QU>(answer_list, info, BDB_CONFIG_DB, key);
                     ret = read_ok;
                     if (!ret) {
                        // stop at the first failure as the job loop does: a later success overwrote ret
                        // and qmaster started with a cluster queue missing its queue instances (CS-3121)
                        break;
                     }
                     if (qinstances) {
                        queue.set<CQ_qinstances>(std::move(qinstances));
                     }
                  }
               }
               break;
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
               break;
            case SGE_TYPE_JOB:
               /* read all jobs */
               ret = read_into_list(BDB_JOB_DB);
               if (ret) {
                  const char *ja_task_table;
                  /* for all jobs: read ja_tasks */
                  ja_task_table = object_type_get_name(SGE_TYPE_JATASK);
                  // the database holds jobs (JB), array tasks (JAT) and parallel tasks (PET) under
                  // these keys, so the lists are taken without a check
                  for (const ocs::tom::Element<ocs::tom::JB> job : ocs::tom::List<ocs::tom::JB>::from_typed(*list)) {
                     const uint32_t job_id = job.get<JB_job_number>();

                     key = sge_dstring_sprintf(&key_dstring, "%s:%10" sge_fuu32 ".", ja_task_table, job_id);
                     auto [read_ok, task_list] =
                        ocs_spool_berkeleydb_read_list<ocs::tom::JAT>(answer_list, info, BDB_JOB_DB, key);
                     ret = read_ok;
                     /* reading ja_tasks succeeded */
                     if (ret) {
                        /* we actually found ja_tasks for this job */
                        if (task_list) {
                           const char *pe_task_table;

                           job.set<JB_ja_tasks>(std::move(task_list));
                           pe_task_table = object_type_get_name(SGE_TYPE_PETASK);

                           for (const ocs::tom::Element<ocs::tom::JAT> ja_task : job.get<JB_ja_tasks>()) {
                              const uint32_t ja_task_id = ja_task.get<JAT_task_number>();
                              key = sge_dstring_sprintf(&key_dstring, "%s:%10" sge_fuu32 ".%10" sge_fuu32 ".",
                                                        pe_task_table, job_id, ja_task_id);

                              auto [pe_read_ok, pe_task_list] =
                                 ocs_spool_berkeleydb_read_list<ocs::tom::PET>(answer_list, info, BDB_JOB_DB, key);
                              ret = pe_read_ok;
                              if (ret) {
                                 if (pe_task_list) {
                                    ja_task.set<JAT_task_list>(std::move(pe_task_list));
                                 }
                              } else {
                                 break;
                              }
                           }
                        }
                     }
                     job_list_register_new_job(
                        ocs::DataStore::master_list<SGE_TYPE_JOB>(),
                        mconf_get_max_jobs(), 1);
                     suser_register_new_job(job, mconf_get_max_u_jobs(), 1, master_suser_list);
                     if (!ret) {
                        break;
                     }
                  }
               }
               break;
            default:
               ret = read_into_list(BDB_CONFIG_DB);
               break;
         }
      }
   }

   if (ret) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
      spooling_validate_func validate =
         (spooling_validate_func)rule.get<SPR_validate_func>();
      spooling_validate_list_func validate_list =
         (spooling_validate_list_func)rule.get<SPR_validate_list_func>();
#pragma GCC diagnostic pop

      /* validate each individual object */
      /* JG: TODO: is it valid to validate after reading all objects? */
      for (const ocs::tom::RawElement ep : ocs::tom::RawList(*list)) {
         ret = validate(answer_list, type, rule, ep, object_type);
         if (!ret) {
            /* error message has been created in the validate func */
            break;
         }
      }

      if (ret) {
         /* validate complete list */
         ret = validate_list(answer_list, type, rule, object_type);
      }
   }



   DRETURN(ret);
}

/**
 * @brief Read objects through berkeleydb spooling
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param key unique key specifying the object
 * @param object_type object type
 *
 * @return the object, if it could be read, else nullptr
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 *       MT-NOTE: spool_berkeleydb_default_read_func() is MT safe
 *
 * @see `spool_read_object()`
 */
lListElem *
spool_berkeleydb_default_read_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                   const ocs::tom::ConstElement<ocs::tom::SPT> type,
                                   const ocs::tom::ConstElement<ocs::tom::SPR> rule, const char *key,
                                   const sge_object_type object_type) {
   DENTER(BDB_LAYER);

   bool ret = true;
   lListElem *ep = nullptr;

   bdb_info info;

   info = (bdb_info)rule.get<SPR_clientdata>();
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_WARNING,
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              rule.get<SPR_url>());
      ret = false;
   }

   if (ret) {
      ret = spool_berkeleydb_check_reopen_database(answer_list, info);
   }

   if (ret) {
      bdb_database database = BDB_CONFIG_DB;

      switch (object_type) {
         case SGE_TYPE_JOBSCRIPT:
            {
               const char *exec_file;
               char *dup = strdup(key);
               char *db_key = jobscript_parse_key(dup, &exec_file);
               char *str;
               str = spool_berkeleydb_read_string(answer_list, info, BDB_JOB_DB,
                                                  db_key);

               if (str != nullptr) {
                  auto script = ocs::tom::ElementPtr<ocs::tom::STU>::create();
                  str = script->exchange<STU_name>(str);
                  ep = script.release();
               } else {
                  ret = false;
               }
               sge_free(&dup);
            }
            break;
         case SGE_TYPE_JATASK:
         case SGE_TYPE_PETASK:
         case SGE_TYPE_JOB:
            database = BDB_JOB_DB;
            /* no break, further handling in default branch */
         default:
            ep = spool_berkeleydb_read_object(answer_list, info, database, key).release();
            break;
      }
   }

   DRETURN(ep);
}

/**
 * @brief List the keys the database holds below a key
 *
 * The #spooling_read_keys_func of this backend, and the only implementation
 * of it in the tree - the flatfile backend leaves the callback nullptr, which
 * is why `spooledit list` only works against a Berkeley DB spool.
 *
 * @param answer_list to return error messages
 * @param rule the spooling rule
 * @param list receives the keys (`STU`)
 * @param key the key prefix to list below
 *
 * @return true on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 */
bool spool_berkeleydb_default_read_keys_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                             const ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                             ocs::tom::ListPtr<ocs::tom::STU> *list,
                                             const char *key) {
   DENTER(BDB_LAYER);

   bool ret = true;

   bdb_info info;

   info = (bdb_info)rule.get<SPR_clientdata>();

   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_WARNING,
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              rule.get<SPR_url>());
      ret = false;
   }

   if (ret) {
      ret = spool_berkeleydb_check_reopen_database(answer_list, info);
   }

   if (ret) {
      if (key == nullptr || strlen(key) == 0) {
         /* return all keys
          * - we first fetch all keys from the config db
          * - then append the all keys from the job db
          */
         ret = spool_berkeleydb_read_keys(answer_list, info, BDB_CONFIG_DB, list, "");
         if (ret) {
            ret = spool_berkeleydb_read_keys(answer_list, info, BDB_JOB_DB, list, "");
         }
      } else {
         /* return all keys starting with the given pattern
          * need to figure out the database
          * only the first letter(s) of the key need to be given, we have in the job database
          * J(OB), J(ATASK) - no J(C) in Open Cluster Scheduler yet
          * PET(ASK) - PE would be in the config database
          * all other keys are in the config db
          */
         bdb_database database;
         if (strncmp(key, "J", 1) == 0 ||
             strncmp(key, "PET", 3) == 0) {
            database = BDB_JOB_DB;
         } else {
            database = BDB_CONFIG_DB;
         }
         ret = spool_berkeleydb_read_keys(answer_list, info, database, list, key);
      }
   }

   DRETURN(ret);
}

/**
 * @brief Write objects through berkeleydb spooling
 *
 * Writes an object through the appropriate berkeleydb spooling functions.
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param object object to spool
 * @param key unique key
 * @param object_type object type
 *
 * @return true on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 *       MT-NOTE: spool_berkeleydb_default_write_func() is MT safe
 *
 * @see `spool_delete_object()`
 */
bool spool_berkeleydb_default_write_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                         const ocs::tom::ConstElement<ocs::tom::SPT> type,
                                         const ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                         ocs::tom::ConstRawElement object,
                                         const char *key,
                                         const sge_object_type object_type) {
   DENTER(BDB_LAYER);

   bool ret = true;
   bool local_transaction = false; /* did we start a transaction? */
   bdb_info info;

   DPRINTF("spool_berkeleydb_default_write_func called for %s with key %s\n",
           object_type_get_name(object_type), key != nullptr ? key : "<null>");

   info = (bdb_info)rule.get<SPR_clientdata>();
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_WARNING,
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              rule.get<SPR_url>());
      ret = false;
   }

   if (key == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_WARNING,
                              MSG_BERKELEY_NULLVALUEASKEY,
                              rule.get<SPR_url>());
      ret = false;
   }

   if (ret) {
      ret = spool_berkeleydb_check_reopen_database(answer_list, info);
   }

   if (ret) {
      /* if no transaction was opened from outside, open a new one */
      DB_TXN *txn = bdb_get_txn(info);
      if (txn == nullptr) {
         ret = spool_berkeleydb_start_transaction(answer_list, info);
         if (ret) {
            local_transaction = true;
         }
      }

      // CS-1597: for exec hosts only static load values shall be spooled: a copy without the dynamic ones is written
      ocs::tom::ElementPtr<ocs::tom::EH> exechost_copy;
      if (ret && object_type == SGE_TYPE_EXECHOST) {
         exechost_copy = ocs_spool_exechost_static_copy(ocs::tom::ConstElement<ocs::tom::EH>(object));
         object = exechost_copy.get();
      }

      if (ret) {
         switch (object_type) {
            case SGE_TYPE_JOB:
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
               {
                  uint32_t job_id, ja_task_id;
                  char *pe_task_id;
                  char *dup = strdup(key);
                  bool only_job;

                  job_parse_key(dup, &job_id, &ja_task_id, &pe_task_id, &only_job);

                  // a parallel task is written as it is, without a check, as before; the array task and the job
                  // writers take a sub-list out of the object, which checked its type already before
                  if (object_type == SGE_TYPE_PETASK) {
                     ret = spool_berkeleydb_write_pe_task(answer_list, info,
                                                          ocs::tom::ConstElement<ocs::tom::PET>::from_typed(object),
                                                          job_id, ja_task_id,
                                                          pe_task_id);
                  } else if (object_type == SGE_TYPE_JATASK) {
                     ret = spool_berkeleydb_write_ja_task(answer_list, info,
                                                          ocs::tom::ConstElement<ocs::tom::JAT>(object),
                                                          job_id, ja_task_id);
                  } else {
                     ret = spool_berkeleydb_write_job(answer_list, info, ocs::tom::ConstElement<ocs::tom::JB>(object),
                                                      job_id, ja_task_id, only_job);
                  }
                  sge_free(&dup);
               }
               break;
            case SGE_TYPE_JOBSCRIPT:
               {
                  const char *exec_file;
                  char *dup = strdup(key);
                  const char *db_key = jobscript_parse_key(dup, &exec_file);
                  const char *script = ocs::tom::ConstElement<ocs::tom::JB>(object).get<JB_script_ptr>();
                  /* switch script */
                  ret = spool_berkeleydb_write_string(answer_list, info,
                                                      BDB_JOB_DB,
                                                      db_key, script);
                  sge_free(&dup);
               }
               break;
            case SGE_TYPE_CQUEUE:
               ret = spool_berkeleydb_write_cqueue(answer_list, info, ocs::tom::ConstElement<ocs::tom::CQ>(object),
                                                   key);
               break;
            default:
               {
                  dstring dbkey_dstring;
                  char dbkey_buffer[MAX_STRING_SIZE];
                  const char *dbkey;

                  sge_dstring_init(&dbkey_dstring,
                                   dbkey_buffer, sizeof(dbkey_buffer));

                  dbkey = sge_dstring_sprintf(&dbkey_dstring, "%s:%s",
                                              object_type_get_name(object_type),
                                              key);

                  ret = spool_berkeleydb_write_object(answer_list, info,
                                                      BDB_CONFIG_DB,
                                                      object, dbkey);
               }
               break;
         }
      }

      exechost_copy.reset();

      /* spooling is done, now end the transaction, if we have an own one */
      if (local_transaction) {
         ret = spool_berkeleydb_end_transaction(answer_list, info, ret);
      }
   }

   DRETURN(ret);
}

/**
 * @brief Delete object in berkeleydb spooling
 *
 * Deletes an object in the berkeleydb spooling.
 *
 * @param answer_list to return error messages
 * @param type object type description
 * @param rule rule to use
 * @param key unique key
 * @param object_type object type
 *
 * @return true on success, else false
 *
 * @note This function should not be called directly, it is called by the
 *       spooling framework.
 *
 *       MT-NOTE: spool_berkeleydb_default_delete_func() is MT safe
 *
 * @see `spool_delete_object()`
 */
bool spool_berkeleydb_default_delete_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                          const ocs::tom::ConstElement<ocs::tom::SPT> type,
                                          const ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                          const char *key,
                                          const sge_object_type object_type) {
   DENTER(BDB_LAYER);

   bool ret = true;
   bool local_transaction = false; /* did we start a transaction? */
   const char *table_name;
   bdb_info info;

   dstring dbkey_dstring;
   char dbkey_buffer[MAX_STRING_SIZE];
   const char *dbkey;

   sge_dstring_init(&dbkey_dstring, dbkey_buffer, sizeof(dbkey_buffer));
   info = (bdb_info)rule.get<SPR_clientdata>();
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_WARNING,
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              rule.get<SPR_url>());
      ret = false;
   }

   if (ret) {
      ret = spool_berkeleydb_check_reopen_database(answer_list, info);
   }

   if (ret) {
      DB_TXN *txn = bdb_get_txn(info);
      /* if no transaction was opened from outside, open a new one */
      if (txn == nullptr) {
         ret = spool_berkeleydb_start_transaction(answer_list, info);
         if (ret) {
            local_transaction = true;
         }
      }

      if (ret) {
         switch (object_type) {
            case SGE_TYPE_CQUEUE:
               ret = spool_berkeleydb_delete_cqueue(answer_list, info, key);
               break;
            case SGE_TYPE_JOB:
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
               {
                  uint32_t job_id, ja_task_id;
                  char *pe_task_id;
                  char *dup = strdup(key);
                  bool only_job;

                  job_parse_key(dup, &job_id, &ja_task_id, &pe_task_id, &only_job);

                  if (pe_task_id != nullptr) {
                     dbkey = sge_dstring_sprintf(&dbkey_dstring, "%10" sge_fuu32 ".%10" sge_fuu32 " %s",
                                                 job_id, ja_task_id, pe_task_id);
                     ret = spool_berkeleydb_delete_pe_task(answer_list, info,
                                                           dbkey, false);
                  } else if (ja_task_id != 0) {
                     dbkey = sge_dstring_sprintf(&dbkey_dstring, "%10" sge_fuu32 ".%10" sge_fuu32,
                                                 job_id, ja_task_id);
                     ret = spool_berkeleydb_delete_ja_task(answer_list, info,
                                                           dbkey, false);
                  } else {
                     dbkey = sge_dstring_sprintf(&dbkey_dstring, "%10" sge_fuu32,
                                                 job_id);
                     ret = spool_berkeleydb_delete_job(answer_list, info,
                                                       dbkey, false);
                  }
                  sge_free(&dup);
               }
               break;
            case SGE_TYPE_JOBSCRIPT:
               {
                  const char *exec_file;
                  char *dup = strdup(key);
                  const char *db_key = jobscript_parse_key(dup, &exec_file);
                  ret = spool_berkeleydb_delete_object(answer_list, info,
                                                    BDB_JOB_DB,
                                                    db_key,
                                                    false);
                  sge_free(&dup);
               }
               break;
            default:
               table_name = object_type_get_name(object_type);
               dbkey = sge_dstring_sprintf(&dbkey_dstring, "%s:%s",
                                           table_name,
                                           key);

               ret = spool_berkeleydb_delete_object(answer_list, info,
                                                    BDB_CONFIG_DB,
                                                    dbkey, false);
               break;
         }

         /* spooling is done, now end the transaction, if we have an own one */
         if (local_transaction) {
            ret = spool_berkeleydb_end_transaction(answer_list, info, ret);
         }
      }
   }

   DRETURN(ret);
}

/**
 * @brief Apply a berkeleydb specific option to a rule
 *
 * The #spooling_option_func of this backend. It knows the option `RECOVER`
 * (a boolean, e.g. `recover=true`); other tokens are ignored.
 *
 * @param answer_list to return error and info messages
 * @param rule the rule whose database gets the option
 * @param option the option string, tokens separated by `,`, `;` or blanks
 *
 * @return always true
 */
static bool
spool_berkeleydb_option_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::Element<ocs::tom::SPR> rule,
                             const char *option) {
   DENTER(BDB_LAYER);

   bool ret = true;
   const char *delimiter = ",; ";
   bdb_info info;

   info = (bdb_info)rule.get<SPR_clientdata>();

   if (info != nullptr && option != nullptr && strlen(option) != 0) {
      struct saved_vars_s *context = nullptr;
      const char *token;
      bool recover = false;

      for (token = sge_strtok_r(option, delimiter, &context); token != nullptr;
           token = sge_strtok_r(nullptr, delimiter, &context)) {
         if (parse_bool_param(token, "RECOVER", &recover)) {
            bdb_set_recover(info, recover);
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                                    ANSWER_QUALITY_INFO,
                                    MSG_BERKELEY_SETOPTIONTO_SS,
                                    "RECOVER",
                                    recover ? TRUE_STR : FALSE_STR);

            continue;
         }
      }

      sge_free_saved_vars(context);
   }

   DRETURN(ret);
}
