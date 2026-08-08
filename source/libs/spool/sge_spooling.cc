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
 * @brief The public interface of the spooling framework
 */

#include "uti/sge_profiling.h"
#include "uti/sge_string.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"

#include "spool/msg_spoollib.h"
#include "spool/sge_spooling.h"

static lList *Default_Spool_Context_List;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
/**
 * @brief Create a new spooing context
 *
 * Create a new spooling context.
 *
 * @code
 * lListElem *context;
 * context = spool_create_context(answer_list, "my spooling context");
 * ...
 * @endcode
 *
 * @param answer_list to return error messages
 * @param name name of the context
 *
 * @return the new spooling context
 *
 * @note Callers do not normally use this directly. Each backend provides a
 *       service function that creates its own context and fills it with the
 *       rules and types it needs, e.g. `spool_classic_create_context()` in
 *       `flatfile/sge_spooling_flatfile.h`.
 *
 * @see #spool_free_context
 */
lListElem *
spool_create_context(lList **answer_list, const char *name)
{
   lListElem *ep = nullptr;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (name == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_CONTEXTNEEDSNAME);
   } else {
      ep = lCreateElem(SPC_Type);
      lSetString(ep, SPC_name, name);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ep);
}

/**
 * @brief Free resources of a spooling context
 *
 * Performs a shutdown of the spooling context and releases
 * all allocated resources.
 *
 * @code
 * lListElem *context;
 * ...
 * context = spool_free_context(answer_list, context);
 * @endcode
 *
 * @param answer_list to return error messages
 * @param context the context to free
 *
 * @return nullptr
 *
 * @see #spool_create_context, #spool_shutdown_context
 */
lListElem *
spool_free_context(lList **answer_list, lListElem *context)
{
   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);
  
   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S, 
                              __func__);
   } else {
      spool_shutdown_context(answer_list, context);
      lFreeElem(&context);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(context);
}

/**
 * @brief Pass a backend specific option to every rule of a context
 *
 * Stops at the first rule whose #spooling_option_func rejects the option.
 * Rules that registered no option callback are skipped.
 *
 * @param answer_list to return error messages
 * @param context     the context whose rules get the option
 * @param option      the option string, interpreted by the backend
 *
 * @return true if no rule rejected the option, else false
 */
bool
spool_set_option(lList **answer_list, lListElem *context, const char *option)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);
  
   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S, 
                              __func__);
   } else {
      for_each_rw_lv(rule, lGetList(context, SPC_rules)) {
         auto func = (spooling_option_func) lGetRef(rule, SPR_option_func);
         if (func != nullptr) {
            if (!func(answer_list, rule, option)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_SETOPTIONOFRULEFAILED_SS,
                                       lGetString(rule, SPR_name), 
                                       lGetString(context, SPC_name));
               ret = false;
               break;
            }
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ret);
}

/**
 * @brief Startup a spooling context
 *
 * Starts up the spooling context.
 * Checks consistency of the spooling context.
 * Then the startup callback for all rules will be called, which will
 * startup the different rules.
 * For file based spooling, this can been to chdir into the spool directory,
 * for database spooling it means opening the database connection.
 * If the parameter check is set to true, the startup callbacks
 * check the data base, e.g. if the spooling database
 * was created for the current Cluster Scheduler version.
 * This check shall be done for all operations, except when creating the
 * database.
 *
 * @param answer_list to return error messages
 * @param context the context to startup
 * @param check check database?
 *
 * @return true, if the context is OK and all startup callbacks reported success, else false
 *
 * @see #spool_shutdown_context
 */
