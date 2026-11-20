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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Building and evaluating selection conditions (`lWhere`)
 */

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <fnmatch.h>

/* do not compile in monitoring code */
#ifndef NO_SGE_COMPILE_DEBUG
#define NO_SGE_COMPILE_DEBUG
#endif

#include "uti/sge_hostname.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "cull/msg_cull.h"
#include "cull/cull_listP.h"
#include "cull/cull_whereP.h"
#include "cull/cull_parse.h"
#include "cull/cull_lerrnoP.h"
#include "tom/ocs_FieldException.h"
#include "tom/ocs_ListCore.h"
#include "tom/ocs_PrintCore.h"
#include "tom/ocs_SelectCore.h"

static lCondition *read_val(const lDescr *dp, cull_parse_state *state, va_list *app);

static lCondition *factor(const lDescr *dp, cull_parse_state *state, va_list *app);

static lCondition *product(const lDescr *dp, cull_parse_state *state, va_list *app);

static lCondition *negfactor(const lDescr *dp, cull_parse_state *state, va_list *app);

static lCondition *sum(const lDescr *dp, cull_parse_state *state, va_list *app);

static lCondition *subscope(cull_parse_state *state, va_list *app);


/**
 * @brief The comparison of the core for an operator token of lWhere()
 * @param token the token, one of EQUAL to HOSTNAMECMP
 * @return the comparison
 */
static ocs::tom::CompareOp compare_op_of(const int token) {
   switch (token) {
      case NOT_EQUAL:
         return ocs::tom::CompareOp::NotEqual;
      case LOWER_EQUAL:
         return ocs::tom::CompareOp::LowerEqual;
      case LOWER:
         return ocs::tom::CompareOp::Lower;
      case GREATER_EQUAL:
         return ocs::tom::CompareOp::GreaterEqual;
      case GREATER:
         return ocs::tom::CompareOp::Greater;
      case BITMASK:
         return ocs::tom::CompareOp::Bitmask;
      case STRCASECMP:
         return ocs::tom::CompareOp::NoCase;
      case PATTERNCMP:
         return ocs::tom::CompareOp::Pattern;
      case HOSTNAMECMP:
         return ocs::tom::CompareOp::SameHost;
      default:
         return ocs::tom::CompareOp::Equal;
   }
}

/**
 * @brief Combines two conditions with an OR
 *
 * Combines the conditions 'cp0' and 'cp1' logically with an OR
 *
 * @param cp0 first condition
 * @param cp1 second condition
 *
 * @return cp0 OR cp1
 *
 * @note A thin wrapper over ocs::tom::make_logic(); the operands are taken over.
 * @see Typed form: `std::move(a) || std::move(b)` on ocs::tom::Where<T>
 */
