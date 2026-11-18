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
 * @brief Turns the parsed command line of a submit client into a job (`JB_Type`) object
 */
#include <cstring>
#include <climits>
#include <cctype>
#include <unistd.h>
#include <cstdlib>
#include <cerrno>

#include "uti/sge_io.h"
#include "uti/sge_language.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"

#include "sgeobj/ocs_Binding.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_var.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_JRS.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PA.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_VA.h"

#include "symbols.h"
#include "parse_qsub.h"
#include "parse_job_cull.h"

#include "msg_common.h"

/** @brief Build this file for the submit clients rather than for `qsh`
 *
 * The parser is shared, but `qsh` accepts a different set of options; this
 * selects the `qsub` set.
 */
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

/** @brief Fill in a job object from a parsed command line
 *
 * The second half of submitting: #cull_parse_cmdline turns the argument vector
 * into a list of options, and this turns that list into the job. Called with
 * `cmdline` as an empty handle it produces a job carrying nothing but the defaults,
 * which is how a template job is made.
 *
 * @param uid the submitting user's uid
 * @param username the submitting user's name
 * @param cell_root the cell directory, for resolving the default files
 * @param unqualified_hostname the submit host, unqualified
 * @param qualified_hostname the submit host, fully qualified
 * @param cmdline the parsed options, or an empty handle for defaults only; the options taken over are removed
 * @param pjob the job to fill in; created if the handle is empty
 * @param sync_options receives the `-sync` bits
 *
 * @return `nullptr` when everything was fine, otherwise an answer list
 *         (`AN_Type`) carrying one of
 *         - `STATUS_EUNKNOWN` - an internal error, a null pointer or out of memory
 *         - `STATUS_EDISK` - `getcwd()` failed
 *         - `STATUS_ENOIMP` - an unknown switch, or `-help`
 */
