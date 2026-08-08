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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Sorting the execution hosts by the load formula
 *
 * The load formula of the scheduler configuration is an expression over load
 * values and complex attributes, e.g. `np_load_avg` or
 * `np_load_avg+0.5*mem_used`. scaled_mixed_load() evaluates it for one host
 * and sort_host_list() sorts the host list by the result, which is the order
 * in which hosts are then offered to a job.
 */
#include <cstring>
#include <cstdlib>

#if !defined(DARWIN) && !defined(FREEBSD)
#   include <malloc.h>
#endif

#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_schedd_conf.h"

#include "sched/sge_select_queue.h"
#include "sched/sge_complex_schedd.h"

#include "sort_hosts.h"
#include "uti/sge.h"

/** The operators the load formula parser knows, in the order of the LOAD_OP_* values */
static const char load_ops[]={
        '+',
        '-',
        '*',
        '/',
        '&',
        '|',
        '^',
        '\0'
};


/**
 * @brief The operators of the load formula
 *
 * The values other than #LOAD_OP_NONE are indices into `load_ops`, which holds
 * the character of each operator.
 */
enum {
        LOAD_OP_NONE=-1,   ///< No operator seen yet
        LOAD_OP_PLUS,      ///< Addition, `+`
        LOAD_OP_MINUS,     ///< Subtraction, `-`
        LOAD_OP_TIMES,     ///< Multiplication, `*`
        LOAD_OP_DIV,       ///< Division, `/`
        LOAD_OP_AND,       ///< Bitwise and, `&`
        LOAD_OP_OR,        ///< Bitwise or, `|`
        LOAD_OP_XOR        ///< Bitwise exclusive or, `^`
};

/* prototypes */
static int get_load_value(double *dvalp, lListElem *global, lListElem *host, const lList *centry_list, const char *attrname);

/**
 * @brief Sorts the host list by the load evaluation formula
 *
 * The formula is taken from the scheduler configuration. The global host is
 * left where it is - it is not a candidate for a job.
 *
 * @param[in,out] hl          the host list to be sorted (`EH_Type`), sorted
 *                            in place
 * @param[in]     centry_list the complex entry list (`CE_Type`)
 *
 * @return 0 on success, -1 otherwise
 */
int sort_host_list(lList *hl, const lList *centry_list)
{
   DENTER(TOP_LAYER);
   lListElem *global = host_list_locate(hl, SGE_GLOBAL_NAME);
   const char *load_formula = sconf_get_load_formula();

   for_each_rw_lv(hlp, hl) {
      if (hlp == global) {
         continue;
      }

      /* build complexes for that host */
      double load = scaled_mixed_load(load_formula, global, hlp, centry_list);
      lSetDouble(hlp, EH_sort_value, load);
      DPRINTF("%s: %f\n", lGetHost(hlp, EH_name), load);
   }
   sge_free(&load_formula);

   if (lPSortList(hl,"%I+", EH_sort_value)) {
      DRETURN(-1);
   } else {
      DRETURN(0);
   }
}


/**
 * @brief Computes the scaled and weighted load of one host
 *
 * The load formula is evaluated against the host's load values and its load
 * scaling list, so the result is comparable between hosts of different size.
 * The load correction of correct_load() is already contained in the values
 * this reads.
 *
 * @param[in] load_formula the load evaluation formula, without blanks
 * @param[in] global       the global host, for the globally reported values
 * @param[in] host         the host to compute the load for (`EH_Type`)
 * @param[in] centry_list  the complex entry list (`CE_Type`)
 *
 * @return the load value the host list is sorted by, or #ERROR_LOAD_VAL on
 *         error - which sorts a host with incorrect load reporting to the end
 *         instead of to the front
 */
