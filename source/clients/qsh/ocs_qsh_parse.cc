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
 * @brief qsh - command line parsing specific to the interactive clients
 */
#include <cstring>
#include <climits>
#include <unistd.h>
#include <cstdlib>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_Binding.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_jsv.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_VA.h"

#include "parse_qsub.h"
#include "ocs_qsh_parse.h"
#include "symbols.h"
#include "msg_common.h"

/** @brief Fill in a job object from a parsed `qsh`/`qlogin`/`qrsh` command line
 *
 * The interactive counterpart of #cull_parse_job_parameter. It differs mainly
 * in the defaults: an interactive job is not rerunnable, gets no output files,
 * and is submitted with `-now yes` unless told otherwise.
 *
 * @param prog_number which of `qsh`, `qlogin` and `qrsh` is running
 * @param uid the submitting user's uid
 * @param username the submitting user's name
 * @param cell_root the cell directory, for resolving the default files
 * @param unqualified_hostname the submit host, unqualified
 * @param qualified_hostname the submit host, fully qualified
 * @param cmdline the parsed options (`SPA_Type`), or `nullptr` for defaults only
 * @param pjob the job (`JB_Type`) to fill in
 *
 * @return `nullptr` when everything was fine, otherwise an answer list
 *         (`AN_Type`) carrying one of
 *         - `STATUS_EUNKNOWN` - an internal error, a null pointer or out of memory
 *         - `STATUS_EDISK` - `getcwd()` failed
 *         - `STATUS_ENOIMP` - an unknown switch, or `-help`
 */
