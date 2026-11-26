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
#include "sgeobj/ocs_parse_util.h"
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
#include "tom/ocs_Raw.h"
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
 * The interactive counterpart of #ocs_parse_job_parameter. It differs mainly
 * in the defaults: an interactive job is not rerunnable, gets no output files,
 * and is submitted with `-now yes` unless told otherwise.
 *
 * @param prog_number which of `qsh`, `qlogin` and `qrsh` is running
 * @param uid the submitting user's uid
 * @param username the submitting user's name
 * @param cell_root the cell directory, for resolving the default files
 * @param unqualified_hostname the submit host, unqualified
 * @param qualified_hostname the submit host, fully qualified
 * @param cmdline the parsed options (`SPA`), or an empty handle for defaults only; the options used are
 *                removed, those left are reported as unknown
 * @param job the job (`JB`) to fill in
 *
 * @return `nullptr` when everything was fine, otherwise an answer list
 *         (`AN`) carrying one of
 *         - `STATUS_EUNKNOWN` - an internal error, a null pointer or out of memory
 *         - `STATUS_EDISK` - `getcwd()` failed
 *         - `STATUS_ENOIMP` - an unknown switch, or `-help`
 */
ocs::tom::ListPtr<ocs::tom::AN> ocs_parse_qsh_parameter(const uint32_t prog_number, [[maybe_unused]] uint32_t uid,
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

   job.set<JB_priority>(BASE_PRIORITY);
   job.set<JB_jobshare>(0);
   job.set<JB_verify_suitable_queues>(SKIP_VERIFY);

   /*
   ** it does not make sense to rerun interactive jobs
   ** the default is to decide via the queue configuration
   ** we turn this off here explicitly (2 is no)
   */
   job_now = job.get<JB_type>();
   if (JOB_TYPE_IS_QSH(job_now) || JOB_TYPE_IS_QLOGIN(job_now)
       || JOB_TYPE_IS_QRSH(job_now) || JOB_TYPE_IS_QRLOGIN(job_now)) {
      job.set<JB_restart>(2);
   }

   while (JOB_TYPE_IS_QRSH(job_now)
          && (ep = cmdline.find_first<SPA_switch_val>("-notify"))) {
      job.set<JB_notify>(true);
      cmdline.erase(ep);
   }  

   /* 
    * turn on immediate scheduling as default  
    * can be overwriten by option -now n 
    */
   {
      uint32_t type = job.get<JB_type>();

      JOB_TYPE_SET_IMMEDIATE(type);
      job.set<JB_type>(type);
   }

   // -btype host | slot
   while ((ep = cmdline.find_first<SPA_switch_val>("-btype"))) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(job, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      binding_elem.set<BN_new_type>(ep.get<SPA_argval_lIntT>());

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
      binding_elem.set<BN_amount>(ep.get<SPA_argval_lIntT>());

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
      binding_elem.set<BN_instance>(ep.get<SPA_argval_lIntT>());

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
      binding_elem.set<BN_unit>(ep.get<SPA_argval_lIntT>());

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
      binding_elem.set<BN_filter>(ep.get<SPA_argval_lStringT>());

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
      binding_elem.set<BN_sort>(ep.get<SPA_argval_lStringT>());

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
      binding_elem.set<BN_start>(ep.get<SPA_argval_lIntT>());

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
      binding_elem.set<BN_stop>(ep.get<SPA_argval_lIntT>());

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
      binding_elem.set<BN_strategy>(ep.get<SPA_argval_lIntT>());

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
         const char *cp_switch = prev.get<SPA_switch_val>();
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
      job.set<JB_account>(ep.get<SPA_argval_lStringT>());
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
   ** a little redesign of the list library would be nice
   ** see parse_list_simple
   */
   job_set_submit_task_ids(job, 1, 1, 1);
   job_initialize_id_lists(job, &answer);
   if (answer != nullptr) {
      DRETURN(answer);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-ar"))) {
      /* the old advance reservation is overwritten */
      job.set<JB_ar>(ep.get<SPA_argval_lUlongT>());
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-dept"))) {
      job.set<JB_department>(ep.get<SPA_argval_lStringT>());
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-e"))) {
      // the parser fills the argument of -e with a PN path list
      job.set<JB_stderr_path_list>(ep.get_as<SPA_argval_lListT, ocs::tom::PN>().clone("stderr_path_list"));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-h"))) {
      if (ep.get<SPA_argval_lIntT>() & MINUS_H_TGT_USER) {
         job.set<JB_ja_u_h_ids>(job.get<JB_ja_n_h_ids>().clone("task_id_range"));
      }
      job.set<JB_ja_n_h_ids>(nullptr);
      cmdline.erase(ep);
   }

   /* -hold_jid */
   if (cmdline.find_first<SPA_switch_val>("-hold_jid")) {
      // created with the first reference, as the free emplace_back() created it
      ocs::tom::ListPtr<ocs::tom::JRE> jref_list;
      while ((ep = cmdline.find_first<SPA_switch_val>("-hold_jid"))) {
         // the parser fills the argument with an ST name list
         for (const auto sep : ep.get_as<SPA_argval_lListT, ocs::tom::ST>()) {
            const char *name = sep.get<ST_name>();
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
      job.set<JB_jid_request_list>(std::move(jref_list));
   }

   /* -hold_jid_ad */
   if (cmdline.find_first<SPA_switch_val>("-hold_jid_ad")) {
      // created with the first reference, as the free emplace_back() created it
      ocs::tom::ListPtr<ocs::tom::JRE> jref_list;
      while ((ep = cmdline.find_first<SPA_switch_val>("-hold_jid_ad"))) {
         // the parser fills the argument with an ST name list
         for (const auto sep : ep.get_as<SPA_argval_lListT, ocs::tom::ST>()) {
            const char *name = sep.get<ST_name>();
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
      job.set<JB_ja_ad_request_list>(std::move(jref_list));
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
      job.set<JB_merge_stderr>(ep.get<SPA_argval_lIntT>());
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-jsv"))) {
      // the parser fills the argument of -jsv with a PN path list
      const auto list = ep.get_as<SPA_argval_lListT, ocs::tom::PN>();
      const char *file = list.front().get<PN_path>();

      jsv_list_add("jsv_switch", JSV_CONTEXT_CLIENT, nullptr, file);
      cmdline.erase(ep);
   }

   parse_list_hardsoft<JRS_hard_resource_list, JRS_soft_resource_list>(cmdline, "-l", job, JRS_SCOPE_GLOBAL);
   parse_list_hardsoft<JRS_hard_resource_list, JRS_soft_resource_list>(cmdline, "-l", job, JRS_SCOPE_MASTER);
   parse_list_hardsoft<JRS_hard_resource_list, JRS_soft_resource_list>(cmdline, "-l", job, JRS_SCOPE_SLAVE);
   job_request_set_remove_duplicates(job);

   while ((ep = cmdline.find_first<SPA_switch_val>("-m"))) {
      uint32_t ul;
      uint32_t old_mail_opts;

      ul = ep.get<SPA_argval_lIntT>();
      if  ((ul & NO_MAIL)) {
         job.set<JB_mail_options>(0);
      }
      else {
         old_mail_opts = job.get<JB_mail_options>();
         job.set<JB_mail_options>(ul | old_mail_opts);
      }
      cmdline.erase(ep);
   }

   ocs_parse_mail_list_merge<JB_mail_list>(cmdline, "-M", job);

   if (!job.get<JB_mail_list>()) {
      const auto mail_ep = job.emplace_back_in<JB_mail_list, MR_user>(username);
      mail_ep.set<MR_host>(qualified_hostname);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-N"))) {
      job.set<JB_job_name>(ep.get<SPA_argval_lStringT>());
      cmdline.erase(ep);
   }
   
   while ((ep = cmdline.find_first<SPA_switch_val>("-now"))) {
      uint32_t jb_now = job.get<JB_type>();
      if(ep.get<SPA_argval_lIntT>()) {
         JOB_TYPE_SET_IMMEDIATE(jb_now);
      } else {
         JOB_TYPE_CLEAR_IMMEDIATE(jb_now);
      }

      job.set<JB_type>(jb_now);

      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-o"))) {
      // the parser fills the argument of -o with a PN path list
      job.set<JB_stdout_path_list>(ep.get_as<SPA_argval_lListT, ocs::tom::PN>().clone("stdout_path_list"));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-P"))) {
      /* the old project string is overwritten */
      job.set<JB_project>(ep.get<SPA_argval_lStringT>());
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-p"))) {
      job.set<JB_priority>(
         BASE_PRIORITY + ep.get<SPA_argval_lIntT>());
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-par"))) {
      const char *allocation_rule = ep.get<SPA_switch_arg>();
      long scope = ep.get<SPA_argval_lLongT>();
      const auto jrs = job_get_or_create_request_setRW(job, scope);
      if (jrs) {
         jrs.set<JRS_allocation_rule>(allocation_rule);
      }
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-js"))) {
      job.set<JB_jobshare>(ep.get<SPA_argval_lUlongT>());
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-pe"))) {

      job.set<JB_pe>(ep.get<SPA_argval_lStringT>());

      /* put sublist from parsing into job */
      // SPA_argval_lListT carries lists of several types; at -pe the parser fills it with a slot range, so the
      // list is taken out and adopted checked as RN here; the range held before is freed by the set
      job.set<JB_pe_range>(ocs::tom::adopt<ocs::tom::RN>(
         ep.exchange<SPA_argval_lListT>(ocs::tom::ListPtr<ocs::tom::RN>{})));
      cmdline.erase(ep);
   }

   parse_list_hardsoft<JRS_hard_queue_list, JRS_soft_queue_list>(cmdline, "-q", job, JRS_SCOPE_GLOBAL);
   parse_list_hardsoft<JRS_hard_queue_list, JRS_soft_queue_list>(cmdline, "-q", job, JRS_SCOPE_MASTER);
   parse_list_hardsoft<JRS_hard_queue_list, JRS_soft_queue_list>(cmdline, "-q", job, JRS_SCOPE_SLAVE);
   parse_list_hardsoft<JRS_hard_queue_list>(cmdline, "-masterq", job, JRS_SCOPE_MASTER);

   while ((ep = cmdline.find_first<SPA_switch_val>("-R"))) {
      job.set<JB_reserve>(ep.get<SPA_argval_lIntT>());
      cmdline.erase(ep);
   }

   parse_list_merge<JB_shell_list, PN_host, PN_path>(cmdline, "-S", job);

   /* context switches are sensitive to order */
   ep = cmdline.front();
   while (ep) {
      if (!strcmp(ep.get<SPA_switch_val>(), "-ac") ||
          !strcmp(ep.get<SPA_switch_val>(), "-dc") ||
          !strcmp(ep.get<SPA_switch_val>(), "-sc")) {
         // the switches are concatenated in their order, as instructions for the qmaster (CS-3058)
         auto change = job.exchange<JB_context_change>(nullptr);
         // the parser fills the argument of the context switches with a VA list
         ocs_context_change_add(change, ep.get_as<SPA_argval_lListT, ocs::tom::VA>());
         job.set<JB_context_change>(std::move(change));
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
      ep.set<SPA_switch_val>("-v");
   }
   parse_list_merge<JB_env_list, VA_variable, VA_value>(cmdline, "-v", job);

   /* -w e is default for interactive jobs */
   /* JG: changed default behaviour: skip verify to be consistant to qsub */
   job.set<JB_verify_suitable_queues>(SKIP_VERIFY);

   while ((ep = cmdline.find_first<SPA_switch_val>("-w"))) {
      job.set<JB_verify_suitable_queues>(ep.get<SPA_argval_lIntT>());
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-wd"))) {
      const char *path = ep.get<SPA_argval_lStringT>();
      // the rerooted current working directory; reroot_path() returns strdup'd memory
      ocs::uti::c_ptr<char> rerooted_cwd;

      if (path == nullptr) {
         char tmp_str[SGE_PATH_MAX + 1];

         if (!getcwd(tmp_str, sizeof(tmp_str))) {
            /* If getcwd() fails... */
            answer_list_add(&answer, MSG_ANSWER_GETCWDFAILED,
                            STATUS_EDISK, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }
         
         rerooted_cwd.reset(reroot_path(job, tmp_str, &answer));
         path = rerooted_cwd.get();

         if (path == nullptr) {
            DRETURN(answer);
         }
      }

      job.set<JB_cwd>(path);
      cmdline.erase(ep);
      
      job.set<JB_path_aliases>(path_alias->clone("PathAliases"));
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

      vep->set<VA_variable>("DISPLAY");
      vep->set<VA_value>(ep.get<SPA_argval_lStringT>());
      job.get_or_create<JB_env_list>("env list").push_back(std::move(vep));

      cmdline.erase(ep);
   }
   job.get<JB_env_list>().keep_last<VA_variable>();


   while ((ep = cmdline.find_first<SPA_switch_val>("-verify"))) {
      job.set<JB_verify>(true);
      cmdline.erase(ep);
   }

   {
      auto lp = job.get<JB_job_args>().clone("job args");

      while ((ep = cmdline.find_first<SPA_switch_val>(STR_PSEUDO_JOBARG))) {
         // a null argument added nothing in the free emplace_back(); the list is created with the first one
         if (const char *arg = ep.get<SPA_argval_lStringT>(); arg != nullptr) {
            if (!lp) {
               lp = ocs::tom::ListPtr<ocs::tom::ST>::create("job args");
            }
            lp->emplace_back<ST_name>(arg);
         }

         cmdline.erase(ep);
      }
      job.set<JB_job_args>(std::move(lp));
   }

   
   for (const auto unknown : cmdline) {
      char str[1024];

      snprintf(str, sizeof(str), MSG_ANSWER_UNKNOWNOPTIONX_S, unknown.get<SPA_switch_val>());
      cp = unknown.get<SPA_switch_arg>();
      if (cp) {
         strcat(str, " ");
         strcat(str, cp);
      }
      answer_list_add(&answer, str, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   } 

   /* if DISPLAY not set from -display or -v option,
    * try to read it from the environment
    */
   if (!job.find_first_in<JB_env_list, VA_variable>("DISPLAY")) {
      const char *display = getenv("DISPLAY");

      if (display != nullptr) {
         const auto display_ep = job.emplace_back_in<JB_env_list, VA_variable>("DISPLAY");
         display_ep.set<VA_value>(display);
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