bool 
spool_startup_context(lList **answer_list, lListElem *context, bool check)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else if (lGetNumberOfElem(lGetList(context, SPC_types)) == 0) {
      /* check consistency: the context has to contain types */
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_CONTEXTCONTAINSNOTYPES_S, 
                              lGetString(context, SPC_name));
      ret = false;
   } else {

      /* each type needs at least one rule and exactly one default rule */
      for_each_ep_lv(type, lGetList(context, SPC_types)) {
         int default_rules = 0;
         
         if (lGetNumberOfElem(lGetList(type, SPT_rules)) == 0) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_TYPECONTAINSNORULES_SS, 
                                    lGetString(type, SPT_name),
                                    lGetString(context, SPC_name));
            ret = false;
            goto error;
         }

         /* count default rules */
         for_each_ep_lv(type_rule, lGetList(type, SPT_rules)) {
            if(lGetBool(type_rule, SPTR_is_default)) {
               default_rules++;
            }
         }
         
         if (default_rules == 0) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_TYPEHASNODEFAULTRULE_SS,
                                    lGetString(type, SPT_name),
                                    lGetString(context, SPC_name));
            ret = false;
            goto error;
         }

         if (default_rules > 1) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_TYPEHASMORETHANONEDEFAULTRULE_SS,
                                    lGetString(type, SPT_name),
                                    lGetString(context, SPC_name));
            ret = false;
            goto error;
         }
      }
    
      /* the context has to contain rules */
      if (lGetNumberOfElem(lGetList(context, SPC_rules)) == 0) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_CONTEXTCONTAINSNORULES_S, 
                                 lGetString(context, SPC_name));
         ret = false;
         goto error;
      }
      
      for_each_ep_lv(rule, lGetList(context, SPC_rules)) {
         auto func = (spooling_startup_func)lGetRef(rule, SPR_startup_func);
         if (func != nullptr) {
            if (!func(answer_list, rule, check)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_STARTUPOFRULEFAILED_SS,
                                       lGetString(rule, SPR_name), 
                                       lGetString(context, SPC_name));
               ret = false;
               goto error;
            }
         }
      }
   }

error:
   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ret);
}

/**
 * @brief Maintain a context
 *
 * Do maintenance on spooling context's database.
 * Calls the maintenance callback for all defined spooling rules.
 * These callbacks will
 *    - initialize the database
 *    - backup
 *    - switch between spooling with/without history
 *    - etc.
 *
 * @param answer_list to return error messages
 * @param context the context to maintain
 * @param cmd what to do - see #spooling_maintenance_command
 * @param args arguments to the maintenance callback, e.g. the backup path
 *
 * @return true, if all maintenance callbacks reported success, else false
 *
 * @see #spool_startup_context
 */
bool 
spool_maintain_context(lList **answer_list, lListElem *context, 
                       const spooling_maintenance_command cmd,
                       const char *args)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else {
      for_each_ep_lv(rule, lGetList(context, SPC_rules)) {
         auto func = (spooling_maintenance_func) lGetRef(rule, SPR_maintenance_func);
         if (func != nullptr) {
            if (!func(answer_list, rule, cmd, args)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_MAINTENANCEOFRULEFAILED_SS,
                                       lGetString(rule, SPR_name), 
                                       lGetString(context, SPC_name));
               ret = false;
               break;
            }
         }
      }
   }
   
   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ret);
}


/**
 * @brief Shutdown a context
 *
 * Shut down a spooling context.
 * Calls the shutdown callback for all defined spooling rules.
 * Usually these callbacks will flush unwritten data, close
 * file handles, close database connections etc.
 * A context that has been shutdown can be reused by calling
 * spool_startup_context()
 *
 * @param answer_list to return error messages
 * @param context the context to shutdown
 *
 * @return true, if all shutdown callbacks reported success, else false
 *
 * @see #spool_startup_context
 */
bool 
spool_shutdown_context(lList **answer_list, const lListElem *context)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else {
      for_each_ep_lv(rule, lGetList(context, SPC_rules)) {
         auto func = (spooling_shutdown_func)lGetRef(rule, SPR_shutdown_func);
         if (func != nullptr) {
            if (!func(answer_list, rule)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_SHUTDOWNOFRULEFAILED_SS,
                                       lGetString(rule, SPR_name), 
                                       lGetString(context, SPC_name));
               ret = false;
               break;
            }
         }
      }
   }
   
   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ret);
}

/**
 * @brief Let every rule do its recurring housekeeping
 *
 * Called from the qmaster's periodic task. Each rule's
 * #spooling_trigger_func writes into `next_trigger` when it wants to be
 * called again - so with more than one rule registered, the last one to run
 * decides.
 *
 * @param answer_list  to return error messages
 * @param context      the context whose rules are triggered
 * @param trigger      the time this call was due
 * @param next_trigger receives the time of the next call
 *
 * @return true if every trigger callback succeeded, else false
 */
