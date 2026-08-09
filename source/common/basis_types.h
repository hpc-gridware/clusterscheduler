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
 *  Portions of this code are Copyright (c) 2011 Univa Corporation.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Gettext wrappers, so that a message can be marked for translation whether or not the build has gettext
 */

#ifdef __SGE_COMPILE_WITH_GETTEXT__

#  include <libintl.h>
#  include <locale.h>
#  include "uti/sge_language.h"

#  define SGE_ADD_MSG_ID(x) (sge_set_message_id_output(1),(x),sge_set_message_id_output(0))
#  define _(x)               sge_gettext(x)
#  define _MESSAGE(x, y)      sge_gettext_((x),(y))
#  define _SGE_GETTEXT__(x)  sge_gettext__(x)
#else
#  define SGE_ADD_MSG_ID(x) (x)
#  define _(x)              (x)
#  define _MESSAGE(x,y)     (y)
#  define _SGE_GETTEXT__(x) (x)
#endif

/* set sge_u32 and sge_x32 for 64 or 32 bit machines */
/* sge_u32 for strictly unsigned, not nice, but did I use %d for an unsigned? */
#if defined(TARGET_64BIT) || defined(FREEBSD) || defined(NETBSD)
#  define sge_u32_letter  "u"
#  define sge_u64          "%lu"
#  define sge_u32         "%u"
#  define sge_x32          "%x"
#  define sge_fu32         "d"
#  define sge_fuu32        "u"
#else
#  define sge_u32_letter  "lu"
#  define sge_u64          "%llu"
#  define sge_u32         "%lu"
#  define sge_x32          "%lx"
#  define sge_fu32         "ld"
#  define sge_fuu32        "lu"
#endif

#define uid_t_fmt "%u"
#define gid_t_fmt "%u"
#define pid_t_fmt "%d"

/* _POSIX_PATH_MAX is only 255 and this is less than in most real systmes */
#define SGE_PATH_MAX    static_cast<uint32_t>(1024)

#define MAX_STRING_SIZE 2048
typedef char stringT[MAX_STRING_SIZE];

#define MAX_VERIFY_STRING 512

