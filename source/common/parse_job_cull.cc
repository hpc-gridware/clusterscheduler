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
#include <limits.h>
#include <cctype>
#include <unistd.h>
#include <cstdlib>
#include <cerrno>

#include "uti/sge_bootstrap.h"
#include "uti/sge_io.h"
#include "uti/sge_language.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_binding.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_var.h"

#include "symbols.h"
#include "parse_qsub.h"
#include "parse_job_cull.h"
#include "unparse_job_cull.h"

#include "msg_common.h"

#define USE_CLIENT_QSUB 1

/*
** set the correct defines
** USE_CLIENT_QSUB or
** USE_CLIENT_QALTER or
** USE_CLIENT_QSH
*/

const char *default_prefix = "#$";

/* static int skip_line(char *s); */

/* returns true if line has only white spaces */
/* static int skip_line( */
/* char *s  */
/* ) { */
/*    while ( *s != '\0' && *s != '\n' && isspace((int)*s))  */
/*       s++; */
/*    return (*s == '\0' || *s == '\n'); */
/* } */

/*
** NAME
**   cull_parse_job_parameter
** PARAMETER
**   cmdline            - nullptr or SPA_Type, if nullptr, *pjob is initialised with defaults
**   pjob               - pointer to job element, is filled according to cmdline
**
** RETURN
**   answer list, AN_Type or nullptr if everything ok, the following stati can occur:
**   STATUS_EUNKNOWN   - bad internal error like nullptr pointer received or no memory
**   STATUS_EDISK      - getcwd() failed
**   STATUS_ENOIMP     - unknown switch or -help occurred
** EXTERNAL
**   me
** DESCRIPTION
*/
lList *cull_parse_job_parameter(u_long32 uid, const char *username, const char *cell_root, 
                                const char *unqualified_hostname, const char *qualified_hostname, 
                                lList *cmdline, lListElem **pjob) 
{
   const char *cp;
   lListElem *ep;
   lList *answer = nullptr;
   lList *path_alias = nullptr;
   char error_string[MAX_STRING_SIZE];

   DENTER(TOP_LAYER); 

   if (!pjob) {
      answer_list_add(&answer,  MSG_PARSE_NULLPOINTERRECEIVED, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   if (!*pjob) {
      *pjob = lCreateElem(JB_Type);
      if (!*pjob) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_MEM_MEMORYALLOCFAILED_S, __func__);
         answer_list_add(&answer, SGE_EVENT, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
         DRETURN(answer);
      }
   }

   /*
   ** path aliasing
   */
   if (path_alias_list_initialize(&path_alias, &answer, cell_root, username, qualified_hostname) == -1) {
      DRETURN(answer);
   }

   job_initialize_env(*pjob, &answer, path_alias, unqualified_hostname, qualified_hostname);
   if (answer) {
      DRETURN(answer);
   }

   lSetUlong(*pjob, JB_priority, BASE_PRIORITY);

   lSetUlong(*pjob, JB_jobshare, 0);

   /*
    * -b
    */
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-b"))) {
      u_long32 jb_now = lGetUlong(*pjob, JB_type);

      if (lGetInt(ep, SPA_argval_lIntT) == 1) {
         JOB_TYPE_SET_BINARY(jb_now);
      }
      else {
         JOB_TYPE_UNSET_BINARY(jb_now);
      }
      
      lSetUlong(*pjob, JB_type, jb_now);
      lRemoveElem(cmdline, &ep);
   }
   
   /* 
    * -binding : when using "-binding linear" overwrite previous 
    *      DG:TODO      but not in with "-binding one_per_socket x" 
    *      DG:TODO      or "-binding striding offset x"
    *  binding n offset <- how should the error handling be done?
    */
   ep = lGetElemStrRW(cmdline, SPA_switch_val, "-binding");
   if (ep != nullptr) {
      const lList *binding_list = lGetList(ep, SPA_argval_lListT);
      lList *new_binding_list = lCopyList("binding",  binding_list);
      
      lSetList(*pjob, JB_binding, new_binding_list);
      lRemoveElem(cmdline, &ep);
   }

   /*
    * -shell
    */
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-shell"))) {
      u_long32 jb_now = lGetUlong(*pjob, JB_type);

      if (lGetInt(ep, SPA_argval_lIntT) == 1) {
         JOB_TYPE_UNSET_NO_SHELL(jb_now);
      }
      else {
         JOB_TYPE_SET_NO_SHELL(jb_now);
      }
      
      lSetUlong(*pjob, JB_type, jb_now);
      lRemoveElem(cmdline, &ep);
   }

   /*
    * -t
    */
   ep = lGetElemStrRW(cmdline, SPA_switch_val, "-t");
   if (ep != nullptr) {
      const lList *range_list = lGetList(ep, SPA_argval_lListT);
      lList *new_range_list = lCopyList("task_id_range",  range_list);

      lSetList(*pjob, JB_ja_structure, new_range_list);
      lRemoveElem(cmdline, &ep);
   
      {
         u_long32 job_type = lGetUlong(*pjob, JB_type);
         JOB_TYPE_SET_ARRAY(job_type);
         lSetUlong(*pjob, JB_type, job_type);
      }
   } else {
      job_set_submit_task_ids(*pjob, 1, 1, 1);
   }
   job_initialize_id_lists(*pjob, &answer);
   if (answer != nullptr) {
      DRETURN(answer);
   }

   /*
   ** -tc option throttle the number of concurrent tasks
   */
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-tc"))) {
      lSetUlong(*pjob, JB_ja_task_concurrency, lGetUlong(ep, SPA_argval_lUlongT));
      lRemoveElem(cmdline, &ep);
   }

   /*
   ** -clear option is special, is sensitive to order
   ** kills all options that come before
   ** there might be more than one -clear
   */
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-clear"))) {
      lListElem *ep_run;
      const char *cp_switch;

      for (ep_run = lFirstRW(cmdline); ep_run;) {
         /*
         ** remove -clear itsself
         */
         if (ep_run == ep) {
            lRemoveElem(cmdline, &ep_run);
            break;
         }
         /*
         ** lNext can never be nullptr here, because the -clear
         ** element is the last one to delete
         ** in general, these two lines wont work!
         */
         ep_run = lNextRW(ep_run);
         
         /*
         ** remove switch only if it is not a pseudo-arg
         */
         cp_switch = lGetString(lPrev(ep_run), SPA_switch_val);
         if (cp_switch && (*cp_switch == '-')) {
            lListElem *prev = lPrevRW(ep_run); 
            lRemoveElem(cmdline, &prev);
         }
      }

   }

   /*
   ** general remark: There is a while loop looping through the option
   **                 list. So inside the while loop there must be made
   **                 a decision if a second occurence of an option has
   **                 to be handled as error, should be warned, overwritten
   **                 or simply ignored.
   */ 

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-a"))) {
      lSetUlong(*pjob, JB_execution_time, lGetUlong(ep, SPA_argval_lUlongT));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-A"))) {
      /* the old account string is overwritten */
      lSetString(*pjob, JB_account, lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-ar"))) {
      /* the old advance reservation is overwritten */
      lSetUlong(*pjob, JB_ar, lGetUlong(ep, SPA_argval_lUlongT));
      lRemoveElem(cmdline, &ep);
   }
   
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-dl"))) {
      lSetUlong(*pjob, JB_deadline, lGetUlong(ep, SPA_argval_lUlongT));
      lRemoveElem(cmdline, &ep);
   }
   
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-c"))) {
      if (lGetUlong(ep, SPA_argtype) == lLongT)
         lSetUlong(*pjob, JB_checkpoint_interval, lGetLong(ep, SPA_argval_lLongT));
      if (lGetUlong(ep, SPA_argtype) == lIntT) 
         lSetUlong(*pjob, JB_checkpoint_attr, lGetInt(ep, SPA_argval_lIntT));

      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-ckpt"))) {
      lSetString(*pjob, JB_checkpoint_name, lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-C"))) {
      lSetString(*pjob, JB_directive_prefix, 
         lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-jsv"))) {
      const lList *list = lGetList(ep, SPA_argval_lListT);
      const char *file = lGetString(lFirst(list), PN_path);

      jsv_list_add("jsv_switch", JSV_CONTEXT_CLIENT, nullptr, file);
      lRemoveElem(cmdline, &ep);
   }

   /*
   ** the handling of the path lists is not so trivial
   ** if we have several path lists we have to check
   ** 1. is there only one path entry without host
   ** 2. do the entries collide, i.e. are there two
   **    entries for the same host with different paths
   ** These restrictions seem reasonable to me but are
   ** not addressed right now
   */

   /*
   ** to use lAddList correctly we have to get the list out
   ** of the option struct otherwise we free the list once again
   ** in the lSetList(ep, SPA_argval_lListT, nullptr); call
   ** this can lead to a core dump
   ** so a better method is to xchange the list in the option struct 
   ** with a null pointer, this is not nice but safe
   ** a little redesign of cull would be nice
   ** see parse_list_simple
   */
   parse_list_simple(cmdline, "-e", *pjob, JB_stderr_path_list, PN_host, PN_path, FLG_LIST_MERGE);

   /* -h */
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-h"))) {
      if (lGetInt(ep, SPA_argval_lIntT) & MINUS_H_TGT_USER) {
         lSetList(*pjob, JB_ja_u_h_ids, lCopyList("task_id_range",
                  lGetList(*pjob, JB_ja_n_h_ids)));
         lSetList(*pjob, JB_ja_n_h_ids, nullptr);
      }
      lRemoveElem(cmdline, &ep);
   }

   /* not needed in job struct */
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-hard"))) {
      lRemoveElem(cmdline, &ep);
   }

   if ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-help"))) {
      lRemoveElem(cmdline, &ep);
      answer_list_add_sprintf(&answer, STATUS_ENOIMP, ANSWER_QUALITY_ERROR,
                              MSG_ANSWER_HELPNOTALLOWEDINCONTEXT);
      DRETURN(answer);
   }

   /* -hold_jid */
   if (lGetElemStr(cmdline, SPA_switch_val, "-hold_jid")) {
      lListElem *ep;
      lListElem *sep;
      lList *jref_list = nullptr;
      while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-hold_jid"))) {
         for_each_rw(sep, lGetList(ep, SPA_argval_lListT)) {
            DPRINTF(("-hold_jid %s\n", lGetString(sep, ST_name)));
            lAddElemStr(&jref_list, JRE_job_name, lGetString(sep, ST_name), JRE_Type);
         }
         lRemoveElem(cmdline, &ep);
      }
      lSetList(*pjob, JB_jid_request_list, jref_list);
   }

   /* -hold_jid_ad */
   if (lGetElemStr(cmdline, SPA_switch_val, "-hold_jid_ad")) {
      lListElem *ep;
      const lListElem *sep;
      lList *jref_list = nullptr;
      while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-hold_jid_ad"))) {
         for_each_ep(sep, lGetList(ep, SPA_argval_lListT)) {
            DPRINTF(("-hold_jid_ad %s\n", lGetString(sep, ST_name)));
            lAddElemStr(&jref_list, JRE_job_name, lGetString(sep, ST_name), JRE_Type);
         }
         lRemoveElem(cmdline, &ep);
      }
      lSetList(*pjob, JB_ja_ad_request_list, jref_list);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-j"))) {
      lSetBool(*pjob, JB_merge_stderr, lGetInt(ep, SPA_argval_lIntT));
      lRemoveElem(cmdline, &ep);
   }

   parse_list_simple(cmdline, "-jid", *pjob, JB_job_identifier_list, 
                        0, 0, FLG_LIST_APPEND);

   parse_list_hardsoft(cmdline, "-l", *pjob, 
                        JB_hard_resource_list, JB_soft_resource_list);

   centry_list_remove_duplicates(lGetListRW(*pjob, JB_hard_resource_list));
   centry_list_remove_duplicates(lGetListRW(*pjob, JB_soft_resource_list));

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-m"))) {
      u_long32 ul;
      u_long32 old_mail_opts;

      ul = lGetInt(ep, SPA_argval_lIntT);
      if  ((ul & NO_MAIL)) {
         lSetUlong(*pjob, JB_mail_options, 0);
      }
      else {
         old_mail_opts = lGetUlong(*pjob, JB_mail_options);
         lSetUlong(*pjob, JB_mail_options, ul | old_mail_opts);
      }
      lRemoveElem(cmdline, &ep);
   }

   parse_list_simple(cmdline, "-M", *pjob, JB_mail_list, MR_host, MR_user, FLG_LIST_MERGE);

