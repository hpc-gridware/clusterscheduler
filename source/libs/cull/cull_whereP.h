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


#include "cull/cull_where.h"
#include "cull/cull_multitypeP.h"

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


/* new data structure for dynamically buildable args for lWhere         */
/* the var_args model requires the knowledge of the fields at run time  */
/// One field/value pair handed to the `lWhere()` parser
struct _WhereArg {
   lDescr *descriptor; ///< descriptor of the object type the field belongs to
   int field;          ///< the field to compare
   lMultiType value;   ///< the value to compare against
}; 
