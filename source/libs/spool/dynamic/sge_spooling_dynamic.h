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
 * @brief Choosing the spooling method at runtime, by loading a shared library
 */

#include "cull/cull_list.h"

#include "spool/sge_spooling.h"
#include "spool/sge_spooling_utilities.h"

/** @defgroup spool_dynamic Dynamic spooling
 * @brief Pick the storage backend at runtime instead of at link time
 *
 * The qmaster does not know at build time whether the site spools to flat
 * files or to a Berkeley DB. This backend defers the decision: it `dlopen()`s
 * the `libspool<method>` shared library named in the configuration, asks it
 * for its #spooling_get_method_func, checks the answer against the method
 * that was requested, and then hands over to that library's
 * #spooling_create_context_func.
 *
 * Everything above the framework is unaffected - the result is an ordinary
 * spooling context.
 * @{
 */

/** @brief Report `"dynamic"` as this library's spooling method
 *
 * @return the string `"dynamic"`
 *
 * @note When the library is built *as* the dynamic spooling library
 *       (`SPOOLING_dynamic`) this is the exported `get_spooling_method()`
 *       the loader looks for; otherwise it carries the longer name so that
 *       it does not collide with the backend actually linked in.
 */
#ifdef SPOOLING_dynamic
const char *get_spooling_method();
#else
const char *get_dynamic_spooling_method();
#endif

lListElem *
spool_dynamic_create_context(lList **answer_list, const char *method,
                             const char *shlib_name, const char *args);

/** @} */
