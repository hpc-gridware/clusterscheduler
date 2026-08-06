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
 * @brief A per-thread "last error" with an id and a message
 *
 * Each thread has one error slot. #sge_err_set overwrites it; the accessors
 * read it back. Nothing is allocated by the caller — the storage is
 * thread-local and released when the thread ends.
 */

#include <cinttypes>

/// What kind of error was recorded
enum _sge_err_t {
   SGE_ERR_SUCCESS = 0,  ///< no error; the value the slot is cleared to
   SGE_ERR_MEMORY,       ///< an allocation failed
   SGE_ERR_PARAMETER,    ///< a function was called with an invalid argument
   SGE_ERR_FILE_EXIST    ///< a file that was expected to be absent already exists
};

/// What kind of error was recorded; see @ref _sge_err_t
typedef enum _sge_err_t sge_err_t;

void
sge_err_set(sge_err_t id, const char *format, ...);

void
sge_err_get(uint32_t pos, sge_err_t *id, char *message, size_t size);

/**
 * @brief Number of errors recorded by the calling thread
 *
 * @warning **Declared but never defined.** There is no implementation in
 *          `sge_err.cc` or anywhere else, so any call fails to link. Kept only
 *          because removing a public declaration is a code change; it is a
 *          deletion candidate.
 *
 * @return would be the number of recorded errors
 */
uint32_t
sge_err_get_errors();

bool
sge_err_has_error();

void
sge_err_clear(); 
