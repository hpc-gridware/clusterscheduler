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
 * @brief The berkeleydb spooling backend
 */

#include <ctime>

#include "tom/ocs_Types.h"

#include "spool/sge_spooling.h"
#include "spool/sge_spooling_utilities.h"
#include "sgeobj/sge_answer.h"

/** @defgroup spool_berkeleydb Berkeley DB spooling backend
 * @brief The master lists in a Berkeley DB database
 *
 * The alternative to @ref spool_classic. Two databases - one for jobs, one
 * for everything else, see #bdb_database - inside one Berkeley DB
 * environment.
 *
 * It is the only backend that registers all the callbacks: it is the only one
 * with real transactions (#spooling_transaction_func), the only one that can
 * list keys without reading the records (#spooling_read_keys_func), and the
 * only one with recurring housekeeping to do (#spooling_trigger_func -
 * checkpointing and trimming the transaction log).
 *
 * @see @ref spool_framework, @ref spool_classic
 * @{
 */

extern "C" {
#ifdef SPOOLING_berkeleydb
const char *get_spooling_method();
#else
const char *get_berkeleydb_spooling_method();
#endif

ocs::tom::ElementData *
spool_berkeleydb_create_context(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char *args);
}

bool 
spool_berkeleydb_default_startup_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                    ocs::tom::ConstElement<ocs::tom::SPR> rule, bool check);

/** @brief The part of the startup that master and clients share
 *
 * @param answer_list to return error messages
 * @param rule the spooling rule being started up
 *
 * @return true on success, else false
 */
bool 
spool_berkeleydb_common_startup_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                   ocs::tom::ConstElement<ocs::tom::SPR> rule);

bool 
spool_berkeleydb_default_shutdown_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                     ocs::tom::ConstElement<ocs::tom::SPR> rule);
bool 
spool_berkeleydb_default_maintenance_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                        ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                        const spooling_maintenance_command cmd,
                                        const char *args);

bool
spool_berkeleydb_trigger_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, ocs::tom::ConstElement<ocs::tom::SPR> rule,
                              uint64_t trigger, uint64_t *next_trigger);

bool
spool_berkeleydb_transaction_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                  ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                  spooling_transaction_command cmd);
bool 
spool_berkeleydb_default_list_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                 ocs::tom::ConstElement<ocs::tom::SPT> type, 
                                 ocs::tom::ConstElement<ocs::tom::SPR> rule, ocs::tom::ListData **list, 
                                 const sge_object_type object_type);
ocs::tom::ElementData *
spool_berkeleydb_default_read_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                 ocs::tom::ConstElement<ocs::tom::SPT> type, 
                                 ocs::tom::ConstElement<ocs::tom::SPR> rule, const char *key, 
                                 const sge_object_type object_type);
bool
spool_berkeleydb_default_read_keys_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                        ocs::tom::ConstElement<ocs::tom::SPR> rule,
                                        ocs::tom::ListPtr<ocs::tom::STU> *list,
                                        const char *key);
bool 
spool_berkeleydb_default_write_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                  ocs::tom::ConstElement<ocs::tom::SPT> type, 
                                  ocs::tom::ConstElement<ocs::tom::SPR> rule, 
                                  ocs::tom::ConstRawElement object, const char *key, 
                                  const sge_object_type object_type);
bool 
spool_berkeleydb_default_delete_func(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, 
                                   ocs::tom::ConstElement<ocs::tom::SPT> type, 
                                   ocs::tom::ConstElement<ocs::tom::SPR> rule, 
                                   const char *key, 
                                   const sge_object_type object_type);

/** @} */