bool
spool_trigger_context(lList **answer_list, const lListElem *context,
                      uint64_t trigger, uint64_t *next_trigger)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else {
      for_each_ep_lv(rule, lGetList(context, SPC_rules)) {
         auto func = (spooling_trigger_func)lGetRef(rule, SPR_trigger_func);
         if (func != nullptr) {
            if (!func(answer_list, rule, trigger, next_trigger)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_TRIGGEROFRULEFAILED_SS,
                                       lGetString(rule, SPR_name), 
                                       lGetString(context, SPC_name));
               ret = false;
               break;
            }
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ret);
}

/**
 * @brief Begin, commit or roll back a transaction over the whole context
 *
 * @param answer_list to return error messages
 * @param context     the context to run the transaction on
 * @param cmd         begin, commit or rollback
 *
 * @return true if every rule accepted the command, else false
 *
 * @note A rule without a #spooling_transaction_func is skipped and the call
 *       still reports success, so the qmaster can bracket its modifications
 *       in transactions regardless of the configured spooling method. Only a
 *       transactional backend actually makes them atomic.
 */
bool spool_transaction(lList **answer_list, const lListElem *context, 
                       spooling_transaction_command cmd)
{
   bool ret = true;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else {
      for_each_ep_lv(rule, lGetList(context, SPC_rules)) {
         auto func = (spooling_transaction_func)lGetRef(rule, SPR_transaction_func);
         if (func != nullptr) {
            if (!func(answer_list, rule, cmd)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_TRANSACTIONOFRULEFAILED_SS,
                                       lGetString(rule, SPR_name), 
                                       lGetString(context, SPC_name));
               ret = false;
               break;
            }
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ret);
}


/**
 * @brief Set a default context
 *
 * The spooling framework can have a default context.
 * A context that has been created before can be set as
 * default context using this function.
 * The default context can be retrieved later with the function
 * spool_get_default_context().
 *
 * @param context the context to be the default context
 *
 * @see #spool_get_default_context
 */
void spool_set_default_context(lListElem *context)
{
   if (Default_Spool_Context_List == nullptr) {
      Default_Spool_Context_List = lCreateList(nullptr, SPC_Type);
   }
   lAppendElem(Default_Spool_Context_List, context);
}

/**
 * @brief Retrieve the default spooling context
 *
 * Retrieves a spooling context that has been set earlier using the function
 * spool_set_default_context()
 *
 * @return the spooling context, or nullptr, if no default context has been set.
 *
 * @see #spool_set_default_context
 */
lListElem *
spool_get_default_context()
{
   return lFirstRW(Default_Spool_Context_List);
}

/**
 * @brief Search a certain rule
 *
 * Searches a certain rule (given by its name) in a given spooling context.
 *
 * @param context the context to search
 * @param name name of the rule
 *
 * @return the rule, if it exists, else nullptr
 */
lListElem *
spool_context_search_rule(const lListElem *context, const char *name)
{
   return lGetElemStrRW(lGetList(context, SPC_rules), SPR_name, name);
}

/**
 * @brief Create a rule in a spooling context
 *
 * Creates a rule in the given context and assigns it the given attributes.
 *
 * @param answer_list to return error messages
 * @param context the context to contain the new rule
 * @param name the name of the rule
 * @param url the name of the url
 * @param option_func function to set options for the rule
 * @param startup_func startup function for the rule
 * @param shutdown_func shutdown function
 * @param maintenance_func maintenance function (initialization, backup, etc.)
 * @param trigger_func function for recurring housekeeping
 * @param transaction_func function beginning, committing and rolling back
 * @param list_func function reading a list of objects
 * @param read_func function reading an individual object
 * @param read_keys_func function listing the keys below a key
 * @param write_func function writing an individual object
 * @param delete_func function deleting an individual object
 * @param validate_func function validateing an individual object
 * @param validate_list_func function for validating the new list
 *
 * @return the new rule, if it could be created, else nullptr
 */
lListElem *
spool_context_create_rule(lList **answer_list, lListElem *context, 
                          const char *name, const char *url,
                          spooling_option_func option_func, 
                          spooling_startup_func startup_func, 
                          spooling_shutdown_func shutdown_func, 
                          spooling_maintenance_func maintenance_func, 
                          spooling_trigger_func trigger_func, 
                          spooling_transaction_func transaction_func, 
                          spooling_list_func list_func, 
                          spooling_read_func read_func,
                          spooling_read_keys_func read_keys_func,
                          spooling_write_func write_func,
                          spooling_delete_func delete_func,
                          spooling_validate_func validate_func,
                          spooling_validate_list_func validate_list_func)
{
   lListElem *ep = nullptr;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else if (lGetElemStr(lGetList(context, SPC_rules), SPR_name, name) 
             != nullptr) {
      /* check for duplicates */
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_RULEALREADYEXISTS_SS, 
                              name, lGetString(context, SPC_name));
   } else {
      lList *lp;

      /* create rule */
      ep = lCreateElem(SPR_Type);
      lSetString(ep, SPR_name, name);
      lSetString(ep, SPR_url, url);
      lSetRef(ep, SPR_option_func, (void *)option_func);
      lSetRef(ep, SPR_startup_func, (void *)startup_func);
      lSetRef(ep, SPR_shutdown_func, (void *)shutdown_func);
      lSetRef(ep, SPR_maintenance_func, (void *)maintenance_func);
      lSetRef(ep, SPR_trigger_func, (void *)trigger_func);
      lSetRef(ep, SPR_transaction_func, (void *)transaction_func);
      lSetRef(ep, SPR_list_func, (void *)list_func);
      lSetRef(ep, SPR_read_func, (void *)read_func);
      lSetRef(ep, SPR_read_keys_func, (void *)read_keys_func);
      lSetRef(ep, SPR_write_func, (void *)write_func);
      lSetRef(ep, SPR_delete_func, (void *)delete_func);
      lSetRef(ep, SPR_validate_func, (void *)validate_func);
      lSetRef(ep, SPR_validate_list_func, (void *)validate_list_func);

      /* append rule to rule list */
      lp = lGetListRW(context, SPC_rules);
      if (lp == nullptr) {
         lp = lCreateList("spooling rules", SPR_Type);
         lSetList(context, SPC_rules, lp);
      }

      lAppendElem(lp, ep);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ep);
}

