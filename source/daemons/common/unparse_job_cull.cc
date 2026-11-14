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
 * @brief Turning a job back into the command line that would submit it again
 */
#include <cstring>

#include "uti/sge_bitfield.h"
#include "uti/sge_dstring.h"
#include "uti/sge_hostname.h"
#include "uti/sge_language.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/parse.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_answer.h" 
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_qref.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_VA.h"

#include "msg_daemons_common.h"
#include "parse_qsub.h"
#include "sge_options.h"
#include "symbols.h"
#include "unparse_job_cull.h"
#include "uti/sge.h"


static char *sge_unparse_checkpoint_attr(int opr, char *string);
/* static char *sge_unparse_hold_list(uint32_t hold); */
static char *sge_unparse_mail_options(uint32_t mail_opt);
static int sge_unparse_checkpoint_option(lListElem *job, lList **pcmdline, lList **alpp);
static int sge_unparse_account_string(lListElem *job, lList **pcmdline, lList **alpp);
static int sge_unparse_path_list(lListElem *job, int nm, const char *option, lList **pcmdline, lList **alpp);
static int sge_unparse_pe(lListElem *job, lList **pcmdline, lList **alpp);

static int sge_unparse_resource_list(lList *resource_list, bool hard, lList **pcmdline, lList **alpp);
static int sge_unparse_string_option(lListElem *job, int nm, const char *option, lList **pcmdline, lList **alpp);

/** @brief Turn a job back into the command line that would submit it again
 *
 * The inverse of the submit-time parsing, used by `qalter -w` and by the
 * `submit_cmd_line` row of `qstat -j`.
 *
 * @param[out] pcmdline receives the rendered options
 * @param job the job
 * @param flags #FLG_FULL_CMDLINE to render defaults as well
 * @return an answer list on error, nullptr on success
 */