lCondition *lOrWhere(const lCondition *cp0, const lCondition *cp1) {
   if (!cp0 || !cp1) {
      LERROR(LECONDNULL);
      return nullptr;
   }
   try {
      // the combination takes both operands over
      return ocs::tom::make_logic(ocs::tom::LogicOp::Or, const_cast<lCondition *>(cp0), const_cast<lCondition *>(cp1));
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Cobines two conditions with an AND
 *
 * Combines the conditions 'cp0' and 'cp1' with an logical AND.
 *
 * @param cp0 first condition
 * @param cp1 second condition
 *
 * @return 'cp0' AND 'cp1'
 *
 * @note A thin wrapper over ocs::tom::make_logic(); the operands are taken over.
 * @see Typed form: `std::move(a) && std::move(b)` on ocs::tom::Where<T>
 */
lCondition *lAndWhere(const lCondition *cp0, const lCondition *cp1) {
   if (!cp0 || !cp1) {
      LERROR(LECONDNULL);
      return nullptr;
   }
   try {
      // the combination takes both operands over
      return ocs::tom::make_logic(ocs::tom::LogicOp::And, const_cast<lCondition *>(cp0), const_cast<lCondition *>(cp1));
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}

/**
 * @brief Write a condition struct to file stream
 *
 * Write a condition struct to file stream.
 *
 * @param cp condition
 * @param fp file stream
 *
 * @note A thin wrapper over ocs::tom::print_where() of the core (CS-3077). A null stream writes nothing, as
 *       before, where the text went to the debug output, which this file does not compile in.
 *
 * @see Typed form: ocs::tom::Where<T>::print().
 */
void lWriteWhereTo(const lCondition *cp, FILE *fp) {
   dstring buffer = DSTRING_INIT;
   try {
      ocs::tom::print_where(&buffer, cp, 0);
   } catch (const ocs::tom::ListCoreError &) {
      // the core has set the error number
   }
   const char *str = sge_dstring_get_string(&buffer);
   if (fp != nullptr && str != nullptr) {
      fprintf(fp, "%s", str);
   }
   sge_dstring_free(&buffer);
}

/**
 * @brief Creates a condition tree
 *
 * Creates a condition tree. The condition is stated as a format
 * string and an associated list of additional parameters.
 *
 * @param fmt format string %I                         - JB_job_number (!= lList) %T                         - ocs::tom::JB::descriptor.e (descriptor) ==, <, >, <=, >=, !=, ==   - comp. operator %s                         - string %d                         - int %u                         - ulong ...             - additional Arguments
 *
 * @return new condition
 *
 * @note The format is parsed here; every node is built by ocs::tom::make_compare(), ocs::tom::make_subscope()
 *       and ocs::tom::make_logic().
 * @see Typed form: ocs::tom::field<F>() with its operators and named comparisons
 */
lCondition *lWhere(const char *fmt, ...) {
   DENTER(CULL_LAYER);

   lCondition *cond;
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
   cond = subscope(&state, &ap);
   va_end(ap);

   if (!cond) {
      LERROR(LEPARSECOND);
      DRETURN(nullptr);
   }

   DRETURN(cond);
}

static lCondition *subscope(cull_parse_state *state, va_list *app) {
   DENTER(CULL_LAYER);

   const lDescr *dp = nullptr;
   lCondition *cp = nullptr;

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

   if (!(cp = sum(dp, state, app))) {
      LERROR(LESUM);
      DRETURN(nullptr);
   }

   if (scan(nullptr, state) != KET) {
      LERROR(LESYNTAX);
      lFreeWhere(&cp);
      DRETURN(nullptr);
   }
   eat_token(state);                 /* eat ) */

   DRETURN(cp);
}

/**
 * @brief Parse a sum of an lWhere() format: products joined by `||`
 * @param dp the descriptor of the level
 * @param state the scanner state
 * @param app the variable arguments
 * @return the condition, or nullptr with the error number set
 */
static lCondition *sum(const lDescr *dp, cull_parse_state *state, va_list *app) {
   DENTER(CULL_LAYER);

   lCondition *cp = product(dp, state, app);

   while (cp != nullptr && scan(nullptr, state) == OR) {
      eat_token(state);              /* eat || */
      try {
         cp = ocs::tom::make_logic(ocs::tom::LogicOp::Or, cp, product(dp, state, app));
      } catch (const ocs::tom::FieldException &) {
         // an operand that did not parse; the core freed the other one
         DRETURN(nullptr);
      } catch (const ocs::tom::ListCoreError &e) {
         LERROR(e.lerrno());
         DRETURN(nullptr);
      }
   }

   DRETURN(cp);
}

/**
 * @brief Parse a product of an lWhere() format: factors joined by `&&`
 * @param dp the descriptor of the level
 * @param state the scanner state
 * @param app the variable arguments
 * @return the condition, or nullptr with the error number set
 */
static lCondition *product(const lDescr *dp, cull_parse_state *state, va_list *app) {
   DENTER(CULL_LAYER);

   lCondition *cp = factor(dp, state, app);

   while (cp != nullptr && scan(nullptr, state) == AND) {
      eat_token(state);              /* eat && */
      try {
         cp = ocs::tom::make_logic(ocs::tom::LogicOp::And, cp, factor(dp, state, app));
      } catch (const ocs::tom::FieldException &) {
         DRETURN(nullptr);
      } catch (const ocs::tom::ListCoreError &e) {
         LERROR(e.lerrno());
         DRETURN(nullptr);
      }
   }

   DRETURN(cp);
}

/**
 * @brief Parse a factor of an lWhere() format: a negated or a plain bracketed sum or comparison
 * @param dp the descriptor of the level
 * @param state the scanner state
 * @param app the variable arguments
 * @return the condition, or nullptr with the error number set
 */
static lCondition *factor(const lDescr *dp, cull_parse_state *state, va_list *app) {
   DENTER(CULL_LAYER);

   if (scan(nullptr, state) == NEG) {
      eat_token(state);
      try {
         DRETURN(ocs::tom::make_logic(ocs::tom::LogicOp::Not, negfactor(dp, state, app), nullptr));
      } catch (const ocs::tom::FieldException &) {
         DRETURN(nullptr);
      } catch (const ocs::tom::ListCoreError &e) {
         LERROR(e.lerrno());
         DRETURN(nullptr);
      }
   }

   DRETURN(negfactor(dp, state, app));
}

static lCondition *negfactor(
        const lDescr *dp,
        cull_parse_state *state,
        va_list *app
) {
   DENTER(CULL_LAYER);

   lCondition *cp;

   if (scan(nullptr, state) == BRA) {
      eat_token(state);

      cp = sum(dp, state, app);

      if (scan(nullptr, state) != KET) {
         lFreeWhere(&cp);
         LERROR(LESYNTAX);
         DRETURN(nullptr);
      }
      eat_token(state);

      DRETURN(cp);
   }

   DRETURN(read_val(dp, state, app));

}

/**
 * @brief Parse one comparison of an lWhere() format and build it through the core
 *
 * Reads `%I`, the operator and the value, or `%I -> %T(...)` for a condition
 * on the elements of a list field.
 *
 * @param dp the descriptor of the level
 * @param state the scanner state
 * @param app the variable arguments, at the field
 * @return the condition, or nullptr with the error number set; a value of another type than the field aborts
 */
static lCondition *read_val(const lDescr *dp, cull_parse_state *state, va_list *app) {
   DENTER(CULL_LAYER);

   if (!dp) {
      LERROR(LEDESCRNULL);
      DRETURN(nullptr);
   }

   if (scan(nullptr, state) != FIELD) {
      LERROR(LESYNTAX);
      DRETURN(nullptr);
   }
   eat_token(state);

   const int nm = va_arg(*app, int);
   const int pos = lGetPosInDescr(dp, nm);
   if (pos < 0) {
      LERROR(LENAMENOT);
      DRETURN(nullptr);
   }
   const int type = mt_get_type(dp[pos].mt);

   const int op = scan(nullptr, state);
   switch (op) {
      case EQUAL:
      case NOT_EQUAL:
      case LOWER_EQUAL:
      case LOWER:
      case GREATER_EQUAL:
      case GREATER:
      case BITMASK:
      case STRCASECMP:
      case PATTERNCMP:
      case HOSTNAMECMP:
         eat_token(state);
         break;

      case SUBSCOPE:
         eat_token(state);
         if (type != lListT) {
            LERROR(LEINCTYPE);
            DRETURN(nullptr);
         }
         try {
            // a nested condition that did not parse is taken as none, as before
            DRETURN(ocs::tom::make_subscope(dp, nm, subscope(state, app)));
         } catch (const ocs::tom::FieldException &) {
            LERROR(LEINCTYPE);
         } catch (const ocs::tom::ListCoreError &e) {
            LERROR(e.lerrno());
         }
         DRETURN(nullptr);

      default:
         LERROR(LEOPUNKNOWN);
         DRETURN(nullptr);
   }

   lMultiType value{};
   int value_type;
   switch (scan(nullptr, state)) {
      case INT:
         if (type != lIntT)
            incompatibleType(MSG_CULL_WHERE_SHOULDBEINTT);
         value.i = va_arg(*app, lInt);
         value_type = lIntT;
         break;

      case STRING:
         if (type != lStringT && type != lHostT)
            incompatibleType(MSG_CULL_WHERE_SHOULDBESTRINGT);
         value.str = va_arg(*app, char *);
         value_type = lStringT;
         break;

      case ULONG:
         if (type != lUlongT)
            incompatibleType(MSG_CULL_WHERE_SHOULDBEULONGT);
         value.ul = va_arg(*app, lUlong);
         value_type = lUlongT;
         break;

      case ULONG64:
         if (type != lUlong64T)
            incompatibleType(MSG_CULL_WHERE_SHOULDBEULONG64T);
         value.ul64 = va_arg(*app, lUlong64);
         value_type = lUlong64T;
         break;

      case DOUBLE:
         if (type != lDoubleT)
            incompatibleType(MSG_CULL_WHERE_SHOULDBEDOUBLET);
         value.db = va_arg(*app, lDouble);
         value_type = lDoubleT;
         break;

      case LONG:
         if (type != lLongT)
            incompatibleType(MSG_CULL_WHERE_SHOULDBELONGT);
         value.l = va_arg(*app, lLong);
         value_type = lLongT;
         break;

      case BOOL:
         if (type != lBoolT)
            incompatibleType(MSG_CULL_WHERE_SHOULDBEBOOL);
         // a bool is promoted to int in variable arguments
         value.b = va_arg(*app, int);
         value_type = lBoolT;
         break;

      case REF:
         if (type != lRefT)
            incompatibleType(MSG_CULL_WHERE_SHOULDBEREFT);
         value.ref = va_arg(*app, lRef);
         value_type = lRefT;
         break;

      default:
         unknownType("lWhere");
         DRETURN(nullptr);
   }
   eat_token(state);

   try {
      DRETURN(ocs::tom::make_compare(dp, nm, compare_op_of(op), value_type, value));
   } catch (const ocs::tom::FieldException &) {
      LERROR(LEINCTYPE);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   DRETURN(nullptr);
}


/**
 * @brief Free a condition
 *
 * Free a condition.
 *
 * @param cp condition, will be set to nullptr
 */
void lFreeWhere(lCondition **cp) {
   if (cp != nullptr) {
      ocs::tom::free_where(*cp);
   }
}

/**
 * @brief Decide if a element suffices a condition
 *
 * Decide if a element suffices a condition.
 *
 * @param ep element
 * @param cp condition
 *
 * @return result 0 - false 1 - true
 *
 * @note A thin wrapper over ocs::tom::compare().
 * @see Typed form: ocs::tom::ConstElement<T>::matches()
 */
int lCompare(const lListElem *ep, const lCondition *cp) {
   try {
      return ocs::tom::compare(ep, cp) ? 1 : 0;
   } catch (const ocs::tom::FieldException &) {
      LERROR(LEELEMNULL);
   }
   return 0;
}

/**
 * @brief Copy a condition
 *
 * Copy a condition.
 *
 * @param cp condition
 *
 * @return Copy of 'cp'
 */
lCondition *lCopyWhere(const lCondition *cp) {
   try {
      return ocs::tom::copy_where(cp);
   } catch (const ocs::tom::ListCoreError &e) {
      LERROR(e.lerrno());
   }
   return nullptr;
}
