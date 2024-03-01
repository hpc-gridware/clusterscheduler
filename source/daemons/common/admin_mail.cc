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
#include <cstring>
#include <cerrno>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_usage.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_report.h"

#include "admin_mail.h"
#include "execution_states.h"
#include "mail.h"
#include "msg_common.h"
#include "msg_daemons_common.h"
#include "uti/sge.h"

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
/* 29 SSTATE_AFS_PROBLEM         */   0,
/* 30 SSTATE_APPERROR            */   0,
/* 31 SSTATE_PASSWD_FILE_ERROR   */   0,
/* 32 SSTATE_PASSWD_MISSING      */   0,
/* 33 SSTATE_PASSWD_WRONG        */   0,
/* 34 SSTATE_HELPER_SERVICE_ERROR */  0,
/* 35 SSTATE_HELPER_SERVICE_BEFORE_JOB */ 0,
/* 36 SSTATE_CHECK_DAEMON_CONFIG */   0 };

u_long32 admail_times[MAX_SSTATE + 1];

/*
** this functions reports job failures to the admin
** it might not be apt to report on errors that
** have nothing to do with a particular job
*/
void job_related_adminmail(u_long32 progid, lListElem *jr, int is_array, const char *job_owner)
{
   static int first = 1;
   char sge_mail_subj[1024];
   char sge_mail_body[2048];
   char sge_mail_start[128];
   char sge_mail_end[128];
   char str_general[MAX_STRING_SIZE] = "";
   u_long32 jobid, jataskid, failed, general;
   const char *q;
   lListElem *ep;
   lList *lp_mail = nullptr;
   u_long32 now;
   int ret;
   const char *shepherd_filenames[] = { "trace", "error", "pe_hostfile" };
   int num_files = 3;
   struct {
      int exists;
      SGE_STRUCT_STAT statbuf;
      char filepath[SGE_PATH_MAX];
   } shepherd_files[3];
   int i;
   dstring dstr_sge_mail_body_total = DSTRING_INIT;
   dstring ds;
   char buffer[256];
   char* administrator_mail = nullptr;
   char *shepherd_file_buf = nullptr;

   DENTER(TOP_LAYER);

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   DPRINTF(("sizeof(admail_times) : %d\n", sizeof(admail_times)));
   if (first) {
      memset(admail_times, 0, sizeof(admail_times));
      first = 0;
   }

   administrator_mail = mconf_get_administrator_mail();

   if (administrator_mail == nullptr) {
      DRETURN_VOID;
   }

   if (!strcasecmp(administrator_mail, "none")) {
      sge_free(&administrator_mail);
      DRETURN_VOID;
   }

   if (!(q=lGetString(jr, JR_queue_name))) {
      q = MSG_MAIL_UNKNOWN_NAME;
   }
   if ((ep=lGetSubStr(jr, UA_name, "start_time", JR_usage))) {
      sge_strlcpy(sge_mail_start, sge_ctime((time_t)lGetDouble(ep, UA_value), &ds), sizeof(sge_mail_start));
   } else {
      sge_strlcpy(sge_mail_start, MSG_MAIL_UNKNOWN_NAME, sizeof(sge_mail_start));
   }
   if ((ep=lGetSubStr(jr, UA_name, "end_time", JR_usage))) {
      sge_strlcpy(sge_mail_end, sge_ctime((time_t)lGetDouble(ep, UA_value), &ds), sizeof(sge_mail_end));
   } else {
      sge_strlcpy(sge_mail_end, MSG_MAIL_UNKNOWN_NAME, sizeof(sge_mail_end));
   }

   jobid = lGetUlong(jr, JR_job_number);
   jataskid = lGetUlong(jr, JR_ja_task_number);

   failed = lGetUlong(jr, JR_failed);
   general = lGetUlong(jr, JR_general_failure);
   now = sge_get_gmt();
   
   if (failed) {
      const char *err_str;
      size_t max_shepherd_files_size = 0;

      if (failed <= MAX_SSTATE) {
         /*
         ** a state might have more than one bit set
         */
         if ((admail_states[failed] & BIT_ADM_NEVER)) {
            DPRINTF(("NEVER SENDING ADMIN MAIL for state %d\n", failed));
            sge_free(&administrator_mail);
            DRETURN_VOID;
         }
         if ((admail_states[failed] & BIT_ADM_NEW_CONF)) {
            if (admail_times[failed]) {
               DPRINTF(("NOT SENDING ADMIN MAIL AGAIN for state %d, again on conf\n", failed));
               sge_free(&administrator_mail);
               DRETURN_VOID;
            }
         }
         if ((admail_states[failed] & BIT_ADM_QCHANGE)) {
            if (admail_times[failed]) {
               DPRINTF(("NOT SENDING ADMIN MAIL AGAIN for state %d, again on qchange\n", failed));
               sge_free(&administrator_mail);
               DRETURN_VOID;
            }
         }
         if ((admail_states[failed] & BIT_ADM_HOUR)) {
            if ((now - admail_times[failed] < 3600))
               DPRINTF(("NOT SENDING ADMIN MAIL AGAIN for state %d, again next hour\n", failed));
               sge_free(&administrator_mail);
               DRETURN_VOID;
         }
         admail_times[failed] = now;
      }
      if (!(err_str=lGetString(jr, JR_err_str))) {
         err_str = MSG_MAIL_UNKNOWN_REASON;
      }

      ret = mailrec_parse(&lp_mail, administrator_mail);
      if (ret) {
         ERROR(MSG_MAIL_PARSE_S, (administrator_mail ? administrator_mail : MSG_NULL));
         sge_free(&administrator_mail);
         DRETURN_VOID;
      }

      if (lGetString(jr, JR_pe_task_id_str) == nullptr) {
          /* This is a regular job */
          if (general == GFSTATE_QUEUE) {
             snprintf(str_general, sizeof(str_general), MSG_GFSTATE_QUEUE_S, q);
          } else if (general == GFSTATE_HOST) {
             const char *s = strchr(q, '@');
             if (s != nullptr) {
               s++;
               snprintf(str_general, sizeof(str_general), MSG_GFSTATE_HOST_S, s);
             } else {
               snprintf(str_general, sizeof(str_general), MSG_GFSTATE_HOST_S, MSG_MAIL_UNKNOWN_NAME);
             }
          } else if (general == GFSTATE_JOB) {
             if (is_array) {
                snprintf(str_general, sizeof(str_general), MSG_GFSTATE_JOB_UU, sge_u32c(jobid), sge_u32c(jataskid));
             } else {
                snprintf(str_general, sizeof(str_general), MSG_GFSTATE_JOB_U, sge_u32c(jobid));
             }
          } else {
             sge_strlcpy(str_general, MSG_NONE, sizeof(str_general));
          }
      } else {
          /* This is a pe task */
          snprintf(str_general, sizeof(str_general), MSG_GFSTATE_PEJOB_U, sge_u32c(jobid));
      }

      if (is_array) {
         snprintf(sge_mail_subj, sizeof(sge_mail_subj), MSG_MAIL_SUBJECT_SUU, 
                 feature_get_product_name(FS_SHORT_VERSION, &ds), sge_u32c(jobid), sge_u32c(jataskid));
      } else {
         snprintf(sge_mail_subj, sizeof(sge_mail_subj), MSG_MAIL_SUBJECT_SU, 
                 feature_get_product_name(FS_SHORT_VERSION, &ds), sge_u32c(jobid));
      }

      snprintf(sge_mail_body, sizeof(sge_mail_body),
              MSG_MAIL_BODY_USSSSSSS,
              sge_u32c(jobid),
              str_general,
              job_owner, q, sge_mail_start, sge_mail_end,
              get_sstate_description(failed),
              err_str);
      /*
      ** attach the trace and error file to admin mail if it is present
      */
      for (i=0; i<num_files; i++) {
         shepherd_files[i].exists = 0;
      }
      for (i=0; i<num_files; i++) {
         /* JG: TODO (254): use function creating path */
         snprintf(shepherd_files[i].filepath, SGE_PATH_MAX, "%s/" sge_u32"." sge_u32"/%s", ACTIVE_DIR,
                     jobid, jataskid, shepherd_filenames[i]);
         if (!SGE_STAT(shepherd_files[i].filepath, &shepherd_files[i].statbuf) 
             && (shepherd_files[i].statbuf.st_size > 0)) {
            shepherd_files[i].exists = 1;
            max_shepherd_files_size = MAX(max_shepherd_files_size, shepherd_files[i].statbuf.st_size);
         }
      }

      sge_dstring_copy_string(&dstr_sge_mail_body_total, sge_mail_body);
     
      if (max_shepherd_files_size > 0) {
         shepherd_file_buf = sge_malloc(max_shepherd_files_size + 1);
         for (i=0; i<num_files; i++) {
            if (shepherd_files[i].exists) {
               FILE *fp;
               sge_dstring_sprintf_append(&dstr_sge_mail_body_total, "\nShepherd %s:\n", shepherd_filenames[i]);
               if ((fp = fopen(shepherd_files[i].filepath, "r"))) {
                  size_t n;

                  n = fread(shepherd_file_buf, 1, max_shepherd_files_size, fp);
                  FCLOSE(fp);
                  shepherd_file_buf[n] = '\0';
                  sge_dstring_append(&dstr_sge_mail_body_total, shepherd_file_buf);
               }
            }
         }
         sge_free(&shepherd_file_buf);
      }

      cull_mail(progid, lp_mail, sge_mail_subj, sge_dstring_get_string(&dstr_sge_mail_body_total),
                MSG_MAIL_TYPE_ADMIN);

      sge_dstring_free(&dstr_sge_mail_body_total);
   }
   lFreeList(&lp_mail);
   sge_free(&administrator_mail);
   DRETURN_VOID;
FCLOSE_ERROR:
   DPRINTF((MSG_FILE_ERRORCLOSEINGXY_SS, shepherd_files[i].filepath, strerror(errno)));
   sge_free(&administrator_mail);
   sge_free(&shepherd_file_buf);
   sge_dstring_free(&dstr_sge_mail_body_total);

   DRETURN_VOID;
}

int adm_mail_reset(int state)
{
   int i;

   DENTER(TOP_LAYER);

   /*
   ** let 0 be a reset all
   */
   if (state == 0) {
      memset(admail_times, 0, sizeof(admail_times));
      DRETURN(0);
   }

   DPRINTF(("resetting admin mail for state %d\n", state));
   for (i = 0; i < MAX_SSTATE + 1; i++) {
      if ((admail_states[i] & state)) {
         admail_times[i] = 0;
      }
   }
   
   DRETURN(0);
}
