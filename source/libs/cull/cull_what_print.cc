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
 * @brief Rendering a field selection for debugging
 */
#include <cstdio>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull_list.h"
#include "cull/cull_db.h"
#include "cull/cull_multitype.h"
#include "cull/cull_whatP.h"
#include "cull/cull_lerrnoP.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_PrintCore.h"

/**
 * @brief Writes a enumeration array to a file stream
 *
 * Writes a enumeration array to a file stream
 *
 * @param ep enumeration
 * @param fp file stream
 *
 * @note A thin wrapper over ocs::tom::print_what() of the core (CS-3077).
 *
 * @see Typed form: ocs::tom::What<T>::print().
 */
void lWriteWhatTo(const lEnumeration *ep, FILE *fp) {
   dstring buffer = DSTRING_INIT;
   try {
      ocs::tom::print_what(&buffer, ep, 0);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number
   }
   const char *str = sge_dstring_get_string(&buffer);
   if (fp != nullptr && str != nullptr) {
      fprintf(fp, "%s", str);
   }
   sge_dstring_free(&buffer);
}

/**
 * @brief Write enumeration to dynamic string
 *
 * Write enumeration to dynamic string
 *
 * @param ep enumeration
 * @param[out] buffer the string to append to
 *
 * @note A thin wrapper over ocs::tom::print_what() of the core (CS-3077).
 */
void lWriteWhatToDString(const lEnumeration *ep, dstring *buffer) {
   try {
      ocs::tom::print_what(buffer, ep, 0);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number
   }
}

