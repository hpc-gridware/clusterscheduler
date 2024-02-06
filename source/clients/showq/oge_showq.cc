/* ___INFO__MARK_BEGIN__ */
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
 *   Copyright: 2009 by Texas Advanced Computing Center
 *
 *   All Rights Reserved.
 *
 ************************************************************************/
/* ___INFO__MARK_END__ */
#include <cstdio>
#include <math.h>

#include "uti/sge_rmon.h"
#include "uti/sge_log.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_answer.h"

#include "gdi/sge_gdi.h"
#include "gdi/sge_gdi2.h"
#include "gdi/sge_gdi_ctx.h"

#include "comm/commlib.h"

#include "sig_handlers.h"
#include "oge_showq_cmdline_tacc.h"
#include "oge_showq_support.h"
#include "msg_common.h"

static bool sge_parse_showq_tacc(lList **alpp, lList ** ppcmdline,
                                 lList ** user_list, const char *username,
                                 int *full, bool *binding, lList **, lList **);

static int showq_show_job_tacc(lList * jid, int full,
                                 const bool binding, lList *, lList *);

/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
int main(int argc, char **argv)
{
   lList          *alp = nullptr;
   lList          *pcmdline = nullptr;
   lList          *user_list = nullptr;
   lList          *sfa_list = nullptr;
   lList          *sfw_list = nullptr;
   lList          *ref_list = nullptr;
   int             full = 0;
   bool            binding = false;
   int             ret = 0;

   DENTER_MAIN(TOP_LAYER, "showq");

   /* Set up the program information name */
   sge_setup_sig_handlers(QSTAT);

   log_state_set_log_gui(1);

   if (sge_gdi2_setup(QSTAT, MAIN_THREAD, &alp) != AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   switch_list_showq_parse_from_cmdline_tacc(&pcmdline, &alp, argv+1);
   if (alp) {
      answer_list_output(&alp); 
      lFreeList(&pcmdline);
      sge_exit(1);
   }

   if (!sge_parse_showq_tacc(&alp, &pcmdline, &user_list, bootstrap_get_username(),
                             &full, &binding, &sfa_list, &sfw_list)) {
      answer_list_output(&alp); 
      lFreeList(&pcmdline);
      lFreeList(&ref_list);
      lFreeList(&user_list);
      sge_exit(1);
   }
   ret = showq_show_job_tacc(user_list, full, binding, sfa_list, sfw_list);

   sge_exit(ret);
   DRETURN(ret);
}


/****
 **** sge_parse_qstat (static)
 ****
 **** 'stage 2' parsing of qstat-options. Gets the options from
 **** ppcmdline, sets the full and empry_qs flags and puts the
 **** queue/res/user-arguments into the lists.
 ****/
static bool sge_parse_showq_tacc(lList **alpp, lList **ppcmdline, lList **user_list,
                  const char *username, int *full, bool *binding, lList **sfa_list,
                  lList **sfw_list)
{
   bool ret = true;
   bool usageshowed = false;
   u_long32 helpflag;
   u_long32 full_sge = 0;
   u_long32 add_me = 0;
   u_long32 bnd = 0;

   DENTER(TOP_LAYER);

   /*
    * Loop over all options. Only valid options can be in the ppcmdline list.
    */
   while (lGetNumberOfElem(*ppcmdline)) {
      if (parse_flag(ppcmdline, "--help", alpp, &helpflag)) {
         usageshowed = showq_usage(stdout);
         ret = false;
         break;
      }
      while (parse_flag(ppcmdline, "-l", alpp, &full_sge)) {
         if (full_sge) {
            *full = 1;
         }
         continue;
      }

      while (parse_flag(ppcmdline, "-cb", alpp, &bnd)) {
         if (bnd) {
            *binding = true;
         }
         continue;
      }

      while (parse_flag(ppcmdline, "-u", alpp, &add_me)) {
         if (add_me) {
            /* add my user name to the user list */
            lAddElemStr(user_list, ST_name, username, ST_Type);
            add_me = 0;
         }
         continue;
      }


      while (parse_multi_stringlist(ppcmdline, "-U", alpp, user_list, ST_Type, ST_name)) {
         continue;
      }

      while (parse_multi_stringlist(ppcmdline, "-sfa", alpp, sfa_list, ST_Type, ST_name)) {
         continue;
      }

      while (parse_multi_stringlist(ppcmdline, "-sfw", alpp, sfw_list, ST_Type, ST_name)) {
         continue;
      }
   }

   if (lGetNumberOfElem(*ppcmdline) && !usageshowed) {
      showq_usage(stderr);
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_TOOMANYOPTIONS);
      ret = false;
   }
   DRETURN(ret);
}


/*
 * * showq_show_job * displays information about a given job * to be extended *
 * 
 * returns 0 on success, non-zero on failure
 */
