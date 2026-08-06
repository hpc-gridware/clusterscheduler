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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Internal representation of a cull field value
 */

#include "cull/cull_list.h"
#include "cull/cull_multitype.h"

/**
 * @brief The value of one cull field
 *
 * Which member is live is decided by the field's type in the descriptor —
 * see @ref _enum_lMultiType and #mt_get_type. Reading the wrong member is not
 * detected by the compiler, which is why the accessors in cull_multitype.cc
 * check the type first and abort through #incompatibleType on a mismatch.
 */
union _lMultiType {
   lDouble db;        ///< value when the field is #lDoubleT
   lUlong ul;         ///< value when the field is #lUlongT
   lUlong64 ul64;     ///< value when the field is #lUlong64T
   lLong l;           ///< value when the field is #lLongT
   lChar c;           ///< value when the field is a single character
   lBool b;           ///< value when the field is #lBoolT
   lInt i;            ///< value when the field is #lIntT
   lString str;       ///< value when the field is #lStringT; owned by the element
   lList *glp;        ///< value when the field is #lListT; owned by the element
   lListElem *obj;    ///< value when the field is #lObjectT; owned by the element
   lRef ref;          ///< value when the field is #lRefT; never owned, never spooled
   lHost host;        ///< value when the field is #lHostT; owned by the element
   lCondition *cp;    ///< a condition, used when a descriptor carries a where clause
};
