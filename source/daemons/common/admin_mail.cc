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
 * @brief Mailing the administrator about job failures, without flooding them
 */
#include <cstring>
#include <cerrno>
#include <map>
#include <string>
#include <utility>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

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

/*
 * How often an administrator mail is sent for an error state.
 *
 * The states are listed by name and looked up, not placed at an index: an entry which is
 * missing or listed twice would otherwise shift every entry below it onto the wrong state,
 * which is what CS-1697 was about. A state which is not listed here is mailed every time,
 * which is BIT_ADM_ALWAYS.
 *
 * BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE means "once until something changes": once per queue
 * instance, and again as soon as the queue it happened in changes or the cluster
 * configuration does. The states which carry it are the ones an administrator configures -
 * a wrong path or a script which exits non-zero produces the same mail for every job which
 * touches the queue, and one of them is enough until the configuration is touched.
 */
static const struct {
   int state;
   int bits;
} admail_state_bits[] = {
        {SSTATE_BEFORE_PROLOG,       BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE},
        {SSTATE_PROLOG_FAILED,       BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE},
        {SSTATE_BEFORE_EPILOG,       BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE},
        {SSTATE_EPILOG_FAILED,       BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE},
        {SSTATE_BEFORE_PESTART,      BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE},
        {SSTATE_PESTART_FAILED,      BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE},
        {SSTATE_BEFORE_PESTOP,       BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE},
        {SSTATE_PESTOP_FAILED,       BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE},
        {SSTATE_CHECK_DAEMON_CONFIG, BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE},

        // a job the execd hands back or hands on, not a failure to report
        {SSTATE_MIGRATE,             BIT_ADM_NEVER},
        {SSTATE_AGAIN,               BIT_ADM_NEVER},
};

/** @brief How often an administrator mail is sent for an error state
 *
 * @param state one of the states of execution_states.h
 * @return the BIT_ADM_* bits of that state, BIT_ADM_ALWAYS for a state not listed
 */
static int
admail_get_bits(int state) {
   for (const auto &entry : admail_state_bits) {
      if (entry.state == state) {
         return entry.bits;
      }
   }
   return BIT_ADM_ALWAYS;
}

/*
 * What has been mailed already, by error state and queue instance.
 *
 * Keeping the queue instance apart matters: the same misconfiguration in two queues is two
 * things to report, and a change to one of them says nothing about the other.
 *
 * The queue version is what tells a change. The qmaster increases it for a queue instance
 * whenever its configuration or its state changes, and also when the exec host, a complex
 * or a userset it refers to changes; the scheduler stores it in the granted destination
 * list of the job, so the execd has it at hand without asking for it. A change of the
 * cluster configuration does not touch it and is handled by adm_mail_reset() instead.
 */
typedef std::pair<int, std::string> admail_key_t;

struct admail_sent_t {
   uint64_t when;        ///< when the mail was sent
   uint32_t qversion;    ///< the version the queue instance had at that moment
};

static std::map<admail_key_t, admail_sent_t> admail_sent;

/** @brief Report a job failure to the administrator, subject to the rate rules
 *
 * The mail carries the job's own trace and error files, because by the time
 * the administrator reads it the spool directory may already be gone.
 *
 * @param progid which daemon is reporting
 * @param jr the job report carrying the failure
 * @param is_array whether the job is an array job
 * @param job_owner the submitting user
 *
 * @note This reports on errors that have nothing to do with a particular job
 *       as well; it might not be apt for those.
 */
