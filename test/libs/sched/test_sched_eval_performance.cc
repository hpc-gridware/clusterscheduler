/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2006
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
 *   Copyright: 2006 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Unit tests for eval performance in `libs/sched`
 */

/*----------------------------------------------------
 *
 * The litte test program matches an "attribute" against
 * a regular exression which follows the above grammar and the
 * regular expression from the Cluster Scheduler complex(5) man page
 * and prints "TRUE" or "FALSE"
 * For Expect test returns 0 .. TRUE
 *                         1 .. FALSE
 *                        -1 .. ERROR
 *
 *----------------------------------------------------*/
#include <cstdio>
#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "sge_complex_schedd.h"

#define T 0        ///< the expression evaluated true; 0 rather than 1, matching the evaluator's convention
#define F 1        ///< the expression evaluated false
#define ERROR -1   ///< the expression could not be evaluated

/** @brief The name of a #T / #F / #ERROR value, for the failure message
 * @param match the value to name
 */
#define RESULT(match) (match == -1) ? "ERROR" : (match == 0) ? "TRUE" : "FALSE"

#ifndef __INSURE__
/** @name How many iterations the benchmark runs
 *
 * Cut down drastically under Insure++, whose instrumentation makes the full
 * count take hours without making the comparison any more informative.
 * @{
 */
#define BALANCE_LOOP_COUNT 20   ///< outer runs, to average out scheduling noise
#define LOOP_COUNT 300000       ///< evaluations per run
#else
#define BALANCE_LOOP_COUNT 2   ///< outer runs under Insure++
#define LOOP_COUNT 300         ///< evaluations per run under Insure++
#endif
/** @} */

/* Local functions and variables */
static int tests(int (*test)(ocs::CEntry::Type, const char *, const char *, int));
static int test_match_new(ocs::CEntry::Type, const char *, const char *, int );
static int test_match_old(ocs::CEntry::Type, const char *, const char *, int );

/*-----------------------------------------------------------
 * call:   test_eval_performace value  expression
 * print out some statistic about the efectivity of the algorithm
 *-----------------------------------------------------------*/
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_evel_performance");

   int ret;
   int i, j;
   uint64_t start_tm=0;
   uint64_t end_tm=0;
   uint64_t new_total_tm=0;
   uint64_t old_total_tm=0;
   
   ret = 0;
   if(argc!=4){
      fprintf(stdout, "\nEval performance tests\n");

      for(j=0;(j<BALANCE_LOOP_COUNT) && (ret==0);j++){
         start_tm = sge_get_gmt64();
         for(i=0;(i<LOOP_COUNT) && (ret==0);i++){
            ret=tests(&test_match_old);
         }
         end_tm = sge_get_gmt64();
         old_total_tm+=(end_tm-start_tm);
         /* Old match */
         start_tm = sge_get_gmt64();
         for(i=0;(i<LOOP_COUNT)&&(ret==0);i++){
            ret=tests(&test_match_new);
         }
         end_tm = sge_get_gmt64();
         new_total_tm+=(end_tm-start_tm);
      } /* End of j */

      fprintf(stdout, "Try:$time test_eval_performance [0..old, 1..new] '<expresion>' '<value>'\n");
      fprintf(stdout, "All eval_tests result is: %s \n", RESULT(ret));
      if(ret==0){
         fprintf(stdout, "The consumed time old is " sge_u64", new  is " sge_u64" \n", old_total_tm, new_total_tm);
         ret = (int)((double) new_total_tm/(3*old_total_tm)); /* No more than 3x slower */
         fprintf(stdout, "Performance tests result is: %s \n", RESULT(ret));
      }
      DRETURN(ret);
   } 
   for(i=0;(i<LOOP_COUNT) && (ret==0);i++){
     if(argv[1][0]=='0'){ 
       string_base_cmp_old(ocs::CEntry::Type::RESTR, argv[2], argv[3]);
     } else {
       string_base_cmp(ocs::CEntry::Type::RESTR, argv[2], argv[3]);
     }
   }     
  DRETURN(0);
}

static int tests(int (*test)(ocs::CEntry::Type, const char *, const char *, int)) {
   int ret=0;
   /* Regular tests */
   ret=ret|test(ocs::CEntry::Type::STR, "solaris", "solaris", T);
   ret=ret|test(ocs::CEntry::Type::CSTR, "Solaris", "solaris", T);
   ret=ret|test(ocs::CEntry::Type::RESTR, "sol-amd64", "sol-amd64", T);
   ret=ret|test(ocs::CEntry::Type::RESTR, "sol-amd*|sol-spa*", "sol-amd64", T);
   
   /* test for host names */
   ret=ret|test(ocs::CEntry::Type::HOST, "latte3.czech.sun.com", "latte3.czech.sun.com", T);
   ret=ret|test(ocs::CEntry::Type::HOST, "latte3", "latte3", T);
   return ret;
}

static int test_match_new(ocs::CEntry::Type type, const char *expression, const char *value, int expected) {
   int match;
   match = string_base_cmp(type, expression, value);
   if(match!=expected) {
      fprintf(stderr, "!!!NEW UNEXPECTED RESULT!!!: %s => eval_expr(%s,%s), expected: %s \n",
      RESULT(match) , expression, value, RESULT(expected) );
      return 1;
   }
   return 0;
}

static int test_match_old(ocs::CEntry::Type type, const char *expression, const char *value, int expected) {
   int match;
   match = string_base_cmp_old(type, expression, value);
   if(match!=expected) {
      fprintf(stderr, "!!!OLD UNEXPECTED RESULT!!!: %s => eval_expr(%s,%s), expected: %s \n",
      RESULT(match) , expression, value, RESULT(expected) );
      return 1;
   }
   return 0;
}
