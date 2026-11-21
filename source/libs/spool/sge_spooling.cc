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

#include <utility>

#include "uti/sge_profiling.h"
#include "uti/sge_string.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_SPC.h"
#include "sgeobj/tom/ocs_tom_SPR.h"
#include "sgeobj/tom/ocs_tom_SPT.h"
#include "sgeobj/tom/ocs_tom_SPTR.h"

#include "tom/ocs_ElementCore.h"

#include "spool/msg_spoollib.h"
#include "spool/sge_spooling.h"

// process-lifetime list of the default contexts (SPC); stays untyped, no owning handle for a static
static lList *Default_Spool_Context_List;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
/**
 * @brief Create a new spooling context
 *
 * Create a new spooling context.
 *
 * @code
 * auto context = ocs_spool_create_context(answer_list, "my spooling context");
 * ...
 * @endcode
 *
 * @param answer_list to return error messages
 * @param name name of the context
 *
 * @return the new spooling context, or an empty handle if @p name is nullptr
 *
 * @note Callers do not normally use this directly. Each backend provides a
 *       service function that creates its own context and fills it with the
 *       rules and types it needs, e.g. `spool_classic_create_context()` in
 *       `flatfile/sge_spooling_flatfile.h`.
 *
 * @see #spool_free_context
 */
ocs::tom::ElementPtr<ocs::tom::SPC>
ocs_spool_create_context(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *name) {
   DENTER(TOP_LAYER);

   ocs::tom::ElementPtr<ocs::tom::SPC> context;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (name == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_CONTEXTNEEDSNAME);
   } else {
      context = ocs::tom::ElementPtr<ocs::tom::SPC>::create();
      context->set<SPC_name>(name);
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(context);
}


/**
 * @brief Free resources of a spooling context
 *
 * Performs a shutdown of the spooling context and releases
 * all allocated resources.
 *
 * @code
 * auto context = ocs_spool_create_context(answer_list, "my spooling context");
 * ...
 * spool_free_context(answer_list, std::move(context));
 * @endcode
 *
 * The function used to return nullptr for an assignment back to the
 * caller's pointer; the owning handle is consumed instead. It has no caller.
 *
 * @param answer_list to return error messages
 * @param context the context to free, handed over
 *
 * @see #ocs_spool_create_context, #spool_shutdown_context
 */
