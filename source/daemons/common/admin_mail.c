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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <stdlib.h>
#include <string.h>

#include "sge_unistd.h"
#include "sge.h"
#include "sge_conf.h"
#include "sge_usageL.h"
#include "sge_time.h"
#include "execution_states.h"
#include "sge_mailrec.h"
#include "admin_mail.h"
#include "mail.h"
#include "sgermon.h"
#include "sge_log.h"
#include "msg_common.h"
#include "msg_daemons_common.h"
#include "sge_feature.h"
#include "sge_report.h"

int admail_states[MAX_SSTATE + 1] = {
                                      0,
                                      0,
/* 2  SSTATE_FAILURE_BEFORE_JOB  */   0,
/* 3  ESSTATE_NO_SHEPHERD        */   0,
/* 4  ESSTATE_NO_CONFIG          */   0,
/* 5  ESSTATE_NO_PID             */   0,
/* 6  SSTATE_READ_CONFIG         */   0,
/* 7  SSTATE_BEFORE_PROLOG       */   BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE,
/* 8  SSTATE_PROLOG_FAILED       */   0,
/* 9  SSTATE_BEFORE_PESTART      */   0,
/* 10 SSTATE_PESTART_FAILED      */   0,
/* 11 SSTATE_BEFORE_JOB          */   0,
/* 12 SSTATE_BEFORE_PESTOP       */   0,
/* 13 SSTATE_PESTOP_FAILED       */   0,
/* 14 SSTATE_BEFORE_EPILOG       */   BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE,
/* 15 SSTATE_EPILOG_FAILED       */   0,
/* 16 SSTATE_EPILOG_FAILED       */   0,
/* 17 ESSTATE_DIED_THRU_SIGNAL   */   0,
/* 18 ESSTATE_SHEPHERD_EXIT      */   0,
/* 19 ESSTATE_NO_EXITSTATUS      */   0,
/* 20 ESSTATE_UNEXP_ERRORFILE    */   0,
/* 21 ESSTATE_UNKNOWN_JOB        */   0,
/* 22 ESSTATE_EXECD_LOST_RUNNING */   0,
/* 23 ESSTATE_PTF_CANT_GET_PIDS  */   0,
/* 24 SSTATE_MIGRATE             */   BIT_ADM_NEVER,
/* 25 SSTATE_AGAIN               */   BIT_ADM_NEVER,
/* 26 SSTATE_OPEN_OUTPUT         */   0,
/* 27 SSTATE_NO_SHELL            */   0,
/* 28 SSTATE_NO_CWD              */   0,
/* 29 SSTATE_AFS_PROBLEM         */   0 };

u_long32 admail_times[MAX_SSTATE + 1];

