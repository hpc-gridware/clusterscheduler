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
 * @brief The public interface of the spooling framework
 */

#include <ctime>

#include "mir/sge_mirror.h"

#include "spool/sge_spooling.h"

#include "sgeobj/cull/sge_spooling_SPC_L.h"
#include "sgeobj/cull/sge_spooling_SPR_L.h"
#include "sgeobj/cull/sge_spooling_SPT_L.h"
#include "sgeobj/cull/sge_spooling_SPTR_L.h"

/** @defgroup spool_framework Spooling framework
 * @brief An abstraction layer between what has to be spooled and where it goes
 *
 * The framework separates the application that wants configuration and data
 * spooled from the storage system that actually holds it. The application
 * talks to a **context**; the context knows nothing about files or databases.
 *
 * A context is built from two kinds of element:
 *
 * - a **rule** (`SPR_Type`) carries one storage backend as a set of callback
 *   function pointers - the `spooling_*_func` types below - plus the URL and
 *   whatever client data that backend needs;
 * - a **type** (`SPT_Type`) stands for one #sge_object_type and lists the
 *   rules that apply to it, exactly one of them marked as the default.
 *
 * So a write goes: application -> #spool_write_object -> the type for that
 * object -> every rule of that type -> the backend's #spooling_write_func.
 * That indirection is what makes it possible to spool jobs to one place and
 * the configuration to another, and it is why the backends
 * (`berkeleydb/`, `classic/`, `flatfile/`, `dynamic/`, `template/`) never
 * appear in the application code.
 *
 * @note A rule may leave any callback as `nullptr`. What happens then is not
 *       uniform - see the individual functions.
 * @{
 */

/** @brief What #spool_maintain_context shall do to the spool database
 *
 * These are the commands `spoolinit` accepts on its command line, and the
 * descriptions are the ones it prints in its own usage text.
 *
 * @warning Only #SPM_init is implemented by any backend in this tree. The
 *          Berkeley DB backend answers every other value with *"unknown
 *          maintenance command"*, and a backend that registers no
 *          #spooling_maintenance_func at all - flatfile does not - makes
 *          #spool_maintain_context succeed without doing anything.
 */
typedef enum {
   SPM_init,      ///< Initialize the database, optionally with history enabled
   SPM_history,   ///< Switch spooling with history on or off
   SPM_backup,    ///< Back the database up to a path
   SPM_purge,     ///< Remove historical data older than a given number of days
   SPM_vacuum,    ///< Compress the database and update its statistics
   SPM_info       ///< Output information about the database
} spooling_maintenance_command;

/** @brief What #spool_transaction shall do with the current transaction
 *
 * Used by the qmaster to make a multi-object modification atomic in the spool:
 * begin, then commit or roll back depending on whether the modification
 * succeeded.
 *
 * @note A backend without a #spooling_transaction_func silently does nothing
 *       here, so the calls are safe to make regardless of the spooling
 *       method - but only a transactional backend actually gives atomicity.
 */
typedef enum {
   STC_begin,     ///< Start a transaction
   STC_commit,    ///< Make the changes of the running transaction permanent
   STC_rollback   ///< Discard the changes of the running transaction
} spooling_transaction_command;

/** @name The callbacks a storage backend provides
 *
 * A rule is nothing but this set of function pointers plus the URL and the
 * backend's own client data. The framework never knows what is behind them,
 * which is the whole point: #spool_write_object looks up the rules for an
 * object type and calls their #spooling_write_func, and whether that ends up
 * in a file or in a database is the backend's business.
 *
 * A backend registers them in one call to #spool_context_create_rule and may
 * pass `nullptr` for anything it does not implement.
 * @{
 */

/** @brief Report the spooling method this shared library implements
 *
 * Used by the dynamic backend to identify a `libspool*` it has loaded, and by
 * `spoolinit method` to print the compiled in method.
 */
typedef const char *
(*spooling_get_method_func)();

/** @brief Build the spooling context for this backend from its argument string
 *
 * The counterpart of #spool_create_context for a concrete backend; `args` is
 * the method specific URL or option string, e.g. the spool directory.
 */
typedef lListElem *
(*spooling_create_context_func)(lList **answer_list, const char *args);

/** @brief Apply one backend specific option to a rule */
typedef bool
(*spooling_option_func)(lList **answer_list, lListElem *rule, 
                        const char *option); 
/** @brief Open the storage
 *
 * For file based spooling this means changing into the spool directory, for
 * database spooling it means opening the connection. When `check` is true the
 * backend additionally verifies that what it found belongs to this version -
 * which every operation wants except the one that creates the database.
 */
typedef bool
(*spooling_startup_func)(lList **answer_list, const lListElem *rule, 
                         bool check); 
/** @brief Flush unwritten data and close the storage again */
typedef bool
(*spooling_shutdown_func)(lList **answer_list, const lListElem *rule); 

/** @brief Carry out a #spooling_maintenance_command on the storage
 *
 * @warning Every backend in this tree implements #SPM_init and nothing else.
 */
typedef bool 
(*spooling_maintenance_func)(lList **answer_list, const lListElem *rule, 
                             const spooling_maintenance_command cmd, 
                             const char *args);

/** @brief Do recurring housekeeping, and say when to be called again
 *
 * `trigger` is the time this call was due; the backend writes the time it
 * wants to be called next into `next_trigger`. The Berkeley DB backend uses
 * it for checkpointing and for trimming the transaction log.
 */
