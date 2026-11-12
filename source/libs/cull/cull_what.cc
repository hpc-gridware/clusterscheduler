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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Building and applying field selections (`lWhat`)
 */
#include <cstdarg>
#include <cstring>
#include <utility>
#include <vector>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_list.h"
#include "cull/cull_db.h"
#include "cull/cull_parse.h"
#include "cull/cull_multitype.h"
#include "cull/cull_whatP.h"
#include "cull/cull_lerrnoP.h"
#include "cull/ocs_cull_descr.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_SelectCore.h"

static lEnumeration *subscope_lWhat(cull_parse_state *state, va_list *app);

/**
 * @brief Build a int vector
 *
 * Marks a field as selected, in the form #lIntVector2What consumes.
 *
 * @param[in,out] job_field the array to mark in, terminated by #NoName
 * @param nm field name id; ignored when already present
 */
void nm_set(int job_field[], int nm) {
   DENTER(TOP_LAYER);

   int i;

   /* seek it */
   for (i = 0; job_field[i] != NoName; i++)
      if (job_field[i] == nm) {
         DRETURN_VOID; /*found*/
      }

   /* set it */
   DPRINTF("%s\n", lNm2Str(nm));
   job_field[i++] = nm;
   job_field[i] = NoName;

   DRETURN_VOID;
}

/**
 * @brief Build the shared reduced descriptor a field selection describes
 *
 * @param dst_dpp the shared descriptor, holding a reference for the caller, who
 *        gives it up with ocs_cull_descr_release(); nullptr if nothing is selected
 * @param src_dp the full descriptor
 * @param enp the field selection
 * @return 0 on success, -1 on error
 */
int lReduceDescr(const lDescr **dst_dpp, const lDescr *src_dp, lEnumeration *enp) {
   DENTER(TOP_LAYER);

   if (!dst_dpp || !src_dp || !enp) {
      DRETURN(-1);
   }
   *dst_dpp = nullptr;

   if (lCountWhat(enp, src_dp) == 0) {
      DRETURN(0);
   }

   bool owned;
   const lDescr *dp = ocs_cull_what_descr(enp, src_dp, &owned);
   if (dp == nullptr) {
      DRETURN(-1);
   }
   *dst_dpp = owned ? dp : ocs_cull_descr_ref(dp);

   DRETURN(0);
}

/* ------------------------------------------------------------ 
   _lWhat creates an enumeration array. This is used in lWhat
   to choose special fields of a list element.
 */
/**
 * @brief Build a field selection from a format and a field list
 *
 * The flat counterpart of #lWhat, behind lIntVector2What(): the format
 * `%T(%I...)`, `%T(ALL)` or `%T(NONE)` names no sub-selections, and the fields
 * come as an array instead of variable arguments.
 *
 * @param fmt the format, with one `%I` per entry of @p nm_list
 * @param dp descriptor of the object type
 * @param nm_list the selected field names
 * @param nr_nm how many entries @p nm_list holds
 * @return the selection, to be released with #lFreeWhat, or nullptr on error
 *
 * @note A thin wrapper over ocs::tom::make_what(); the format is parsed here.
 * @see Typed form: ocs::tom::what<T>(fields)
 */
lEnumeration *_lWhat(const char *fmt, const lDescr *dp,
                     const int *nm_list, int nr_nm) {
   DENTER(CULL_LAYER);

   int n;
   const char *s;
   int error_status;
   cull_parse_state state;

   if (!fmt) {
      error_status = LENOFORMATSTR;
      goto error;
   }
   /* how many fields are selected */
   for (n = 0, s = fmt; *s; s++)
      if (*s == '%')
         n++;

   /* are the number of %'s the number of names - 1 ? */
   if (nr_nm != n - 1) {
      error_status = LESYNTAX;
      goto error;
   }

   memset(&state, 0, sizeof(state));
   if (scan(fmt, &state) != TYPE) {
      /* %T expected */
      error_status = LESYNTAX;
      goto error;
   }
   eat_token(&state);                 /* eat %T */
   if (scan(nullptr, &state) != BRA) {
      /* ( expected */
      error_status = LESYNTAX;
      goto error;
   }
   eat_token(&state);                 /* eat ( */

   /* Special cases CULL_ALL fields are selected or NONE is selected */
   switch (scan(nullptr, &state)) {
      case CULL_ALL:
      case CULL_NONE:
         try {
            const int pos = scan(nullptr, &state) == CULL_ALL ? WHAT_ALL : WHAT_NONE;
            eat_token(&state);
            DRETURN(ocs::tom::make_what_special(pos));
         } catch (const ocs::tom::ListCoreError &e) {
            error_status = e.lerrno();
            goto error;
         }

      default:
         break;
   }

   {
      /* for an empty field list "%T()" the one entry read is the NoName terminating it, as before */
      if (n > 1) {
         n--;
      }
      std::vector<ocs::tom::WhatEntry> entries;
      entries.reserve(n);
      for (int i = 0; i < n; i++) {
         entries.push_back({nm_list[i], nullptr});
         if (lGetPosInDescr(dp, nm_list[i]) < 0) {
            error_status = LENAMENOT;
            goto error;
         }
         if (scan(nullptr, &state) != FIELD) {
            error_status = LESYNTAX;
            goto error;
         }
         eat_token(&state);
      }
      if (scan(nullptr, &state) != KET) {
         error_status = LESYNTAX;
         goto error;
      }
      eat_token(&state);                 /* eat ) */

      try {
         DRETURN(ocs::tom::make_what(dp, entries));
      } catch (const ocs::tom::FieldException &) {
         error_status = dp == nullptr ? LEDESCRNULL : LENAMENOT;
      } catch (const ocs::tom::ListCoreError &e) {
         error_status = e.lerrno();
      }
   }

   error:
   LERROR(error_status);
   DPRINTF("error_status = %d\n", error_status);
   DRETURN(nullptr);
}