ocs::tom::ListPtr<ocs::tom::AN> cull_parse_job_parameter([[maybe_unused]] const uint32_t uid, const char *username,
                                                         const char *cell_root, const char *unqualified_hostname,
                                                         const char *qualified_hostname,
                                                         const ocs::tom::List<ocs::tom::SPA> cmdline,
                                                         ocs::tom::ElementPtr<ocs::tom::JB> &pjob,
                                                         uint32_t *sync_options) {
   DENTER(TOP_LAYER); 

   const char *cp;
   ocs::tom::Element<ocs::tom::SPA> ep;
   ocs::tom::ListPtr<ocs::tom::AN> answer;
   char error_string[MAX_STRING_SIZE];

   if (sync_options == nullptr) {
      answer_list_add(&answer,  MSG_PARSE_NULLPOINTERRECEIVED, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   if (!pjob) {
      // create() throws on a failed allocation where lCreateElem() returned nullptr
      pjob = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   }

   // -sync options
   *sync_options = SYNC_NO;
   while ((ep = cmdline.find_first<SPA_switch_val>("-sync"))) {
      *sync_options = ocs::tom::get<SPA_argval_lUlongT>(ep);
      cmdline.erase(ep);
   }
   job_set_sync_options(*pjob, *sync_options);

   /*
   ** path aliasing
   */
   ocs::tom::ListPtr<ocs::tom::PA> path_alias;
   const int path_alias_ret = path_alias_list_initialize(path_alias, &answer, cell_root, username,
                                                         qualified_hostname);
   if (path_alias_ret == -1) {
      DRETURN(answer);
   }

   job_initialize_env(*pjob, &answer, *path_alias, unqualified_hostname, qualified_hostname);
   if (answer) {
      DRETURN(answer);
   }

   ocs::tom::set<JB_priority>(*pjob, BASE_PRIORITY);

   ocs::tom::set<JB_jobshare>(*pjob, 0);

   /*
    * -b
    */
   while ((ep = cmdline.find_first<SPA_switch_val>("-b"))) {
      uint32_t jb_now = ocs::tom::get<JB_type>(*pjob);

      if (ocs::tom::get<SPA_argval_lIntT>(ep) == 1) {
         JOB_TYPE_SET_BINARY(jb_now);
      } else {
         JOB_TYPE_UNSET_BINARY(jb_now);
      }
      
      ocs::tom::set<JB_type>(*pjob, jb_now);
      cmdline.erase(ep);
   }
   
   // -btype host | slot
   ep = cmdline.find_first<SPA_switch_val>("-btype");
   if (ep != nullptr) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_new_type>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bamount number
   ep = cmdline.find_first<SPA_switch_val>("-bamount");
   if (ep != nullptr) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_amount>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -binstance set | env | pe
   ep = cmdline.find_first<SPA_switch_val>("-binstance");
   if (ep != nullptr) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_instance>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bunit binding_unit
   ep = cmdline.find_first<SPA_switch_val>("-bunit");
   if (ep != nullptr) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_unit>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bfilter topo_string
   ep = cmdline.find_first<SPA_switch_val>("-bfilter");
   if (ep != nullptr) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_filter>(binding_elem, ocs::tom::get<SPA_argval_lStringT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bsort sort_string
   ep = cmdline.find_first<SPA_switch_val>("-bsort");
   if (ep != nullptr) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_sort>(binding_elem, ocs::tom::get<SPA_argval_lStringT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bstart
   ep = cmdline.find_first<SPA_switch_val>("-bstart");
   if (ep != nullptr) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_start>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bstop
   ep = cmdline.find_first<SPA_switch_val>("-bstop");
   if (ep != nullptr) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_stop>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   // -bstrategy
   ep = cmdline.find_first<SPA_switch_val>("-bstrategy");
   if (ep != nullptr) {
      // Create a binding element if it does not exist
      const auto binding_elem = ocs::Job::binding_get_or_create_elem(*pjob, &answer);
      if (!binding_elem) {
         DRETURN(answer);
      }

      // Transfer the binding information from the command line to the job
      ocs::tom::set<BN_strategy>(binding_elem, ocs::tom::get<SPA_argval_lIntT>(ep));

      // Get rid of the binding element from the command line
      cmdline.erase(ep);
   }

   /*
    * -shell
    */
   while ((ep = cmdline.find_first<SPA_switch_val>("-shell"))) {
      uint32_t jb_now = ocs::tom::get<JB_type>(*pjob);

      if (ocs::tom::get<SPA_argval_lIntT>(ep) == 1) {
         JOB_TYPE_UNSET_NO_SHELL(jb_now);
      }
      else {
         JOB_TYPE_SET_NO_SHELL(jb_now);
      }
      
      ocs::tom::set<JB_type>(*pjob, jb_now);
      cmdline.erase(ep);
   }

   /*
    * -t
    */
   ep = cmdline.find_first<SPA_switch_val>("-t");
   if (ep != nullptr) {
      /* the -t argument is a range list (RN_Type), parsed by cull_parse_cmdline() or drmaa_set_bulk_range() */
      const auto range_list = ocs::tom::ConstList<ocs::tom::RN>::from_typed(ocs::tom::get<SPA_argval_lListT>(ep));
      ocs::tom::set<JB_ja_structure>(*pjob, range_list.clone("task_id_range"));
      cmdline.erase(ep);
   
      {
         uint32_t job_type = ocs::tom::get<JB_type>(*pjob);
         JOB_TYPE_SET_ARRAY(job_type);
         ocs::tom::set<JB_type>(*pjob, job_type);
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
   while ((ep = cmdline.find_first<SPA_switch_val>("-tc"))) {
      ocs::tom::set<JB_ja_task_concurrency>(*pjob, ocs::tom::get<SPA_argval_lUlongT>(ep));
      cmdline.erase(ep);
   }

   /*
   ** -clear option is special, is sensitive to order
   ** kills all options that come before
   ** there might be more than one -clear
   */
   while ((ep = cmdline.find_first<SPA_switch_val>("-clear"))) {
      const char *cp_switch;

      for (auto ep_run = cmdline.front(); ep_run;) {
         /*
         ** remove -clear itsself
         */
         if (ep_run == ep) {
            cmdline.erase(ep_run);
            break;
         }
         /*
         ** lNext can never be nullptr here, because the -clear
         ** element is the last one to delete
         ** in general, these two lines wont work!
         */
         ep_run = ep_run.next();
         
         /*
         ** remove switch only if it is not a pseudo-arg
         */
         cp_switch = ocs::tom::get<SPA_switch_val>(ep_run.prev());
         if (cp_switch && (*cp_switch == '-')) {
            cmdline.erase(ep_run.prev());
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

   while ((ep = cmdline.find_first<SPA_switch_val>("-a"))) {
      ocs::tom::set<JB_execution_time>(*pjob, ocs::tom::get<SPA_argval_lUlong64T>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-A"))) {
      /* the old account string is overwritten */
      ocs::tom::set<JB_account>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-ar"))) {
      /* the old advance reservation is overwritten */
      ocs::tom::set<JB_ar>(*pjob, ocs::tom::get<SPA_argval_lUlongT>(ep));
      cmdline.erase(ep);
   }
   
   while ((ep = cmdline.find_first<SPA_switch_val>("-dl"))) {
      ocs::tom::set<JB_deadline>(*pjob, ocs::tom::get<SPA_argval_lUlong64T>(ep));
      cmdline.erase(ep);
   }
   
   while ((ep = cmdline.find_first<SPA_switch_val>("-c"))) {
      if (ocs::tom::get<SPA_argtype>(ep) == lLongT)
         ocs::tom::set<JB_checkpoint_interval>(*pjob, ocs::tom::get<SPA_argval_lLongT>(ep));
      if (ocs::tom::get<SPA_argtype>(ep) == lIntT) 
         ocs::tom::set<JB_checkpoint_attr>(*pjob, ocs::tom::get<SPA_argval_lIntT>(ep));

      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-ckpt"))) {
      ocs::tom::set<JB_checkpoint_name>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-C"))) {
      ocs::tom::set<JB_directive_prefix>(*pjob, 
         ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-dept"))) {
      ocs::tom::set<JB_department>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-jsv"))) {
      /* the -jsv argument is a path list (PN_Type) built by cull_parse_cmdline() */
      const auto list = ocs::tom::ConstList<ocs::tom::PN>::from_typed(ocs::tom::get<SPA_argval_lListT>(ep));
      const char *file = ocs::tom::get<PN_path>(list.front());

      jsv_list_add("jsv_switch", JSV_CONTEXT_CLIENT, nullptr, file);
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
   parse_list_simple(cmdline, "-e", *pjob, JB_stderr_path_list, PN_host, PN_path, FLG_LIST_MERGE);

   /* -h */
   while ((ep = cmdline.find_first<SPA_switch_val>("-h"))) {
      if (ocs::tom::get<SPA_argval_lIntT>(ep) & MINUS_H_TGT_USER) {
         ocs::tom::set<JB_ja_u_h_ids>(*pjob, ocs::tom::get<JB_ja_n_h_ids>(*pjob).clone("task_id_range"));
         ocs::tom::set<JB_ja_n_h_ids>(*pjob, nullptr);
      }
      cmdline.erase(ep);
   }

   /* not needed in job struct */
   while ((ep = cmdline.find_first<SPA_switch_val>("-hard"))) {
      cmdline.erase(ep);
   }

   if ((ep = cmdline.find_first<SPA_switch_val>("-help"))) {
      cmdline.erase(ep);
      answer_list_add_sprintf(&answer, STATUS_ENOIMP, ANSWER_QUALITY_ERROR,
                              MSG_ANSWER_HELPNOTALLOWEDINCONTEXT);
      DRETURN(answer);
   }

   /* -hold_jid */
   if (cmdline.find_first<SPA_switch_val>("-hold_jid")) {
      ocs::tom::ListPtr<ocs::tom::JRE> jref_list;
      while ((ep = cmdline.find_first<SPA_switch_val>("-hold_jid"))) {
         /* the argument is a job name list (ST_Type) built by cull_parse_jid_hold_list() */
         for (const auto sep : ocs::tom::ConstList<ocs::tom::ST>::from_typed(ocs::tom::get<SPA_argval_lListT>(ep))) {
            DPRINTF("-hold_jid %s\n", ocs::tom::get<ST_name>(sep));
            if (!jref_list) {
               // created as the free emplace_back() creates a missing list
               jref_list = ocs::tom::ListPtr<ocs::tom::JRE>::create("");
            }
            jref_list->emplace_back<JRE_job_name>(ocs::tom::get<ST_name>(sep));
         }
         cmdline.erase(ep);
      }
      ocs::tom::set<JB_jid_request_list>(*pjob, std::move(jref_list));
   }

   /* -hold_jid_ad */
   if (cmdline.find_first<SPA_switch_val>("-hold_jid_ad")) {
      ocs::tom::ListPtr<ocs::tom::JRE> jref_list;
      while ((ep = cmdline.find_first<SPA_switch_val>("-hold_jid_ad"))) {
         /* the argument is a job name list (ST_Type) built by cull_parse_jid_hold_list() */
         for (const auto sep : ocs::tom::ConstList<ocs::tom::ST>::from_typed(ocs::tom::get<SPA_argval_lListT>(ep))) {
            DPRINTF("-hold_jid_ad %s\n", ocs::tom::get<ST_name>(sep));
            if (!jref_list) {
               // created as the free emplace_back() creates a missing list
               jref_list = ocs::tom::ListPtr<ocs::tom::JRE>::create("");
            }
            jref_list->emplace_back<JRE_job_name>(ocs::tom::get<ST_name>(sep));
         }
         cmdline.erase(ep);
      }
      ocs::tom::set<JB_ja_ad_request_list>(*pjob, std::move(jref_list));
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-j"))) {
      ocs::tom::set<JB_merge_stderr>(*pjob, ocs::tom::get<SPA_argval_lIntT>(ep));
      cmdline.erase(ep);
   }

   parse_list_simple(cmdline, "-jid", *pjob, JB_job_identifier_list, 0, 0, FLG_LIST_APPEND);

   parse_list_hardsoft(cmdline, "-l", *pjob, JRS_SCOPE_GLOBAL, JRS_hard_resource_list, JRS_soft_resource_list);
   parse_list_hardsoft(cmdline, "-l", *pjob, JRS_SCOPE_MASTER, JRS_hard_resource_list, JRS_soft_resource_list);
   parse_list_hardsoft(cmdline, "-l", *pjob, JRS_SCOPE_SLAVE, JRS_hard_resource_list, JRS_soft_resource_list);
   job_request_set_remove_duplicates(*pjob);

   while ((ep = cmdline.find_first<SPA_switch_val>("-m"))) {
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
      cmdline.erase(ep);
   }

   parse_list_simple(cmdline, "-M", *pjob, JB_mail_list, MR_host, MR_user, FLG_LIST_MERGE);

#ifndef USE_CLIENT_QALTER
   if (!ocs::tom::get<JB_mail_list>(*pjob)) {   
      ocs::tom::Element<ocs::tom::MR> ep = ocs::tom::emplace_back_in<JB_mail_list, MR_user>(*pjob, username);
      ocs::tom::set<MR_host>(ep, qualified_hostname);
   }
#endif

   while ((ep = cmdline.find_first<SPA_switch_val>("-N"))) {
      ocs::tom::set<JB_job_name>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }
   
   while ((ep = cmdline.find_first<SPA_switch_val>("-notify"))) {
      ocs::tom::set<JB_notify>(*pjob, true);
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-now"))) {
      uint32_t jb_now = ocs::tom::get<JB_type>(*pjob);
      if(ocs::tom::get<SPA_argval_lIntT>(ep)) {
         JOB_TYPE_SET_IMMEDIATE(jb_now);
      } else {
         JOB_TYPE_CLEAR_IMMEDIATE(jb_now);
      }

      ocs::tom::set<JB_type>(*pjob, jb_now);

      cmdline.erase(ep);
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

   while ((ep = cmdline.find_first<SPA_switch_val>("-p"))) {
      int pri = ocs::tom::get<SPA_argval_lIntT>(ep);
      ocs::tom::set<JB_priority>(*pjob, BASE_PRIORITY + pri);
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-js"))) {
      ocs::tom::set<JB_jobshare>(*pjob, ocs::tom::get<SPA_argval_lUlongT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-P"))) {
      ocs::tom::set<JB_project>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-par"))) {
      const char *allocation_rule = ocs::tom::get<SPA_switch_arg>(ep);
      uint32_t scope = ocs::tom::get<SPA_argval_lLongT>(ep);
      const auto jrs = job_get_or_create_request_setRW(*pjob, scope);
      if (jrs) {
         ocs::tom::set<JRS_allocation_rule>(jrs, allocation_rule);
      }
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-pe"))) {
      ocs::tom::set<JB_pe>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      // SPA_argval_lListT carries lists of several types; for -pe it is the range list (RN_Type) built by
      // cull_parse_cmdline(), so it is taken over unchecked; the old range list is freed as before with the option
      lList *pe_range = nullptr;
      ocs::tom::exchange<SPA_argval_lListT>(ep, &pe_range);
      ocs::tom::set<JB_pe_range>(*pjob, ocs::tom::ListPtr<ocs::tom::RN>::from_typed(pe_range));
      cmdline.erase(ep);
   }

   parse_list_hardsoft(cmdline, "-q", *pjob, JRS_SCOPE_GLOBAL, JRS_hard_queue_list, JRS_soft_queue_list);
   parse_list_hardsoft(cmdline, "-q", *pjob, JRS_SCOPE_MASTER, JRS_hard_queue_list, JRS_soft_queue_list);
   parse_list_hardsoft(cmdline, "-q", *pjob, JRS_SCOPE_SLAVE, JRS_hard_queue_list, JRS_soft_queue_list);
   parse_list_hardsoft(cmdline, "-masterq", *pjob, JRS_SCOPE_MASTER, JRS_hard_queue_list, 0);
   
   while ((ep = cmdline.find_first<SPA_switch_val>("-R"))) {
      ocs::tom::set<JB_reserve>(*pjob, ocs::tom::get<SPA_argval_lIntT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-pty"))) {
      ocs::tom::set<JB_pty>(*pjob, ocs::tom::get<SPA_argval_lIntT>(ep));
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-r"))) {
      ocs::tom::set<JB_restart>(*pjob, ocs::tom::get<SPA_argval_lIntT>(ep));
      cmdline.erase(ep);
   }

   /* not needed in job struct */
   while ((ep = cmdline.find_first<SPA_switch_val>("-scope"))) {
      cmdline.erase(ep);
   }

   /* not needed in job struct */
   while ((ep = cmdline.find_first<SPA_switch_val>("-soft"))) {
      cmdline.erase(ep);
   }

   parse_list_simple(cmdline, "-S", *pjob, JB_shell_list, PN_host, PN_path, FLG_LIST_MERGE);

   /* -terse option, not needed in job struct */
   while ((ep = cmdline.find_first<SPA_switch_val>("-terse"))) {
      cmdline.erase(ep);
   }

   parse_list_simple(cmdline, "-u", *pjob, JB_user_list, 0, 0, FLG_LIST_APPEND);

   /*
   ** to be processed in original order, set -V equal to -v
   */
   while ((ep = cmdline.find_first<SPA_switch_val>("-V"))) {
      ocs::tom::set<SPA_switch_val>(ep, "-v");
   }
   parse_list_simple(cmdline, "-v", *pjob, JB_env_list, VA_variable, VA_value, FLG_LIST_MERGE);
   cull_compress_definition_list(ocs::tom::get<JB_env_list>(*pjob), VA_variable, VA_value, 0);

   /* context switches are sensitive to order */
   ep = cmdline.front();
   while (ep) {
      if (!strcmp(ocs::tom::get<SPA_switch_val>(ep), "-ac") ||
          !strcmp(ocs::tom::get<SPA_switch_val>(ep), "-dc") ||
          !strcmp(ocs::tom::get<SPA_switch_val>(ep), "-sc")) {
         // the switches are concatenated in their order, as instructions for the qmaster (CS-3058)
         // ocs_context_change_add() takes the change list raw (no typed form)
         lList *change = nullptr;
         ocs::tom::exchange<JB_context_change>(*pjob, &change);
         // the argument of -ac/-dc/-sc is a variable list (VA_Type) built by cull_parse_cmdline()
         ocs_context_change_add(&change,
                                ocs::tom::ConstList<ocs::tom::VA>::from_typed(ocs::tom::get<SPA_argval_lListT>(ep)));
         ocs::tom::exchange<JB_context_change>(*pjob, &change);
         const auto temp = ep.next();
         cmdline.erase(ep);
         ep = temp;
      } else {
         ep = ep.next();
      }
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-verify"))) {

      ocs::tom::set<JB_verify>(*pjob, true);
      cmdline.erase(ep);
   }

   while ((ep = cmdline.find_first<SPA_switch_val>("-w"))) {
      ocs::tom::set<JB_verify_suitable_queues>(*pjob, ocs::tom::get<SPA_argval_lIntT>(ep));
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
         
         path = reroot_path(*pjob, tmp_str, &answer);
         
         if (path == nullptr) {
            DRETURN(answer);
         }
      }

      ocs::tom::set<JB_cwd>(*pjob, path);
      cmdline.erase(ep);
      
      ocs::tom::set<JB_path_aliases>(*pjob, path_alias->clone("PathAliases"));

      if (is_cwd) {
         sge_free(&path);
      }
   }

   /*
   ** no switch - must be scriptfile
   ** only for qalter and qsub, not for qsh
   */
   if ((ep = cmdline.find_first<SPA_switch_val>(STR_PSEUDO_SCRIPT))) {
      ocs::tom::set<JB_script_file>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }

   if ((ep = cmdline.find_first<SPA_switch_val>(STR_PSEUDO_SCRIPTLEN))) {
      ocs::tom::set<JB_script_size>(*pjob, ocs::tom::get<SPA_argval_lUlongT>(ep));
      cmdline.erase(ep);
   }

   if ((ep = cmdline.find_first<SPA_switch_val>(STR_PSEUDO_SCRIPTPTR))) {
      ocs::tom::set<JB_script_ptr>(*pjob, ocs::tom::get<SPA_argval_lStringT>(ep));
      cmdline.erase(ep);
   }

   {
      auto job_args = ocs::tom::get<JB_job_args>(*pjob).clone("job args");

      while ((ep = cmdline.find_first<SPA_switch_val>(STR_PSEUDO_JOBARG))) {
         // a null argument adds nothing, as the free emplace_back() did
         if (const char *job_arg = ocs::tom::get<SPA_argval_lStringT>(ep); job_arg != nullptr) {
            if (!job_args) {
               // created as the free emplace_back() creates a missing list
               job_args = ocs::tom::ListPtr<ocs::tom::ST>::create("");
            }
            job_args->emplace_back<ST_name>(job_arg);
         }

         cmdline.erase(ep);
      }
      ocs::tom::set<JB_job_args>(*pjob, std::move(job_args));
   }

   // if there are any elements left in cmdline it is unknown options
   // which we didn't handle above
   for (const auto unknown : cmdline) {
      snprintf(error_string, sizeof(error_string), MSG_ANSWER_UNKNOWNOPTIONX_S,
               ocs::tom::get<SPA_switch_val>(unknown));
      cp = ocs::tom::get<SPA_switch_arg>(unknown);
      if (cp) {
         strcat(error_string, " ");
         strcat(error_string, cp);
      }
      strcat(error_string, "\n");
      answer_list_add(&answer, error_string, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   } 

   cp = ocs::tom::get<JB_script_file>(*pjob);
   
   if (!cp || !strcmp(cp, "-")) {
      uint32_t jb_now = ocs::tom::get<JB_type>(*pjob);
      if( JOB_TYPE_IS_BINARY(jb_now) ) {
         answer_list_add(&answer, MSG_COMMAND_REQUIRED_FOR_BINARY_JOB,
                         STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         DRETURN(answer);
      }
      ocs::tom::set<JB_script_file>(*pjob, "STDIN");
   }
   cp = ocs::tom::get<JB_job_name>(*pjob);
   if (!cp) {
      cp = sge_basename(ocs::tom::get<JB_script_file>(*pjob), '/');
      ocs::tom::set<JB_job_name>(*pjob,  cp);
   }

   DRETURN(answer);
}

/**
 * @brief cull_parse_job_parameter() for an untyped caller
 *
 * @param uid the submitting user's uid
 * @param username the submitting user's name
 * @param cell_root the cell directory, for resolving the default files
 * @param unqualified_hostname the submit host, unqualified
 * @param qualified_hostname the submit host, fully qualified
 * @param cmdline the parsed options (`SPA_Type`), or `nullptr` for defaults only
 * @param pjob the job (`JB_Type`) to fill in; created if `*pjob` is nullptr
 * @param sync_options receives the `-sync` bits
 *
 * @return `nullptr` when everything was fine, otherwise an answer list, see the typed form
 */
ocs::tom::ListPtr<ocs::tom::AN> cull_parse_job_parameter(const uint32_t uid, const char *username,
                                                         const char *cell_root, const char *unqualified_hostname,
                                                         const char *qualified_hostname, lList *cmdline,
                                                         lListElem **pjob, uint32_t *sync_options) {
   if (pjob == nullptr) {
      ocs::tom::ListPtr<ocs::tom::AN> answer;
      answer_list_add(&answer, MSG_PARSE_NULLPOINTERRECEIVED, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      return answer;
   }
   // the option list is built by the option parsers with SPA_Type, the job by the callers with JB_Type
   auto job = ocs::tom::ElementPtr<ocs::tom::JB>::from_typed(*pjob);
   auto answer = cull_parse_job_parameter(uid, username, cell_root, unqualified_hostname, qualified_hostname,
                                            ocs::tom::List<ocs::tom::SPA>::from_typed(cmdline), job, sync_options);
   *pjob = job.release();
   return answer;
}

/**
 * @brief Parse a job script or a job defaults file
 *
 * Searches for special comments in script files and parses contained
 * SGE options or
 * parses SGE options in job defaults files (sge_request).
 * Script files are parsed with directive prefix nullptr,
 * default files are parsed with directive prefix "" and FLG_USE_NO_PSEUSOS.
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param script_file script file name or nullptr or "-" in the latter two cases the job script is read from stdin
 * @param directive_prefix only lines beginning with this prefix are parsed; nullptr causes the function to look
 *        in the lpp_options list whether the -C option has been set. If it has, this prefix is used. If not,
 *        the default prefix "#$" is used. "" causes the function to parse all lines not starting with "#"
 *        (comment lines).
 * @param lpp_options the option list to store the recognized switches in, created if it is empty but there are
 *        options to be returned
 * @param envp environment pointer
 * @param flags FLG_HIGHER_PRIOR: new options are appended to list; FLG_LOWER_PRIOR: new options are inserted at
 *        the beginning of list; FLG_USE_NO_PSEUDOS: do not create pseudoargs for script pointer and length;
 *        FLG_IGN_NO_FILE: do not show an error if script_file was not found
 *
 * @return answer list, AN_Type, or nullptr if everything was ok, the following stati can occur:
 *         STATUS_EUNKNOWN - bad internal error like nullptr pointer received or no memory;
 *         STATUS_EDISK - file could not be opened
 *
 * @note Special comments in script files have to start in the first column of a
 *       line.
 *       Comments in job defaults files have to start in the first column of a
 *       line.
 *       If a line is longer than MAX_STRING_SIZE bytes, contents (SGE options)
 *       starting from position MAX_STRING_SIZE + 1 are silently ignored.
 *       MAX_STRING_SIZE is defined in common/basis_types.h (current value 2048).
 *
 *       MT-NOTE: parse_script_file() is MT safe
 *
 * @see #centry_list_parse_from_string
 */
ocs::tom::ListPtr<ocs::tom::AN> parse_script_file(const uint32_t prog_number, const char *script_file,
                                                  const char *directive_prefix,
                                                  ocs::tom::ListPtr<ocs::tom::SPA> &lpp_options, char **envp,
                                                  const uint32_t flags) {
   DENTER(TOP_LAYER);

   unsigned int dpl; /* directive_prefix length */
   FILE *fp;
   char *filestrptr = nullptr;
   int script_len = 0;
   char **str_table = nullptr;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ListPtr<ocs::tom::AN> answer;
   int i;
   int do_exit = 0;
   ocs::tom::Element<ocs::tom::SPA> ep_opt;
   ocs::tom::ListPtr<ocs::tom::SPA> lp_new_opts;
   /* snprintf takes the nullptr terminator into account. */
   char error_string[MAX_STRING_SIZE];

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
         DPRINTF("directive prefix = <null> - will skip parsing of script\n");
         dpl = 0;
      } else {
         DPRINTF("directive prefix = " SFQ "\n", directive_prefix);
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
               DPRINTF("parameter in script: %s\n", parameters);

               /*
               ** here cull comes in 
               */

               /* so str_table has to be freed afterwards */
               str_table = string_list(parameters, " \t\n", nullptr);
               
               for (i=0; str_table[i]; i++) {
                  DPRINTF("str_table[%d] = '%s'\n", i, str_table[i]);
               }

               sge_strip_quotes(str_table);

               for (i=0; str_table[i]; i++) {
                  DPRINTF("str_table[%d] = '%s'\n", i, str_table[i]);
               }

               /*
               ** problem: error handling missing here and above
               */
               alp = cull_parse_cmdline(prog_number, (const char **)str_table, envp, lp_new_opts, 0);

               for_each_rw_lv (aep, alp) {
                  answer_quality_t quality;
                  uint32_t status = STATUS_OK;

                  status = ocs::tom::get<AN_status>(aep);
                  quality = (answer_quality_t)ocs::tom::get<AN_quality>(aep);

                  if (quality == ANSWER_QUALITY_ERROR) {
                     DPRINTF("%s", ocs::tom::get<AN_text>(aep));
                     do_exit = 1;
                  }
                  else {
                     DPRINTF("Warning: %s\n", ocs::tom::get<AN_text>(aep));
                  }
                  answer_list_add(&answer, ocs::tom::get<AN_text>(aep), status,
                                  quality);
               }

               sge_free(&str_table);
               alp.reset();
               sge_free(&free_me);
               parameters = nullptr;
               
               if (do_exit) {
                  sge_free(&filestrptr);
                  DRETURN(answer);
               }

               /*
               ** -C option is ignored in scriptfile - delete all occurences
               */
               for (auto ep = lp_new_opts->find_first<SPA_switch_val>("-C"); ep;
                    ep = lp_new_opts->find_first<SPA_switch_val>("-C")) {
                  lp_new_opts->erase(ep);
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
         if (!lpp_options->find_first<SPA_switch_val>(STR_PSEUDO_SCRIPT)) {
            ep_opt = sge_add_arg(lp_new_opts, 0, lStringT, STR_PSEUDO_SCRIPT, nullptr);
            ocs::tom::set<SPA_argval_lStringT>(ep_opt, 
               ((!script_file || !strcmp(script_file, "-")) ? "STDIN" : script_file));
         }
      }
      ep_opt = sge_add_arg(lp_new_opts, 0, lUlongT, STR_PSEUDO_SCRIPTLEN, nullptr);
      ocs::tom::set<SPA_argval_lUlongT>(ep_opt, script_len);

      ep_opt = sge_add_arg(lp_new_opts, 0, lStringT, STR_PSEUDO_SCRIPTPTR, nullptr);
      ocs::tom::set<SPA_argval_lStringT>(ep_opt, filestrptr);
   }

   if (!lp_new_opts) {
      sge_free(&filestrptr);
      DRETURN(answer);
   }

   if (!lpp_options) {
      lpp_options = std::move(lp_new_opts);
   } else {
      if (flags & FLG_LOWER_PRIOR) {
         lp_new_opts->splice(std::move(lpp_options));
         lpp_options = std::move(lp_new_opts);
      } else {
         lpp_options->splice(std::move(lp_new_opts));
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

/**
 * @brief parse_script_file() for an untyped caller
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param script_file the script or defaults file, see the typed form
 * @param directive_prefix the directive prefix, see the typed form
 * @param lpp_options the option list (`SPA_Type`) to store the recognized switches in; `*lpp_options` is
 *        created if it is nullptr but there are options to be returned
 * @param envp environment pointer
 * @param flags see the typed form
 *
 * @return answer list, AN_Type, or nullptr if everything was ok, see the typed form
 */
ocs::tom::ListPtr<ocs::tom::AN> parse_script_file(const uint32_t prog_number, const char *script_file,
                                                  const char *directive_prefix,
                                                  lList **lpp_options, char **envp, const uint32_t flags) {
   if (lpp_options == nullptr) {
      /* no place where to put result */
      ocs::tom::ListPtr<ocs::tom::AN> answer;
      answer_list_add(&answer, MSG_ANSWER_CANTPROCESSNULLLIST, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      return answer;
   }
   // the callers hand in their option list, which is created by the option parsers with SPA_Type
   auto options = ocs::tom::ListPtr<ocs::tom::SPA>::from_typed(*lpp_options);
   auto answer = parse_script_file(prog_number, script_file, directive_prefix, options, envp, flags);
   *lpp_options = options.release();
   return answer;
}