static int showq_show_job_tacc(lList * user_list, int full, const bool binding,
                               lList * sfa_list, lList * sfw_list)
{
   const lListElem      *j_elem = 0;
   lList          *jlp = nullptr;
   lList          *ilp = nullptr;
   lCondition     *where = nullptr, *newcp = nullptr;
   lEnumeration   *what = nullptr;
   lList          *alp = nullptr;
   int            total_slot_count;
   int            active_slot_count;
   int            active_job_count;
   int            total_job_count;
   int            unsched_job_count;
   int            dep_waiting_job_count;
   int            waiting_job_count;
   lList          *active_dj_list = nullptr;
   lList          *waiting_dj_list = nullptr;
   lList          *dep_waiting_dj_list = nullptr;
   lList          *unsched_dj_list = nullptr;

   DENTER(TOP_LAYER);

   /* if the user_list is empty, display all users' jobs */
   if (lGetNumberOfElem(user_list) != 0) {
      for_each_ep(j_elem, user_list) {
         newcp = lWhere("%T(%I p= %s)", JB_Type, JB_owner, lGetString(j_elem, ST_name));
         if (newcp != nullptr) {
            if (where == nullptr) {
               where = newcp;
            } else {
               where = lOrWhere(where, newcp);
            } 
         }
      }
   }

   /* get job data */
   what = lWhat("%T(ALL)", JB_Type);
   alp = sge_gdi2(SGE_JB_LIST, SGE_GDI_GET, &jlp, where, what);
   if (alp != nullptr) {
      answer_list_output(&alp);
   }
   lFreeWhere(&where);
   lFreeWhat(&what);

   extract_dj_lists(jlp, &active_dj_list, &waiting_dj_list, &dep_waiting_dj_list, &unsched_dj_list);

   active_job_count = lGetNumberOfElem(active_dj_list);

   active_slot_count = sum_slots(active_dj_list);
   waiting_job_count = lGetNumberOfElem(waiting_dj_list);
   dep_waiting_job_count = lGetNumberOfElem(dep_waiting_dj_list);
   unsched_job_count = lGetNumberOfElem(unsched_dj_list);

   /* apply active job sort */
   sort_dj_list(active_dj_list, sfa_list, false);

   /* apply waiting job sort */
   sort_dj_list(waiting_dj_list, sfw_list, true);
   sort_dj_list(dep_waiting_dj_list, sfw_list, true);
   sort_dj_list(unsched_dj_list, sfw_list, true);


   printf("ACTIVE JOBS--------------------------\n");
   if (full) {
      if (binding == false) {
         printf("JOBID     JOBNAME    USERNAME      STATE   CORE  HOST  QUEUE        REMAINING  STARTTIME\n");
         printf("==================================================================================================\n");
      } else {
         printf("JOBID     JOBNAME    USERNAME      STATE   CORE  HOST  QUEUE        REMAINING  STARTTIME           CORE_BINDING\n");
         printf("===============================================================================================================\n");
      }
   } else {
      if (binding == false) {
         printf("JOBID     JOBNAME    USERNAME      STATE   CORE  REMAINING  STARTTIME\n");
         printf("================================================================================\n");
      } else {
         printf("JOBID     JOBNAME    USERNAME      STATE   CORE  REMAINING  STARTTIME           CORE_BINDING\n");
         printf("============================================================================================\n");
      }
   }

   /* print running jobs */
   show_active_jobs(active_dj_list, full, binding);

   printf("\n");
   total_slot_count = 82 * 4 * 12 * 16;
   printf("%6d active jobs : %4d of %4d hosts (%6.2f %%)\n", active_job_count, (int) ceil(active_slot_count / 16.0), (int) ceil(total_slot_count / 16.0),
          100 * active_slot_count / (float) total_slot_count);
   printf("\n");

   printf("WAITING JOBS------------------------\n");
   if (full) {
      printf("JOBID     JOBNAME    USERNAME      STATE   CORE  HOST  QUEUE        WCLIMIT    QUEUETIME\n");
      printf("==================================================================================================\n");
   } else {
      printf("JOBID     JOBNAME    USERNAME      STATE   CORE  WCLIMIT    QUEUETIME\n");
      printf("================================================================================\n");
   }

   show_waiting_jobs(waiting_dj_list, full);
   printf("\n");

   printf("WAITING JOBS WITH JOB DEPENDENCIES---\n");
   if (full) {
      printf("JOBID     JOBNAME    USERNAME      STATE   CORE  HOST  QUEUE        WCLIMIT    QUEUETIME\n");
      printf("==================================================================================================\n");
   } else {
      printf("JOBID     JOBNAME    USERNAME      STATE   CORE  WCLIMIT    QUEUETIME\n");
      printf("================================================================================\n");
   }

   show_waiting_jobs(dep_waiting_dj_list, full);
   printf("\n");
   printf("UNSCHEDULED JOBS---------------------\n");
   if (full) {
      printf("JOBID     JOBNAME    USERNAME      STATE   CORE  HOST  QUEUE        WCLIMIT    QUEUETIME\n");
      printf("==================================================================================================\n");
   } else {
      printf("JOBID     JOBNAME    USERNAME      STATE   CORE  WCLIMIT    QUEUETIME\n");
      printf("================================================================================\n");
   }
   /* print unscheduled jobs */

   show_waiting_jobs(unsched_dj_list, full);
   printf("\n");

   total_job_count = active_job_count + waiting_job_count + dep_waiting_job_count + unsched_job_count;
   printf("Total jobs: %-5d Active Jobs: %-5d Waiting Jobs: %-5d Dep/Unsched Jobs: %-5d\n",
          total_job_count, active_job_count, waiting_job_count, dep_waiting_job_count + unsched_job_count);

   lFreeList(&active_dj_list);
   lFreeList(&waiting_dj_list);
   lFreeList(&dep_waiting_dj_list);
   lFreeList(&unsched_dj_list);

   lFreeList(&ilp);
   lFreeList(&jlp);
   DRETURN(0);
}