/**
 * @brief Create a ne enumeration
 *
 * Create a new enumeration. fmt describes the format of the enumeration
 *
 * @param fmt format string: element := type "(" attribute_list ")" . type := "%T" . attribute_list := "ALL" | "NONE" | attributes . attributes = { "%I" | "%I" "->" element } . examples: 1) "%T(NONE)" 2) "%T(ALL)" 3) "%T(%I%I)" 4) "%T(%I%I->%T(%I%I))" ...             - varibale list of arguments varargs corresponding to examples above: 1) JB_Type 2) JB_Type 3) JB_Type JB_job_numer JB_ja_tasks 4) JB_Type JB_job_numer JB_ja_tasks JAT_Type JAT_task_number JAT_status
 *
 * @return new enumeration
 *
 * @note "%I" is equivalent with "%I->%T(ALL)"
 *       "" is NOT equivalent with "%I->%T(NONE)"
 *
 * @note A thin wrapper over ocs::tom::make_what() and ocs::tom::make_what_special(); the format is parsed here.
 *       A field the type does not hold gives nullptr with LENAMENOT; before, it left an entry uninitialised.
 * @see Typed form: ocs::tom::what<F...>(), What<T>::with<F>(), ocs::tom::what_all<T>()
 */
lEnumeration *lWhat(const char *fmt, ...) {
   DENTER(CULL_LAYER);

   lEnumeration *enumeration = nullptr;
   va_list ap;
   cull_parse_state state;

   if (!fmt) {
      LERROR(LENOFORMATSTR);
      DRETURN(nullptr);
   }

   /* 
      initialize scan function, the actual token is scanned again 
      in the subscope fuction. There we call eat_token to go ahead
    */
   memset(&state, 0, sizeof(state));
   scan(fmt, &state);

   /* parse */
   va_start(ap, fmt);
   enumeration = subscope_lWhat(&state, &ap);
   va_end(ap);

   if (!enumeration) {
      LERROR(LEPARSECOND);
      DRETURN(nullptr);
   }

   DRETURN(enumeration);
}

/**
 * @brief Parse one level of an lWhat() format and build its selection through the core
 *
 * Reads `%T(` with the descriptor, then `ALL`, `NONE` or the fields, each
 * optionally followed by `->` and a nested level, then `)`.
 *
 * @param state the scanner state, at `%T`
 * @param app the variable arguments, at the descriptor of this level
 * @return the selection, or nullptr with the error number set
 */
