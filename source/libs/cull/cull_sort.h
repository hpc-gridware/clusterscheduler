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
 *  Portions of this software are Copyright (c) 2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Sorting cull lists by one or more fields
 */

#include <cstdarg>

#include "cull/cull_list.h"

/**
 * @brief Compare two elements according to a sort order
 *
 * @param ep0 first element
 * @param ep1 second element
 * @param sp the sort order to apply
 * @return negative, 0 or positive as for `strcmp()`
 */
int lSortCompare(const lListElem *ep0, const lListElem *ep1, const lSortOrder *sp);

/**
 * @brief Insert an element at the position its sort order dictates
 *
 * @param so the sort order to honour
 * @param ep the element to insert
 * @param lp the list to insert into, already sorted by @p so
 * @return 0 on success, -1 when any argument is nullptr
 */
int lInsertSorted(const lSortOrder *so, lListElem *ep, lList *lp);

/**
 * @brief Move an element to the position its sort order now dictates
 *
 * For use after a field the order sorts on has been changed: the element is
 * unchained and re-inserted.
 *
 * @param so the sort order to honour
 * @param ep the element to move; must be in @p lp
 * @param lp the list holding @p ep
 * @return always 0
 */
int lResortElem(const lSortOrder *so, lListElem *ep, lList *lp);

/**
 * @brief Build a sort order from a format string and field names
 *
 * Each criterion is written as `%I` followed by `+` for ascending or `-` for
 * descending, with the field name passed as the matching variadic argument:
 *
 * @code
 * lSortOrder *so = lParseSortOrderVarArg(JR_Type, "%I-", JR_pe_task_id_str);
 * @endcode
 *
 * @param dp descriptor of the object type being sorted
 * @param fmt the criteria, e.g. `"%I+%I-"`
 * @param ... one field name per `%I`, in order
 * @return the sort order, to be released with #lFreeSortOrder, or nullptr on a
 *         syntax error or an unknown field
 */
lSortOrder *lParseSortOrderVarArg(const lDescr *dp, const char *fmt, ...);

lSortOrder *lParseSortOrder(const lDescr *dp, const char *fmt, va_list ap);

/**
 * @brief Release a sort order
 *
 * @param[in,out] so the order to free; set to nullptr on return
 */
void lFreeSortOrder(lSortOrder **so);

#if 0
/* for debugging purposes */
void lWriteSortOrder(const lSortOrder *sp);
#endif


/**
 * @brief Allocate a sort order with room for @p n criteria
 *
 * The criteria are filled in with #lAddSortCriteria. Room for the terminating
 * entry is added automatically.
 *
 * @param n number of criteria the order will hold
 * @return the new order, to be released with #lFreeSortOrder, or nullptr when
 *         it could not be allocated
 */
lSortOrder *lCreateSortOrder(int n);

/**
 * @brief Append one criterion to a sort order
 *
 * @param dp descriptor of the object type being sorted
 * @param so the order to append to, from #lCreateSortOrder
 * @param nm the field to compare
 * @param up_down_flag +1 to sort ascending, -1 descending
 * @return 0 on success, -1 when @p nm is not a field of @p dp
 */
int lAddSortCriteria(const lDescr *dp, lSortOrder *so, int nm, int up_down_flag);