/**
 * @brief Search an object type description
 *
 * Searches the object type description with the given type in the
 * given context.
 * If no specific description for the given type is found, but a
 * default type description (for all object types) exists, this
 * default type description is returned.
 *
 * @param context the context to search
 * @param object_type the object type to search
 *
 * @return an object type description or nullptr, if none was found.
 */
lListElem *
spool_context_search_type(const lListElem *context, 
                          sge_object_type object_type)
{
   lListElem *ep;

   /* search fitting rule */
   ep = lGetElemUlongRW(lGetList(context, SPC_types), SPT_type, object_type);

   /* if no specific rule is found, return default rule */
   if (ep == nullptr) {
      ep = lGetElemUlongRW(lGetList(context, SPC_types), SPT_type, SGE_TYPE_ALL);
   }
   
   return ep;
}

/**
 * @brief Create an object type description
 *
 * Creates a new description how a certain object type shall be
 * spooled.
 * If the given object_type is SGE_TYPE_ALL, the description will
 * be the default for object types that are not individually
 * handled.
 *
 * @param answer_list to return error messages
 * @param context the context to contain the new description
 * @param object_type the object type
 *
 * @return the new object type description
 */
lListElem *
spool_context_create_type(lList **answer_list, lListElem *context, 
                          sge_object_type object_type)
{
   lListElem *ep = nullptr;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      lList *lp;

      /* create new type */
      ep = lCreateElem(SPT_Type);
      lSetUlong(ep, SPT_type, object_type);
      lSetString(ep, SPT_name, object_type_get_name(object_type));
    
      /* append it to the types list of the context */
      lp = lGetListRW(context, SPC_types);
      if (lp == nullptr) {
         lp = lCreateList("spooling object types", SPT_Type);
         lSetList(context, SPC_types, lp);
      }

      lAppendElem(lp, ep);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ep);
}