void
spool_free_context(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ElementPtr<ocs::tom::SPC> &&context) {
   DENTER(TOP_LAYER);
   auto owned_context = std::move(context);
   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);
  
   if (!owned_context) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S, 
                              __func__);
   } else {
      spool_shutdown_context(answer_list, owned_context.get());
      owned_context.reset();
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN_VOID;
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
bool spool_set_option(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, lListElem *context, const char *option) {
   DENTER(TOP_LAYER);

   bool ret = true;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);
  
   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S, 
                              __func__);
   } else {
      for (ocs::tom::Element<ocs::tom::SPR> rule : ocs::tom::get<SPC_rules>(context)) {
         auto func = (spooling_option_func) ocs::tom::get<SPR_option_func>(rule);
         if (func != nullptr) {
            if (!func(answer_list, rule, option)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_SETOPTIONOFRULEFAILED_SS,
                                       ocs::tom::get<SPR_name>(rule), 
                                       ocs::tom::get<SPC_name>(context));
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
bool spool_startup_context(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, lListElem *context, bool check) {
   DENTER(TOP_LAYER);

   bool ret = true;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else if (ocs::tom::get<SPC_types>(context).empty()) {
      /* check consistency: the context has to contain types */
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_CONTEXTCONTAINSNOTYPES_S, 
                              ocs::tom::get<SPC_name>(context));
      ret = false;
   } else {

      /* each type needs at least one rule and exactly one default rule */
      for (ocs::tom::ConstElement<ocs::tom::SPT> type : ocs::tom::get<SPC_types>(context)) {
         int default_rules = 0;
         
         if (ocs::tom::get<SPT_rules>(type).empty()) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_TYPECONTAINSNORULES_SS, 
                                    ocs::tom::get<SPT_name>(type),
                                    ocs::tom::get<SPC_name>(context));
            ret = false;
            goto error;
         }

         /* count default rules */
         for (ocs::tom::ConstElement<ocs::tom::SPTR> type_rule : ocs::tom::get<SPT_rules>(type)) {
            if(ocs::tom::get<SPTR_is_default>(type_rule)) {
               default_rules++;
            }
         }
         
         if (default_rules == 0) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_TYPEHASNODEFAULTRULE_SS,
                                    ocs::tom::get<SPT_name>(type),
                                    ocs::tom::get<SPC_name>(context));
            ret = false;
            goto error;
         }

         if (default_rules > 1) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_TYPEHASMORETHANONEDEFAULTRULE_SS,
                                    ocs::tom::get<SPT_name>(type),
                                    ocs::tom::get<SPC_name>(context));
            ret = false;
            goto error;
         }
      }
    
      /* the context has to contain rules */
      if (ocs::tom::get<SPC_rules>(context).empty()) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_CONTEXTCONTAINSNORULES_S, 
                                 ocs::tom::get<SPC_name>(context));
         ret = false;
         goto error;
      }
      
      for (ocs::tom::ConstElement<ocs::tom::SPR> rule : ocs::tom::get<SPC_rules>(context)) {
         auto func = (spooling_startup_func)ocs::tom::get<SPR_startup_func>(rule);
         if (func != nullptr) {
            if (!func(answer_list, rule, check)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_STARTUPOFRULEFAILED_SS,
                                       ocs::tom::get<SPR_name>(rule), 
                                       ocs::tom::get<SPC_name>(context));
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
bool spool_maintain_context(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, lListElem *context,
                            const spooling_maintenance_command cmd,
                            const char *args) {
   DENTER(TOP_LAYER);

   bool ret = true;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else {
      for (ocs::tom::ConstElement<ocs::tom::SPR> rule : ocs::tom::get<SPC_rules>(context)) {
         auto func = (spooling_maintenance_func) ocs::tom::get<SPR_maintenance_func>(rule);
         if (func != nullptr) {
            if (!func(answer_list, rule, cmd, args)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_MAINTENANCEOFRULEFAILED_SS,
                                       ocs::tom::get<SPR_name>(rule), 
                                       ocs::tom::get<SPC_name>(context));
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
bool spool_shutdown_context(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *context) {
   DENTER(TOP_LAYER);

   bool ret = true;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else {
      for (ocs::tom::ConstElement<ocs::tom::SPR> rule : ocs::tom::get<SPC_rules>(context)) {
         auto func = (spooling_shutdown_func)ocs::tom::get<SPR_shutdown_func>(rule);
         if (func != nullptr) {
            if (!func(answer_list, rule)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_SHUTDOWNOFRULEFAILED_SS,
                                       ocs::tom::get<SPR_name>(rule), 
                                       ocs::tom::get<SPC_name>(context));
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
bool spool_trigger_context(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *context,
                           uint64_t trigger, uint64_t *next_trigger) {
   DENTER(TOP_LAYER);

   bool ret = true;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else {
      for (ocs::tom::ConstElement<ocs::tom::SPR> rule : ocs::tom::get<SPC_rules>(context)) {
         auto func = (spooling_trigger_func)ocs::tom::get<SPR_trigger_func>(rule);
         if (func != nullptr) {
            if (!func(answer_list, rule, trigger, next_trigger)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_TRIGGEROFRULEFAILED_SS,
                                       ocs::tom::get<SPR_name>(rule), 
                                       ocs::tom::get<SPC_name>(context));
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
bool spool_transaction(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *context,
                       spooling_transaction_command cmd) {
   DENTER(TOP_LAYER);

   bool ret = true;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
      ret = false;
   } else {
      for (ocs::tom::ConstElement<ocs::tom::SPR> rule : ocs::tom::get<SPC_rules>(context)) {
         auto func = (spooling_transaction_func)ocs::tom::get<SPR_transaction_func>(rule);
         if (func != nullptr) {
            if (!func(answer_list, rule, cmd)) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_TRANSACTIONOFRULEFAILED_SS,
                                       ocs::tom::get<SPR_name>(rule), 
                                       ocs::tom::get<SPC_name>(context));
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
 * The parameter stays untyped: the qmaster, the spooling utilities and the
 * tests pass the context the dynamic loader gave them raw.
 *
 * @param context the context to be the default context (SPC), handed over to the default list;
 *                nullptr is ignored (ocs_spool_clear_default_context() removes the default context)
 *
 * @see #spool_get_default_context
 */
void spool_set_default_context(lListElem *context) {
   DENTER(TOP_LAYER);
   if (Default_Spool_Context_List == nullptr) {
      // process-lifetime static: no owning handle, the list is handed over raw
      Default_Spool_Context_List = ocs::tom::ListPtr<ocs::tom::SPC>::create(nullptr).release();
   }
   if (context != nullptr) {
      // the caller hands the context over to the list
      ocs::tom::List<ocs::tom::SPC>::from_typed(Default_Spool_Context_List)
         .push_back(ocs::tom::ElementPtr<ocs::tom::SPC>(context));
   }
   DRETURN_VOID;
}

/**
 * @brief Retrieve the default spooling context
 *
 * Retrieves a spooling context that has been set earlier using the function
 * spool_set_default_context()
 *
 * Without DENTER: called for every spooling operation of the qmaster.
 *
 * @return the spooling context, or an empty handle, if no default context has been set.
 *
 * @see #spool_set_default_context
 */
ocs::tom::Element<ocs::tom::SPC>
spool_get_default_context() {
   // the list is created in spool_set_default_context() with SPC
   return ocs::tom::List<ocs::tom::SPC>::from_typed(Default_Spool_Context_List).front();
}

/**
 * @brief Remove the default spooling context and hand it to the caller (CS-3191)
 *
 * Takes the context out of the default list, so that spool_get_default_context()
 * gives an empty handle afterwards. The caller owns the context: it shuts it
 * down and frees it. Freeing the element spool_get_default_context() returns
 * would leave the list pointing at freed memory.
 *
 * @return the default context, or an empty owner if none is set
 *
 * @see #spool_set_default_context
 * @see #spool_get_default_context
 */
ocs::tom::ElementPtr<ocs::tom::SPC>
ocs_spool_clear_default_context() {
   DENTER(TOP_LAYER);
   ocs::tom::ElementPtr<ocs::tom::SPC> context;
   // the list is created in spool_set_default_context() with SPC
   const auto list = ocs::tom::List<ocs::tom::SPC>::from_typed(Default_Spool_Context_List);
   if (const auto first = list.front()) {
      context = list.extract(first);
   }
   DRETURN(context);
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
 * @return the new rule, if it could be created, else an empty handle
 */
ocs::tom::Element<ocs::tom::SPR>
spool_context_create_rule(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::Element<ocs::tom::SPC> context,
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
                          spooling_validate_list_func validate_list_func) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::SPR> ep;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else if (context.get<SPC_rules>().find_first<SPR_name>(name)) {
      /* check for duplicates */
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_RULEALREADYEXISTS_SS, 
                              name, context.get<SPC_name>());
   } else {
      /* create rule */
      auto rule = ocs::tom::ElementPtr<ocs::tom::SPR>::create();
      rule->set<SPR_name>(name);
      rule->set<SPR_url>(url);
      rule->set<SPR_option_func>((void *)option_func);
      rule->set<SPR_startup_func>((void *)startup_func);
      rule->set<SPR_shutdown_func>((void *)shutdown_func);
      rule->set<SPR_maintenance_func>((void *)maintenance_func);
      rule->set<SPR_trigger_func>((void *)trigger_func);
      rule->set<SPR_transaction_func>((void *)transaction_func);
      rule->set<SPR_list_func>((void *)list_func);
      rule->set<SPR_read_func>((void *)read_func);
      rule->set<SPR_read_keys_func>((void *)read_keys_func);
      rule->set<SPR_write_func>((void *)write_func);
      rule->set<SPR_delete_func>((void *)delete_func);
      rule->set<SPR_validate_func>((void *)validate_func);
      rule->set<SPR_validate_list_func>((void *)validate_list_func);

      /* append rule to rule list */
      ep = context.get_or_create<SPC_rules>("spooling rules").push_back(std::move(rule));
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
 * Without DENTER: called for every spooling operation of the qmaster.
 *
 * @param context the context to search
 * @param object_type the object type to search
 *
 * @return an object type description or an empty handle, if none was found.
 */
ocs::tom::Element<ocs::tom::SPT>
spool_context_search_type(const ocs::tom::ConstElement<ocs::tom::SPC> context,
                          const sge_object_type object_type) {
   // the types are searched read-write, as the caller gets a mutable type description from a read-only context
   const auto types = context.get<SPC_types>();

   /* search fitting rule */
   auto ep = ocs::tom::find_first_rw<SPT_type>(types, object_type);

   /* if no specific rule is found, return default rule */
   if (!ep) {
      ep = ocs::tom::find_first_rw<SPT_type>(types, SGE_TYPE_ALL);
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
 * @return the new object type description, or an empty handle if @p context is empty
 */
ocs::tom::Element<ocs::tom::SPT>
spool_context_create_type(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::Element<ocs::tom::SPC> context,
                          const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::SPT> ep;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      /* create new type */
      auto type = ocs::tom::ElementPtr<ocs::tom::SPT>::create();
      type->set<SPT_type>(object_type);
      type->set<SPT_name>(object_type_get_name(object_type));

      /* append it to the types list of the context */
      ep = context.get_or_create<SPC_types>("spooling object types").push_back(std::move(type));
   }

   PROF_STOP_MEASUREMENT(SGE_PROF_SPOOLING);
   DRETURN(ep);
}


/**
 * @brief Search the default rule
 *
 * Searches and returns the default spooling rule for a certain object type.
 *
 * The rule is referenced through the Ref field SPTR_rule, which only
 * spool_type_add_rule() sets, with a rule (SPR); it is taken without a
 * check. Without DENTER: called for every spooling operation of the qmaster.
 *
 * @param spool_type the object type
 *
 * @return the default rule, or an empty handle, if no rule could be found.
 */
ocs::tom::Element<ocs::tom::SPR>
spool_type_search_default_rule(const ocs::tom::ConstElement<ocs::tom::SPT> spool_type) {
   ocs::tom::Element<ocs::tom::SPR> rule;

   for (const ocs::tom::ConstElement<ocs::tom::SPTR> ep : spool_type.get<SPT_rules>()) {
      if (ep.get<SPTR_is_default>()) {
         rule = ocs::tom::Element<ocs::tom::SPR>::from_typed(static_cast<lListElem *>(ep.get<SPTR_rule>()));
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
 * @return the newly created mapping object between type and rule, or an empty handle, if an error occurred.
 *
 * @see #spool_context_create_type, #spool_context_create_rule
 */
ocs::tom::Element<ocs::tom::SPTR>
spool_type_add_rule(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::tom::Element<ocs::tom::SPT> spool_type,
                    const ocs::tom::ConstElement<ocs::tom::SPR> rule, const lBool is_default) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::SPTR> ep;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (!spool_type) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_NOVALIDSPOOLTYPE_S, __func__);
   } else if (!rule) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDRULE_S,
                              __func__);
   } else if (is_default && spool_type_search_default_rule(spool_type)) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, 
                              MSG_SPOOL_TYPEALREADYHASDEFAULTRULE_S, 
                              spool_type.get<SPT_name>());
   } else {
      /* create mapping object; the rule is referenced through a Ref field and so stays untyped */
      auto type_rule = ocs::tom::ElementPtr<ocs::tom::SPTR>::create();
      type_rule->set<SPTR_is_default>(is_default);
      type_rule->set<SPTR_rule_name>(rule.get<SPR_name>());
      type_rule->set<SPTR_rule>((void *)rule.raw());

      /* append it to the list of mapping for this type */
      ep = spool_type.get_or_create<SPT_rules>("spooling object type rules").push_back(std::move(type_rule));
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
 * The debug check for duplicate keys in unique attributes is suspended while
 * the list is read: a spool written by an earlier version may hold such
 * duplicates, which the qmaster reports or repairs after reading (N-H-18).
 * The backends drop a second object with a taken key and record it.
 *
 * @param answer_list to return error messages
 * @param context the context to use for reading
 * @param list the target list
 * @param object_type the object type
 *
 * @return true, on success, false, if an error occurred
 */
bool spool_read_list(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *context,
                     lList **list, const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   bool ret = false;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      // the context comes from ocs_spool_create_context() (SPC); no check on the spooling hot path
      const auto type = spool_context_search_type(ocs::tom::ConstElement<ocs::tom::SPC>::from_typed(context),
                                                  object_type);
      if (!type) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_UNKNOWNOBJECTTYPEINCONTEXT_SS, 
                                 object_type_get_name(object_type), 
                                 ocs::tom::get<SPC_name>(context));
      } else {
         /* use the default rule to read list */
         const auto rule = spool_type_search_default_rule(type);
         if (!rule) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_NODEFAULTRULEFORTYPEINCONTEXT_SS,
                                    object_type_get_name(object_type), 
                                    ocs::tom::get<SPC_name>(context));
         } else {
            spooling_list_func func;

            /* read and call the list callback function */
            func = (spooling_list_func)ocs::tom::get<SPR_list_func>(rule);
            if (func == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                       ocs::tom::get<SPR_name>(rule), 
                                       ocs::tom::get<SPC_name>(context),
                                       __func__);
            } else {
               const ocs::tom::UniqueCheckSuspension suspension;
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
spool_read_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *context,
                  const sge_object_type object_type, const char *key) {
   DENTER(TOP_LAYER);

   lListElem *result = nullptr;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      // the context comes from ocs_spool_create_context() (SPC); no check on the spooling hot path
      const auto type = spool_context_search_type(ocs::tom::ConstElement<ocs::tom::SPC>::from_typed(context),
                                                  object_type);
      if (!type) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_UNKNOWNOBJECTTYPEINCONTEXT_SS,
                                 object_type_get_name(object_type), 
                                 ocs::tom::get<SPC_name>(context));
      } else {
         /* use the default rule to read object */
         const auto rule = spool_type_search_default_rule(type);
         if (!rule) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_NODEFAULTRULEFORTYPEINCONTEXT_SS,
                                    object_type_get_name(object_type), 
                                    ocs::tom::get<SPC_name>(context));
         } else {
            spooling_read_func func;

            /* retrieve and execute the read callback */
            func = (spooling_read_func)ocs::tom::get<SPR_read_func>(rule);
            if (func == nullptr) {
               answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                       ocs::tom::get<SPR_name>(rule), 
                                       ocs::tom::get<SPC_name>(context),
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

/**
 * @brief Read the keys the storage holds below a given key
 *
 * @param answer_list to return error messages
 * @param context     the context to read from
 * @param list        receives the keys (`STU`)
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
bool
spool_read_keys(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *context,
                lList **list, const char *key) {
   DENTER(TOP_LAYER);

   bool result = true;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      ocs::tom::ConstList<ocs::tom::SPR> rules = ocs::tom::get<SPC_rules>(context);

      /* use the default rule to read object */
      for (const ocs::tom::ConstElement<ocs::tom::SPR> rule : rules) {

         /* retrieve and execute the read callback */
         auto func = (spooling_read_keys_func)ocs::tom::get<SPR_read_keys_func>(rule);
         if (func == nullptr) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                    ocs::tom::get<SPR_name>(rule), 
                                    ocs::tom::get<SPC_name>(context),
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
 *
 * @return true, if writing was successful, else false
 */
bool spool_write_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *context,
                        const lListElem *object, const char *key,
                        const sge_object_type object_type) {
   DENTER(TOP_LAYER);

   bool ret = false;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      // the context comes from ocs_spool_create_context() (SPC); no check on the spooling hot path
      const auto type = spool_context_search_type(ocs::tom::ConstElement<ocs::tom::SPC>::from_typed(context),
                                                  object_type);
      if (!type) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_UNKNOWNOBJECTTYPEINCONTEXT_SS,
                                 object_type_get_name(object_type), 
                                 ocs::tom::get<SPC_name>(context));
      } else {
         /* loop over all rules and call the writing callbacks */
         const auto type_rules = type.get<SPT_rules>();
         if (type_rules.empty()) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_NORULESFORTYPEINCONTEXT_SS,
                                    object_type_get_name(object_type), 
                                    ocs::tom::get<SPC_name>(context));
         } else {
            ret = true;

            /* spool using multiple rules */
            for (ocs::tom::ConstElement<ocs::tom::SPTR> type_rule : type_rules) {
               // the rule is referenced through a Ref field, set by spool_type_add_rule() with an SPR rule
               const auto rule = ocs::tom::ConstElement<ocs::tom::SPR>::from_typed(
                  static_cast<const lListElem *>(type_rule.get<SPTR_rule>()));
               auto func = (spooling_write_func)rule.get<SPR_write_func>();
               if (func == nullptr) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                          ANSWER_QUALITY_ERROR, 
                                          MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                          ocs::tom::get<SPR_name>(rule), 
                                          ocs::tom::get<SPC_name>(context),
                                          __func__);
                   ret = false;
               } else {
                  if (!func(answer_list, type, rule, object, key, object_type)) {
                     answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                             ANSWER_QUALITY_WARNING, 
                                             MSG_SPOOL_RULEINCONTEXTFAILEDWRITING_SS,
                                             ocs::tom::get<SPR_name>(rule), 
                                             ocs::tom::get<SPC_name>(context));
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
 *
 * @return true, if all rules reported success, else false
 */
bool spool_delete_object(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *context,
                         const sge_object_type object_type, const char *key) {
   DENTER(TOP_LAYER);

   bool ret = false;

   PROF_START_MEASUREMENT(SGE_PROF_SPOOLING);

   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                              ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S,
                              __func__);
   } else {
      // the context comes from ocs_spool_create_context() (SPC); no check on the spooling hot path
      const auto type = spool_context_search_type(ocs::tom::ConstElement<ocs::tom::SPC>::from_typed(context),
                                                  object_type);
      if (!type) {
         answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                 ANSWER_QUALITY_ERROR, 
                                 MSG_SPOOL_UNKNOWNOBJECTTYPEINCONTEXT_SS,
                                 object_type_get_name(object_type), 
                                 ocs::tom::get<SPC_name>(context));
      } else {
         /* loop over all rules and call the deleting callbacks */
         const auto type_rules = type.get<SPT_rules>();
         if (type_rules.empty()) {
            answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                    ANSWER_QUALITY_ERROR, 
                                    MSG_SPOOL_NORULESFORTYPEINCONTEXT_SS,
                                    object_type_get_name(object_type), 
                                    ocs::tom::get<SPC_name>(context));
         } else {
            ret = true;

            /* delete object using all spooling rules */
            for (ocs::tom::ConstElement<ocs::tom::SPTR> type_rule : type_rules) {
               // the rule is referenced through a Ref field, set by spool_type_add_rule() with an SPR rule
               const auto rule = ocs::tom::ConstElement<ocs::tom::SPR>::from_typed(
                  static_cast<const lListElem *>(type_rule.get<SPTR_rule>()));
               auto func = (spooling_delete_func)rule.get<SPR_delete_func>();
               if (func == nullptr) {
                  answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                          ANSWER_QUALITY_ERROR, 
                                          MSG_SPOOL_CORRUPTRULEINCONTEXT_SSS,
                                          ocs::tom::get<SPR_name>(rule), 
                                          ocs::tom::get<SPC_name>(context),
                                          __func__);
                  ret = false;
               } else {
                  if (!func(answer_list, type, rule, key, object_type)) {
                     answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, 
                                             ANSWER_QUALITY_WARNING, 
                                             MSG_SPOOL_RULEINCONTEXTFAILEDWRITING_SS,
                                             ocs::tom::get<SPR_name>(rule), 
                                             ocs::tom::get<SPC_name>(context));
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
bool spool_compare_objects(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lListElem *context,
                           const sge_object_type object_type,
                           const lListElem *ep1, const lListElem *ep2) {
   DENTER(TOP_LAYER);
   if (context == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR, MSG_SPOOL_NOVALIDCONTEXT_S, __func__);
   }

   bool ret = true;
   DRETURN(ret);
}
#pragma GCC diagnostic pop