lList *cull_unparse_job_parameter(lList **pcmdline, lListElem *job, int flags) {
   DENTER(TOP_LAYER);

   const char *cp;
   uint32_t ul;
   lList *answer = nullptr;
   char str[MAX_STRING_SIZE];
   const lList *lp;
   int ret;
   lListElem *ep_opt;
   const char *username = component_get_username();
   const char *qualified_hostname = component_get_qualified_hostname();

   /*
   ** -a
   ** problem with submission time, but that is not a good
   ** default option anyway, is not unparsed
   */

   /*
   ** -A
   */
   if (sge_unparse_account_string(job, pcmdline, &answer) != 0) {
      DRETURN(answer);
   }

   /*
   ** -c
   */
   if (sge_unparse_checkpoint_option(job, pcmdline, &answer) != 0) {
      DRETURN(answer);
   }
  
   /*
    * -ckpt 
    */
   if (sge_unparse_string_option(job, JB_checkpoint_name, "-ckpt", 
            pcmdline, &answer) != 0) {
      DRETURN(answer);
   }


   /*
   ** -cwd
   */
   if (ocs::tom::get<JB_cwd>(job)) {
      ep_opt = sge_add_noarg(pcmdline, cwd_OPT, "-cwd", nullptr);
   }

   /*
    * -P
    */
   if (sge_unparse_string_option(job, JB_project, "-P",
            pcmdline, &answer) != 0) {
      DRETURN(answer);
   }

#if 0
   /*
   ** -C
   */
   if (sge_unparse_string_option(job, JB_directive_prefix, "-C", 
            pcmdline, &answer) != 0) {
      DRETURN(answer);
   }
#endif   

   /*
   ** -e
   */
   if (sge_unparse_path_list(job, JB_stderr_path_list, "-e", pcmdline, 
                     &answer) != 0) {
      DRETURN(answer);
   }

   /*
   ** -h, here only user hold supported at the moment
   */
   if  ((ul = ocs::tom::get<JAT_hold>(lFirst(ocs::tom::get<JB_ja_tasks>(job))))) {
      ep_opt = sge_add_noarg(pcmdline, h_OPT, "-h", nullptr);
   }

   /*
   ** -hold_jid
   */
   if ((lp = ocs::tom::get<JB_jid_request_list>(job))) {
      int fields[] = { JRE_job_name, 0 };
      const char *delis[] = {nullptr, ",", nullptr};

      ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp, fields, delis, 0);
      if (ret) {
         DPRINTF("Error %d formatting jid_request_list as -hold_jid\n", ret);
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGJIDPREDECESSORLISTASHOLDJID);
         return answer;
      }
      ep_opt = sge_add_arg(pcmdline, hold_jid_OPT, lListT, "-hold_jid", str);
      ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList("hold_jid list", lp));      
   }

   /*
   ** -hold_jid_ad
   */
   if ((lp = ocs::tom::get<JB_ja_ad_request_list>(job))) {
      int fields[] = { JRE_job_name, 0 };
      const char *delis[] = {nullptr, ",", nullptr};

      ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp, fields, delis, 0);
      if (ret) {
         DPRINTF("Error %d formatting ja_ad_request_list as -hold_jid_ad\n", ret);
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGJIDPREDECESSORLISTASHOLDJIDAD);
         return answer;
      }
      ep_opt = sge_add_arg(pcmdline, hold_jid_ad_OPT, lListT, "-hold_jid_ad", str);
      ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList("hold_jid_ad list", lp));      
   }

   /*
   ** -i
   */
   if (sge_unparse_path_list(job, JB_stdin_path_list, "-i", pcmdline, 
                     &answer) != 0) {
      DRETURN(answer);
   }

   /*
   ** -j
   */
   if ((ul = ocs::tom::get<JB_merge_stderr>(job))) {
      ep_opt = sge_add_arg(pcmdline, j_OPT, lIntT, "-j", "y");
      ocs::tom::set<SPA_argval_lIntT>(ep_opt, true);
   }
   
   /*
   ** -jid
   */
   if ((lp = ocs::tom::get<JB_job_identifier_list>(job))) {
      int fields[] = { JRE_job_number, 0};
      const char *delis[] = {"", ",", nullptr};

      ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp, fields, delis,
         0);
      if (ret) {
         DPRINTF("Error %d formatting job_identifier_list as -jid\n", ret);
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGJOBIDENTIFIERLISTASJID);
         DRETURN(answer);
      }
      ep_opt = sge_add_arg(pcmdline, jid_OPT, lListT, "-jid", str);
      ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList("jid list", lp));      
   }

   /*
   ** -js
   */
   if ((ul = ocs::tom::get<JB_jobshare>(job)) != 0)  {
      snprintf(str, sizeof(str), sge_u32, ul);
      ep_opt = sge_add_arg(pcmdline, js_OPT, lUlongT, "-js", str);
      ocs::tom::set<SPA_argval_lUlongT>(ep_opt, ul);
   }

   /*
   ** -lj is in parsing but can't be unparsed here
   */

   /*
   ** -l
   */
   if (sge_unparse_resource_list(job_get_hard_resource_listRW(job), true, pcmdline, &answer) != 0) {
      DRETURN(answer);
   }
   if (sge_unparse_resource_list(job_get_soft_resource_listRW(job), false, pcmdline, &answer) != 0) {
      DRETURN(answer);
   }






   /*
   ** -m
   */
   if ((ul = ocs::tom::get<JB_mail_options>(job))) {
      cp = sge_unparse_mail_options(ul);
      if (!cp) {
         DPRINTF("Error unparsing mail options\n");
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_ERRORUNPARSINGMAILOPTIONS);
         return answer;
      }
      ep_opt = sge_add_arg(pcmdline, m_OPT, lIntT, "-m", cp);
      ocs::tom::set<SPA_argval_lIntT>(ep_opt, ul);
   }

   /*
   ** -M obviously a problem!!!
   ** not unparsed at the moment
   ** does it make sense as a default, after all?
   */
   if ((lp = ocs::tom::get<JB_mail_list>(job))) {
      lList *lp_new = nullptr;
      lListElem *ep_new = nullptr;
      const char *host;
      const char *user;

      /*
      ** or rather take all if there are more than one elements?
      */
      for_each_ep_lv(ep, lp) {
         user = ocs::tom::get<MR_user>(ep);
         host = ocs::tom::get<MR_host>(ep);
         if (sge_strnullcmp(user, username) || 
             sge_hostcmp(host, qualified_hostname)) {
            lp_new = lCreateList("mail list", MR_Type);
            ep_new = ocs::tom::emplace_back<MR_user>(&lp_new, user);
            ocs::tom::set<MR_host>(ep_new, host);
         }
      }
      if (lp_new) {
         int fields[] = { MR_user, MR_host, 0 };
         const char *delis[] = {"@", ",", nullptr};

         ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp_new, fields, delis, FLG_NO_DELIS_STRINGS);
         if (ret) {
            DPRINTF("Error %d formatting mail list as -M\n", ret);
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_LIST_ERRORFORMATTINGMAILLISTASM);
            return answer;
         }
         ep_opt = sge_add_arg(pcmdline, M_OPT, lListT, "-M", str);
         ocs::tom::set<SPA_argval_lListT>(ep_opt, lp_new);
 
      }
   }

   /*
   ** -N
   ** dont unparse the job name!
   ** each job has a jobname, but not each defaults file wants a -N
   */