/**
 * @brief Search the default rule
 *
 * Searches and returns the default spooling rule for a certain object type.
 *
 * @param spool_type the object type
 *
 * @return the default rule, or nullptr, if no rule could be found.
 */
lListElem *
spool_type_search_default_rule(const lListElem *spool_type)
{  
   lListElem *rule = nullptr;

   const lList *lp = lGetList(spool_type, SPT_rules);
   for_each_ep_lv(ep, lp) {
      if (lGetBool(ep, SPTR_is_default)) {
         rule = (lListElem *)lGetRef(ep, SPTR_rule);
         break;
      }
   }

   return rule;
}

/**
 * @brief Adds a rule for a spooling object type
 *
 * Adds a spooling rule to an object type description.
 * The rule can be installed as default rule for this object type.
 *
 * @param answer_list to return error messages
 * @param spool_type the object type description
 * @param rule the rule to add
 * @param is_default is the rule the default rule?
 *
 * @return the newly created mapping object between type and rule (SPTR_Type), or nullptr, if an error occurred.
 *
 * @see #spool_context_create_type, #spool_context_create_rule
 */
lListElem *
spool_type_add_rule(lList **answer_list, lListElem *spool_type, 
                    const lListElem *rule, lBool is_default)
{
   lListElem *ep = nullptr;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if(spool_type == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_NOVALIDSPOOLTYPE_S, __func__);
   } else if(rule == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDRULE_S,
                              __func__);
   } else if(is_default && spool_type_search_default_rule(spool_type) != nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_TYPEALREADYHASDEFAULTRULE_S, 
                              lGetString(spool_type, SPT_name));
   } else {
      lList *lp;

      /* create mapping object */
      ep = lCreateElem(SPTR_Type);
      lSetBool(ep, SPTR_is_default, is_default);
      lSetString(ep, SPTR_rule_name, lGetString(rule, SPR_name));
      lSetRef(ep, SPTR_rule, (void *)rule);

      /* append it to the list of mapping for this type */
      lp = lGetListRW(spool_type, SPT_rules);
      if (lp == nullptr) {
         lp = lCreateList("spooling object type rules", SPTR_Type);
         lSetList(spool_type, SPT_rules, lp);
      }
      
      lAppendElem(lp, ep);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ep);
}

/**
 * @brief Read a list of objects from spooled data
 *
 * Read the list of objects associated with a certain object type
 * from the spooled data and store it into the given list.
 * The function will call the read_list callback from the default rule
 * for the given object type.
 *
 * @param answer_list to return error messages
 * @param context the context to use for reading
 * @param list the target list
 * @param object_type the object type
 *
 * @return true, on success, false, if an error occurred
 */
bool 
spool_read_list(lList **answer_list, const lListElem *context, 
                lList **list, const sge_object_type object_type)
{
   bool ret = false;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      lListElem *type;

      /* find the object type description */
      type = spool_context_search_type(context, object_type);
      if (type == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_UNKNOWNOBJECTTYPEINCONTEXT_SS, 
                                 object_type_get_name(object_type), 
                                 lGetString(context, SPC_name));
      } else {
         lListElem *rule;

         /* use the default rule to read list */
         rule = spool_type_search_default_rule(type);
         if (rule == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_NODEFAULTRULEFORTYPEINCONTEXT_SS,
                                    object_type_get_name(object_type), 
                                    lGetString(context, SPC_name));
         } else {
            spooling_list_func func;

            /* read and call the list callback function */
            func = (spooling_list_func)lGetRef(rule, SPR_list_func);
            if (func == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                       lGetString(rule, SPR_name), 
                                       lGetString(context, SPC_name),
                                       __func__);
            } else {
               ret = func(answer_list, type, rule, list, object_type);
            }
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ret);
}

/**
 * @brief Read a single object from spooled data
 *
 * Read an objects characterized by its type and a unique key
 * from the spooled data.
 * The function will call the read callback from the default rule
 * for the given object type.
 *
 * @param answer_list to return error messages
 * @param context the context to use
 * @param object_type object type
 * @param key unique key
 *
 * @return the object, if it could be read, else nullptr
 */