/*
** this functions reports job failures to the admin
** it might not be apt to report on errors that
** have nothing to do with a particular job
*/
void job_related_adminmail(
lListElem *jr,
int is_array 
) {
   static int first = 1;
   char sge_mail_subj[1024];
   char sge_mail_body[2048];
   char sge_mail_start[128];
   char sge_mail_end[128];
   char str_general[512] = "";
   u_long32 jobid, jataskid, failed, general;
   const char *q, *h;
   lListElem *ep;
   lList *lp_mail = NULL;
   u_long32 now;
   int ret;
   char *shepherd_filenames[] = { "trace", "error", "pe_hostfile" };
   int num_files = 3;
   struct {
      int exists;
      SGE_STRUCT_STAT statbuf;
      char filepath[SGE_PATH_MAX];
   } shepherd_files[3];
   int i;
   char *sge_mail_body_total = NULL;
   int sge_mail_body_total_size = 0;
   FILE *fp;
   int start = 0;
   const char *job_owner;   

   DENTER(TOP_LAYER, "job_related_adminmail");

   DPRINTF(("sizeof(admail_times) : %d\n", sizeof(admail_times)));
   if (first) {
      memset(admail_times, sizeof(admail_times), 0);
      first = 0;
   }

   if (!conf.administrator_mail) {
      return;
   }

   if (!strcasecmp(conf.administrator_mail, "none")) {
      return;
   }

   if (!(q=lGetString(jr, JR_queue_name)))
      q = MSG_MAIL_UNKNOWN_NAME;
   if (!(h=lGetHost(jr, JR_host_name)))
      h = MSG_MAIL_UNKNOWN_NAME;
   if ((ep=lGetSubStr(jr, UA_name, "start_time", JR_usage)))
      strcpy(sge_mail_start, sge_ctime((u_long32)lGetDouble(ep, UA_value)));
   else   
      strcpy(sge_mail_start, MSG_MAIL_UNKNOWN_NAME);
   if ((ep=lGetSubStr(jr, UA_name, "end_time", JR_usage)))
      strcpy(sge_mail_end, sge_ctime((u_long32)lGetDouble(ep, UA_value)));
   else   
      strcpy(sge_mail_end, MSG_MAIL_UNKNOWN_NAME);

   jobid = lGetUlong(jr, JR_job_number);
   jataskid = lGetUlong(jr, JR_ja_task_number);
   if (!( job_owner = lGetString(jr, JR_owner)))
      job_owner = "MSG_MAIL_UNKNOWN_NAME";

   failed = lGetUlong(jr, JR_failed);
   general = lGetUlong(jr, JR_general_failure);
   now = sge_get_gmt();
   
   if (failed) {
      const char *err_str;

      if (failed <= MAX_SSTATE) {
         /*
         ** a state might have more than one bit set
         */
         if ((admail_states[failed] & BIT_ADM_NEVER)) {
            DPRINTF(("NEVER SENDING ADMIN MAIL for state %d\n", failed));
            DEXIT;
            return;
         }
         if ((admail_states[failed] & BIT_ADM_NEW_CONF)) {
            if (admail_times[failed]) {
               DPRINTF(("NOT SENDING ADMIN MAIL AGAIN for state %d, again on conf\n", failed));
               DEXIT;
               return;
            }
         }
         if ((admail_states[failed] & BIT_ADM_QCHANGE)) {
            if (admail_times[failed]) {
               DPRINTF(("NOT SENDING ADMIN MAIL AGAIN for state %d, again on qchange\n", failed));
               DEXIT;
               return;
            }
         }
         if ((admail_states[failed] & BIT_ADM_HOUR)) {
            if ((now - admail_times[failed] < 3600))
               DPRINTF(("NOT SENDING ADMIN MAIL AGAIN for state %d, again next hour\n", failed));
               DEXIT;
               return;
         }
         admail_times[failed] = now;
      }
      if (!(err_str=lGetString(jr, JR_err_str)))
         err_str = MSG_MAIL_UNKNOWN_REASON;

      ret = mailrec_parse(&lp_mail, conf.administrator_mail);
      if (ret) {
         ERROR((SGE_EVENT, MSG_MAIL_PARSE_S,
            (conf.administrator_mail ? conf.administrator_mail : MSG_NULL)));
         DEXIT;
         return;
      }

      if (general == GFSTATE_QUEUE) {
         sprintf(str_general, MSG_GFSTATE_QUEUE_S, q);
      }
      else if (general == GFSTATE_HOST) {
         sprintf(str_general, MSG_GFSTATE_HOST_S, h);
      }
      else if (general == GFSTATE_JOB) {
         if (is_array)
            sprintf(str_general, MSG_GFSTATE_JOB_UU, u32c(jobid), u32c(jataskid));
         else
            sprintf(str_general, MSG_GFSTATE_JOB_U, u32c(jobid));
      }
      else {
         sprintf(str_general, MSG_NONE);
      }
      if (is_array)
         sprintf(sge_mail_subj, MSG_MAIL_SUBJECT_SUU, 
                 feature_get_product_name(FS_SHORT_VERSION), u32c(jobid), u32c(jataskid));
      else
         sprintf(sge_mail_subj, MSG_MAIL_SUBJECT_SU, 
                 feature_get_product_name(FS_SHORT_VERSION), u32c(jobid));
      sprintf(sge_mail_body,
              MSG_MAIL_BODY_USSSSSSSS,
              u32c(jobid),
              str_general,
              job_owner, q, h, sge_mail_start, sge_mail_end,
              get_sstate_description(failed),
              err_str);
      /*
      ** attach the trace and error file to admin mail if it is present
      */
      sge_mail_body_total_size = strlen(sge_mail_body) + 1000;

      for (i=0; i<num_files; i++) {
         shepherd_files[i].exists = 0;
      }
      for (i=0; i<num_files; i++) {
         /* JG: TODO (254): use function creating path */
         sprintf(shepherd_files[i].filepath, "%s/" u32"."u32"/%s", ACTIVE_DIR, 
                     jobid, jataskid, shepherd_filenames[i]);
         if (!SGE_STAT(shepherd_files[i].filepath, &shepherd_files[i].statbuf) 
             && (shepherd_files[i].statbuf.st_size > 0)) {
            sge_mail_body_total_size += shepherd_files[i].statbuf.st_size;
            shepherd_files[i].exists = 1;
         }
      }
      /*
      ** allocate enough space for trace and error file
      */
      sge_mail_body_total = (char*) malloc(sizeof(char) * 
                                           sge_mail_body_total_size); 
      
      strcpy(sge_mail_body_total, sge_mail_body);

      
      for (i=0; i<num_files; i++) {
         if (shepherd_files[i].exists) {
            sprintf(sge_mail_body_total, "%s\nShepherd %s:\n", 
                      sge_mail_body_total, shepherd_filenames[i]);
            start = strlen(sge_mail_body_total);
            if ((fp = fopen(shepherd_files[i].filepath, "r"))) {
               int n;

               n=fread(sge_mail_body_total+start, 1, 
                        sge_mail_body_total_size - start, fp);
               fclose(fp);
               sge_mail_body_total[start + n] = '\0';
            }
         }
      }

      cull_mail(lp_mail, sge_mail_subj, sge_mail_body_total, 
                MSG_MAIL_TYPE_ADMIN);

      if (sge_mail_body_total)
         free((char*)sge_mail_body_total);
   }
   if (lp_mail)
      lFreeList(lp_mail);

   DEXIT;
   return;
}

int adm_mail_reset(
int state 
) {
   int i;

   DENTER(TOP_LAYER, "adm_mail_reset");

   /*
   ** let 0 be a reset all
   */
   if (!state) {
      memset(admail_times, sizeof(admail_times), 0);
      return 0;
   }

   DPRINTF(("resetting admin mail for state %d\n", state));
   for (i = 0; i < MAX_SSTATE + 1; i++) {
      if ((admail_states[i] & state)) {
         admail_times[i] = 0;
      }
   }
   
   DEXIT;
   return 0;
}
