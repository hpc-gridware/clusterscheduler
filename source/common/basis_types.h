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

#  define SGE_ADD_MSG_ID(x) (sge_set_message_id_output(1),(x),sge_set_message_id_output(0))   ///< prefix the message with its numeric id, so a message can be identified whatever language it was printed in
#  define _(x)               sge_gettext(x)          ///< translate a literal message
#  define _MESSAGE(x, y)      sge_gettext_((x),(y))  ///< translate message number @p x, whose untranslated text is @p y
#  define _SGE_GETTEXT__(x)  sge_gettext__(x)        ///< translate without adding a message id
#else
#  define SGE_ADD_MSG_ID(x) (x)   ///< without gettext there is nothing to prefix
#  define _(x)              (x)   ///< without gettext the message is its own translation
#  define _MESSAGE(x,y)     (y)   ///< without gettext the untranslated text is used
#  define _SGE_GETTEXT__(x) (x)   ///< without gettext the message is its own translation
#endif

/** @name printf formats for the fixed-width types
 *
 * `uint32_t` is `unsigned int` on a 64-bit target and `unsigned long` on a
 * 32-bit one, so no single literal format string is correct for both. These
 * macros pick the right one at compile time, and every `printf` of a `uint32_t`
 * or `uint64_t` in the tree goes through them.
 *
 * @note The original comment here reads "sge_u32 for strictly unsigned, not
 *       nice, but did I use %d for an unsigned?" - the author's own doubt about
 *       whether every call site was converted. Kept as a warning to check
 *       rather than assume.
 * @{
 */
#if defined(TARGET_64BIT) || defined(FREEBSD) || defined(NETBSD)
#  define sge_u32_letter  "u"   ///< the length modifier alone, for building a format string
#  define sge_u64          "%lu"   ///< printf format for a `uint64_t`
#  define sge_u32         "%u"   ///< printf format for a `uint32_t`, unsigned
#  define sge_x32          "%x"   ///< printf format for a `uint32_t`, hexadecimal
#  define sge_fu32         "d"   ///< the format body for a signed 32-bit value, without the `%`
#  define sge_fuu32        "u"   ///< the format body for an unsigned 32-bit value, without the `%`
#else   // 32 bit
#  define sge_u32_letter  "lu"   ///< the length modifier alone, for building a format string
#  define sge_u64          "%llu"   ///< printf format for a `uint64_t`
#  define sge_u32         "%lu"   ///< printf format for a `uint32_t`, unsigned
#  define sge_x32          "%lx"   ///< printf format for a `uint32_t`, hexadecimal
#  define sge_fu32         "ld"   ///< the format body for a signed 32-bit value, without the `%`
#  define sge_fuu32        "lu"   ///< the format body for an unsigned 32-bit value, without the `%`
#endif
/** @} */

#define uid_t_fmt "%u"   ///< printf format for a `uid_t`
#define gid_t_fmt "%u"   ///< printf format for a `gid_t`
#define pid_t_fmt "%d"   ///< printf format for a `pid_t`, which is signed

/* _POSIX_PATH_MAX is only 255 and this is less than in most real systmes */
/** @brief Longest path the cluster handles
 *
 * Deliberately larger than `_POSIX_PATH_MAX`, which is only 255 and smaller
 * than what real systems allow.
 */
#define SGE_PATH_MAX    static_cast<uint32_t>(1024)

#define MAX_STRING_SIZE 2048   ///< size of a #stringT buffer
/** @brief A fixed-size string buffer, used where a `dstring` would be overkill */
typedef char stringT[MAX_STRING_SIZE];

#define MAX_VERIFY_STRING 512   ///< longest string accepted when verifying a job request