void job_related_adminmail(uint32_t progid, lListElem *jr, int is_array, const char *job_owner,
                           uint32_t qversion) {
   DENTER(TOP_LAYER);

   char sge_mail_subj[1024];
   char sge_mail_body[20480];
   char sge_mail_start[128];
   char sge_mail_end[128];
   char str_general[MAX_STRING_SIZE] = "";
   uint32_t jobid, jataskid, failed, general;
   const char *q;
   const lListElem *ep;
   lList *lp_mail = nullptr;
   uint64_t now;
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
   char *shepherd_file_buf = nullptr;

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   char* administrator_mail = mconf_get_administrator_mail();
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
      sge_strlcpy(sge_mail_start, sge_ctime64(lGetDouble(ep, UA_value), &ds), sizeof(sge_mail_start));
   } else {
      sge_strlcpy(sge_mail_start, MSG_MAIL_UNKNOWN_NAME, sizeof(sge_mail_start));
   }
   if ((ep=lGetSubStr(jr, UA_name, "end_time", JR_usage))) {
      sge_strlcpy(sge_mail_end, sge_ctime64(lGetDouble(ep, UA_value), &ds), sizeof(sge_mail_end));
   } else {
      sge_strlcpy(sge_mail_end, MSG_MAIL_UNKNOWN_NAME, sizeof(sge_mail_end));
   }

   jobid = lGetUlong(jr, JR_job_number);
   jataskid = lGetUlong(jr, JR_ja_task_number);

   failed = lGetUlong(jr, JR_failed);
   general = lGetUlong(jr, JR_general_failure);
   now = sge_get_gmt64();
   
   if (failed) {
      const char *err_str;
      size_t max_shepherd_files_size = 0;

      // a state might have more than one bit set
      int admail_bits = admail_get_bits(failed);
      admail_key_t admail_key{static_cast<int>(failed), q};

      if ((admail_bits & BIT_ADM_NEVER) != 0) {
         DPRINTF("NEVER SENDING ADMIN MAIL for state %d\n", failed);
         sge_free(&administrator_mail);
         DRETURN_VOID;
      }
      if ((admail_bits & (BIT_ADM_NEW_CONF | BIT_ADM_QCHANGE)) != 0) {
         auto it = admail_sent.find(admail_key);
         if (it != admail_sent.end() && it->second.qversion == qversion) {
            DPRINTF("NOT SENDING ADMIN MAIL AGAIN for state %d in queue " SFN
                    ": nothing changed since the last one\n", failed, q);
            sge_free(&administrator_mail);
            DRETURN_VOID;
         }
      }
      if ((admail_bits & BIT_ADM_HOUR) != 0) {
         auto it = admail_sent.find(admail_key);
         if (it != admail_sent.end() && now - it->second.when < sge_gmt32_to_gmt64(3600)) {
            DPRINTF("NOT SENDING ADMIN MAIL AGAIN for state %d in queue " SFN
                    ": less than an hour since the last one\n", failed, q);
            sge_free(&administrator_mail);
            DRETURN_VOID;
         }
      }
      admail_sent[admail_key] = {now, qversion};
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
                snprintf(str_general, sizeof(str_general), MSG_GFSTATE_JOB_UU, jobid, jataskid);
             } else {
                snprintf(str_general, sizeof(str_general), MSG_GFSTATE_JOB_U, jobid);
             }
          } else {
             sge_strlcpy(str_general, MSG_NONE, sizeof(str_general));
          }
      } else {
          /* This is a pe task */
          snprintf(str_general, sizeof(str_general), MSG_GFSTATE_PEJOB_U, jobid);
      }

      // find the mail tag to be used
      const char *mail_tag_str = mconf_get_mail_tag();
      std::string mail_tag;
      if (mail_tag_str == nullptr || strcmp(mail_tag_str, "none") == 0) {
         mail_tag = "";
      } else {
         mail_tag = mail_tag_str;
      }
      sge_free(&mail_tag_str);

      if (is_array) {
         snprintf(sge_mail_subj, sizeof(sge_mail_subj), MSG_MAIL_SUBJECT_SSUU, mail_tag.c_str(),
                  feature_get_product_name(FS_SHORT_VERSION, &ds), jobid, jataskid);
      } else {
         snprintf(sge_mail_subj, sizeof(sge_mail_subj), MSG_MAIL_SUBJECT_SSU, mail_tag.c_str(),
                  feature_get_product_name(FS_SHORT_VERSION, &ds), jobid);
      }

      snprintf(sge_mail_body, sizeof(sge_mail_body),
              MSG_MAIL_BODY_USSSSSSS,
              jobid,
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
         snprintf(shepherd_files[i].filepath, SGE_PATH_MAX, "%s/" sge_u32 "." sge_u32 "/%s", ACTIVE_DIR, jobid, jataskid, shepherd_filenames[i]);
         if (!SGE_STAT(shepherd_files[i].filepath, &shepherd_files[i].statbuf) && (shepherd_files[i].statbuf.st_size > 0)) {
            shepherd_files[i].exists = 1;
            max_shepherd_files_size = std::max(max_shepherd_files_size, static_cast<size_t>(shepherd_files[i].statbuf.st_size));
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
   DPRINTF(MSG_FILE_ERRORCLOSEINGXY_SS, shepherd_files[i].filepath, strerror(errno));
   sge_free(&administrator_mail);
   sge_free(&shepherd_file_buf);
   sge_dstring_free(&dstr_sge_mail_body_total);

   DRETURN_VOID;
}

/** @brief Forget that a state was already mailed about, so the next one is sent
 * @param state the failure state, or 0 to reset every state
 * @return 0
 */
int adm_mail_reset(int state) {
   DENTER(TOP_LAYER);

   /*
   ** let 0 be a reset all
   */
   if (state == 0) {
      admail_sent.clear();
      DRETURN(0);
   }

   DPRINTF("resetting admin mail for the states with bit %d\n", state);
   for (auto it = admail_sent.begin(); it != admail_sent.end(); ) {
      if ((admail_get_bits(it->first.first) & state) != 0) {
         it = admail_sent.erase(it);
      } else {
         ++it;
      }
   }

   DRETURN(0);
}
