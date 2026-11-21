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
 * @brief Internal representation of a selection condition
 */


#include "tom/ocs_Types.h"
#include "tom/detail/ocs_MultitypeP.h"

/* ---- tokens of the format strings and operators of a condition, returned by scan() ---- */

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

/**
 * @brief One node of a selection condition, as a binary expression tree
 *
 * A condition is either a *comparison* of a field against a value, or a
 * *logical* combination of one or two sub-conditions. _lCondition::op decides
 * which, and therefore which arm of the union is live — the same discipline
 * as @ref _lMultiType.
 */
struct _lCondition {
   int op;                      ///< the operator; decides which arm of #operand is live
   union {
      /// live when #op is a comparison
      struct {
         int pos;               ///< position of the field in the descriptor
         int mt;                ///< type of the compare value
         int nm;                ///< the field being compared
         lMultiType val;        ///< the value to compare against
      } cmp;
      /// live when #op is a logical operator
      struct {
         lCondition *first;     ///< first operand
         lCondition *second;    ///< second operand, or nullptr for a unary operator
      } log;
   } operand;                   ///< the comparison or the logical operands
};
