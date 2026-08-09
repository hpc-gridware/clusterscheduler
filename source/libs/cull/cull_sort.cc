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
 */
int lInsertSorted(const lSortOrder *so, lListElem *ep, lList *lp) {
   DENTER(TOP_LAYER);

   if (!so || !ep || !lp) {
      DRETURN(-1);
   }

   lListElem *tmp;
   for_each_rw(tmp, lp) {
      if (lSortCompare(ep, tmp, so) <= 0) {
         break;                    /* insert before tmp */
      }
   }

   if (tmp) {
      /* insert before tmp */
      tmp = lPrevRW(tmp);
      lInsertElem(lp, tmp, ep);
   } else {
      /* append to list */
      lAppendElem(lp, ep);
   }

   DRETURN(0);
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
 */
int lResortElem(const lSortOrder *so, lListElem *ep, lList *lp) {
   lDechainElem(lp, ep);
   lInsertSorted(so, ep, lp);
   return 0;
}

#if 0
/* ------------------------------------------------------------ 

   writes a sort order (for debugging purposes)

 */
void lWriteSortOrder(
        const lSortOrder *sp
) {
   int i;

   DENTER(CULL_LAYER);

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

/* ----------------------------------------

   wrapper function adding global_sort_order
   to the passed parameters and calls
   lSortCompare()

 */
int lSortCompareUsingGlobal(const void *ep0, const void *ep1) {
   return lSortCompare(*(lListElem **) ep0, *(lListElem **) ep1, cull_state_get_global_sort_order());
}

/* ------------------------------------------------------------ 

   compares two elementes ep0 and ep1 due to a given sort
   order sp 

   returns compare values like strcmp:

   <  -1 
   == 0 
   >  1 

 */
int lSortCompare(
        const lListElem *ep0,
        const lListElem *ep1,
        const lSortOrder *sp
) {
   int i, result = 0;

   DENTER(CULL_LAYER);

   for (i = 0; !result && sp[i].nm != NoName; i++) {

      switch (mt_get_type(sp[i].mt)) {
         case lIntT:
            result = intcmp(lGetPosInt(ep0, sp[i].pos), lGetPosInt(ep1, sp[i].pos));
            break;
         case lStringT:
            result = sge_strnullcmp(lGetPosString(ep0, sp[i].pos), lGetPosString(ep1, sp[i].pos));
            break;
         case lHostT:
            result = sge_strnullcmp(lGetPosHost(ep0, sp[i].pos), lGetPosHost(ep1, sp[i].pos));
            break;
         case lUlongT:
            result = ulongcmp(lGetPosUlong(ep0, sp[i].pos), lGetPosUlong(ep1, sp[i].pos));
            break;
         case lUlong64T:
            result = ulong64cmp(lGetPosUlong64(ep0, sp[i].pos), lGetPosUlong64(ep1, sp[i].pos));
            break;
         case lDoubleT:
            result = doublecmp(lGetPosDouble(ep0, sp[i].pos), lGetPosDouble(ep1, sp[i].pos));
            break;
         case lLongT:
            result = longcmp(lGetPosLong(ep0, sp[i].pos), lGetPosLong(ep1, sp[i].pos));
            break;
         case lBoolT:
            result = boolcmp(lGetPosBool(ep0, sp[i].pos), lGetPosBool(ep1, sp[i].pos));
            break;
         case lRefT:
            result = refcmp(lGetPosRef(ep0, sp[i].pos), lGetPosRef(ep1, sp[i].pos));
            break;
         default:
            unknownType("lSortCompare");
      }
      result *= sp[i].ad;
   }

   DRETURN(result);
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
 */
lSortOrder *lParseSortOrder(const lDescr *dp, const char *fmt, va_list ap) {
   const char *s = nullptr;
   lSortOrder *sp = nullptr;
   int i, n;
   cull_parse_state state;

   DENTER(CULL_LAYER);

   if (!dp || !fmt) {
      DRETURN(nullptr);
   }

   /* how many fields are selected (for malloc) */
   for (n = 0, s = fmt; *s; s++) {
      if (*s == '%') {
         n++;
      }
   }

   if (!(sp = (lSortOrder *) sge_malloc(sizeof(lSortOrder) * (n + 1)))) {
      LERROR(LEMALLOC);
      DRETURN(nullptr);
   }

   memset(&state, 0, sizeof(state));
   scan(fmt, &state);                   /* Initialize scan */
   for (i = 0; i < n; i++) {
      sp[i].nm = va_arg(ap, int);
      if ((sp[i].pos = lGetPosInDescr(dp, sp[i].nm)) < 0) {
         sge_free(&sp);
         LERROR(LENAMENOT);
         DRETURN(nullptr);
      }
      sp[i].mt = dp[sp[i].pos].mt;

      /* next token */
      if (scan(nullptr, &state) != FIELD) {
         sge_free(&sp);
         LERROR(LESYNTAX);
         DRETURN(nullptr);
      }
      /* THIS IS FOR TYPE CHECKING */
      /* COMMENTED OUT
         switch( scan(nullptr, &state) ) {
         case INT:
         if (mt_get_type(sp[i].mt) != lIntT )
         incompatibleType("lSortList (should be a lIntT)\n");
         break;

         case STRING:
         if (mt_get_type(sp[i].mt) !=lStringT )
         incompatibleType("lSortList (should be a lStringT)\n");
         break;

         case ULONG:
         if (mt_get_type(sp[i].mt) !=lUlongT )
         incompatibleType("lSortList (should be a lUlongT)\n");
         break;

         case DOUBLE:
         if (mt_get_type(sp[i].mt) !=lDoubleT )
         incompatibleType("lSortList (should be a lDoubleT)\n");
         break;
         default:
         sge_free(&sp);
         unknownType("lSortList");
         } 
       */
      eat_token(&state);              /* eat %I */
      switch (scan(nullptr, &state)) {
         case PLUS:
            sp[i].ad = 1;
            break;
         case MINUS:
            sp[i].ad = -1;
            break;
         default:
            /* +/- is missing */
            sge_free(&sp);
            LERROR(LESYNTAX);
            DRETURN(nullptr);
      }
      eat_token(&state);
   }
   sp[n].nm = NoName;
   sp[n].mt = lEndT;

   DRETURN(sp);
}

/**
 * @brief Release a sort order
 *
 * @param[in,out] so the order to free; set to nullptr on return
 */
void lFreeSortOrder(lSortOrder **so) {
   sge_free(so);
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
   if ((so[i].pos = lGetPosInDescr(dp, so[i].nm)) < 0)
      return -1;

   so[i].mt = dp[so[i].pos].mt;

   /* set end mark */
   i++;
   so[i].nm = NoName;

   return 0;
}

