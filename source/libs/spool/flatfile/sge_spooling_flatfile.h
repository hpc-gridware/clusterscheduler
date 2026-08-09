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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/       

/** @file
 * @brief The classic spooling backend: master lists as flat files
 */

#include "cull/cull.h"

#include "spool/sge_spooling.h"
#include "spool/sge_spooling_utilities.h"

/** @defgroup spool_classic Classic spooling backend
 * @brief The master lists as flat files in the spool directory
 *
 * The backend the framework calls `classic` and the installer offers as
 * *"classic spooling"*: one directory per object type under the spool
 * directory, one file per object, written by @ref spool_flatfile.
 *
 * It registers only the callbacks it needs - startup, shutdown, list, read,
 * write, delete and the two validate hooks. Options, maintenance, triggers,
 * transactions and reading keys are all `nullptr`, which is why
 * #spool_maintain_context and #spool_transaction do nothing under this
 * backend and #spool_read_keys behaves the way it does.
 *
 * @see @ref spool_flatfile, @ref spool_framework
 * @{
 */


/*
 * spooling framework functions
 */

extern "C" {
#ifdef SPOOLING_classic
const char *get_spooling_method();
#else
const char *get_classic_spooling_method();
#endif

lListElem *
spool_classic_create_context(lList **answer_list, const char *args);
}

bool 
spool_classic_default_startup_func(lList **answer_list, 
                                    const lListElem *rule, bool check);

bool
spool_classic_default_shutdown_func(lList **answer_list,
                                    const lListElem *rule);

bool
spool_classic_default_list_func(lList **answer_list,
                                 const lListElem *type, 
                                 const lListElem *rule,
                                 lList **list, 
                                 const sge_object_type object_type);
lListElem *
spool_classic_default_read_func(lList **answer_list, 
                                 const lListElem *type, 
                                 const lListElem *rule,
                                 const char *key, 
                                 const sge_object_type object_type);
bool 
spool_classic_default_write_func(lList **answer_list, 
                                  const lListElem *type, 
                                  const lListElem *rule, 
                                  const lListElem *object, 
                                  const char *key, 
                                  const sge_object_type object_type);
bool
spool_classic_default_delete_func(lList **answer_list,
                                   const lListElem *type,
                                   const lListElem *rule,
                                   const char *key,
                                   const sge_object_type object_type);

bool
spool_flatfile_key_is_safe(const char *key);

/** @} */