double scaled_mixed_load(const char* load_formula, lListElem *global,
                         lListElem *host, const lList *centry_list)
{
   char *cp = nullptr;
   char *tf = nullptr;
   char *ptr = nullptr;
   char *ptr2 = nullptr;
   char *par_name = nullptr;
   char *op_ptr=nullptr;

   double val=0, val2=0;
   double load=0;
   int op_pos, next_op=LOAD_OP_NONE;
   char *lasts = nullptr;

   DENTER(TOP_LAYER);

   /* we'll use strtok ==> we need a safety copy */
   if ((tf = strdup(load_formula)) == nullptr) {
      DRETURN(ERROR_LOAD_VAL);
   }

   /* 
    * + and - have the lowest precedence. all else are equal,
    * thus formula is delimited by + or - signs
    * if the load formula begins with a "-" we need to multiply the
    * first load value with -1
    */
   if (tf[0] == '-') {
      next_op = LOAD_OP_MINUS;
   }

   for (cp=strtok_r(tf, "+-", &lasts); cp; cp = strtok_r(nullptr, "+-", &lasts)) {

      /* ---------------------------------------- */
      /* get scaled load value                    */
      /* determine 1st components value           */
      if (!(val = strtod(cp, &ptr)) && ptr == cp) {
         /* it is not an integer ==> it's got to be a load value */
         if (!(par_name = sge_delim_str(cp, &ptr, load_ops)) ||
               get_load_value(&val, global, host, centry_list, par_name)) {
            sge_free(&par_name);
            sge_free(&tf);

            DRETURN(ERROR_LOAD_VAL);
         }
         sge_free(&par_name);
      }

      /* ---------------------------------------- */
      /* for the load value                       */
      /* *ptr now contains the delimiting character for val */
      if (*ptr) {
         /* if the delimiter is not \0 it's got to be a operator -> find it */
         if (!(op_ptr=strchr((char *)load_ops, (int) *ptr))) {
            sge_free(&tf);
            DRETURN(ERROR_LOAD_VAL);
         }
         op_pos = (int) (op_ptr - load_ops);

         /* ------------------------------- */
         /* look for a weightening factors  */
         /* determine 2nd component's value */
         ptr++;
         if (!(val2 = strtod(ptr,&ptr2)) && ptr2 == ptr) {
            /* it is not an integer ==> it's got to be a load value */
            if (!(par_name = sge_delim_str(ptr,nullptr,load_ops)) ||
               get_load_value(&val2, global, host, centry_list, par_name)) {
               sge_free(&par_name);
               sge_free(&tf);
               DRETURN(ERROR_LOAD_VAL);
            }
            sge_free(&par_name);
         }

         /* ------------------------------- */
         /*  apply according load operator  */
         switch (op_pos) {
            case LOAD_OP_TIMES:
               val *= val2;
               break;
            case LOAD_OP_DIV:
               val /= val2;
               break;
            case LOAD_OP_AND: {
               uint32_t tmp;
               tmp = (uint32_t)val & (uint32_t)val2;
               val = (double)tmp;
               break;
            }
            case LOAD_OP_OR: {
               uint32_t tmp;
               tmp = (uint32_t)val | (uint32_t)val2;
               val = (double)tmp;
               break;
            }
            case LOAD_OP_XOR: {
               uint32_t tmp;
               tmp = (uint32_t)val ^ (uint32_t)val2;
               val = (double)tmp;
               break;
            }
         }     /* switch (op_pos) */
      }     /* if (*ptr) */

      /* now we have the intermediate result from the subexpression in
       * between a + or - operator in val. next we've to add or
       * subtract from the current result value.
       */

      /* next_op is the next operation from the last run of the while loop.
       * thus next_op now is the operation to applied
       */
      switch (next_op) {
         case LOAD_OP_NONE:
            /* this is the first run -> just set load */
            load = val;
            break;
         case LOAD_OP_PLUS:
            load += val;
            break;
         case LOAD_OP_MINUS:
            load -= val;
            break;
      }

      /* determine next_op from the safety copy of the stripped formula */
      if (load_formula[cp-tf+strlen(cp)] == '+') {
         next_op = LOAD_OP_PLUS;
      }   
      else {
         next_op = LOAD_OP_MINUS;
      }   
   }

   sge_free(&tf);

   DRETURN(load);
}



/***********************************************************************

   get_load_value

 ***********************************************************************/
static int get_load_value(double *dvalp, lListElem *global, lListElem *host, const lList *centry_list, const char *attrname) 
{
   lListElem *cep;

   DENTER(TOP_LAYER);
   
   /* search complex */
   if (strchr(attrname, '$')) {
      attrname++;
   }

   if(!(cep = get_attribute_by_name(global, host, nullptr, attrname, centry_list, nullptr, DISPATCH_TIME_NOW, 0))){
      /* neither load nor consumable available for that host */
      DRETURN(1);
   }

   if (lGetUlong(cep, CE_pj_dominant) & DOMINANT_TYPE_VALUE) {
      *dvalp = lGetDouble(cep, CE_doubleval);
   } else {
      *dvalp = lGetDouble(cep, CE_pj_doubleval);
   }

   lFreeElem(&cep);

   /*
    * No value available.
    */

   DRETURN(0);
}
