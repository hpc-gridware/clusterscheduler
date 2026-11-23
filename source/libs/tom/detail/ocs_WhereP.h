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

/* ---- the operators of a condition ---- */

/// the operator of a condition node, _lCondition::op; numbered without gaps, 0 is no operator
enum {
   NO_OP = 0,     ///< no operator; what a node initialised with zeros holds
   SUBSCOPE,      ///< a nested condition on a sub-list or sub-object
   EQUAL,         ///< `==`
   NOT_EQUAL,     ///< `!=`
   LOWER_EQUAL,   ///< `<=`
   LOWER,         ///< `<`
   GREATER_EQUAL, ///< `>=`
   GREATER,       ///< `>`
   BITMASK,       ///< all bits of the operand are set
   STRCASECMP,    ///< string comparison ignoring case
   PATTERNCMP,    ///< the value matches a shell style pattern
   HOSTNAMECMP,   ///< host name comparison, honouring aliases
   AND,           ///< `&&`
   OR,            ///< `||`
   NEG            ///< `!`
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
