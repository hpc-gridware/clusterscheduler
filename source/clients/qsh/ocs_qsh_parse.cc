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
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_PA.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
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
 * @param cmdline the parsed options (`SPA_Type`), or an empty handle for defaults only; the options used are
 *                removed, those left are reported as unknown
 * @param job the job (`JB_Type`) to fill in
 *
 * @return `nullptr` when everything was fine, otherwise an answer list
 *         (`AN_Type`) carrying one of
 *         - `STATUS_EUNKNOWN` - an internal error, a null pointer or out of memory
 *         - `STATUS_EDISK` - `getcwd()` failed
 *         - `STATUS_ENOIMP` - an unknown switch, or `-help`
 */
ocs::tom::ListPtr<ocs::tom::AN> cull_parse_qsh_parameter(const uint32_t prog_number, [[maybe_unused]] uint32_t uid,
                                                         const char *username,
                                                         const char *cell_root, const char *unqualified_hostname,
                                                         const char *qualified_hostname,
                                                         const ocs::tom::List<ocs::tom::SPA> cmdline,
                                                         const ocs::tom::Element<ocs::tom::JB> job) {
   DENTER(TOP_LAYER);

   const char *cp;
   ocs::tom::Element<ocs::tom::SPA> ep;
   ocs::tom::ListPtr<ocs::tom::AN> answer;
   ocs::tom::ListPtr<ocs::tom::PA> path_alias;
   uint32_t job_now;

   if (!job) {
      answer_list_add(&answer, MSG_PARSE_NULLPOINTERRECEIVED, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   /*
   ** path aliasing
   */
   if (path_alias_list_initialize(path_alias, &answer, cell_root, username, qualified_hostname) == -1) {
      DRETURN(answer);
   }

   job_initialize_env(job, &answer, *path_alias, unqualified_hostname, qualified_hostname);
   if (answer) {
      DRETURN(answer);
   }

   ocs::tom::set<JB_priority>(job, BASE_PRIORITY);
   ocs::tom::set<JB_jobshare>(job, 0);
   ocs::tom::set<JB_verify_suitable_queues>(job, SKIP_VERIFY);

   /*
   ** it does not make sense to rerun interactive jobs
   ** the default is to decide via the queue configuration
   ** we turn this off here explicitly (2 is no)
   */
   job_now = ocs::tom::get<JB_type>(job);
   if (JOB_TYPE_IS_QSH(job_now) || JOB_TYPE_IS_QLOGIN(job_now)
       || JOB_TYPE_IS_QRSH(job_now) || JOB_TYPE_IS_QRLOGIN(job_now)) {
      ocs::tom::set<JB_restart>(job, 2);
   }

   while (JOB_TYPE_IS_QRSH(job_now)
          && (ep = cmdline.find_first<SPA_switch_val>("-notify"))) {
      ocs::tom::set<JB_notify>(job, true);
      cmdline.erase(ep);
   }  

   /* 
    * turn on immediate scheduling as default  
    * can be overwriten by option -now n 
    */
   {
      uint32_t type = ocs::tom::get<JB_type>(job);

      JOB_TYPE_SET_IMMEDIATE(type);
      ocs::tom::set<JB_type>(job, type);
   }

   // -btype host | slot
   while ((ep = cmdline.find_first<SPA_switch_val>("-btype"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_new_type>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bamount number
   while ((ep = cmdline.find_first<SPA_switch_val>("-bamount"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_amount>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -binstance set | env | pe
   while ((ep = cmdline.find_first<SPA_switch_val>("-binstance"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_instance>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bunit binding_unit
   while ((ep = cmdline.find_first<SPA_switch_val>("-bunit"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_unit>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bfilter topo_string
   while ((ep = cmdline.find_first<SPA_switch_val>("-bfilter"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_filter>(binding_elem, ocs::tom::get<SPA_argval_lStringT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bsort sort_string
   while ((ep = cmdline.find_first<SPA_switch_val>("-bsort"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_sort>(binding_elem, ocs::tom::get<SPA_argval_lStringT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bstart
   while ((ep = cmdline.find_first<SPA_switch_val>("-bstart"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_start>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bstop
   while ((ep = cmdline.find_first<SPA_switch_val>("-bstop"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_stop>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bstrategy
   while ((ep = cmdline.find_first<SPA_switch_val>("-bstrategy"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_strategy>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   /*
   ** -clear option is special, is sensitive to order
   ** kills all options that come before
   ** there might be more than one -clear
   */
   while ((ep = cmdline.find_first<SPA_switch_val>("-clear"))) {
      for (auto ep_run = cmdline.front(); ep_run;) {
         /*
         ** remove -clear itsself
         */
         if (ep_run == ep) {
            cmdline.erase(ep_run);
            break;
         }
         /*
         ** next() can never be empty here, because the -clear
         ** element is the last one to delete
         ** in general, these two lines wont work!
         */
         ep_run = ep_run.next();

         /*
         ** remove switch only if it is not a pseudo-arg
         */
         const auto prev = ep_run.prev();
         const char *cp_switch = ocs::tom::get<SPA_switch_val>(prev);
         if (cp_switch && (*cp_switch == '-')) {
            cmdline.erase(prev);
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

   while ((ep = cmdline.find_first<SPA_switch_val>("-A"))) {
      /* the old account string is overwritten */
      ocs::tom::set<JB_account>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
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
   job_set_submit_task_ids(job, 1, 1, 1);
   job_initialize_id_lists(job, &answer);
   if (answer != nullptr) {
      DRETURN(answer);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-ar"))) {
      /* the old advance reservation is overwritten */
      ocs::tom::set<JB_ar>(job, ocs::tom::get<SPA_argval_lUlongT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-dept"))) {
      ocs::tom::set<JB_department>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-e"))) {
      // the parser fills the argument of -e with a PN_Type path list
      ocs::tom::set<JB_stderr_path_list>(job, ocs::tom::ConstList<ocs::tom::PN>::from_typed(
                                                 ocs::tom::get<SPA_argval_lListT>(ep)).clone("stderr_path_list"));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-h"))) {
      if (ocs::tom::get<SPA_argval_lIntT>(ep) & MINUS_H_TGT_USER) {
         ocs::tom::set<JB_ja_u_h_ids>(job, ocs::tom::get<JB_ja_n_h_ids>(job).clone("task_id_range"));
      }
      ocs::tom::set<JB_ja_n_h_ids>(job, nullptr);
      cmdline.erase(ep);
   }

   /* -hold_jid */
   if (cmdline.find_first<SPA_switch_val>("-hold_jid")) {
      // created with the first reference, as the free emplace_back() created it
      ocs::tom::ListPtr<ocs::tom::JRE> jref_list;
      while ((ep = cmdline.find_first<SPA_switch_val>("-hold_jid"))) {
         // the parser fills the argument with an ST_Type name list
         for (const auto sep : ocs::tom::ConstList<ocs::tom::ST>::from_typed(ocs::tom::get<SPA_argval_lListT>(ep))) {
            const char *name = ocs::tom::get<ST_name>(sep);
            DPRINTF("-hold_jid %s\n", name);
            // a null name added nothing in the free emplace_back()
            if (name == nullptr) {
               continue;
            }
            if (!jref_list) {
               jref_list = ocs::tom::ListPtr<ocs::tom::JRE>::create("job reference list");
            }
            jref_list->emplace_back<JRE_job_name>(name);
         }
         cmdline.erase(ep);
      }
      ocs::tom::set<JB_jid_request_list>(job, std::move(jref_list));
   }

   /* -hold_jid_ad */
   if (cmdline.find_first<SPA_switch_val>("-hold_jid_ad")) {
      // created with the first reference, as the free emplace_back() created it
      ocs::tom::ListPtr<ocs::tom::JRE> jref_list;
      while ((ep = cmdline.find_first<SPA_switch_val>("-hold_jid_ad"))) {
         // the parser fills the argument with an ST_Type name list
         for (const auto sep : ocs::tom::ConstList<ocs::tom::ST>::from_typed(ocs::tom::get<SPA_argval_lListT>(ep))) {
            const char *name = ocs::tom::get<ST_name>(sep);
            DPRINTF("-hold_jid_ad %s\n", name);
            // a null name added nothing in the free emplace_back()
            if (name == nullptr) {
               continue;
            }
            if (!jref_list) {
               jref_list = ocs::tom::ListPtr<ocs::tom::JRE>::create("job reference list");
            }
            jref_list->emplace_back<JRE_job_name>(name);
         }
         cmdline.erase(ep);
      }
      ocs::tom::set<JB_ja_ad_request_list>(job, std::move(jref_list));
   }

   /* not needed in job struct */
   while ((ep = cmdline.find_first<SPA_switch_val>("-hard"))) {
      cmdline.erase(ep);
   }

   if ((ep = cmdline.find_first<SPA_switch_val>("-help"))) {
      cmdline.erase(ep);
      answer_list_add_sprintf(&answer, STATUS_ENOIMP, ANSWER_QUALITY_ERROR,
                              SFNMAX, MSG_ANSWER_HELPNOTALLOWEDINCONTEXT);
      DRETURN(answer);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-j"))) {
      ocs::tom::set<JB_merge_stderr>(job, ocs::tom::get<SPA_argval_lIntT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-jsv"))) {
      // the parser fills the argument of -jsv with a PN_Type path list
      const auto list = ocs::tom::ConstList<ocs::tom::PN>::from_typed(ocs::tom::get<SPA_argval_lListT>(ep));
      const char *file = ocs::tom::get<PN_path>(list.front());

      jsv_list_add("jsv_switch", JSV_CONTEXT_CLIENT, nullptr, file);
      cmdline.erase(ep);
   }

   parse_list_hardsoft(cmdline, "-l", job, JRS_SCOPE_GLOBAL, JRS_hard_resource_list, JRS_soft_resource_list);
   parse_list_hardsoft(cmdline, "-l", job, JRS_SCOPE_MASTER, JRS_hard_resource_list, JRS_soft_resource_list);
   parse_list_hardsoft(cmdline, "-l", job, JRS_SCOPE_SLAVE, JRS_hard_resource_list, JRS_soft_resource_list);
   job_request_set_remove_duplicates(job);

   while ((ep = cmdline.find_first<SPA_switch_val>("-m"))) {
      uint32_t ul;
      uint32_t old_mail_opts;

      ul = ocs::tom::get<SPA_argval_lIntT>(ep);
      if  ((ul & NO_MAIL)) {
         ocs::tom::set<JB_mail_options>(job, 0);
      }
      else {
         old_mail_opts = ocs::tom::get<JB_mail_options>(job);
         ocs::tom::set<JB_mail_options>(job, ul | old_mail_opts);
      }
      cmdline.erase(ep);
   }

   parse_list_merge<JB_mail_list, MR_host, MR_user>(cmdline, "-M", job);

   if (!ocs::tom::get<JB_mail_list>(job)) {
      const auto mail_ep = ocs::tom::emplace_back_in<JB_mail_list, MR_user>(job, username);
      ocs::tom::set<MR_host>(mail_ep, qualified_hostname);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-N"))) {
      ocs::tom::set<JB_job_name>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }
   
   while ((ep = cmdline.find_first<SPA_switch_val>("-now"))) {
      uint32_t jb_now = ocs::tom::get<JB_type>(job);
      if(ocs::tom::get<SPA_argval_lIntT>(ep)) {
         JOB_TYPE_SET_IMMEDIATE(jb_now);
      } else {
         JOB_TYPE_CLEAR_IMMEDIATE(jb_now);
      }

      ocs::tom::set<JB_type>(job, jb_now);

      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-o"))) {
      // the parser fills the argument of -o with a PN_Type path list
      ocs::tom::set<JB_stdout_path_list>(job, ocs::tom::ConstList<ocs::tom::PN>::from_typed(
                                                 ocs::tom::get<SPA_argval_lListT>(ep)).clone("stdout_path_list"));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-P"))) {
      /* the old project string is overwritten */
      ocs::tom::set<JB_project>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-p"))) {
      ocs::tom::set<JB_priority>(job, 
         BASE_PRIORITY + ocs::tom::get<SPA_argval_lIntT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-par"))) {
      const char *allocation_rule = ocs::tom::get<SPA_switch_arg>(ep);
      long scope = ocs::tom::get<SPA_argval_lLongT>(ep);
      const auto jrs = job_get_or_create_request_setRW(job, scope);
      if (jrs) {
         ocs::tom::set<JRS_allocation_rule>(jrs, allocation_rule);
      }
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-js"))) {
      ocs::tom::set<JB_jobshare>(job, ocs::tom::get<SPA_argval_lUlongT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-pe"))) {

      ocs::tom::set<JB_pe>(job, ocs::tom::get<SPA_argval_lStringT>(ep));

      /* put sublist from parsing into job */
      // SPA_argval_lListT carries lists of several types and has no typed swap
      lSwapList(job, JB_pe_range, ep, SPA_argval_lListT);
      cmdline.erase(ep);
   }

   parse_list_hardsoft(cmdline, "-q", job, JRS_SCOPE_GLOBAL, JRS_hard_queue_list, JRS_soft_queue_list);
   parse_list_hardsoft(cmdline, "-q", job, JRS_SCOPE_MASTER, JRS_hard_queue_list, JRS_soft_queue_list);
   parse_list_hardsoft(cmdline, "-q", job, JRS_SCOPE_SLAVE, JRS_hard_queue_list, JRS_soft_queue_list);
   parse_list_hardsoft(cmdline, "-masterq", job, JRS_SCOPE_MASTER, JRS_hard_queue_list, 0);

   while ((ep = cmdline.find_first<SPA_switch_val>("-R"))) {
      ocs::tom::set<JB_reserve>(job, ocs::tom::get<SPA_argval_lIntT>(ep));
      cmdline.erase(ep);
   }

   parse_list_merge<JB_shell_list, PN_host, PN_path>(cmdline, "-S", job);

   /* context switches are sensitive to order */
   ep = cmdline.front();
   while (ep) {
      if (!strcmp(ocs::tom::get<SPA_switch_val>(ep), "-ac") ||
          !strcmp(ocs::tom::get<SPA_switch_val>(ep), "-dc") ||
          !strcmp(ocs::tom::get<SPA_switch_val>(ep), "-sc")) {
         // the switches are concatenated in their order, as instructions for the qmaster (CS-3058);
         // ocs_context_change_add() has no typed form for the change list yet
         lList *change = nullptr;
         ocs::tom::exchange<JB_context_change>(job, &change);
         // the parser fills the argument of the context switches with a VA_Type list
         ocs_context_change_add(&change, ocs::tom::ConstList<ocs::tom::VA>::from_typed(
                                            ocs::tom::get<SPA_argval_lListT>(ep)));
         ocs::tom::exchange<JB_context_change>(job, &change);
         const auto temp = ep.next();
         cmdline.erase(ep);
         ep = temp;
      } else {
         ep = ep.next();
      }
   }

   /* not needed in job struct */
   while ((ep = cmdline.find_first<SPA_switch_val>("-scope"))) {
      cmdline.erase(ep);
   }

   /* not needed in job struct */
   while ((ep = cmdline.find_first<SPA_switch_val>("-soft"))) {
      cmdline.erase(ep);
   }

   /*
   ** to be processed in original order, set -V equal to -v
   */
   while ((ep = cmdline.find_first<SPA_switch_val>("-V"))) {
      ocs::tom::set<SPA_switch_val>(ep, "-v");
   }
   parse_list_merge<JB_env_list, VA_variable, VA_value>(cmdline, "-v", job);

   /* -w e is default for interactive jobs */
   /* JG: changed default behaviour: skip verify to be consistant to qsub */
   ocs::tom::set<JB_verify_suitable_queues>(job, SKIP_VERIFY);

   while ((ep = cmdline.find_first<SPA_switch_val>("-w"))) {
      ocs::tom::set<JB_verify_suitable_queues>(job, ocs::tom::get<SPA_argval_lIntT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-wd"))) {
      const char *path = ocs::tom::get<SPA_argval_lStringT>(ep);
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
         
         path = reroot_path(job, tmp_str, &answer);

         if (path == nullptr) {
            DRETURN(answer);
         }
      }

      ocs::tom::set<JB_cwd>(job, path);
      cmdline.erase(ep);
      
      ocs::tom::set<JB_path_aliases>(job, path_alias->clone("PathAliases"));

      if (is_cwd) {
         sge_free(&path);
      }
   }

   path_alias.reset();

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
   while ((ep = cmdline.find_first<SPA_switch_val>("-display"))) {
      auto vep = ocs::tom::ElementPtr<ocs::tom::VA>::create();

      ocs::tom::set<VA_variable>(vep, "DISPLAY");
      ocs::tom::set<VA_value>(vep, ocs::tom::get<SPA_argval_lStringT>(ep));
      job.get_or_create<JB_env_list>("env list").push_back(std::move(vep));

      cmdline.erase(ep);
   }
   ocs::tom::keep_last<VA_variable>(ocs::tom::get<JB_env_list>(job));


   while ((ep = cmdline.find_first<SPA_switch_val>("-verify"))) {
      ocs::tom::set<JB_verify>(job, true);
      cmdline.erase(ep);
   }

   {
      auto lp = ocs::tom::get<JB_job_args>(job).clone("job args");

      while ((ep = cmdline.find_first<SPA_switch_val>(STR_PSEUDO_JOBARG))) {
         // a null argument added nothing in the free emplace_back(); the list is created with the first one
         if (const char *arg = ocs::tom::get<SPA_argval_lStringT>(ep); arg != nullptr) {
            if (!lp) {
               lp = ocs::tom::ListPtr<ocs::tom::ST>::create("job args");
            }
            lp->emplace_back<ST_name>(arg);
         }

         cmdline.erase(ep);
      }
      ocs::tom::set<JB_job_args>(job, std::move(lp));
   }

   
   for (const auto unknown : cmdline) {
      char str[1024];

      snprintf(str, sizeof(str), MSG_ANSWER_UNKNOWNOPTIONX_S, ocs::tom::get<SPA_switch_val>(unknown));
      cp = ocs::tom::get<SPA_switch_arg>(unknown);
      if (cp) {
         strcat(str, " ");
         strcat(str, cp);
      }
      answer_list_add(&answer, str, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   } 

   /* if DISPLAY not set from -display or -v option,
    * try to read it from the environment
    */
   if (!ocs::tom::find_first_in<JB_env_list, VA_variable>(job, "DISPLAY")) {
      const char *display = getenv("DISPLAY");

      if (display != nullptr) {
         const auto display_ep = ocs::tom::emplace_back_in<JB_env_list, VA_variable>(job, "DISPLAY");
         ocs::tom::set<VA_value>(display_ep, display);
      }
   }

   /* check DISPLAY on the client side before submitting job to qmaster 
    * only needed for qsh 
    */
   if(prog_number == QSH) {
      job_check_qsh_display(job, &answer, false);
   }

   DRETURN(answer);
}
