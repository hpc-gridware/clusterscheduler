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
 * @brief Interface to the flex scanner that tokenizes flatfile input
 *
 * The scanner itself is generated from `sge_spooling_flatfile_scanner.l`
 * into `sge_spooling_flatfile_scanner.cc`, which is checked in and excluded
 * from the documentation gate. This header is hand written and is the only
 * part of it the rest of the module touches.
 */

#include <sys/types.h>
#include <cstdio>

/** @brief What #spool_lex just recognised
 *
 * Starts at 1 because #spool_lex returns 0 for end of input, the flex
 * convention.
 */
typedef enum {
   SPFT_INT = 1,        ///< A run of digits; negative numbers are not recognised
   SPFT_FLOAT,          ///< Digits, a dot, optional digits; likewise unsigned
   SPFT_TIME,           ///< `h:m:s`
   SPFT_WORD,           ///< An identifier, and the catch-all for anything the reader treats as a value
   SPFT_RANGE,          ///< `n-m`
   SPFT_COMPOP,         ///< A two character comparison operator, as used in resource requests
   SPFT_DELIMITER,      ///< One of the delimiter characters a #spool_flatfile_instr can name
   SPFT_WHITESPACE,     ///< Only returned while #spool_return_whitespace is set
   SPFT_NEWLINE,        ///< End of line, including a line continued with a backslash
   SPFT_UNKNOWN,        ///< Any other character
   SPFT_ERROR_NO_MEMORY ///< Allocation failed while building the token text
} spool_flatfile_token;

extern int spool_line;                ///< Line number of the current token, for error messages
extern int spool_return_whitespace;   ///< While non-zero, whitespace is returned as #SPFT_WHITESPACE instead of being skipped
extern int spool_finish_line;         ///< While non-zero, the rest of the line is returned as one #SPFT_WORD - how a value containing spaces is read
extern char *spool_text;              ///< The text of the current token

/** @brief Fetch the next token
 *
 * @return a #spool_flatfile_token, or 0 at end of input
 */
int spool_lex();

/** @brief Point the scanner at an input stream and reset its state
 *
 * @param input the stream to read
 *
 * @return 0 on success
 */
int spool_scanner_initialize(FILE *input);

/** @brief Release the scanner's buffers */
void spool_scanner_shutdown();