lList *cull_parse_qsh_parameter(uint32_t prog_number, uint32_t uid, const char *username, const char *cell_root,
                                const char *unqualified_hostname, const char *qualified_hostname, lList *cmdline, lListElem **pjob) {
   DENTER(TOP_LAYER); 

   const char *cp;
   lListElem *ep;
   lList *answer = nullptr;
   lList *path_alias = nullptr;
   uint32_t job_now;

   if (!pjob) {
      answer_list_add(&answer, MSG_PARSE_NULLPOINTERRECEIVED, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   /*
   ** path aliasing
   */
   if (path_alias_list_initialize(&path_alias, &answer, cell_root, username, 
                                  qualified_hostname) == -1) {
      lFreeList(&path_alias);
      DRETURN(answer);
   }

   job_initialize_env(*pjob, &answer, path_alias, unqualified_hostname, qualified_hostname);
   if (answer) {
      lFreeList(&path_alias);
      DRETURN(answer);
   }

   ocs::tom::set<JB_priority>(*pjob, BASE_PRIORITY);
   ocs::tom::set<JB_jobshare>(*pjob, 0);
   ocs::tom::set<JB_verify_suitable_queues>(*pjob, SKIP_VERIFY);

   /*
   ** it does not make sense to rerun interactive jobs
   ** the default is to decide via the queue configuration
   ** we turn this off here explicitly (2 is no)
   */
   job_now = ocs::tom::get<JB_type>(*pjob);
   if (JOB_TYPE_IS_QSH(job_now) || JOB_TYPE_IS_QLOGIN(job_now)
       || JOB_TYPE_IS_QRSH(job_now) || JOB_TYPE_IS_QRLOGIN(job_now)) {
      ocs::tom::set<JB_restart>(*pjob, 2);
   }

   while (JOB_TYPE_IS_QRSH(job_now)
          && (ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-notify"))) {
      ocs::tom::set<JB_notify>(*pjob, true);
      lRemoveElem(cmdline, &ep);
   }  

   /* 
    * turn on immediate scheduling as default  
    * can be overwriten by option -now n 
    */
   {
      uint32_t type = ocs::tom::get<JB_type>(*pjob);

      JOB_TYPE_SET_IMMEDIATE(type);
      ocs::tom::set<JB_type>(*pjob, type);
   }

   // -btype host | slot
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-btype"))) {
      // Create a binding element if it does not exist
      lListElem *binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (binding_elem == nullptr) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_new_type>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      lRemoveElem(cmdline, &ep);
   }

   // -bamount number
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-bamount"))) {
      // Create a binding element if it does not exist
      lListElem *binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (binding_elem == nullptr) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_amount>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      lRemoveElem(cmdline, &ep);
   }

   // -binstance set | env | pe
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-binstance"))) {
      // Create a binding element if it does not exist
      lListElem *binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (binding_elem == nullptr) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_instance>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      lRemoveElem(cmdline, &ep);
   }

   // -bunit binding_unit
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-bunit"))) {
      // Create a binding element if it does not exist
      lListElem *binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (binding_elem == nullptr) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_unit>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      lRemoveElem(cmdline, &ep);
   }

   // -bfilter topo_string
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-bfilter"))) {
      // Create a binding element if it does not exist
      lListElem *binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (binding_elem == nullptr) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_filter>(binding_elem, ocs::tom::get<SPA_argval_lStringT>(ep));

      // Get rid of the binding element from the command line
      lRemoveElem(cmdline, &ep);
   }

   // -bsort sort_string
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-bsort"))) {
      // Create a binding element if it does not exist
      lListElem *binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (binding_elem == nullptr) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_sort>(binding_elem, ocs::tom::get<SPA_argval_lStringT>(ep));

      // Get rid of the binding element from the command line
      lRemoveElem(cmdline, &ep);
   }

   // -bstart
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-bstart"))) {
      // Create a binding element if it does not exist
      lListElem *binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (binding_elem == nullptr) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_start>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      lRemoveElem(cmdline, &ep);
   }

   // -bstop
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-bstop"))) {
      // Create a binding element if it does not exist
      lListElem *binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (binding_elem == nullptr) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_stop>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      lRemoveElem(cmdline, &ep);
   }

   // -bstrategy
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-bstrategy"))) {
      // Create a binding element if it does not exist
      lListElem *binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (binding_elem == nullptr) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_strategy>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      lRemoveElem(cmdline, &ep);
   }

   /*
   ** -clear option is special, is sensitive to order
   ** kills all options that come before
   ** there might be more than one -clear
   */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-clear"))) {
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
         cp_switch = ocs::tom::get<SPA_switch_val>(lPrev(ep_run));
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

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-A"))) {
      /* the old account string is overwritten */
      ocs::tom::set<JB_account>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
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
   job_set_submit_task_ids(*pjob, 1, 1, 1);
   job_initialize_id_lists(*pjob, &answer);
   if (answer != nullptr) {
      lFreeList(&path_alias);
      DRETURN(answer);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-ar"))) {
      /* the old advance reservation is overwritten */
      ocs::tom::set<JB_ar>(*pjob, ocs::tom::get<SPA_argval_lUlongT>(ep));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-dept"))) {
      ocs::tom::set<JB_department>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-e"))) {
      ocs::tom::set<JB_stderr_path_list>(*pjob, lCopyList("stderr_path_list", ocs::tom::get<SPA_argval_lListT>(ep)));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-h"))) {
      if (ocs::tom::get<SPA_argval_lIntT>(ep) & MINUS_H_TGT_USER) {
         ocs::tom::set<JB_ja_u_h_ids>(*pjob, lCopyList("task_id_range",
            ocs::tom::get<JB_ja_n_h_ids>(*pjob)));
      }
      ocs::tom::set<JB_ja_n_h_ids>(*pjob, nullptr);
      lRemoveElem(cmdline, &ep);
   }

   /* -hold_jid */
   if (ocs::tom::find_first<SPA_switch_val>(cmdline, "-hold_jid")) {
      lListElem *ep;
      lList *jref_list = nullptr;
      while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-hold_jid"))) {
         for_each_ep_lv(sep, ocs::tom::get<SPA_argval_lListT>(ep)) {
            DPRINTF("-hold_jid %s\n", ocs::tom::get<ST_name>(sep));
            ocs::tom::emplace_back<JRE_job_name>(&jref_list, ocs::tom::get<ST_name>(sep));
         }
         lRemoveElem(cmdline, &ep);
      }
      ocs::tom::set<JB_jid_request_list>(*pjob, jref_list);
   }

   /* -hold_jid_ad */
   if (ocs::tom::find_first<SPA_switch_val>(cmdline, "-hold_jid_ad")) {
      lListElem *ep;
      lList *jref_list = nullptr;
      while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-hold_jid_ad"))) {
         for_each_ep_lv(sep, ocs::tom::get<SPA_argval_lListT>(ep)) {
            DPRINTF("-hold_jid_ad %s\n", ocs::tom::get<ST_name>(sep));
            ocs::tom::emplace_back<JRE_job_name>(&jref_list, ocs::tom::get<ST_name>(sep));
         }
         lRemoveElem(cmdline, &ep);
      }
      ocs::tom::set<JB_ja_ad_request_list>(*pjob, jref_list);
   }

   /* not needed in job struct */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-hard"))) {
      lRemoveElem(cmdline, &ep);
   }

   if ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-help"))) {
      lRemoveElem(cmdline, &ep);
      answer_list_add_sprintf(&answer, STATUS_ENOIMP, ANSWER_QUALITY_ERROR,
                              SFNMAX, MSG_ANSWER_HELPNOTALLOWEDINCONTEXT);
      lFreeList(&path_alias);
      DRETURN(answer);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-j"))) {
      ocs::tom::set<JB_merge_stderr>(*pjob, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-jsv"))) {
      const lList *list = ocs::tom::get<SPA_argval_lListT>(ep);
      const char *file = ocs::tom::get<PN_path>(lFirst(list));

      jsv_list_add("jsv_switch", JSV_CONTEXT_CLIENT, nullptr, file);
      lRemoveElem(cmdline, &ep);
   }

   parse_list_hardsoft(cmdline, "-l", *pjob, JRS_SCOPE_GLOBAL, JRS_hard_resource_list, JRS_soft_resource_list);
   parse_list_hardsoft(cmdline, "-l", *pjob, JRS_SCOPE_MASTER, JRS_hard_resource_list, JRS_soft_resource_list);
   parse_list_hardsoft(cmdline, "-l", *pjob, JRS_SCOPE_SLAVE, JRS_hard_resource_list, JRS_soft_resource_list);
   job_request_set_remove_duplicates(*pjob);

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-m"))) {
      uint32_t ul;
      uint32_t old_mail_opts;

      ul = ocs::tom::get<SPA_argval_lIntT>(ep);
      if  ((ul & NO_MAIL)) {
         ocs::tom::set<JB_mail_options>(*pjob, 0);
      }
      else {
         old_mail_opts = ocs::tom::get<JB_mail_options>(*pjob);
         ocs::tom::set<JB_mail_options>(*pjob, ul | old_mail_opts);
      }
      lRemoveElem(cmdline, &ep);
   }

   parse_list_simple(cmdline, "-M", *pjob, JB_mail_list, MR_host, MR_user, FLG_LIST_MERGE);

   if (!ocs::tom::get<JB_mail_list>(*pjob)) {   
      ocs::tom::Element<ocs::tom::MR> ep = ocs::tom::emplace_back_in<JB_mail_list, MR_user>(*pjob, username);
      ocs::tom::set<MR_host>(ep, qualified_hostname);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-N"))) {
      ocs::tom::set<JB_job_name>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      lRemoveElem(cmdline, &ep);
   }
   
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-now"))) {
      uint32_t jb_now = ocs::tom::get<JB_type>(*pjob);
      if(ocs::tom::get<SPA_argval_lIntT>(ep)) {
         JOB_TYPE_SET_IMMEDIATE(jb_now);
      } else {
         JOB_TYPE_CLEAR_IMMEDIATE(jb_now);
      }

      ocs::tom::set<JB_type>(*pjob, jb_now);

      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-o"))) {
      ocs::tom::set<JB_stdout_path_list>(*pjob, lCopyList("stdout_path_list", ocs::tom::get<SPA_argval_lListT>(ep))); 
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-P"))) {
      /* the old project string is overwritten */
      ocs::tom::set<JB_project>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-p"))) {
      ocs::tom::set<JB_priority>(*pjob, 
         BASE_PRIORITY + ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-par"))) {
      const char *allocation_rule = ocs::tom::get<SPA_switch_arg>(ep);
      long scope = ocs::tom::get<SPA_argval_lLongT>(ep);
      lListElem *jrs = job_get_or_create_request_setRW(*pjob, scope);
      if (jrs != nullptr) {
         ocs::tom::set<JRS_allocation_rule>(jrs, allocation_rule);
      }
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-js"))) {
      ocs::tom::set<JB_jobshare>(*pjob, ocs::tom::get<SPA_argval_lUlongT>(ep));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-pe"))) {

      ocs::tom::set<JB_pe>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));

      /* put sublist from parsing into job */
      lSwapList(*pjob, JB_pe_range, ep, SPA_argval_lListT);
      lRemoveElem(cmdline, &ep);
   }

   parse_list_hardsoft(cmdline, "-q", *pjob, JRS_SCOPE_GLOBAL, JRS_hard_queue_list, JRS_soft_queue_list);
   parse_list_hardsoft(cmdline, "-q", *pjob, JRS_SCOPE_MASTER, JRS_hard_queue_list, JRS_soft_queue_list);
   parse_list_hardsoft(cmdline, "-q", *pjob, JRS_SCOPE_SLAVE, JRS_hard_queue_list, JRS_soft_queue_list);
   parse_list_hardsoft(cmdline, "-masterq", *pjob, JRS_SCOPE_MASTER, JRS_hard_queue_list, 0);

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-R"))) {
      ocs::tom::set<JB_reserve>(*pjob, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(cmdline, &ep);
   }

   parse_list_simple(cmdline, "-S", *pjob, JB_shell_list, PN_host, PN_path, FLG_LIST_MERGE);

   /* context switches are sensitive to order */
   ep = lFirstRW(cmdline);
   while(ep) {
      if (!strcmp(ocs::tom::get<SPA_switch_val>(ep), "-ac") ||
          !strcmp(ocs::tom::get<SPA_switch_val>(ep), "-dc") ||
          !strcmp(ocs::tom::get<SPA_switch_val>(ep), "-sc")) {
         lListElem *temp;
         // the switches are concatenated in their order, as instructions for the qmaster (CS-3058)
         lList *change = nullptr;
         ocs::tom::exchange<JB_context_change>(*pjob, &change);
         ocs_context_change_add(&change, ocs::tom::get<SPA_argval_lListT>(ep));
         ocs::tom::exchange<JB_context_change>(*pjob, &change);
         temp = lNextRW(ep);
         lRemoveElem(cmdline, &ep);
         ep = temp;
      } else {
         ep = lNextRW(ep);
      }
   }

   /* not needed in job struct */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-scope"))) {
      lRemoveElem(cmdline, &ep);
   }

   /* not needed in job struct */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-soft"))) {
      lRemoveElem(cmdline, &ep);
   }

   /*
   ** to be processed in original order, set -V equal to -v
   */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-V"))) {
      ocs::tom::set<SPA_switch_val>(ep, "-v");
   }
   parse_list_simple(cmdline, "-v", *pjob, JB_env_list, VA_variable, VA_value, FLG_LIST_MERGE);

   /* -w e is default for interactive jobs */
   /* JG: changed default behaviour: skip verify to be consistant to qsub */
   ocs::tom::set<JB_verify_suitable_queues>(*pjob, SKIP_VERIFY);

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-w"))) {
      ocs::tom::set<JB_verify_suitable_queues>(*pjob, ocs::tom::get<SPA_argval_lIntT>(ep));
      lRemoveElem(cmdline, &ep);
   }

   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-wd"))) {
      const char *path = ocs::tom::get<SPA_argval_lStringT>(ep);
      bool is_cwd = false;

      if (path == nullptr) {
         char tmp_str[SGE_PATH_MAX + 1];

         is_cwd = true;
         if (!getcwd(tmp_str, sizeof(tmp_str))) {
            /* If getcwd() fails... */
            answer_list_add(&answer, MSG_ANSWER_GETCWDFAILED, 
                            STATUS_EDISK, ANSWER_QUALITY_ERROR);
            lFreeList(&path_alias);
            DRETURN(answer);
         }
         
         path = reroot_path(*pjob, tmp_str, &answer);
         
         if (path == nullptr) {
            lFreeList(&path_alias);
            DRETURN(answer);
         }
      }

      ocs::tom::set<JB_cwd>(*pjob, path);
      lRemoveElem(cmdline, &ep);
      
      ocs::tom::set<JB_path_aliases>(*pjob, lCopyList("PathAliases", path_alias));

      if (is_cwd) {
         sge_free(&path);
      }
   }

   lFreeList(&path_alias);
   
   /*
   * handling for display:
   * command line (-display) has highest priority
   * then comes environment variable DISPLAY which was given via -v
   * then comes environment variable DISPLAY read from environment
   *
   * If a value for DISPLAY is set, it is checked for completeness:
   * In a GRID environment, it has to contain the name of the display host.
   * DISPLAY variables of form ":id" are not allowed and are deleted.
   *
   * we put the display switch into the variable list
   * otherwise, we'd have to add a field to the job structure
   */
   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-display"))) {
      lList *lp = ocs::tom::get<JB_env_list>(*pjob);
      lListElem *vep = lCreateElem(VA_Type);

      ocs::tom::set<VA_variable>(vep, "DISPLAY");
      ocs::tom::set<VA_value>(vep, ocs::tom::get<SPA_argval_lStringT>(ep));
      if (!lp) {
         lp = lCreateList("env list", VA_Type);
         ocs::tom::set<JB_env_list>(*pjob, lp);
      }
      lAppendElem(lp, vep);
      
      lRemoveElem(cmdline, &ep);
   }
   cull_compress_definition_list(ocs::tom::get<JB_env_list>(*pjob), VA_variable, VA_value, 0);


   while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, "-verify"))) {
      ocs::tom::set<JB_verify>(*pjob, true);
      lRemoveElem(cmdline, &ep);
   }

   {
      lList *lp;
      
      lp = lCopyList("job args", ocs::tom::get<JB_job_args>(*pjob));

      while ((ep = ocs::tom::find_first_rw<SPA_switch_val>(cmdline, STR_PSEUDO_JOBARG))) {
         ocs::tom::emplace_back<ST_name>(&lp, ocs::tom::get<SPA_argval_lStringT>(ep));
         
         lRemoveElem(cmdline, &ep);
      }
      ocs::tom::set<JB_job_args>(*pjob, lp);
   }

   
   for_each_rw(ep, cmdline) {
      char str[1024];

      snprintf(str, sizeof(str), MSG_ANSWER_UNKNOWNOPTIONX_S, ocs::tom::get<SPA_switch_val>(ep));
      cp = ocs::tom::get<SPA_switch_arg>(ep);
      if (cp) {
         strcat(str, " ");
         strcat(str, cp);
      }
      answer_list_add(&answer, str, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   } 

   {
      lListElem *ep = ocs::tom::find_first_in_rw<JB_env_list, VA_variable>(*pjob, "DISPLAY");

      /* if DISPLAY not set from -display or -v option,
       * try to read it from the environment
       */
      if (ep == nullptr) {
         const char *display;
         display = getenv("DISPLAY");

         if(display != nullptr) {
            ocs::tom::Element<ocs::tom::VA> ep = ocs::tom::emplace_back_in<JB_env_list, VA_variable>(*pjob, "DISPLAY");
            ocs::tom::set<VA_value>(ep, display);   
         }   
      }
   }

   /* check DISPLAY on the client side before submitting job to qmaster 
    * only needed for qsh 
    */
   if(prog_number == QSH) {
      job_check_qsh_display(*pjob, &answer, false);
   }

   DRETURN(answer);
}