#if 0
   if ((cp = lGetString(job, JB_job_name))) {
      ep_opt = sge_add_arg(pcmdline, N_OPT, lStringT, "-N", cp);
      lSetString(ep_opt, SPA_argval_lStringT, cp);
   }
#endif
   /*
   ** -notify
   */
   if  ((ul = ocs::tom::get<JB_notify>(job))) {
      ep_opt = sge_add_noarg(pcmdline, notify_OPT, "-notify", nullptr);
   }

   /*
   ** -o
   */
   if (sge_unparse_path_list(job, JB_stdout_path_list, "-o", pcmdline, 
                     &answer) != 0) {
      DRETURN(answer);
   }

   /*
   ** -p
   */
   if ((ul = ocs::tom::get<JB_priority>(job)) != BASE_PRIORITY)  {
      int prty;

      prty = ul - BASE_PRIORITY;
      if (prty > 1024) {
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PROC_INVALIDPROIRITYMUSTBELESSTHAN1025);
         DRETURN(answer);
      }
      if (prty < -1023) {
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_PROC_INVALIDPRIORITYMUSTBEGREATERTHANMINUS1024);
         DRETURN(answer);
      }
      snprintf(str, sizeof(str), "%d", prty);
      ep_opt = sge_add_arg(pcmdline, p_OPT, lIntT, "-p", str);
      ocs::tom::set<SPA_argval_lIntT>(ep_opt, prty);
   }

   /*
   ** -pe
   */
   if (sge_unparse_pe(job, pcmdline, &answer) != 0) {
      DRETURN(answer);
   }


#if 1
   /*
   ** -q
   ** exec-string is suppressed because uni_print_list can't do this
   ** is not in the manual anyway
   */
   if ((lp = job_get_hard_resource_list(job)) != nullptr) {
      int fields[] = { QR_name, 0 };
      const char *delis[] = {"@", ",", nullptr};

      sge_add_noarg(pcmdline, hard_OPT, "-hard", nullptr);
      ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp, fields, delis,
         FLG_NO_DELIS_STRINGS);
      if (ret) {
         DPRINTF("Error %d formatting hard_queue_list as -q\n", ret);
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGHARDQUEUELISTASQ);
         return answer;
      }
      ep_opt = sge_add_arg(pcmdline, q_OPT, lListT, "-q", str);
      ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList("hard queue list", lp));      
      ocs::tom::set<SPA_argval_lIntT>(ep_opt, 1); /* means hard */
   }
   if ((lp = job_get_soft_queue_list(job)) != nullptr) {
      int fields[] = { QR_name, 0 };
      const char *delis[] = {"@", ",", nullptr};

      sge_add_noarg(pcmdline, soft_OPT, "-soft", nullptr);
      ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp, fields, delis,
         FLG_NO_DELIS_STRINGS);
      if (ret) {
         DPRINTF("Error %d formatting soft_queue_list as -q\n", ret);
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGSOFTQUEUELISTASQ);
         return answer;
      }
      ep_opt = sge_add_arg(pcmdline, q_OPT, lListT, "-q", str);
      ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList("soft queue list", lp));      
      ocs::tom::set<SPA_argval_lIntT>(ep_opt, 2); /* means soft */
   }
