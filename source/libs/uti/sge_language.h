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
 * @brief Localisation: gettext initialisation and message translation
 */

#include <cinttypes>

#ifdef __SGE_COMPILE_WITH_GETTEXT__
/// no-op when the build has no gettext support
#define sge_init_language(x, y) sge_init_languagefunc((x), (y))

int sge_init_languagefunc(char *package, char *localeDir);

#else
/// no-op when the build has no gettext support
#define sge_init_language(x, y) 
#endif

#ifndef EXTRACT_MESSAGES

/* define all language function types */
/** @brief Translates one message, normally `gettext` */
typedef char *(*gettext_func_type)(const char *);

/** @brief Selects the locale, normally `setlocale` */
typedef char *(*setlocale_func_type)(int lc, const char *name);

/** @brief Binds a message domain to a directory, normally `bindtextdomain` */
typedef char *(*bindtextdomain_func_type)(const char *domainname, const char *dirname);

/** @brief Selects the message domain, normally `textdomain` */
typedef char *(*textdomain_func_type)(const char *donainname);

/** @brief Install the localisation functions to use
 *
 * Lets the caller supply its own implementations, which is what makes the
 * message extraction build work without a real gettext.
 */
void sge_init_language_func(gettext_func_type, setlocale_func_type, bindtextdomain_func_type, textdomain_func_type);

/** @brief Translate a message without adding a message id
 * @param x the message to translate
 * @return the translation, or @p x when none exists
 */
const char *sge_gettext__(const char *x);

/** @brief Translate a message, for use from application code
 * @param x the message to translate
 * @return the translation, or @p x when none exists
 */
const char *sge_gettext(const char *x);
/** @brief Translate a message and prefix it with its id when that is enabled
 * @param msg_id numeric id of the message
 * @param msg_str the message text
 * @return the translation, prefixed with @p msg_id when id output is on
 * @see #sge_set_message_id_output
 */
const char *sge_gettext_(int msg_id, const char *msg_str);

/** @brief Switch printing of numeric message ids on or off
 * @param flag non-zero to prefix messages with their id
 */
void sge_set_message_id_output(int flag);

/** @brief Is printing of numeric message ids switched on?
 * @return non-zero when messages are prefixed with their id
 */
int sge_get_message_id_output();

#endif
