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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Dynamically growing string buffer
 */

#include <cinttypes>

#include <cstdio>

/**
 * @brief Define to initialize dstring variables
 *
 * Define to preinitialize dstring variables
 *
 * @code
 * {
 *    dstring error_msg = DSTRING_INIT;
 * }
 * @endcode
 *
 * @note The DSTRING_INIT counterpart for static buffers is sge_dstring_init()
 */

#define DSTRING_INIT {nullptr, 0, 0, false}

/** @brief Declare a #dstring backed by a stack buffer of @p s bytes
 *
 * Declares both the buffer and the #dstring named @p n wrapping it, so no
 * allocation happens unless the content outgrows the buffer.
 *
 * @code
 * DSTRING_STATIC(error_msg, MAX_STRING_SIZE);
 * @endcode
 */
#define DSTRING_STATIC(n, s) char _buffer_for_##n[s] = "\0"; \
                                    dstring n = {_buffer_for_##n, 0, s, true}

/** @brief A string buffer that grows as needed
 *
 * Initialise it with #DSTRING_INIT for a purely dynamic buffer,
 * #DSTRING_STATIC or #sge_dstring_init to start from a caller supplied buffer,
 * or #sge_dstring_init_dynamic to preallocate. Release it with
 * #sge_dstring_free.
 *
 * A static buffer switches to an allocated one as soon as the content no longer
 * fits, so the caller never has to check for overflow.
 */
typedef struct {
   char *s;         ///< the buffer: allocated, or caller supplied when #is_static
   size_t length;   ///< length of the string, excluding the terminating NUL
   size_t size;     ///< capacity of #s in bytes
   bool is_static;  ///< true while #s is the caller's buffer and must not be freed
} dstring;

/* DSTRING_INIT counterpart when static buffers are wrapped with dstring */
void sge_dstring_init(dstring *sb, char *buffer, size_t size);

dstring *sge_dstring_init_dynamic(dstring *sb, size_t size);

const char *sge_dstring_append(dstring *sb, const char *a);

const char *sge_dstring_nappend(dstring *sb, const char *a, size_t n);

const char *sge_dstring_append_dstring(dstring *sb, const dstring *a);

const char *sge_dstring_append_char(dstring *sb, const char a);

const char *sge_dstring_append_mailopt(dstring *sb, uint32_t mailopt);

const char *sge_dstring_sprintf(dstring *sb, const char *fmt, ...);

const char *sge_dstring_vsprintf(dstring *sb, const char *fmt, va_list ap);

const char *sge_dstring_sprintf_append(dstring *sb, const char *fmt, ...);

void sge_dstring_clear(dstring *sb);

void sge_dstring_free(dstring *sb);

const char *sge_dstring_get_string(const dstring *string);

char *sge_dstring_get_string_rw(dstring *string);

const char *sge_dstring_copy_string(dstring *sb, const char *str);

const char *sge_dstring_copy_dstring(dstring *sb1, const dstring *sb2);

size_t sge_dstring_strlen(const dstring *string);

size_t sge_dstring_remaining(const dstring *string);

const char *sge_dstring_ulong_to_binstring(dstring *sb, uint32_t number);

bool sge_dstring_split(dstring *string, char character, dstring *before, dstring *after);

void sge_dstring_strip_trailing_blanks(dstring *string);

const char *sge_dstring_from_argv(dstring *dstr, int argc, const char *argv[],
                                  bool quote_whitespace, bool quote_patterns);

const char *sge_strerror(int errnum, dstring *buffer);