#ifndef USE_CLIENT_QALTER
   if (!lGetList(*pjob, JB_mail_list)) {   
      ep = lAddSubStr(*pjob, MR_user, username, JB_mail_list, MR_Type);
      lSetHost(ep, MR_host, qualified_hostname);
   }
#endif

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-N"))) {
      lSetString(*pjob, JB_job_name, lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(cmdline, &ep);
   }
   
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-notify"))) {
      lSetBool(*pjob, JB_notify, TRUE);
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-now"))) {
      u_long32 jb_now = lGetUlong(*pjob, JB_type);
      if(lGetInt(ep, SPA_argval_lIntT)) {
         JOB_TYPE_SET_IMMEDIATE(jb_now);
      } else {
         JOB_TYPE_CLEAR_IMMEDIATE(jb_now);
      }

      lSetUlong(*pjob, JB_type, jb_now);

      lRemoveElem(cmdline, &ep);
   }
   
   /*
   ** this is exactly the same as for error list and it would be nice
   ** to have generalized funcs, that do all this while stuff and so
   ** on and are encapsulated in a function where I can give in the
   ** job element pointer the field name the option list and the option
   ** name and the field is filled
   */
   parse_list_simple(cmdline, "-o", *pjob, JB_stdout_path_list, PN_host, 
                     PN_path, FLG_LIST_MERGE);
   parse_list_simple(cmdline, "-i", *pjob, JB_stdin_path_list, PN_host, 
                     PN_path, FLG_LIST_MERGE);

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-p"))) {
      int pri = lGetInt(ep, SPA_argval_lIntT);
      lSetUlong(*pjob, JB_priority, BASE_PRIORITY + pri);
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-js"))) {
      lSetUlong(*pjob, JB_jobshare, lGetUlong(ep, SPA_argval_lUlongT));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-P"))) {
      lSetString(*pjob, JB_project, lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-pe"))) {
      lSetString(*pjob, JB_pe, lGetString(ep, SPA_argval_lStringT));
      lSwapList(*pjob, JB_pe_range, ep, SPA_argval_lListT);
      lRemoveElem(cmdline, &ep);
   }

   parse_list_hardsoft(cmdline, "-q", *pjob, 
                        JB_hard_queue_list, JB_soft_queue_list);

   parse_list_hardsoft(cmdline, "-masterq", *pjob, 
                        JB_master_hard_queue_list, 0);
   
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-R"))) {
      lSetBool(*pjob, JB_reserve, lGetInt(ep, SPA_argval_lIntT));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-pty"))) {
      lSetUlong(*pjob, JB_pty, lGetInt(ep, SPA_argval_lIntT));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-r"))) {
      lSetUlong(*pjob, JB_restart, lGetInt(ep, SPA_argval_lIntT));
      lRemoveElem(cmdline, &ep);
   }

   /* not needed in job struct */
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-soft"))) {
      lRemoveElem(cmdline, &ep);
   }

   parse_list_simple(cmdline, "-S", *pjob, JB_shell_list, PN_host, PN_path, FLG_LIST_MERGE);

   /* -terse option, not needed in job struct */
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-terse"))) {
      lRemoveElem(cmdline, &ep);
   }

   parse_list_simple(cmdline, "-u", *pjob, JB_user_list, 0, 0, FLG_LIST_APPEND);

   /*
   ** to be processed in original order, set -V equal to -v
   */
   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-V"))) {
      lSetString(ep, SPA_switch_val, "-v");
   }
   parse_list_simple(cmdline, "-v", *pjob, JB_env_list, VA_variable, VA_value, FLG_LIST_MERGE);
   cull_compress_definition_list(lGetListRW(*pjob, JB_env_list), VA_variable, VA_value, 0);

   /* context switches are sensitive to order */
   ep = lFirstRW(cmdline);
   while(ep)
      if(!strcmp(lGetString(ep, SPA_switch_val), "-ac") ||
         !strcmp(lGetString(ep, SPA_switch_val), "-dc") ||
         !strcmp(lGetString(ep, SPA_switch_val), "-sc")) {
         lListElem* temp;
         if(!lGetList(*pjob, JB_context)) {
            lSetList(*pjob, JB_context, lCopyList("context", lGetList(ep, SPA_argval_lListT)));
         }
         else {
            lList *copy = lCopyList("context", lGetList(ep, SPA_argval_lListT));
            lAddList(lGetListRW(*pjob, JB_context), &copy);
         }
         temp = lNextRW(ep);
         lRemoveElem(cmdline, &ep);
         ep = temp;
      } else {
         ep = lNextRW(ep);
      }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-verify"))) {

      lSetUlong(*pjob, JB_verify, TRUE);
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-w"))) {
      lSetUlong(*pjob, JB_verify_suitable_queues, lGetInt(ep, SPA_argval_lIntT));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, "-wd"))) {
      const char *path = lGetString(ep, SPA_argval_lStringT);
      bool is_cwd = false;

      if (path == nullptr) {
         char tmp_str[SGE_PATH_MAX + 1];

         is_cwd = true;
         if (!getcwd(tmp_str, sizeof(tmp_str))) {
            /* If getcwd() fails... */
            answer_list_add(&answer, MSG_ANSWER_GETCWDFAILED, 
                            STATUS_EDISK, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }
         
         path = reroot_path(*pjob, tmp_str, &answer);
         
         if (path == nullptr) {
            DRETURN(answer);
         }
      }

      lSetString(*pjob, JB_cwd, path);
      lRemoveElem(cmdline, &ep);
      
      lSetList(*pjob, JB_path_aliases, lCopyList("PathAliases", path_alias));

      if (is_cwd) {
         sge_free(&path);
      }
   }

   lFreeList(&path_alias);
   
   /*
   ** no switch - must be scriptfile
   ** only for qalter and qsub, not for qsh
   */
   if ((ep = lGetElemStrRW(cmdline, SPA_switch_val, STR_PSEUDO_SCRIPT))) {
      lSetString(*pjob, JB_script_file, lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(cmdline, &ep);
   }

   if ((ep = lGetElemStrRW(cmdline, SPA_switch_val, STR_PSEUDO_SCRIPTLEN))) {
      lSetUlong(*pjob, JB_script_size, lGetUlong(ep, SPA_argval_lUlongT));
      lRemoveElem(cmdline, &ep);
   }

   if ((ep = lGetElemStrRW(cmdline, SPA_switch_val, STR_PSEUDO_SCRIPTPTR))) {
      lSetString(*pjob, JB_script_ptr, lGetString(ep, SPA_argval_lStringT));
      lRemoveElem(cmdline, &ep);
   }

   {
      lList *lp;
      
      lp = lCopyList("job args", lGetList(*pjob, JB_job_args));

      while ((ep = lGetElemStrRW(cmdline, SPA_switch_val, STR_PSEUDO_JOBARG))) {
         lAddElemStr(&lp, ST_name, lGetString(ep, SPA_argval_lStringT), ST_Type);
         
         lRemoveElem(cmdline, &ep);
      }
      lSetList(*pjob, JB_job_args, lp);
   }
   
   for_each_rw(ep, cmdline) {
      snprintf(error_string, sizeof(error_string), MSG_ANSWER_UNKOWNOPTIONX_S, lGetString(ep, SPA_switch_val));
      cp = lGetString(ep, SPA_switch_arg);
      if (cp) {
         strcat(error_string, " ");
         strcat(error_string, cp);
      }
      strcat(error_string, "\n");
      answer_list_add(&answer, error_string, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   } 

   cp = lGetString(*pjob, JB_script_file);
   
   if (!cp || !strcmp(cp, "-")) {
      u_long32 jb_now = lGetUlong(*pjob, JB_type);
      if( JOB_TYPE_IS_BINARY(jb_now) ) {
         answer_list_add(&answer, MSG_COMMAND_REQUIRED_FOR_BINARY_JOB,
                         STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(answer);
      }
      lSetString(*pjob, JB_script_file, "STDIN");
   }
   cp = lGetString(*pjob, JB_job_name);
   if (!cp) {
      cp = sge_basename(lGetString(*pjob, JB_script_file), '/');
      lSetString(*pjob, JB_job_name,  cp);
   }

   DRETURN(answer);
}

/****** client/parse_job_cull/parse_script_file() *****************************
*  NAME
*     parse_script_file() -- parse a job script or a job defaults file
*
*  SYNOPSIS
*     lList* parse_script_file(char *script_file, 
*                              const char *directive_prefix, 
*                              lList **lpp_options, 
*                              char **envp, 
*                              u_long32 flags);
*
*  FUNCTION
*     Searches for special comments in script files and parses contained 
*     SGE options or
*     parses SGE options in job defaults files (sge_request).
*     Script files are parsed with directive prefix nullptr,
*     default files are parsed with directive prefix "" and FLG_USE_NO_PSEUSOS.
*
*  INPUTS
*     char *script_file      - script file name or nullptr or "-"
*                              in the latter two cases the job script is read 
*                              from stdin
*     char *directive_prefix - only lines beginning with this prefix are parsed
*                              nullptr causes function to look in the lpp_options
*                              list whether the -C option has been set. 
*                              If it has, this prefix is used.
*                              If not, the default prefix "#$" is used. 
*                              "" causes the function to parse all lines not 
*                              starting with "#" (comment lines).
*     lList **lpp_options    - list pointer-pointer, SPA_Type
*                              list to store the recognized switches in, is 
*                              created if it doesnt exist but there are 
*                              options to be returned
*     char **envp            - environment pointer
*     u_long32 flags         - FLG_HIGHER_PRIOR:    new options are appended 
*                                                   to list
*                              FLG_LOWER_PRIOR:     new options are inserted 
*                                                   at the beginning of list
*                              FLG_USE_NO_PSEUDOS:  do not create pseudoargs 
*                                                   for script pointer and 
*                                                   length
*                              FLG_IGN_NO_FILE:     do not show an error if 
*                                                   script_file was not found
*  RESULT
*     lList* - answer list, AN_Type, or nullptr if everything was ok,
*              the following stati can occur:
*                 STATUS_EUNKNOWN - bad internal error like nullptr pointer
*                                   received or no memory
*                 STATUS_EDISK    - file could not be opened
*
*  NOTES
*     Special comments in script files have to start in the first column of a 
*     line.
*     Comments in job defaults files have to start in the first column of a 
*     line.
*     If a line is longer than MAX_STRING_SIZE bytes, contents (SGE options) 
*     starting from position MAX_STRING_SIZE + 1 are silently ignored.
*     MAX_STRING_SIZE is defined in common/basis_types.h (current value 2048).
*
*     MT-NOTE: parse_script_file() is MT safe
*
*  SEE ALSO
*     centry_list_parse_from_string()
*     basis_types.h
*     sge_request(5)
*******************************************************************************/
lList *parse_script_file(
u_long32 prog_number,
const char *script_file,
const char *directive_prefix,
lList **lpp_options, 
char **envp,
u_long32 flags 
) {
   unsigned int dpl; /* directive_prefix length */
   FILE *fp;
   char *filestrptr = nullptr;
   int script_len = 0;
   char **str_table = nullptr;
   lList *alp, *answer = nullptr;
   lListElem *aep;
   int i;
   int do_exit = 0;
   lListElem *ep_opt;
   lList *lp_new_opts = nullptr;
   /* snprintf takes the nullptr terminator into account. */
   char error_string[MAX_STRING_SIZE];

   DENTER(TOP_LAYER);

   if (!lpp_options) {
      /* no place where to put result */
      answer_list_add(&answer, MSG_ANSWER_CANTPROCESSNULLLIST, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   if ((flags & FLG_IGN_NO_FILE) && sge_is_file(script_file) == 0) {
      DRETURN(answer);
   }

   if (!(flags & FLG_IGNORE_EMBEDED_OPTS)) {
      if (script_file && strcmp(script_file, "-")) {
         /* are we able to access this file? */
         if ((fp = fopen(script_file, "r")) == nullptr) {
            snprintf(error_string, sizeof(error_string),
                     MSG_FILE_ERROROPENINGXY_SS, script_file, strerror(errno));
            answer_list_add(&answer, error_string, STATUS_EDISK, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }
         
         FCLOSE(fp);

         /* read the script file in one sweep */
         filestrptr = sge_file2string(script_file, &script_len);

         if (filestrptr == nullptr) {
            snprintf(error_string, sizeof(error_string),
                     MSG_ANSWER_ERRORREADINGFROMFILEX_S, script_file);
            answer_list_add(&answer, error_string,
                            STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }
      } else {
         /* no script file but input from stdin */
         filestrptr = sge_stream2string(stdin, &script_len);
         if (filestrptr == nullptr) {
            answer_list_add(&answer, MSG_ANSWER_ERRORREADINGFROMSTDIN, 
                            STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }
         else if (filestrptr[0] == '\0') {
            answer_list_add(&answer, MSG_ANSWER_NOINPUT, 
                            STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            sge_free(&filestrptr);
            DRETURN(answer);
         }
      }

      if (directive_prefix == nullptr) {
         DPRINTF(("directive prefix = <null> - will skip parsing of script\n"));
         dpl = 0;
      } else {
         DPRINTF(("directive prefix = " SFQ "\n", directive_prefix));
         dpl = strlen(directive_prefix);
      }

      if (directive_prefix != nullptr) {
         char *parameters = nullptr;
         char *free_me = nullptr;
         char *s = nullptr;
         int nt_index = -1;
         
         /* now look for job parameters in script file */
         s = filestrptr;

         nt_index = strlen (s);

         while (*s != '\0') {
            int length = 0;
            char *newline = nullptr;
            int nl_index = -1;

            newline = strchr (s, '\n');

            if (newline != nullptr) {
               /* I'm doing this math very carefully because I'm not entirely
                * certain how the compiler will interpret subtracting a pointer
                * from a pointer.  Better safe than sorry. */
               nl_index = (int)(((long)newline - (long)s) / sizeof (char));
            }

            if (nl_index != -1) {
               parameters = sge_malloc (sizeof (char) * (nl_index + 1));
               
               if (parameters == nullptr) {
                  answer_list_add(&answer, MSG_SGETEXT_NOMEM, 
                                  STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                  DRETURN(answer);
               }
               
               strncpy (parameters, s, nl_index);
               parameters[nl_index] = '\0';
               /* The newline counts as a parsed character even though it isn't
                * included in the parameter string. */
               length = nl_index + 1;
            }
            else {
               parameters = sge_malloc (sizeof (char) * (nt_index + 1));
               
               if (parameters == nullptr) {
                  answer_list_add(&answer, MSG_SGETEXT_NOMEM, 
                                  STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
                  DRETURN(answer);
               }
               
               /* strcpy copies everything up to and including the nullptr
                * termination. */
               strcpy (parameters, s);
               length = nt_index;
            }
            
            /* Advance the pointer past the string we just copied. */
            s += length;

            /* Update the location of the nullptr terminator. */
            nt_index -= length;

            /* Store a copy of the memory pointer. */
            free_me = parameters;
            
            /*
            ** If directive prefix is zero string then all lines except
            ** comment lines are read, this makes it possible to parse
            ** defaults files with this function.
            ** If the line contains no SGE options, we set parameters to nullptr.
            */

            if (dpl == 0) {
               /* we parse a settings file (e.g. sge_request): skip comment lines */
               if (*parameters == '#') {
                  parameters = nullptr;
               }
            } else {
               /* we parse a script file with special comments */
               if (strncmp(parameters, directive_prefix, dpl) == 0) {
                  parameters += dpl;

                  while (isspace(*parameters)) {
                     parameters++;
                  }
               } else {
                  parameters = nullptr;
               }
            }

            /* delete trailing garbage */
            if (parameters != nullptr) {
               char *additional_comment;

               /* don't copy additional comments */
               if ((additional_comment = strchr(parameters, '#')) != nullptr) {
                  additional_comment[0] = '\0';
               }

               /* Start one character before the nullptr terminator. */
               i = strlen(parameters) - 1;
               
               while (i >= 0) {
                  if (!isspace(parameters[i])) {
                     /* We start one character before the nullptr terminator, so
                      * we are guaranteed to always be able to access the
                      * character at i+1. */
                     parameters[i + 1] = '\0';
                     break;
                  }

                  i--;
               }
            }

            if ((parameters != nullptr) && (*parameters != '\0')) {
               lListElem *ep = nullptr;
               
               DPRINTF(("parameter in script: %s\n", parameters));

               /*
               ** here cull comes in 
               */

               /* so str_table has to be freed afterwards */
               str_table = string_list(parameters, " \t\n", nullptr);
               
               for (i=0; str_table[i]; i++) {
                  DPRINTF(("str_table[%d] = '%s'\n", i, str_table[i])); 
               }

               sge_strip_quotes(str_table);

               for (i=0; str_table[i]; i++) {
                  DPRINTF(("str_table[%d] = '%s'\n", i, str_table[i]));      
               }

               /*
               ** problem: error handling missing here and above
               */
               alp = cull_parse_cmdline(prog_number, str_table, envp, &lp_new_opts, 0);

               for_each_rw (aep, alp) {
                  answer_quality_t quality;
                  u_long32 status = STATUS_OK;

                  status = lGetUlong(aep, AN_status);
                  quality = (answer_quality_t)lGetUlong(aep, AN_quality);

                  if (quality == ANSWER_QUALITY_ERROR) {
                     DPRINTF(("%s", lGetString(aep, AN_text)));
                     do_exit = 1;
                  }
                  else {
                     DPRINTF(("Warning: %s\n", lGetString(aep, AN_text)));
                  }
                  answer_list_add(&answer, lGetString(aep, AN_text), status,
                                  quality);
               } /* for_each_ep(aep in alp) */

               sge_free(&str_table);
               lFreeList(&alp);
               sge_free(&free_me);
               parameters = nullptr;
               
               if (do_exit) {
                  sge_free(&filestrptr);
                  DRETURN(answer);
               }

               /*
               ** -C option is ignored in scriptfile - delete all occurences
               */
               while ((ep = lGetElemStrRW(lp_new_opts, SPA_switch_val, "-C"))) {
                  lRemoveElem(lp_new_opts, &ep);
               }
            } /* if (parameters is not empty) */
            else {
               sge_free(&free_me);
               parameters = nullptr;
            }
         } /* while (*s != '\0') */
      }   
   }

   if (!(flags & FLG_USE_NO_PSEUDOS)) {
      /*
      ** if script is not yet there we add it to the command line,
      ** except if the caller requests not to
      */
      if (!(flags & FLG_DONT_ADD_SCRIPT)) {
         if (!lpp_options || !lGetElemStr(*lpp_options, SPA_switch_val, STR_PSEUDO_SCRIPT)) {
            ep_opt = sge_add_arg(&lp_new_opts, 0, lStringT, STR_PSEUDO_SCRIPT, nullptr);
            lSetString(ep_opt, SPA_argval_lStringT, 
               ((!script_file || !strcmp(script_file, "-")) ? "STDIN" : script_file));
         }
      }
      ep_opt = sge_add_arg(&lp_new_opts, 0, lUlongT, STR_PSEUDO_SCRIPTLEN, nullptr);
      lSetUlong(ep_opt, SPA_argval_lUlongT, script_len);

      ep_opt = sge_add_arg(&lp_new_opts, 0, lStringT, STR_PSEUDO_SCRIPTPTR, nullptr);
      lSetString(ep_opt, SPA_argval_lStringT, filestrptr);
   }

   if (!lp_new_opts) {
      sge_free(&filestrptr);
      DRETURN(answer);
   }

   if (!*lpp_options) {
      *lpp_options = lp_new_opts;
   } else {
      if (flags & FLG_LOWER_PRIOR) {
         lAddList(lp_new_opts, lpp_options);
         *lpp_options = lp_new_opts;
      } else {
         lAddList(*lpp_options, &lp_new_opts);
         lp_new_opts = nullptr;
      }
   }

   sge_free(&filestrptr);

   DRETURN(answer);
FCLOSE_ERROR:
   snprintf(error_string, sizeof(error_string),
            MSG_FILE_ERRORCLOSEINGXY_SS, script_file, strerror(errno));
   answer_list_add(&answer, error_string,
                   STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);

   DRETURN(answer);
}

