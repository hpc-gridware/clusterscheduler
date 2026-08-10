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
 * @brief Load values an execution host reports
 *
 * @see sge_load.h
 */

#include <cstring>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_load.h"

/**
 * @brief Is this a load value that never changes while the host runs?
 *
 * Static values - the architecture, the processor count - are reported once
 * rather than with every load report.
 *
 * @param name the load value name
 * @return true when the value is static
 */
bool sge_is_static_load_value(const char *name) {
   DENTER(BASIS_LAYER);

   bool ret = false;

   if (name != nullptr) {
      if (strcmp(name, LOAD_ATTR_ARCH) == 0 || 
          strcmp(name, LOAD_ATTR_NUM_PROC) == 0 ||
          strcmp(name, LOAD_ATTR_MEM_TOTAL) == 0 ||
          strcmp(name, LOAD_ATTR_SWAP_TOTAL) == 0 ||
          strcmp(name, LOAD_ATTR_VIRTUAL_TOTAL) == 0 ) {
        ret = true;
      }
   }

   DRETURN(ret);
}

/**
 * @brief The processor count a host reported
 *
 * @param load_list the host's load values
 * @return the count, or 1 when the host did not report one
 */
int
load_list_get_nproc(const lList *load_list) {
   int nproc = 1;

   const lListElem *ep_nproc = lGetElemStr(load_list, HL_name, LOAD_ATTR_NUM_PROC);
   if (ep_nproc != nullptr) {
      const char *cp = lGetString(ep_nproc, HL_value);
      if (cp != nullptr) {
         nproc = std::max(1, atoi(cp));
      }
   }

   return nproc;
}
