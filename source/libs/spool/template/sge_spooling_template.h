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
 * @brief A skeleton backend to copy when writing a new one
 */

#include "cull/cull.h"

#include "spool/sge_spooling.h"
#include "spool/sge_spooling_utilities.h"

/** @defgroup spool_template Template spooling backend
 * @brief The skeleton to copy when writing a new storage backend
 *
 * Every callback of #spool_context_create_rule appears here with the right
 * signature and an empty body, so that a new backend can start from a file
 * that already compiles and only has to fill in the storage specific parts.
 *
 * @note Nothing links this backend into a running system, and
 *       #spool_template_create_context returns nullptr. It is documentation
 *       in the form of code.
 * @{
 */

/** @brief Report `"template"` as this library's spooling method
 *
 * @return the string `"template"`
 */
#ifdef SPOOLING_template
const char *get_spooling_method();
#else
const char *get_template_spooling_method();
#endif

lListElem *
spool_template_create_context(lList **answer_list, const char *args);

bool 
spool_template_default_startup_func(lList **answer_list, 
                                    const lListElem *rule, bool check);

/** @brief The part of the startup that is the same in master and clients
 *
 * @param answer_list to return error messages
 * @param rule        the spooling rule being started up
 *
 * @return true if the startup succeeded, else false
 */
bool 
spool_template_common_startup_func(lList **answer_list, 
                                   const lListElem *rule);

bool 
spool_template_default_shutdown_func(lList **answer_list, 
                                     const lListElem *rule);
bool 
spool_template_default_maintenance_func(lList **answer_list, 
                                        const lListElem *rule,
                                        const spooling_maintenance_command cmd,
                                        const char *args);

bool 
spool_template_default_list_func(lList **answer_list, 
                                 const lListElem *type, 
                                 const lListElem *rule, lList **list, 
                                 const sge_object_type object_type);
lListElem *
spool_template_default_read_func(lList **answer_list, 
                                 const lListElem *type, 
                                 const lListElem *rule, const char *key, 
                                 const sge_object_type object_type);
bool 
spool_template_default_write_func(lList **answer_list, 
                                  const lListElem *type, 
                                  const lListElem *rule, 
                                  const lListElem *object, const char *key, 
                                  const sge_object_type object_type);
bool 
spool_template_default_delete_func(lList **answer_list, 
                                   const lListElem *type, 
                                   const lListElem *rule, 
                                   const char *key, 
                                   const sge_object_type object_type);

/** @} */
