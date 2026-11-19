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
 * @brief Error codes and messages of the cull library
 */
#include <cstdarg>
#include <cstdio>
#include <cstring>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "tom/cull_lerrnoP.h"
#include "tom/msg_cull.h"

#include <cinttypes>

/// Last cull error, as one of the `LE*` codes
int lerrno = 0;

/**
 * @brief Report a field accessed as the wrong type, and abort
 *
 * @param str name of the function that detected the mismatch
 * @return never returns; the process is aborted
 */
int incompatibleType(const char *str) {
   DENTER(TOP_LAYER);

   int i;

   for (i = 0; i < 5; i++)
           DPRINTF("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
   DPRINTF("incompatible type in function %s()\n", str);
   for (i = 0; i < 5; i++)
           DPRINTF("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

   ocs::TerminationManager::trigger_abort();
   DRETURN(-1);
}

/**
 * @brief Report a field accessed as the wrong type, with a formatted message, and abort
 *
 * @param fmt `printf` style format naming the field and the types involved
 * @param ... the format's arguments
 * @return never returns; the process is aborted
 */
int incompatibleType2(const char *fmt, ...) {
   DENTER(TOP_LAYER);

   va_list ap;
   char buf[MAX_STRING_SIZE];

   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   va_end(ap);

   CRITICAL(SFNMAX, buf);
   fprintf(stderr, SFNMAX, buf);

   ocs::TerminationManager::trigger_abort();
   DRETURN(-1);
}

/**
 * @brief Report an unknown field type
 *
 * Only logs and sets LEUNKTYPE; it does not abort. The caller decides how to go on.
 *
 * @param str name of the function that detected it
 * @return -1
 */
int unknownType(const char *str) {
   DENTER(CULL_LAYER);

   DPRINTF("Unknown Type in %s.\n", str);
   LERROR(LEUNKTYPE);
   DRETURN(-1);
}