static lEnumeration *subscope_lWhat(cull_parse_state *state, va_list *app) {
   DENTER(CULL_LAYER);

   const lDescr *dp = nullptr;
   int token;

   if (scan(nullptr, state) != TYPE) {
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }
   eat_token(state);                 /* eat %T */
   if (!(dp = va_arg(*app, const lDescr *))) {
      LERROR(LEDESCRNULL);
      DRETURN(nullptr);
   }

   if (scan(nullptr, state) != BRA) {
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }
   eat_token(state);                 /* eat ( */

   token = scan(nullptr, state);

   if (token == CULL_ALL || token == CULL_NONE) {
      eat_token(state);
      if (scan(nullptr, state) != KET) {
         LERROR(LESYNTAX);
         DRETURN(nullptr);
      }
      eat_token(state);                 /* eat ) */
      try {
         DRETURN(ocs::tom::make_what_special(token == CULL_ALL ? WHAT_ALL : WHAT_NONE));
      } catch (const ocs::tom::ListCoreError &e) {
         LERROR(e.lerrno());
         DRETURN(nullptr);
      }
   }

   // the sub-selections are owned by the entries until the core takes them over
   std::vector<ocs::tom::WhatEntry> entries;
   const auto free_entries = [&entries]() {
      for (ocs::tom::WhatEntry &entry : entries) {
         lFreeWhat(&entry.sub);
      }
   };
   do {
      if (token != FIELD) {
         free_entries();
         LERROR(LESYNTAX);
         DRETURN(nullptr);
      }
      eat_token(state);                 /* eat %I */
      entries.push_back({va_arg(*app, int), nullptr});

      token = scan(nullptr, state);
      if (token == SUBSCOPE) {
         eat_token(state);                 /* eat -> */
         entries.back().sub = subscope_lWhat(state, app);
         token = scan(nullptr, state);
      }
   } while (token == FIELD);

   if (scan(nullptr, state) != KET) {
      free_entries();
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }
   eat_token(state);                 /* eat ) */

   try {
      DRETURN(ocs::tom::make_what(dp, entries));
   } catch (const ocs::tom::FieldException &) {
      // a field the type does not hold; before, it left an entry of the selection uninitialised
      LERROR(LENAMENOT);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   DRETURN(nullptr);
}

/**
 * @brief Creates a enumeration array requesting all elements
 *
 * Creates a enumeration array that requests complete elements
 * of whatever typed list. This is a shortcut for
 * `lWhat("%T(ALL)", <List_type>)`, cause for all the descriptor is not
 * needed anyway, it is available from the list itself.
 *
 * @return enumeration
 *
 * @note A thin wrapper over ocs::tom::make_what_special().
 * @see Typed form: ocs::tom::what_all<T>()
 */
lEnumeration *lWhatAll() {
   try {
      return ocs::tom::make_what_special(WHAT_ALL);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Frees a enumeration array
 *
 * Frees a enumeration array
 *
 * @param ep enumeration, will be set to nullptr
 */
void lFreeWhat(lEnumeration **ep) {
   if (ep != nullptr) {
      ocs::tom::free_what(*ep);
   }
}

/**
 * @brief Returns size of enumeration
 *
 * Returns size of enumeration
 *
 * @param enp enumeration
 * @param dp descriptor
 *
 * @return number of fields in enumeration
 *
 * @note A thin wrapper over ocs::tom::count_what().
 */
int lCountWhat(const lEnumeration *enp, const lDescr *dp) {
   return ocs::tom::count_what(enp, dp);
}

/**
 * @brief Copy a enumeration array
 *
 * Copy a enumeration array
 *
 * @param ep enumeration
 *
 * @return new copy of enumeration
 */
lEnumeration *lCopyWhat(const lEnumeration *ep) {
   try {
      return ocs::tom::copy_what(ep);
   } catch (const ocs::tom::FieldException &) {
      LERROR(LEENUMNULL);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Create a enumeration from int array
 *
 * Create a enumeration from int array
 *
 * @param dp descriptor of the object type
 * @param intv the selected field names, terminated by #NoName
 *
 * @return enumeration
 *
 * @note Built through _lWhat() and so through ocs::tom::make_what().
 * @see Typed form: ocs::tom::what<T>(fields)
 */
lEnumeration *lIntVector2What(const lDescr *dp, const int intv[]) {
   DENTER(CULL_LAYER);

   lEnumeration *what;
   char fmtstr[2000];
   int i;

   /*
      reduce a descriptor to get one with only
      these fields that are needed for modify
    */
   strcpy(fmtstr, "%T(");
   for (i = 0; intv[i] != NoName; i++) {
      strcat(fmtstr, "%I");
   }
   strcat(fmtstr, ")");

   DPRINTF("fmtstr: %s\n", fmtstr);

   what = _lWhat(fmtstr, dp, intv, i);

   DRETURN(what);
}

/**
 * @brief Merge one field selection into another
 *
 * The fields of @p what2 are added to @p what1, so the result selects the
 * union of both.
 *
 * @param[in,out] what1 the selection to extend
 * @param[in,out] what2 the selection to merge in; consumed and set to nullptr
 * @return 0 on success, -1 on error
 *
 * @note A thin wrapper over ocs::tom::merge_what() of the core (CS-3077). A field both select keeps the
 *       merge of both nested selections; before, the nested selection of @p what2 was lost, see merge_what().
 *
 * @see Typed form: ocs::tom::What<T>::merge().
 */
int lMergeWhat(lEnumeration **what1, lEnumeration **what2) {
   try {
      *what1 = ocs::tom::merge_what(*what1, std::exchange(*what2, nullptr));
   } catch (const ocs::tom::ListCoreError &e) {
      // both are freed
      *what1 = nullptr;
      LERROR(e.lerrno());
      return -1;
   }
   return 0;
}

/**
 * @brief Attach a field selection to a sub-list or sub-object field
 *
 * Lets a selection say "of this sub-list, only these fields".
 *
 * @param what1 the selection to modify
 * @param nm the sub-list or sub-object field the selection applies inside
 * @param[in,out] what2 the selection for that field; consumed and set to nullptr
 * @return 0 on success, -1 when @p nm is not part of @p what1
 *
 * @note A thin wrapper over ocs::tom::set_sub_what() of the core (CS-3077).
 *
 * @see Typed form: ocs::tom::What<T>::with() for a field not yet selected.
 */
int lWhatSetSubWhat(lEnumeration *what1, int nm, lEnumeration **what2) {
   if (what1 == nullptr || what2 == nullptr) {
      return -1;
   }
   try {
      ocs::tom::set_sub_what(what1, nm, std::exchange(*what2, nullptr));
   } catch (const ocs::tom::FieldException &) {
      return -1;
   }
   return 0;
}