#endif
   
   /*
   ** -R
   */
   if ((ul = ocs::tom::get<JB_reserve>(job))) {
      ep_opt = sge_add_arg(pcmdline, R_OPT, lIntT, "-R", "y");
      ocs::tom::set<SPA_argval_lIntT>(ep_opt, true);
   }

   /*
   ** -r
   */
   if ((ocs::tom::get<JB_restart>(job) == 1)) {
      ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, "-r", "y");
      ocs::tom::set<SPA_argval_lIntT>(ep_opt, 1);
   } else if ((ocs::tom::get<JB_restart>(job) == 2)) {
      ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, "-r", "n");
      ocs::tom::set<SPA_argval_lIntT>(ep_opt, 2);
   }

   /*
   ** -S
   */
#if 1   
   if (sge_unparse_path_list(job, JB_shell_list, "-S", pcmdline, 
                     &answer) != 0) {
      DRETURN(answer);
   }
#else
   if ((lp = lGetList(job, JB_shell_list))) {
      int fields[] = { PN_host, PN_file_host, PN_path, PN_file_staging, 0 };
      const char *delis[] = {":", ",", nullptr};

      ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp, fields, delis, FLG_NO_DELIS_STRINGS);
      if (ret) {
         DPRINTF("Error %d formatting shell_list\n", ret);
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGSHELLLIST);
         return answer;
      }
      ep_opt = sge_add_arg(pcmdline, S_OPT, lListT, "-S", str);
      lSetList(ep_opt, SPA_argval_lListT, lCopyList("shell list", lp));
   }
#endif

   /*
   ** -v, -V
   ** we always generate a -v statement, which means the user should not
   ** declare a job generated with -V as default
   */
   if ((lp = ocs::tom::get<JB_env_list>(job))) {
      int fields[] = { VA_variable, VA_value, 0};
      const char *delis[] = {"=", ",", nullptr};

      ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp, fields, delis,
         FLG_NO_DELIS_STRINGS);
      if (ret) {
         DPRINTF("Error %d formatting environment list as -v\n", ret);
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGENVIRONMENTLISTASV);
         return answer;
      }
      ep_opt = sge_add_arg(pcmdline, v_OPT, lListT, "-v", str);
      ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList("env list", lp));      
   }

   /*
   ** -verify is not unparsed
   */

   /*
   ** for full cmdline, script is needed
   */
   if ((flags & FLG_FULL_CMDLINE) &&
      (cp = ocs::tom::get<JB_script_file>(job))) {
      if (!strcmp(cp, "STDIN")) {
         if (ocs::tom::get<JB_job_args>(job)) {
         ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_SCRIPT, "--");
         ocs::tom::set<SPA_argval_lStringT>(ep_opt, "--");
         }
      }
      else {
      }
   }

   /*
   ** for full cmdline, job args are also needed
   */
   if ((flags & FLG_FULL_CMDLINE) &&
      (lp = ocs::tom::get<JB_job_args>(job))) {
      int fields[] = { ST_name, 0};
      const char *delis[] = {nullptr, " ", nullptr};

      ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp, fields, delis,
         FLG_NO_DELIS_STRINGS);
      if (ret) {
         DPRINTF("Error %d formatting job arguments\n", ret);
         answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGJOBARGUMENTS);
         return answer;
      }
      ep_opt = sge_add_arg(pcmdline, 0, lListT, STR_PSEUDO_JOBARG, str);
      ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList("job arguments", lp));      
   }




   DRETURN(answer);
}


