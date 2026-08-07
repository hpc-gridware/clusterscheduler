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
 *  Portions of this code are Copyright 2011 Univa Inc.
 *
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The scanner used by the cull format strings
 */


/**
 * @brief Scanner state, held by the caller so parsing is reentrant
 *
 * The cull format strings (`lWhat()`, `lWhere()`, `lParseSortOrder()`) are
 * scanned with #scan; keeping the position here rather than in a global lets
 * several parses run at once.
 */
typedef struct {
   int token_is_valid;    ///< true when #token holds a token not yet consumed
   const char *t;         ///< current position in the format string
   int token;             ///< the token last recognised
} cull_parse_state;

/**
 * @brief Consume the token #scan last returned
 *
 * @param state the scanner state
 */
void eat_token(cull_parse_state *state);

/**
 * @brief Return the next token of a cull format string
 *
 * @param s the format string to start scanning, or nullptr to continue
 * @param state the scanner state
 * @return the token, one of #NO_TOKEN and its neighbours
 */
int scan(const char *s, cull_parse_state *state);


/* -------------- values returned by scan() --------------------- */

enum {
   NO_TOKEN = 0, ///< end of input, or nothing recognised

   TYPE, ///< `%T`, an object type
   FIELD, ///< `%I`, a field name
   SUBSCOPE, ///< a nested scope, for sub-lists and sub-objects
   PLUS, ///< `+`, sort ascending
   MINUS, ///< `-`, sort descending
   INT, ///< `%d`, an int value
   STRING, ///< `%s`, a string value
   ULONG, ///< `%u`, a 32 bit value
   ULONG64, ///< a 64 bit value
   FLOAT, ///< a float value
   DOUBLE, ///< `%f`, a double value
   LONG, ///< a long value
   CHAR, ///< `%c`, a character value
   BOOL, ///< `%b`, a boolean value
   REF, ///< an opaque reference value
   CULL_ALL, ///< `ALL`, select every field
   CULL_NONE, ///< `NONE`, select no field
   EQUAL, ///< `==`
   NOT_EQUAL, ///< `!=`
   LOWER_EQUAL, ///< `<=`
   LOWER, ///< `<`
   GREATER_EQUAL, ///< `>=`
   GREATER, ///< `>`
   BITMASK, ///< `m`, all bits of the operand are set
   STRCASECMP, ///< string comparison ignoring case
   PATTERNCMP, ///< the value matches a shell style pattern
   HOSTNAMECMP, ///< host name comparison, honouring aliases
   AND, ///< `&&`
   OR, ///< `||`
   NEG, ///< `!`
   BRA, ///< `(`
   KET ///< `)`
};
