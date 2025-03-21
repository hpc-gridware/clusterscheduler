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

#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#define BDB_LAYER BASIS_LAYER

#include <cerrno>
#include <cstring>
#include <ctime>

#include "uti/sge_rmon_macros.h"
#include "uti/config_file.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

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

#include "spool/sge_spooling_utilities.h"
#include "spool/msg_spoollib.h"

#include "spool/berkeleydb/msg_spoollib_berkeleydb.h"
#include "spool/berkeleydb/sge_bdb.h"
#include "spool/berkeleydb/sge_spooling_berkeleydb.h"

#include "msg_common.h"

static const char *spooling_method = "berkeleydb";

#ifdef SPOOLING_berkeleydb
const char *get_spooling_method()
#else
const char *get_berkeleydb_spooling_method()
#endif
{
   return spooling_method;
}

static bool
spool_berkeleydb_option_func(lList **answer_list, lListElem *rule, 
                             const char *option);

/****** spool/berkeleydb/spool_berkeleydb_create_context() ********************
*  NAME
*     spool_berkeleydb_create_context() -- create a berkeleydb spooling context
*
*  SYNOPSIS
*     lListElem* 
*     spool_berkeleydb_create_context(lList **answer_list, const char *args)
*
*  FUNCTION
*     Create a spooling context for the berkeleydb spooling.
*     
*     Argv has as first (and currently only) parameter the Database URL.
*
*     It currently consists of the path to the database directory.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const char *args    - arguments to the spooling method, see above.
*
*  RESULT
*     lListElem* - on success, the new spooling context, else nullptr
*
*  SEE ALSO
*     spool/--Spooling
*     spool/berkeleydb/--BerkeleyDB-Spooling
*******************************************************************************/
lListElem *
spool_berkeleydb_create_context(lList **answer_list, const char *args)
{
   lListElem *context = nullptr;

   DENTER(BDB_LAYER);

   /* check input parameter */
   if (args != nullptr) {
      lListElem *rule, *type;
      bdb_info info;
      char *path   = nullptr;
      
      /* create spooling context */
      context = spool_create_context(answer_list, "berkeleydb spooling");
      
      /* create rule and type for all objects spooled in the spool dir */
      rule = spool_context_create_rule(answer_list, context, 
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
      lSetRef(rule, SPR_clientdata, info);
      type = spool_context_create_type(answer_list, context, SGE_TYPE_ALL);
      spool_type_add_rule(answer_list, type, rule, true);
   }

   DRETURN(context);
}

/****** spool/berkeleydb/spool_berkeleydb_default_startup_func() **************
*  NAME
*     spool_berkeleydb_default_startup_func() -- setup 
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_default_startup_func(lList **answer_list, 
*                                         const char *args, bool check)
*
*  FUNCTION
*
*  INPUTS
*     lList **answer_list   - to return error messages
*     const lListElem *rule - the rule containing data necessary for
*                             the startup (e.g. path to the spool directory)
*     bool check            - check the spooling database
*
*  RESULT
*     bool - true, if the startup succeeded, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*     MT-NOTE: spool_berkeleydb_default_startup_func() is MT safe 
*
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_startup_context()
*******************************************************************************/
bool
spool_berkeleydb_default_startup_func(lList **answer_list, 
                                      const lListElem *rule, bool check)
{
   bool ret = true;
   bdb_info info;

   DENTER(BDB_LAYER);

   info = (bdb_info)lGetRef(rule, SPR_clientdata);

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

/****** spool/berkeleydb/spool_berkeleydb_default_shutdown_func() **************
*  NAME
*     spool_berkeleydb_default_shutdown_func() -- shutdown spooling context
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_default_shutdown_func(lList **answer_list, 
*                                          lListElem *rule);
*
*  FUNCTION
*     Shuts down the context, e.g. the database connection.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *rule - the rule containing data necessary for
*                             the shutdown (e.g. path to the spool directory)
*
*  RESULT
*     bool - true, if the shutdown succeeded, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*     MT-NOTE: spool_berkeleydb_default_shutdown_func() is MT safe 
*
*  SEE ALSO
*     spool/berkeleydb/--Spooling-BerkeleyDB
*     spool/spool_shutdown_context()
*******************************************************************************/
bool
spool_berkeleydb_default_shutdown_func(lList **answer_list, 
                                    const lListElem *rule)
{
   bool ret = true;

   bdb_info info;

   DENTER(BDB_LAYER);

   info = (bdb_info)lGetRef(rule, SPR_clientdata);

   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
      ret = false;
   } else {
      ret = spool_berkeleydb_close_database(answer_list, info);
   }

   DRETURN(ret);
}

/****** spool/berkeleydb/spool_berkeleydb_default_maintenance_func() ************
*  NAME
*     spool_berkeleydb_default_maintenance_func() -- maintain database
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_default_maintenance_func(lList **answer_list, 
*                                    lListElem *rule
*                                    const spooling_maintenance_command cmd,
*                                    const char *args);
*
*  FUNCTION
*     Maintains the database:
*        - initialization
*        - ...
*
*  INPUTS
*     lList **answer_list   - to return error messages
*     const lListElem *rule - the rule containing data necessary for
*                             the maintenance (e.g. path to the spool 
*                             directory)
*     const spooling_maintenance_command cmd - the command to execute
*     const char *args      - arguments to the maintenance command
*
*  RESULT
*     bool - true, if the maintenance succeeded, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*     MT-NOTE: spool_berkeleydb_default_maintenance_func() is MT safe 
*
*  SEE ALSO
*     spool/berkeleydb/--Spooling-BerkeleyDB
*     spool/spool_maintain_context()
*******************************************************************************/
bool
spool_berkeleydb_default_maintenance_func(lList **answer_list, 
                                    const lListElem *rule, 
                                    const spooling_maintenance_command cmd,
                                    const char *args)
{
   bool ret = true;

   bdb_info info;

   DENTER(BDB_LAYER);

   info = (bdb_info)lGetRef(rule, SPR_clientdata);

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

/****** spool/berkeleydb/spool_berkeleydb_trigger_func() ****************
*  NAME
*     spool_berkeleydb_trigger_func() -- do recurring tasks
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_trigger_func(lList **answer_list, const lListElem *rule,
*                                   time_t trigger, time_t *next_trigger) 
*
*  FUNCTION
*     Does recurring tasks for the Berkeley DB.
*     
*     In case of spooling to a local filesystem
*     - regular checkpointing is done
*     - a cleanup of the transaction log is done
*
*  INPUTS
*     lList **answer_list   - used to return error messages
*     const lListElem *rule - the spooling rule
*     time_t trigger        - time when this trigger was due
*     time_t *next_trigger  - time when trigger shall be called again
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     MT-NOTE: spool_berkeleydb_trigger_func() is MT safe 
*
*  SEE ALSO
*     spool/berkeleydb/--Spooling-BerkeleyDB
*******************************************************************************/
bool
spool_berkeleydb_trigger_func(lList **answer_list, const lListElem *rule,
                              u_long64 trigger, u_long64 *next_trigger)
{
   bool ret = true;
   bdb_info info;

   DENTER(BDB_LAYER);

   info = (bdb_info)lGetRef(rule, SPR_clientdata);
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
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

/****** spool/berkeleydb/spool_berkeleydb_transaction_func() ************
*  NAME
*     spool_berkeleydb_transaction_func() -- start or end a transaction
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_transaction_func(lList **answer_list, const lListElem *rule, 
*                                       spooling_transaction_command cmd) 
*
*  FUNCTION
*     Starts or ends a transaction (depending on cmd).
*
*     Each thread of a process can have one open transaction.
*
*  INPUTS
*     lList **answer_list              - to return error messages
*     const lListElem *rule            - spooling rule
*     spooling_transaction_command cmd - the transaction command
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     MT-NOTE: spool_berkeleydb_transaction_func() is MT safe 
*
*  SEE ALSO
*     spool/berkeleydb/--Spooling-BerkeleyDB
*******************************************************************************/
bool
spool_berkeleydb_transaction_func(lList **answer_list, const lListElem *rule, 
                                  spooling_transaction_command cmd)
{
   bool ret = true;

   bdb_info info;

   DENTER(BDB_LAYER);

   info = (bdb_info)lGetRef(rule, SPR_clientdata);
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
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

/****** spool/berkeleydb/spool_berkeleydb_default_list_func() *****************
*  NAME
*     spool_berkeleydb_default_list_func() -- read lists through berkeleydb spooling
*
*  SYNOPSIS
*     bool 
*     spool_berkeleydb_default_list_func(
*                                      lList **answer_list, 
*                                      const lListElem *type, 
*                                      const lListElem *rule, lList **list, 
*                                      const sge_object_type object_type) 
*
*  FUNCTION
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to be used 
*     lList **list                    - target list
*     const sge_object_type object_type - object type
*
*  RESULT
*     bool - true, on success, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*     MT-NOTE: spool_berkeleydb_default_list_func() is MT safe 
*  
*              The caller has to make sure, that locking for the list
*              parameter is done correctly!
*
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_read_list()
*******************************************************************************/
bool
spool_berkeleydb_default_list_func(lList **answer_list, 
                                 const lListElem *type, 
                                 const lListElem *rule, lList **list, 
                                 const sge_object_type object_type)
{
   bool ret = true;
#if 0
   bool local_transaction = false; /* did we start a transaction? */
#endif

   const lDescr *descr;
   const char *table_name;
   bdb_info info;
   lList *master_suser_list = *ocs::DataStore::get_master_list_rw(SGE_TYPE_SUSER);

   DENTER(BDB_LAYER);

   info = (bdb_info)lGetRef(rule, SPR_clientdata);
   descr = object_type_get_descr(object_type);
   table_name = object_type_get_name(object_type);

   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
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
#if 0
   /* JG: TODO: why does reading within a transaction give me the error:
    *           Not enough space
    */
      if (db->txn == nullptr) {
         ret = spool_berkeleydb_start_transaction(answer_list, info);
         if (ret) {
            local_transaction = true;
         }
      }
#endif
      dstring key_dstring;
      char key_buffer[MAX_STRING_SIZE];
      const char *key;

      sge_dstring_init(&key_dstring, key_buffer, sizeof(key_buffer));
      key = sge_dstring_sprintf(&key_dstring, "%s:", table_name);
                 
      if (ret) {
         switch (object_type) {
            case SGE_TYPE_QINSTANCE:
               break;
            case SGE_TYPE_CQUEUE:
               /* read all cluster queues */
               ret = spool_berkeleydb_read_list(answer_list, info, 
                                                BDB_CONFIG_DB,
                                                list, descr,
                                                key);
               if (ret) {
                  lListElem *queue;
                  const char *qinstance_table;
                  /* for all cluster queues: read queue instances */
                  qinstance_table = object_type_get_name(SGE_TYPE_QINSTANCE);
                  for_each_rw(queue, *list) {
                     lList *qinstance_list = nullptr;
                     const char *qname = lGetString(queue, CQ_name);

                     key = sge_dstring_sprintf(&key_dstring, "%s:%s/",
                                               qinstance_table,
                                               qname);
                     ret = spool_berkeleydb_read_list(answer_list, info,
                                                      BDB_CONFIG_DB,
                                                      &qinstance_list, QU_Type,
                                                      key);
                     if (ret && qinstance_list != nullptr) {
                        lSetList(queue, CQ_qinstances, qinstance_list);
                     }
                  }
               }
               break;
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
               break;
            case SGE_TYPE_JOB:
               /* read all jobs */
               ret = spool_berkeleydb_read_list(answer_list, info, BDB_JOB_DB, list, descr, key);
               if (ret) {
                  lListElem *job;
                  const char *ja_task_table;
                  /* for all jobs: read ja_tasks */
                  ja_task_table = object_type_get_name(SGE_TYPE_JATASK);
                  for_each_rw(job, *list) {
                     lList *task_list = nullptr;
                     u_long32 job_id = lGetUlong(job, JB_job_number);

                     key = sge_dstring_sprintf(&key_dstring, "%s:%8d.", ja_task_table, job_id);
                     ret = spool_berkeleydb_read_list(answer_list, info, BDB_JOB_DB, &task_list, JAT_Type, key);
                     /* reading ja_tasks succeeded */
                     if (ret) {
                        /* we actually found ja_tasks for this job */
                        if (task_list != nullptr) {
                           lListElem *ja_task;
                           const char *pe_task_table;

                           lSetList(job, JB_ja_tasks, task_list);
                           pe_task_table = object_type_get_name(SGE_TYPE_PETASK);

                           for_each_rw(ja_task, task_list) {
                              lList *pe_task_list = nullptr;
                              u_long32 ja_task_id = lGetUlong(ja_task, JAT_task_number);
                              key = sge_dstring_sprintf(&key_dstring, "%s:%8d.%8d.", pe_task_table, job_id, ja_task_id);
                              
                              ret = spool_berkeleydb_read_list(answer_list, info, BDB_JOB_DB, &pe_task_list, PET_Type, key);
                              if (ret) {
                                 if (pe_task_list != nullptr) {
                                    lSetList(ja_task, JAT_task_list, pe_task_list);
                                 }
                              } else {
                                 break;
                              }
                           }
                        }
                     }
                     job_list_register_new_job(*ocs::DataStore::get_master_list(SGE_TYPE_JOB), mconf_get_max_jobs(), 1);
                     suser_register_new_job(job, mconf_get_max_u_jobs(), 1, master_suser_list);
                     if (!ret) {
                        break;
                     }
                  }
               }
               break;
            default:
               ret = spool_berkeleydb_read_list(answer_list, info, BDB_CONFIG_DB, list, descr, key);
               break;
         }
#if 0
         /* spooling is done, now end the transaction, if we have an own one */
         if (local_transaction) {
            ret = spool_berkeleydb_end_transaction(answer_list, info, ret);
         }
#endif
      }
   }

   if (ret) {
      lListElem *ep;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
      spooling_validate_func validate = 
         (spooling_validate_func)lGetRef(rule, SPR_validate_func);
      spooling_validate_list_func validate_list = 
         (spooling_validate_list_func)lGetRef(rule, SPR_validate_list_func);
#pragma GCC diagnostic pop

      /* validate each individual object */
      /* JG: TODO: is it valid to validate after reading all objects? */
      for_each_rw(ep, *list) {
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

/****** spool/berkeleydb/spool_berkeleydb_default_read_func() *****************
*  NAME
*     spool_berkeleydb_default_read_func() -- read objects through berkeleydb spooling
*
*  SYNOPSIS
*     lListElem* 
*     spool_berkeleydb_default_read_func(lList **answer_list, 
*                                      const lListElem *type, 
*                                      const lListElem *rule, const char *key, 
*                                      const sge_object_type object_type) 
*
*  FUNCTION
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to use
*     const char *key                 - unique key specifying the object
*     const sge_object_type object_type - object type
*
*  RESULT
*     lListElem* - the object, if it could be read, else nullptr
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*     MT-NOTE: spool_berkeleydb_default_read_func() is MT safe 
*  
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_read_object()
*******************************************************************************/
lListElem *
spool_berkeleydb_default_read_func(lList **answer_list, 
                                 const lListElem *type, 
                                 const lListElem *rule, const char *key, 
                                 const sge_object_type object_type)
{
   bool ret = true;
   lListElem *ep = nullptr;

   bdb_info info;

   DENTER(BDB_LAYER);

   info = (bdb_info)lGetRef(rule, SPR_clientdata);
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
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
                  ep = lCreateElem(STU_Type);
                  lXchgString(ep, STU_name, &str);
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
            ep = spool_berkeleydb_read_object(answer_list, info, database, key);
/* do not do any validation
 * this function is only called from spooledit dump for reading individual objects
 * validation will therefore not work as it needs further objects, e.g.
 * for validating a qinstance the complex list is required
 */
#if 0
            if (ep != nullptr) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
               spooling_validate_func validate = 
                  (spooling_validate_func)(void *)lGetRef(rule, SPR_validate_func);
#pragma GCC diagnostic pop
               bool local_ret = validate(answer_list, type, rule, ep, object_type);
               if (!local_ret) {
                  lFreeElem(&ep);
               }
            }
#endif
            break;
      }
   }

   DRETURN(ep);
}

bool
spool_berkeleydb_default_read_keys_func(lList **answer_list, 
                                        const lListElem *rule,
                                        lList **list,
                                        const char *key)
{
   bool ret = true;

   bdb_info info;

   DENTER(BDB_LAYER);

   info = (bdb_info)lGetRef(rule, SPR_clientdata);

   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN,
                              ANSWER_QUALITY_WARNING,
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
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

/****** spool/berkeleydb/spool_berkeleydb_default_write_func() ****************
*  NAME
*     spool_berkeleydb_default_write_func() -- write objects through berkeleydb spooling
*
*  SYNOPSIS
*     bool
*     spool_berkeleydb_default_write_func(lList **answer_list, 
*                                       const lListElem *type, 
*                                       const lListElem *rule, 
*                                       const lListElem *object, 
*                                       const char *key, 
*                                       const sge_object_type object_type) 
*
*  FUNCTION
*     Writes an object through the appropriate berkeleydb spooling functions.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to use
*     const lListElem *object         - object to spool
*     const char *key                 - unique key
*     const sge_object_type object_type - object type
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*     MT-NOTE: spool_berkeleydb_default_write_func() is MT safe 
*
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_delete_object()
*******************************************************************************/
bool
spool_berkeleydb_default_write_func(lList **answer_list, 
                                  const lListElem *type, 
                                  const lListElem *rule, 
                                  const lListElem *object, 
                                  const char *key, 
                                  const sge_object_type object_type)
{
   bool ret = true;
   bool local_transaction = false; /* did we start a transaction? */
   bdb_info info;

   DENTER(BDB_LAYER);

   DPRINTF("spool_berkeleydb_default_write_func called for %s with key %s\n",
           object_type_get_name(object_type), key != nullptr ? key : "<null>");

   info = (bdb_info)lGetRef(rule, SPR_clientdata);
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
      ret = false;
   }
   
   if (key == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NULLVALUEASKEY,
                              lGetString(rule, SPR_url));
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

      if (ret) {
         switch (object_type) {
            case SGE_TYPE_JOB:
            case SGE_TYPE_JATASK:
            case SGE_TYPE_PETASK:
               {
                  u_long32 job_id, ja_task_id;
                  char *pe_task_id;
                  char *dup = strdup(key);
                  bool only_job; 

                  job_parse_key(dup, &job_id, &ja_task_id, &pe_task_id, &only_job);

                  if (object_type == SGE_TYPE_PETASK) {
                     ret = spool_berkeleydb_write_pe_task(answer_list, info,
                                                          object,
                                                          job_id, ja_task_id,
                                                          pe_task_id);
                  } else if (object_type == SGE_TYPE_JATASK) {
                     ret = spool_berkeleydb_write_ja_task(answer_list, info,
                                                          object,
                                                          job_id, ja_task_id);
                  } else {
                     ret = spool_berkeleydb_write_job(answer_list, info, object,
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
                  const char *script = lGetString(object, JB_script_ptr);
                  /* switch script */
                  ret = spool_berkeleydb_write_string(answer_list, info, 
                                                      BDB_JOB_DB,
                                                      db_key, script); 
                  sge_free(&dup);
               }
               break;
            case SGE_TYPE_CQUEUE:
               ret = spool_berkeleydb_write_cqueue(answer_list, info, 
                                                   object, key);
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

      /* spooling is done, now end the transaction, if we have an own one */
      if (local_transaction) {
         ret = spool_berkeleydb_end_transaction(answer_list, info, ret);
      }
   }

   DRETURN(ret);
}

/****** spool/berkeleydb/spool_berkeleydb_default_delete_func() ***************
*  NAME
*     spool_berkeleydb_default_delete_func() -- delete object in berkeleydb spooling
*
*  SYNOPSIS
*     bool
*     spool_berkeleydb_default_delete_func(lList **answer_list, 
*                                        const lListElem *type, 
*                                        const lListElem *rule, 
*                                        const char *key, 
*                                        const sge_object_type object_type) 
*
*  FUNCTION
*     Deletes an object in the berkeleydb spooling.
*
*  INPUTS
*     lList **answer_list - to return error messages
*     const lListElem *type           - object type description
*     const lListElem *rule           - rule to use
*     const char *key                 - unique key 
*     const sge_object_type object_type - object type
*
*  RESULT
*     bool - true on success, else false
*
*  NOTES
*     This function should not be called directly, it is called by the
*     spooling framework.
*
*     MT-NOTE: spool_berkeleydb_default_delete_func() is MT safe 
*
*  SEE ALSO
*     spool/berkeleydb/--BerkeleyDB-Spooling
*     spool/spool_delete_object()
*******************************************************************************/
bool
spool_berkeleydb_default_delete_func(lList **answer_list, 
                                   const lListElem *type, 
                                   const lListElem *rule,
                                   const char *key, 
                                   const sge_object_type object_type)
{
   bool ret = true;
   bool local_transaction = false; /* did we start a transaction? */
   const char *table_name;
   bdb_info info;

   dstring dbkey_dstring;
   char dbkey_buffer[MAX_STRING_SIZE];
   const char *dbkey;

   DENTER(BDB_LAYER);

   sge_dstring_init(&dbkey_dstring, dbkey_buffer, sizeof(dbkey_buffer));
   info = (bdb_info)lGetRef(rule, SPR_clientdata);
   if (info == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_WARNING, 
                              MSG_BERKELEY_NOCONNECTIONOPEN_S,
                              lGetString(rule, SPR_url));
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
                  u_long32 job_id, ja_task_id;
                  char *pe_task_id;
                  char *dup = strdup(key);
                  bool only_job; 

                  job_parse_key(dup, &job_id, &ja_task_id, &pe_task_id, &only_job);

                  if (pe_task_id != nullptr) {
                     dbkey = sge_dstring_sprintf(&dbkey_dstring, "%8d.%8d %s",
                                                 job_id, ja_task_id, pe_task_id);
                     ret = spool_berkeleydb_delete_pe_task(answer_list, info, 
                                                           dbkey, false);
                  } else if (ja_task_id != 0) {
                     dbkey = sge_dstring_sprintf(&dbkey_dstring, "%8d.%8d",
                                                 job_id, ja_task_id);
                     ret = spool_berkeleydb_delete_ja_task(answer_list, info, 
                                                           dbkey, false);
                  } else {
                     dbkey = sge_dstring_sprintf(&dbkey_dstring, "%8d",
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

static bool
spool_berkeleydb_option_func(lList **answer_list, lListElem *rule, 
                             const char *option) 
{
   bool ret = true;
   const char *delimiter = ",; ";
   bdb_info info;

   DENTER(BDB_LAYER);

   info = (bdb_info)lGetRef(rule, SPR_clientdata);

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