static char *sge_unparse_checkpoint_attr(int opr, char *str) {
   int i = 0;

   if (opr & CHECKPOINT_AT_MINIMUM_INTERVAL)
      str[i++] = CHECKPOINT_AT_MINIMUM_INTERVAL_SYM;
   if (opr & CHECKPOINT_AT_SHUTDOWN)
      str[i++] = CHECKPOINT_AT_SHUTDOWN_SYM;
   if (opr & CHECKPOINT_SUSPEND)
      str[i++] = CHECKPOINT_SUSPEND_SYM;
   if (opr & NO_CHECKPOINT)
      str[i++] = NO_CHECKPOINT_SYM;

   str[i] = '\0';

   return str;
}

/*-------------------------------------------------------------------------*/
static char *sge_unparse_mail_options(
uint32_t mail_opt
) {
   DENTER(TOP_LAYER);

   static char mail_str[5 + 1];
   char *pc;

   memset(mail_str, 0, sizeof(mail_str));
   pc = mail_str;
   if (VALID(MAIL_AT_ABORT, mail_opt)) {
      *pc++ = 'a';
   }
   if (VALID(MAIL_AT_BEGINNING, mail_opt)) {
      *pc++ = 'b';
   }
   if (VALID(MAIL_AT_EXIT, mail_opt)) {
      *pc++ = 'e';
   }
   if (VALID(NO_MAIL, mail_opt)) {
      *pc++ = 'n';
   }
   if (VALID(MAIL_AT_SUSPENSION, mail_opt)) {
      *pc++ = 's';
   }
   
   if  (!*mail_str) {
      DRETURN(nullptr);
   }

   DRETURN((mail_str));
}

/*-------------------------------------------------------------------------*/
static int sge_unparse_account_string(
lListElem *job,
lList **pcmdline,
lList **alpp 
) {
   DENTER(TOP_LAYER);

   const char *cp;
   lListElem *ep_opt;

   if ((cp = ocs::tom::get<JB_account>(job))) {
      if (strcmp(cp, DEFAULT_ACCOUNT)) {
         ep_opt = sge_add_arg(pcmdline, A_OPT, lStringT, "-A", cp);
         ocs::tom::set<SPA_argval_lStringT>(ep_opt, cp);
      }
   }
   DRETURN(0);
}


/*-------------------------------------------------------------------------*/
static int sge_unparse_checkpoint_option(lListElem *job, lList **pcmdline, lList **alpp) {
   DENTER(TOP_LAYER);

   lListElem *ep_opt = nullptr;
   char *cp;
   int i;
   uint32_t ul;
   char str[256];

   if ((i = ocs::tom::get<JB_checkpoint_attr>(job))) {
      if ((cp = sge_unparse_checkpoint_attr(i, str))) {
         ep_opt = sge_add_arg(pcmdline, 0, lIntT, "-c", cp);
         ocs::tom::set<SPA_argval_lIntT>(ep_opt, i);
      } else {
         answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_JOB_INVALIDVALUEFORCHECKPOINTATTRIBINJOB_U,
                                 ocs::tom::get<JB_job_number>(job));
         DRETURN(-1);
      }
   }

   if ((ul = ocs::tom::get<JB_checkpoint_interval>(job))) {
      snprintf(str, sizeof(str), sge_u32, ul);
      ep_opt = sge_add_arg(pcmdline, c_OPT, lLongT, "-c", str);
      ocs::tom::set<SPA_argval_lLongT>(ep_opt, (long) ul);
   }

   DRETURN(0);
}


/*-------------------------------------------------------------------------*/
static int sge_unparse_string_option(
lListElem *job,
int nm,
const char *option,
lList **pcmdline,
lList **alpp 
) {
   DENTER(TOP_LAYER);

   lListElem *ep_opt = nullptr;
   const char *cp;

   if ((cp = lGetString(job, nm))) {
      ep_opt = sge_add_arg(pcmdline, 0, lStringT, option, cp);
      ocs::tom::set<SPA_argval_lStringT>(ep_opt, cp);
   }
   DRETURN(0);
}