typedef bool
(*spooling_trigger_func)(lList **answer_list, const lListElem *rule, 
                         uint64_t trigger, uint64_t *next_trigger);
                                  
/** @brief Begin, commit or roll back a transaction on the storage */
typedef bool
(*spooling_transaction_func)(lList **answer_list, const lListElem *rule, 
                             spooling_transaction_command cmd);
                                  
/** @brief Read every object of one type into a list
 *
 * This is also where the backend calls the validate callbacks: per object
 * while reading, and #spooling_validate_list_func once the list is complete.
 */
typedef bool
(*spooling_list_func)(lList **answer_list, 
                      const lListElem *type, const lListElem *rule, 
                      lList **list, 
                      const sge_object_type object_type);
                                  
/** @brief Write one object to the storage under `key` */
typedef bool
(*spooling_write_func)(lList **answer_list, 
                       const lListElem *type, const lListElem *rule, 
                       const lListElem *object, const char *key, 
                       const sge_object_type object_type);

/** @brief Read one object back from the storage by `key` */
typedef lListElem *
(*spooling_read_func)(lList **answer_list,
                      const lListElem *type, const lListElem *rule,
                      const char *key,
                      const sge_object_type object_type);

/** @brief List the keys the storage holds below `key`
 *
 * Only the Berkeley DB backend implements this; flatfile spooling leaves it
 * `nullptr`, which is what makes #spool_read_keys behave the way it does.
 */
typedef bool
(*spooling_read_keys_func)(lList **answer_list,
                      const lListElem *rule, 
                      lList **list,
                      const char *key);

/** @brief Remove the object stored under `key` */
typedef bool
(*spooling_delete_func)(lList **answer_list, 
                        const lListElem *type, const lListElem *rule, 
                        const char *key, 
                        const sge_object_type object_type);

/** @brief Check and complete a single object that was just read */
typedef bool
(*spooling_validate_func)(lList **answer_list, 
                        const lListElem *type, const lListElem *rule, 
                        lListElem *object, 
                        const sge_object_type object_type);

/** @brief Fix up a freshly read list as a whole
 *
 * @note Despite the name this is not a validation. The default implementation
 *       merges the host list and sorts the complex entries - the work that
 *       can only be done once every element of the list is present.
 */
typedef bool
(*spooling_validate_list_func)(lList **answer_list, 
                        const lListElem *type, const lListElem *rule, 
                        const sge_object_type object_type);
/** @} */

/* creation and maintenance of the spooling context */
lListElem *
spool_create_context(lList **answer_list, const char *name);

lListElem *
spool_free_context(lList **answer_list, lListElem *context);

void 
spool_set_default_context(lListElem *context);

lListElem *
spool_get_default_context();

lListElem *
spool_context_search_rule(const lListElem *context, const char *name);

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
                          spooling_validate_list_func validate_list_func);

lListElem *
spool_context_search_type(const lListElem *context, 
                          sge_object_type object_type);

lListElem *
spool_context_create_type(lList **answer_list, lListElem *context, 
                          sge_object_type object_type);

lListElem *
spool_type_search_default_rule(const lListElem *spool_type);

lListElem 
*spool_type_add_rule(lList **answer_list, lListElem *spool_type, 
                     const lListElem *rule, lBool is_default);

bool
spool_set_option(lList **answer_list, lListElem *context, const char *option);

/* startup and shutdown */
bool 
spool_startup_context(lList **answer_list, lListElem *context, bool check);

bool 
spool_shutdown_context(lList **answer_list, const lListElem *context);

bool
spool_maintain_context(lList **answer_list, lListElem *context, 
                       const spooling_maintenance_command cmd,
                       const char *args);

bool
spool_trigger_context(lList **answer_list, const lListElem *context, 
                      uint64_t trigger, uint64_t *next_trigger);

bool spool_transaction(lList **answer_list, const lListElem *context, 
                       spooling_transaction_command cmd);

/* reading */
bool 
spool_read_list(lList **answer_list, const lListElem *context, 
                lList **list, const sge_object_type object_type);

lListElem *
spool_read_object(lList **answer_list, const lListElem *context, 
                  const sge_object_type object_type, const char *key);

bool
spool_read_keys(lList **answer_list, const lListElem *context,
                lList **list, const char *key);

/* writing */
bool 
spool_write_object(lList **answer_list, const lListElem *context, 
                   const lListElem *object, const char *key, 
                   const sge_object_type object_type,
                   bool do_job_spooling);

/* deleting */
bool 
spool_delete_object(lList **answer_list, const lListElem *context, 
                    const sge_object_type object_type, const char *key,
                    bool do_job_spooling);

/* compare spooled attributes of 2 objects */
bool
spool_compare_objects(lList **answer_list, const lListElem *context, 
                      const sge_object_type object_type, 
                      const lListElem *ep1, const lListElem *ep2);

/** @brief The spooling method this binary or shared library was built with
 *
 * Implemented once per backend, not by the framework, and exported with C
 * linkage so that the dynamic backend can resolve it with `dlsym()` in a
 * `libspool*` it has just loaded.
 *
 * @return the method name, e.g. `"classic"` or `"berkeleydb"`
 */
extern "C" { 
const char *get_spooling_method();
}

/** @} */