lListElem *
spool_read_object(lList **answer_list, const lListElem *context, 
                  const sge_object_type object_type, const char *key)
{
   lListElem *result = nullptr;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      lListElem *type;

      /* find the object type description */
      type = spool_context_search_type(context, object_type);
      if (type == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_UNKNOWNOBJECTTYPEINCONTEXT_SS,
                                 object_type_get_name(object_type), 
                                 lGetString(context, SPC_name));
      } else {
         lListElem *rule;

         /* use the default rule to read object */
         rule = spool_type_search_default_rule(type);
         if (rule == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_NODEFAULTRULEFORTYPEINCONTEXT_SS,
                                    object_type_get_name(object_type), 
                                    lGetString(context, SPC_name));
         } else {
            spooling_read_func func;

            /* retrieve and execute the read callback */
            func = (spooling_read_func)lGetRef(rule, SPR_read_func);
            if (func == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                       lGetString(rule, SPR_name), 
                                       lGetString(context, SPC_name),
                                       __func__);
            } else {
               result = func(answer_list, type, rule, key, object_type);
            }
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(result);
}

bool
/**
 * @brief Read the keys the storage holds below a given key
 *
 * @param answer_list to return error messages
 * @param context     the context to read from
 * @param list        receives the keys (`STU_Type`)
 * @param key         the key to list below
 *
 * @return true on success, else false
 *
 * @warning A rule without a #spooling_read_keys_func puts a *"corrupt rule"*
 *          error into `answer_list` but does **not** make the function return
 *          false - `result` keeps the `true` it started with. Flatfile
 *          spooling registers no such callback, so `spooledit list` against a
 *          flatfile spool takes this path: an error message and a success
 *          return, with `list` left empty.
 */
spool_read_keys(lList **answer_list, const lListElem *context, 
                lList **list, const char *key)
{
   bool result = true;

   DENTER(TOP_LAYER);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      const lList *rules = lGetList(context, SPC_rules);

      /* use the default rule to read object */
      for_each_ep_lv(rule, rules) {

         /* retrieve and execute the read callback */
         auto func = (spooling_read_keys_func)lGetRef(rule, SPR_read_keys_func);
         if (func == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                    lGetString(rule, SPR_name), 
                                    lGetString(context, SPC_name),
                                    __func__);
         } else {
            result = func(answer_list, rule, list, key);
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(result);
}


/**
 * @brief Write (spool) a single object
 *
 * Writes a single object using the given spooling context.
 * The function calls all rules associated with the object type
 * description for the given object type.
 *
 * @param answer_list to return error messages
 * @param context context to use
 * @param object object to spool
 * @param key unique key
 * @param object_type type of the object
 * @param do_job_spooling flag whether job_spooling shall be done
 *
 * @return true, if writing was successful, else false
 */
bool 
spool_write_object(lList **answer_list, const lListElem *context, 
                   const lListElem *object, const char *key, 
                   const sge_object_type object_type,
                   bool do_job_spooling)
{
   bool ret = false;
 
   DENTER(TOP_LAYER);

   switch (object_type) {

      case SGE_TYPE_JOB:
      case SGE_TYPE_JATASK:
      case SGE_TYPE_PETASK:
         if (!do_job_spooling) {
            DRETURN(true);
         }
         break;
      default : 
         break;
   }

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      lListElem *type;

      /* find the object type description */
      type = spool_context_search_type(context, object_type);
      if (type == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_UNKNOWNOBJECTTYPEINCONTEXT_SS,
                                 object_type_get_name(object_type), 
                                 lGetString(context, SPC_name));
      } else {
         /* loop over all rules and call the writing callbacks */
         const lList *type_rules = lGetList(type, SPT_rules);
         if (type_rules == nullptr || lGetNumberOfElem(type_rules) == 0) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_NORULESFORTYPEINCONTEXT_SS,
                                    object_type_get_name(object_type), 
                                    lGetString(context, SPC_name));
         } else {
            ret = true;

            /* spool using multiple rules */
            for_each_ep_lv(type_rule, type_rules) {
               lListElem *rule;
               spooling_write_func func;

               rule = (lListElem *)lGetRef(type_rule, SPTR_rule);
               func = (spooling_write_func)lGetRef(rule, SPR_write_func);
               if (func == nullptr) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                          ANSWER_QUALITY_ERROR, 
                                          MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                          lGetString(rule, SPR_name), 
                                          lGetString(context, SPC_name),
                                          __func__);
                   ret = false;
               } else {
                  if (!func(answer_list, type, rule, object, key, object_type)) {
                     answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                             ANSWER_QUALITY_WARNING, 
                                             MSG_SPOOL_RULEINCONTEXTFAILEDWRITING_SS,
                                             lGetString(rule, SPR_name), 
                                             lGetString(context, SPC_name));
                     ret = false;
                  }
               }
            }
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ret);
}