/*-------------------------------------------------------------------------*/
static int sge_unparse_resource_list(lList *resource_list, bool hard, lList **pcmdline, lList **alpp) {
   DENTER(TOP_LAYER);

   int ret = 0;
   char str[MAX_STRING_SIZE];

   if (resource_list != nullptr) {
      if (hard) {
         sge_add_noarg(pcmdline, hard_OPT, "-hard", nullptr);
      } else {
         sge_add_noarg(pcmdline, soft_OPT, "-soft", nullptr);
      }

      ret = centry_list_append_to_string(resource_list, str, sizeof(str) - 1);
      if (ret) {
         DPRINTF("Error %d formatting hard_resource_list as -l\n", ret);
         answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGHARDRESOURCELISTASL);
         return ret;
      }
      if (*str && (str[strlen(str) - 1] == '\n')) {
         str[strlen(str) - 1] = 0;
      }
      lListElem *ep_opt = sge_add_arg(pcmdline, l_OPT, lListT, "-l", str);

      if (hard)
         ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList(nullptr, resource_list));
      else
         ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList(nullptr, resource_list));

      if (hard) 
         ocs::tom::set<SPA_argval_lIntT>(ep_opt, 1); /* means hard */
      else
         ocs::tom::set<SPA_argval_lIntT>(ep_opt, 2); /* means soft */
   }
   DRETURN(ret);
}

/*-------------------------------------------------------------------------*/
static int sge_unparse_pe(
lListElem *job,
lList **pcmdline,
lList **alpp 
) {
   DENTER(TOP_LAYER);

   const char *cp;
   const lList *lp = nullptr;
   lListElem *ep_opt;
   dstring string_buffer = DSTRING_INIT;
   int ret = 0;

   if ((cp = ocs::tom::get<JB_pe>(job))) {
      sge_dstring_append(&string_buffer, cp);
      sge_dstring_append(&string_buffer, " ");
      if (!(lp = ocs::tom::get<JB_pe_range>(job))) {
         DPRINTF("Job has parallel environment with no ranges\n");
         answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_JOB_JOBHASPEWITHNORANGES);
         sge_dstring_free(&string_buffer);
         DRETURN(-1);
      }
      {
         dstring range_string = DSTRING_INIT;

         range_list_print_to_string(lp, &range_string, true, false, false);
         sge_dstring_append(&string_buffer, sge_dstring_get_string(&range_string));
         sge_dstring_free(&range_string);
      }
      if (ret) {
         DPRINTF("Error %d formatting ranges in -pe\n", ret);
         answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGRANGESINPE);
         sge_dstring_free(&string_buffer);
         DRETURN(ret);
      }
      ep_opt = sge_add_arg(pcmdline, pe_OPT, lStringT, "-pe", 
                           sge_dstring_get_string(&string_buffer));
      ocs::tom::set<SPA_argval_lStringT>(ep_opt, cp);
      ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList("pe ranges", lp));
   }
   sge_dstring_free(&string_buffer);
   DRETURN(ret);
}

/*-------------------------------------------------------------------------*/
static int sge_unparse_path_list(lListElem *job, int nm, const char *option, lList **pcmdline, lList **alpp) {
   DENTER(TOP_LAYER);

   const lList *lp = nullptr;
   int ret = 0;
   char str[MAX_STRING_SIZE];
   lListElem *ep_opt;

   if ((lp = lGetList(job, nm))) {
      int fields[] = { PN_host, PN_path, 0 };
      const char *delis[] = {":", ",", nullptr};

      ret = uni_print_list(nullptr, str, sizeof(str) - 1, lp, fields, delis, FLG_NO_DELIS_STRINGS);
      if (ret) {
         DPRINTF("Error %d formatting path_list\n", ret);
         answer_list_add_sprintf(alpp, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                 MSG_LIST_ERRORFORMATINGPATHLIST);
         return ret;
      }
      ep_opt = sge_add_arg(pcmdline, e_OPT, lListT, option, str);
      ocs::tom::set<SPA_argval_lListT>(ep_opt, lCopyList(option, lp));
   }
   DRETURN(ret);
}
