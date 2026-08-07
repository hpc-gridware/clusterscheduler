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
 * @brief Internal representation of a sort order
 */

#include "cull/cull_sort.h"

/**
 * @brief One sort criterion: which field to compare and in which direction
 *
 * A sort order is an array of these, built by #lParseSortOrderVarArg or
 * #lCreateSortOrder plus #lAddSortCriteria, and terminated by an entry whose
 * field name is #NoName. Criteria are applied in order, the next one deciding
 * only where the previous compared equal.
 */
struct _lSortOrder {
   int pos;  ///< position of the field in the descriptor, resolved once when the order is built
   int mt;   ///< the field's type and attributes, copied from the descriptor
   int nm;   ///< the field to compare, or #NoName in the terminating entry
   int ad;   ///< direction: +1 ascending, -1 descending
};

/**
 * @brief Compare two elements using the sort order stored in the cull state
 *
 * Has the signature `qsort()` expects, which is why the order is passed
 * through global state rather than as an argument.
 *
 * @param ep0 first element, as a `lListElem **`
 * @param ep1 second element, as a `lListElem **`
 * @return negative, 0 or positive as for `strcmp()`
 */
int lSortCompareUsingGlobal(const void *ep0, const void *ep1);
