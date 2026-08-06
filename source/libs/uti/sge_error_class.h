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
 * @brief A collector that accumulates several errors instead of just the last
 *
 * Where `sge_err.h` keeps one error per thread, this keeps a list per
 * object, so a validation pass can report everything that was wrong rather
 * than only the final failure.
 *
 * Written in the C "class" idiom that predates the C++ code in this library:
 * the struct carries an opaque handle plus a table of function pointers, and
 * is called as `eh->error(eh, ...)`. Create one with #sge_error_class_create
 * and release it with #sge_error_class_destroy.
 *
 * Reading the errors back means taking an iterator, which must be advanced
 * before the first read and destroyed afterwards:
 *
 * @code
 * sge_error_iterator_class_t *iter = eh->iterator(eh);
 * while (iter->next(iter)) {
 *    printf("%s\n", iter->get_message(iter));
 * }
 * sge_error_iterator_class_destroy(&iter);
 * @endcode
 */

#include <cinttypes>
#include "cull/cull.h"

/// A collector of errors; see @ref sge_error_class_str
typedef struct sge_error_class_str sge_error_class_t;


/// A cursor over collected errors; see @ref sge_error_iterator_class_str
typedef struct sge_error_iterator_class_str sge_error_iterator_class_t;

/**
 * @brief A cursor over the errors collected in a @ref sge_error_class_str
 *
 * Obtained from `sge_error_class_str::iterator`. It points before the first
 * error, so `next` has to be called once before the first `get_message`.
 * Release it with #sge_error_iterator_class_destroy.
 */
struct sge_error_iterator_class_str {
   void *sge_error_iterator_handle; ///< opaque cursor state, owned by the iterator

   /// Message of the error the cursor is on; owned by the collector, do not free
   const char *(*get_message)(sge_error_iterator_class_t *thiz);

   /// Quality of the error the cursor is on
   uint32_t (*get_quality)(sge_error_iterator_class_t *thiz);

   /// Type of the error the cursor is on
   uint32_t (*get_type)(sge_error_iterator_class_t *thiz);

   /// Advance to the next error; returns false once the end is reached
   bool (*next)(sge_error_iterator_class_t *thiz);
};

/**
 * @brief A collector holding every error reported to it, in the order reported
 */
struct sge_error_class_str {
   void *sge_error_handle; ///< opaque list of collected errors, owned by the object

   /// Append an error, formatting the message `printf` style
   void (*error)(sge_error_class_t *thiz, int error_type, int error_quality, const char *fmt, ...);

   /// Append an error from an already started `va_list`
   void (*verror)(sge_error_class_t *thiz, int error_type, int error_quality, const char *fmt, va_list ap);

   /// Discard every collected error, leaving the object reusable
   void (*clear)(sge_error_class_t *thiz);

   /// Has anything been reported?
   bool (*has_error)(sge_error_class_t *thiz);

   /// Is at least one collected error of this quality?
   bool (*has_quality)(sge_error_class_t *thiz, int error_quality);

   /// Is at least one collected error of this type?
   bool (*has_type)(sge_error_class_t *thiz, int error_type);

   /// Take a cursor over the collected errors; the caller must destroy it
   sge_error_iterator_class_t *(*iterator)(sge_error_class_t *thiz);
};


sge_error_class_t *sge_error_class_create();

void sge_error_class_clear(sge_error_class_t *thiz);

void sge_error_class_destroy(sge_error_class_t **error_handler);

void sge_error_iterator_class_destroy(sge_error_iterator_class_t **emc);


/**
 * @brief Print the collected errors
 *
 * @warning **Declared but never defined**, and never called. Any use fails to
 *          link. Kept only because removing a public declaration is a code
 *          change; it is a deletion candidate.
 *
 * @param eh the collector to print
 */
void showError(sge_error_class_t *eh);

/**
 * @brief Append the collected errors to a dstring
 *
 * @warning **Declared but never defined**, and never called. Any use fails to
 *          link. Kept only because removing a public declaration is a code
 *          change; it is a deletion candidate.
 *
 * @param eh the collector to render
 * @param[out] ds the string to append to
 */
void sge_error_to_dstring(sge_error_class_t *eh, dstring *ds);