/**
 * @brief Delete a single object
 *
 * Deletes a certain object characterized by type and a unique key
 * in the spooled data.
 * Calls the delete callback in all rules defined for the given
 * object type.
 *
 * @param answer_list to return error messages
 * @param context the context to use
 * @param object_type object type
 * @param key unique key
 * @param do_job_spooling flag if job_spooling shall be done
 *
 * @return true, if all rules reported success, else false
 */
bool 
spool_delete_object(lList **answer_list, const lListElem *context, 
                    const sge_object_type object_type, const char *key,
                    bool do_job_spooling)
{
   bool ret = false;
   
   DENTER(TOP_LAYER);

   switch (object_type) {

      case SGE_TYPE_JOB:
      case SGE_TYPE_JATASK:
      case SGE_TYPE_PETASK:
            if (!do_job_spooling) {
               DRETURN(true);
            }
         break;
      default : 
         break;
   }
   
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      lListElem *type;

      /* find the object type description */
      type = spool_context_search_type(context, object_type);
      if (type == nullptr) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_UNKNOWNOBJECTTYPEINCONTEXT_SS,
                                 object_type_get_name(object_type), 
                                 lGetString(context, SPC_name));
      } else {
         /* loop over all rules and call the deleting callbacks */
         const lList *type_rules = lGetList(type, SPT_rules);
         if (type_rules == nullptr || lGetNumberOfElem(type_rules) == 0) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_NORULESFORTYPEINCONTEXT_SS,
                                    object_type_get_name(object_type), 
                                    lGetString(context, SPC_name));
         } else {
            ret = true;

            /* delete object using all spooling rules */
            for_each_ep_lv(type_rule, type_rules) {
               lListElem *rule;
               spooling_delete_func func;

               rule = (lListElem *)lGetRef(type_rule, SPTR_rule);
               func = (spooling_delete_func)lGetRef(rule, SPR_delete_func);
               if (func == nullptr) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                          ANSWER_QUALITY_ERROR, 
                                          MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                          lGetString(rule, SPR_name), 
                                          lGetString(context, SPC_name),
                                          __func__);
                  ret = false;
               } else {
                  if (!func(answer_list, type, rule, key, object_type)) {
                     answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                             ANSWER_QUALITY_WARNING, 
                                             MSG_SPOOL_RULEINCONTEXTFAILEDWRITING_SS,
                                             lGetString(rule, SPR_name), 
                                             lGetString(context, SPC_name));
                     ret = false;
                  }
               }
            }
         }
      }
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ret);
}

/**
 * @brief Compare objects by spooled data
 *
 * Compares two objects by comparing only the attributes that shall be
 * spooled.
 *
 * @param answer_list to return error messages
 * @param context context to use
 * @param object_type type of the object
 * @param ep1 object 1
 * @param ep2 object 2
 *
 * @return false, if the objects have no differences, else true
 *
 * @note Not yet implemented.
 *       First the attributes to be spooled have to be defined in the
 *       object definitions (libs/gdi/sge_*L.h).
 */
bool
spool_compare_objects(lList **answer_list, const lListElem *context, 
                      const sge_object_type object_type, 
                      const lListElem *ep1, const lListElem *ep2)
{
   DENTER(TOP_LAYER);
   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S, __func__);
   }

   bool ret = true;
   DRETURN(ret);
}
#pragma GCC diagnostic pop
