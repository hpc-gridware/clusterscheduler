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
 * @brief Sorting cull lists by one or more fields
 */

#include <cstring>
#include <vector>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_listP.h"
#include "cull/cull_parse.h"
#include "cull/cull_multitype.h"
#include "cull/cull_sortP.h"
#include "cull/cull_lerrnoP.h"

#include "tom/ocs_ElementCore.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_SoftPath.h"
#include "tom/ocs_SortCore.h"

/* ------------------------------------------------------------ 

   insert ep into sorted list lp using so as sort order

 */
/**
 * @brief Insert an element at the position its sort order dictates
 *
 * @param so the sort order to honour
 * @param ep the element to insert
 * @param lp the list to insert into, already sorted by @p so
 * @return 0 on success, -1 when any argument is nullptr
 *
 * @note A thin wrapper over ocs::tom::insert_sorted().
 * @see Typed form: ocs::tom::List::insert_sorted()
 */
int lInsertSorted(const lSortOrder *so, lListElem *ep, lList *lp) {
   if (!so || !ep || !lp) {
      return -1;
   }
   try {
      ocs::tom::insert_sorted(lp, ep, so);
   } catch (const ocs::tom::FieldException &e) {
      // a bound element or a duplicate key aborted in lInsertElem() and lAppendElem() before
      ocs::tom::legacy_abort("lInsertSorted", e);
   }
   return 0;
}

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
 *
 * @note A thin wrapper over ocs::tom::resort_elem().
 * @see Typed form: ocs::tom::List::resort()
 */
int lResortElem(const lSortOrder *so, lListElem *ep, lList *lp) {
   try {
      ocs::tom::resort_elem(lp, ep, so);
   } catch (const ocs::tom::FieldException &e) {
      switch (e.reason()) {
         case ocs::tom::FieldException::Reason::NullList:
            LERROR(LELISTNULL);
            break;
         case ocs::tom::FieldException::Reason::NullElement:
            LERROR(LEELEMNULL);
            break;
         default:
            ocs::tom::legacy_abort("lResortElem", e);
      }
   }
   return 0;
}

#if 0
/* ------------------------------------------------------------ 

   writes a sort order (for debugging purposes)

 */
void lWriteSortOrder(
        const lSortOrder *sp
) {
   DENTER(CULL_LAYER);

   int i;

   if (!sp) {
      LERROR(LESORTORDNULL);
      return;
   }

   for (i = 0; mt_get_type(sp[i].mt) != lEndT; i++) {
      DPRINTF(("nm: %d mt: %d pos: %d asc/desc: %d\n", sp[i].nm, sp[i].mt,
              sp[i].pos, sp[i].ad));
   }

   DRETURN_VOID;
}
#endif

/**
 * @brief Compare two elements by a sort order
 *
 * @param ep0 the first element
 * @param ep1 the second element
 * @param sp the sort order
 * @return negative if @p ep0 comes first, positive if @p ep1 comes first, 0 if they are equal
 *
 * @note A thin wrapper over ocs::tom::sort_compare().
 */
int lSortCompare(const lListElem *ep0, const lListElem *ep1, const lSortOrder *sp) {
   return ocs::tom::sort_compare(ep0, ep1, sp);
}

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
lSortOrder *lParseSortOrderVarArg(const lDescr *dp, const char *fmt, ...) {
   va_list ap;
   lSortOrder *ret;

   va_start(ap, fmt);
   ret = lParseSortOrder(dp, fmt, ap);
   va_end(ap);
   return ret;
}

/**
 * @brief Creates a sort order array
 *
 * Create a sort oder array due to the given va_list.
 *
 * @code
 * lParseSortOrder(dp,"%I+ %I-", H_hostname, H_memsize )
 *
 * Returns a sort order array which can be used for sorting an list
 * with ascending H_hostname and descending H_memsize.
 * @endcode
 *
 * @param dp descriptor
 * @param fmt format string %d - int %s - char* %u - ulong +  - ascending -  - descending
 * @param ap Attributes within descriptor
 *
 * @return sort order array
 *
 * @note The format is parsed here; the order is built by ocs::tom::make_sort_order().
 * @see Typed form: ocs::tom::sort_order()
 */
lSortOrder *lParseSortOrder(const lDescr *dp, const char *fmt, va_list ap) {
   DENTER(CULL_LAYER);

   if (!dp || !fmt) {
      DRETURN(nullptr);
   }

   /* how many fields are selected */
   int n = 0;
   for (const char *s = fmt; *s; s++) {
      if (*s == '%') {
         n++;
      }
   }

   std::vector<ocs::tom::SortKey> keys;
   cull_parse_state state;
   memset(&state, 0, sizeof(state));
   scan(fmt, &state);                   /* Initialize scan */
   for (int i = 0; i < n; i++) {
      const int nm = va_arg(ap, int);
      if (ocs::tom::present_pos(dp, nm) < 0) {
         LERROR(LENAMENOT);
         DRETURN(nullptr);
      }

      /* next token */
      if (scan(nullptr, &state) != FIELD) {
         LERROR(LESYNTAX);
         DRETURN(nullptr);
      }
      eat_token(&state);              /* eat %I */
      switch (scan(nullptr, &state)) {
         case PLUS:
            keys.push_back({nm, true});
            break;
         case MINUS:
            keys.push_back({nm, false});
            break;
         default:
            /* +/- is missing */
            LERROR(LESYNTAX);
            DRETURN(nullptr);
      }
      eat_token(&state);
   }

   try {
      DRETURN(ocs::tom::make_sort_order(dp, keys));
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   DRETURN(nullptr);
}

/**
 * @brief Release a sort order
 *
 * @param[in,out] so the order to free; set to nullptr on return
 *
 * @note A thin wrapper over ocs::tom::free_sort_order().
 */
void lFreeSortOrder(lSortOrder **so) {
   if (so != nullptr) {
      ocs::tom::free_sort_order(*so);
   }
}


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
lSortOrder *lCreateSortOrder(
        int n
) {
   lSortOrder *sp;

   if (!(sp = (lSortOrder *) sge_malloc(sizeof(lSortOrder) * (n + 1)))) {
      LERROR(LEMALLOC);
      return nullptr;
   }

   /* set end mark at pos 0 */
   sp[0].nm = NoName;

   return sp;
}


/**
 * @brief Append one criterion to a sort order
 *
 * @param dp descriptor of the object type being sorted
 * @param so the order to append to, from #lCreateSortOrder
 * @param nm the field to compare
 * @param up_down_flag +1 to sort ascending, -1 descending
 * @return 0 on success, -1 when @p nm is not a field of @p dp
 */
int lAddSortCriteria(
        const lDescr *dp,
        lSortOrder *so,
        int nm,
        int up_down_flag
) {
   int i;

   /* search next index for insert */
   for (i = 0; so[i].nm != NoName; i++);

   /* use nm to get type and and pos of field in descr dp of list and append new sort criteria */
   so[i].nm = nm;
   so[i].ad = up_down_flag;
   if ((so[i].pos = ocs::tom::present_pos(dp, so[i].nm)) < 0)
      return -1;

   so[i].mt = dp[so[i].pos].mt;

   /* set end mark */
   i++;
   so[i].nm = NoName;

   return 0;
}

