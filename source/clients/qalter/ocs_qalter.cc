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
 * @brief qalter/qhold/qrls/qresub - changes the attributes of a job that has already been submitted
 */
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <sstream>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"
#include "cull/ocs_cull_descr.h"

#include "sgeobj/ocs_Binding.h"
#include "sgeobj/ocs_Job.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/parse.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_answer.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_JRE.h"
#include "sgeobj/tom/ocs_tom_SME.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_VA.h"

#include "gdi/ocs_gdi_Client.h"

#include "sig_handlers.h"
#include "usage.h"
#include "parse_qsub.h"
#include "symbols.h"
#include "ocs_client_parse.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qalter.h"
#include "procedure/qstat/job/ocs_QStatJobViewPlain.h"


/* when this character is modified, it has also be modified
   the JOB_NAME_DEL in daemons/qmaster/sge_job_qmaster.c 
   It is used, when a job name is modified via job name to
   differ betwen the new job name and the old one.
   */
static const char *JOB_NAME_DEL = ":";

static ocs::tom::ListPtr<ocs::tom::AN> qalter_parse_job_parameter(uint32_t prog_number,
                                                                  ocs::tom::List<ocs::tom::SPA> cmdline,
                                                                  ocs::tom::ListPtr<ocs::tom::JB> &requestlist,
                                                                  int *all_jobs, int *all_users);

/** @brief `-w v` was given: report what would happen and change nothing
 *
 * A global because the parsing helpers have to see it without it being threaded
 * through every one of them.
 */
int verify = 0;

/** @brief The process environment, as the C library defines it
 *
 * Declared here rather than included, and passed to the command line parser so
 * that options which read an environment variable resolve it against the
 * submitting user's environment rather than against `getenv()`.
 */
extern char **environ;

/**
 * @brief Modify, hold, release or resubmit jobs: qalter, qhold, qrls and qresub
 *
 * The program name decides which of the four clients runs.
 *
 * @param argc number of command line arguments
 * @param argv the command line arguments
 *
 * @return the exit status of the client
 */
int main(int argc, const char **argv) {
   DENTER_MAIN(TOP_LAYER, "qalter");
   int ret = STATUS_OK;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ListPtr<ocs::tom::JB> request_list;
   ocs::tom::ListPtr<ocs::tom::SPA> cmdline;
   int all_jobs = 0;
   int all_users = 0;
   ocs::gdi::Command gdi_cmd = ocs::gdi::Command::MOD;
   ocs::gdi::SubCommand sub_cmd = ocs::gdi::SubCommand::NONE;
   int tmp_ret;
   ProgName me_who;

   /*
   ** get command name: qalter or qresub
   */
   if (!strcmp(sge_basename(argv[0], '/'), "qresub")) {
      DPRINTF("QRESUB\n");
      me_who = QRESUB;
   } else if (!strcmp(sge_basename(argv[0], '/'), "qhold")) {
      DPRINTF("QHOLD\n");
      me_who = QHOLD;
   } else if (!strcmp(sge_basename(argv[0], '/'), "qrls")) {
      DPRINTF("QRLS\n");
      me_who = QRLS;
   } else {
      DPRINTF("QALTER\n");
      me_who = QALTER;
   } 

   sge_setup_sig_handlers(me_who);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(me_who, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   /*
   ** begin to work
   */
   opt_list_append_opts_from_qalter_cmdline(me_who, cmdline, &alp, argv + 1, environ);
   tmp_ret = answer_list_print_err_warn(&alp, MSG_QALTER, MSG_QALTER, MSG_QALTERWARNING);
   
   if (tmp_ret > 0) {
      sge_exit(tmp_ret);
   }
   
   /* handling the case that no command line parameter was specified */
   if ((me_who == QHOLD || me_who == QRLS) && cmdline->size() == 1) {
      /* -h option is set implicitly for QHOLD and QRLS */
      sge_usage(me_who, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      sge_exit(1);
   } else if ((me_who == QRESUB || me_who == QALTER) && cmdline->size() == 0) {
      /* qresub and qalter have nothing set */ 
      sge_usage(me_who, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      sge_exit(1);
   } else if (opt_list_has_X(*cmdline, "-help")) {
      /* -help was specified */
      sge_usage(me_who, stdout);
      sge_exit(0);
   }
   
   alp = qalter_parse_job_parameter(me_who, *cmdline, request_list, &all_jobs, &all_users);

   // print() throws on an empty handle where lWriteListTo() printed nothing
   if (sge_getenv("SGE_DEBUG_DUMP_JOB") != nullptr && request_list) {
      request_list->print(stdout);
   }

   DPRINTF("all_jobs = %d, all_user = %d\n", all_jobs, all_users);

   // qalter -verify should enable following section
   //
   // @todo This does not work for all switches, e.g. -w p -verify prints job_name without a job name
   if (request_list && verify) {
      /*
         got a request list containing one element
         for each job to be modified
         save jobid all fields contain the same fields
         so we may use show_job() with the first job
         in our list
         The jobid's in our request list get printed before
         show_job()
      */
      std::stringstream ss;
      ocs::QStatJobViewPlain::cull_show_job(ss, ocs::tom::ConstList<ocs::tom::SME>(), request_list->front(),
                                            FLG_QALTER);
      printf("%s", ss.str().c_str());
      sge_exit(0);
   }

   tmp_ret = answer_list_print_err_warn(&alp, nullptr, nullptr, MSG_WARNING);
   if (tmp_ret > 0) {
      sge_exit(tmp_ret);
   }

   if ((me_who == QALTER) ||
       (me_who == QHOLD) ||
       (me_who == QRLS) 
      ) {
      DPRINTF("QALTER\n");
      gdi_cmd = ocs::gdi::Command::MOD;
   } else if (me_who == QRESUB){
      DPRINTF("QRESUB\n");
      gdi_cmd = ocs::gdi::Command::COPY;
   } else {
      printf("unknown binary name.\n");
      sge_exit(1);
   }

   if (all_jobs)
      sub_cmd = ocs::gdi::SubCommand::ALL_JOBS;
   if (all_users)
      sub_cmd = ocs::gdi::SubCommand::ALL_USERS;

   // the request list is replaced by the response, if any
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::JB_LIST, gdi_cmd, sub_cmd, request_list);
   for_each_ep_lv(aep, alp) {
      printf("%s\n", ocs::tom::get<AN_text>(aep));
      if (ret == STATUS_OK) {
         ret = ocs::tom::get<AN_status>(aep);
      }
   }
   
   /* this is to get the correct exec state */ 
   if (ret == STATUS_OK) {
      ret = 0;
   } else {
      if (me_who == QALTER) {
         if (ret != STATUS_NOTOK_DOAGAIN) {
            ret = 1;
         }
      }
   }

   sge_exit(ret);

   DRETURN(0);
}

/**
 * @brief Turn the parsed command line of qalter, qhold, qrls or qresub into job modify requests
 *
 * First all options are parsed into a dummy job, then a request is built for
 * each job id of the command line from the options of the dummy job and added
 * to the request list.
 *
 * @param me_who the program, QALTER, QHOLD, QRLS or QRESUB
 * @param cmdline the parsed command line (SPA_Type), or an empty handle for the defaults; the options used
 *                are removed
 * @param requestlist receives the job modify requests for ocs::gdi::Client::sge_gdi (reduced JB_Type),
 *                    created with the first request
 * @param all_jobs set to 1 if the requests address all jobs, 0 if they name jobs
 * @param all_users not used
 * @return nullptr on success, else an answer list (AN_Type) with STATUS_EUNKNOWN for
 *         an internal error, STATUS_EDISK if getcwd() failed or STATUS_ENOIMP for an
 *         unknown switch or -help
 */
static ocs::tom::ListPtr<ocs::tom::AN> qalter_parse_job_parameter(const uint32_t me_who,
                                                                  const ocs::tom::List<ocs::tom::SPA> cmdline,
                                                                  ocs::tom::ListPtr<ocs::tom::JB> &requestlist,
                                                                  int *all_jobs, [[maybe_unused]] int *all_users) {
   DENTER(TOP_LAYER);

   ocs::tom::Element<ocs::tom::SPA> ep;
   uint32_t jobid;
   int i;
   lEnumeration *what = nullptr;
   const lDescr *rdp = nullptr;
   ocs::tom::ListPtr<ocs::tom::AN> answer;
   enum {NOTINIT, JOB, ALL} all_or_jidlist = NOTINIT;
   int users_flag = 0;

   int job_field[100];
   bool is_hold_option = false;

   /*
      STEP 1:
         parse all options into a complete job structure 
         and build up an array of job structure names
         that are selected for modification by the options 
   */
   /* we need this job to parse our options in */
   const auto job_owner = ocs::tom::ElementPtr<ocs::tom::JB>::create();
   const auto job = job_owner.get();   // freed with job_owner on every return
   if (job == nullptr) {
      answer_list_add_sprintf(&answer, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                              MSG_MEM_MEMORYALLOCFAILED_S, __func__);
      DRETURN(answer);
   }

   /* initialize job field set */
   job_field[0] = NoName;
   nm_set(job_field, JB_job_number);
   nm_set(job_field, JB_job_name);

   /* don't do verification of schedulability per default */
   ocs::tom::set<JB_verify_suitable_queues>(job, SKIP_VERIFY);
   nm_set(job_field, JB_verify_suitable_queues);


   /*
   ** -help now handled separately in main()
   ** you dont want the user to be able to make qmon do funny things, do you?
   ** (e.g. by putting -help in a script file)
   */
   if ((ep = cmdline.find_first<SPA_switch_val>("-help"))) {
      cmdline.erase(ep);
      answer_list_add_sprintf(&answer, STATUS_ENOIMP, ANSWER_QUALITY_ERROR,
                              SFNMAX, MSG_ANSWER_HELPNOTALLOWEDINCONTEXT);
      DRETURN(answer);
   }

   /* ---------------------------------------------------------- */

   if (me_who != QRESUB) {
      while ((ep = cmdline.find_first<SPA_switch_val>("-a"))) {
         ocs::tom::set<JB_execution_time>(job, ocs::tom::get<SPA_argval_lUlong64T>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_execution_time);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-A"))) {
         ocs::tom::set<JB_account>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_account);
      }
      
      while ((ep = cmdline.find_first<SPA_switch_val>("-ar"))) {
         ocs::tom::set<JB_ar>(job, ocs::tom::get<SPA_argval_lUlongT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_ar);
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
         nm_set(job_field, JB_binding);
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
         nm_set(job_field, JB_binding);
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
         nm_set(job_field, JB_binding);
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
         nm_set(job_field, JB_binding);
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
         nm_set(job_field, JB_binding);
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
         nm_set(job_field, JB_binding);
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
         nm_set(job_field, JB_binding);
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
         nm_set(job_field, JB_binding);
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
         nm_set(job_field, JB_binding);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-cwd"))) {
         char tmp_str[SGE_PATH_MAX + 1];
         char tmp_str2[SGE_PATH_MAX + 1];
         char tmp_str3[SGE_PATH_MAX + 1];
         const char *sge_o_home = job_get_env_string(job, VAR_PREFIX "O_HOME");

         if (!getcwd(tmp_str, sizeof(tmp_str))) {
            answer_list_add(&answer, MSG_ANSWER_GETCWDFAILED,
                            STATUS_EDISK, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }

         if (sge_o_home && chdir(sge_o_home) == 0) {
            if (getcwd(tmp_str2, sizeof(tmp_str2)) == nullptr) {
               answer_list_add(&answer, MSG_ANSWER_GETCWDFAILED,
                               STATUS_EDISK, ANSWER_QUALITY_ERROR);
               DRETURN(answer);
            }

            if (chdir(tmp_str) != 0) {
               dstring ds = DSTRING_INIT;
               answer_list_add_sprintf(&answer, STATUS_EDISK, ANSWER_QUALITY_ERROR,
                                       MSG_FILE_CHDIR_SS, tmp_str, sge_strerror(errno, &ds));
               sge_dstring_free(&ds);
               DRETURN(answer);
            }

            if (!strncmp(tmp_str2, tmp_str, strlen(tmp_str2))) {
               snprintf(tmp_str3, sizeof(tmp_str3), "%s%s", sge_o_home, (char *) tmp_str + strlen(tmp_str2));
               strcpy(tmp_str, tmp_str3);
            }
         }
         ocs::tom::set<JB_cwd>(job, tmp_str);
         cmdline.erase(ep);
         nm_set(job_field, JB_cwd);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-c"))) {
         if (ocs::tom::get<SPA_argtype>(ep) == lLongT) {
            ocs::tom::set<JB_checkpoint_interval>(job, 
                        ocs::tom::get<SPA_argval_lLongT>(ep));
            nm_set(job_field, JB_checkpoint_interval);
         }
         if (ocs::tom::get<SPA_argtype>(ep) == lIntT) { 
            ocs::tom::set<JB_checkpoint_attr>(job, ocs::tom::get<SPA_argval_lIntT>(ep));
            nm_set(job_field, JB_checkpoint_attr);
         }
         cmdline.erase(ep);
      }


      while ((ep = cmdline.find_first<SPA_switch_val>("-ckpt"))) {
         ocs::tom::set<JB_checkpoint_name>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_checkpoint_name);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-dept"))) {
         ocs::tom::set<JB_department>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_department);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-dl"))) {
         ocs::tom::set<JB_deadline>(job, ocs::tom::get<SPA_argval_lUlong64T>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_deadline);
      }

      parse_list_simple(cmdline, "-e", job, JB_stderr_path_list, 0, 0, 
                        FLG_LIST_APPEND);
      if (ocs::tom::get<JB_stderr_path_list>(job))
         nm_set(job_field, JB_stderr_path_list);
      
      parse_list_simple(cmdline, "-i", job, JB_stdin_path_list, 0, 0, 
                        FLG_LIST_APPEND);
      if (ocs::tom::get<JB_stdin_path_list>(job))
         nm_set(job_field, JB_stdin_path_list);
   }

   /* STR_PSEUDO_JOBID */
   if (cmdline.find_first<SPA_switch_val>(STR_PSEUDO_JOBID)) {
      // parse_multi_jobtaskslist() has no typed form for the destination list yet
      lList *jids_out = nullptr;
      const bool parsed = parse_multi_jobtaskslist(cmdline, STR_PSEUDO_JOBID, &answer, &jids_out, true, 0);
      /* parse_multi_jobtaskslist() creates the list with ID_Type, so it is taken over unchecked */
      auto jid_list = ocs::tom::ListPtr<ocs::tom::ID>::from_typed(jids_out);
      if (!parsed) {
         DRETURN(answer);
      }                                                 
      ocs::tom::set<JB_job_identifier_list>(job, std::move(jid_list));
   } 

   if (me_who != QRESUB) {
      while ((ep = cmdline.find_first<SPA_switch_val>("-u"))) {
         auto jid_list = ocs::tom::ListPtr<ocs::tom::ID>::create("");
         jid_list->emplace_back<ID_str>("*");
         ocs::tom::set<JB_job_identifier_list>(job, std::move(jid_list));

         // SPA_argval_lListT carries lists of several types and has no typed exchange; the parser fills the
         // argument of -u with an ST_Type user list
         lList *lp = nullptr;
         ocs::tom::exchange<SPA_argval_lListT>(ep, &lp);
         ocs::tom::set<JB_user_list>(job, ocs::tom::ListPtr<ocs::tom::ST>::from_typed(lp));
         cmdline.erase(ep);
         nm_set(job_field, JB_user_list);
         users_flag = 1;
      }
 
   }                    
   
   while ((ep = cmdline.find_first<SPA_switch_val>("-h"))) {
      is_hold_option = true;

      for (ocs::tom::Element<ocs::tom::ID> jid : ocs::tom::get<JB_job_identifier_list>(job)) {
         ocs::tom::set<ID_force>(jid, (uint32_t) ocs::tom::get<SPA_argval_lIntT>(ep));
      }
      cmdline.erase(ep);
      nm_set(job_field, JB_ja_tasks);
      nm_set(job_field, JB_ja_structure);
   }

   if (me_who != QRESUB) {
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
                  jref_list = ocs::tom::ListPtr<ocs::tom::JRE>::create("");
               }
               jref_list->emplace_back<JRE_job_name>(name);
            }
            cmdline.erase(ep);
         }
         ocs::tom::set<JB_jid_request_list>(job, std::move(jref_list));
         nm_set(job_field, JB_jid_request_list );
         nm_set(job_field, JB_jid_predecessor_list);
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
                  jref_list = ocs::tom::ListPtr<ocs::tom::JRE>::create("");
               }
               jref_list->emplace_back<JRE_job_name>(name);
            }
            cmdline.erase(ep);
         }
         ocs::tom::set<JB_ja_ad_request_list>(job, std::move(jref_list));
         nm_set(job_field, JB_ja_ad_request_list );
         nm_set(job_field, JB_ja_ad_predecessor_list);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-R"))) {
         ocs::tom::set<JB_reserve>(job, ocs::tom::get<SPA_argval_lIntT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_reserve);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-j"))) {
         ocs::tom::set<JB_merge_stderr>(job, ocs::tom::get<SPA_argval_lIntT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_merge_stderr);
      }

      parse_list_hardsoft(cmdline, "-l", job, JRS_SCOPE_GLOBAL, JRS_hard_resource_list, JRS_soft_resource_list);
      parse_list_hardsoft(cmdline, "-l", job, JRS_SCOPE_MASTER, JRS_hard_resource_list, JRS_soft_resource_list);
      parse_list_hardsoft(cmdline, "-l", job, JRS_SCOPE_SLAVE, JRS_hard_resource_list, JRS_soft_resource_list);

      if (job_request_set_remove_duplicates(job)) {
         nm_set(job_field, JB_request_set_list);
      }

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
         nm_set(job_field, JB_mail_options);
      }

      parse_list_simple(cmdline, "-M", job, JB_mail_list, MR_host, MR_user, FLG_LIST_MERGE);
      if (ocs::tom::get<JB_mail_list>(job))
         nm_set(job_field, JB_mail_list);

      while ((ep = cmdline.find_first<SPA_switch_val>("-N"))) {
         ocs::tom::set<JB_job_name>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_job_name); 
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-notify"))) {
         ocs::tom::set<JB_notify>(job, true);
         cmdline.erase(ep);
         nm_set(job_field, JB_notify);
      }

      parse_list_simple(cmdline, "-o", job, JB_stdout_path_list, 0, 0, 
                        FLG_LIST_APPEND);
      if (ocs::tom::get<JB_stdout_path_list>(job))
         nm_set(job_field, JB_stdout_path_list);

      while ((ep = cmdline.find_first<SPA_switch_val>("-p"))) {
         ocs::tom::set<JB_priority>(job, 
            BASE_PRIORITY + ocs::tom::get<SPA_argval_lIntT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_priority);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-js"))) {
         ocs::tom::set<JB_jobshare>(job, ocs::tom::get<SPA_argval_lUlongT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_jobshare);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-P"))) {
         ocs::tom::set<JB_project>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_project);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-par"))) {
         const char *allocation_rule = ocs::tom::get<SPA_switch_arg>(ep);
         uint32_t scope = ocs::tom::get<SPA_argval_lLongT>(ep);
         const auto jrs = job_get_or_create_request_setRW(job, scope);
         if (jrs) {
            ocs::tom::set<JRS_allocation_rule>(jrs, allocation_rule);
         }
         cmdline.erase(ep);
         nm_set(job_field, JB_request_set_list);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-pe"))) {
         ocs::tom::set<JB_pe>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
         /* put sublist from parsing into job */
         // SPA_argval_lListT carries lists of several types and has no typed swap
         lSwapList(job, JB_pe_range, ep, SPA_argval_lListT);
         cmdline.erase(ep);

         /* cannot address fields separately in command line but in gdi interface */ 
         nm_set(job_field, JB_pe);
         nm_set(job_field, JB_pe_range);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-ot"))) {
         ocs::tom::set<JB_override_tickets>(job, ocs::tom::get<SPA_argval_lUlongT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_override_tickets);
      }

      parse_list_hardsoft(cmdline, "-q", job, JRS_SCOPE_GLOBAL, JRS_hard_queue_list, JRS_soft_queue_list);
      parse_list_hardsoft(cmdline, "-q", job, JRS_SCOPE_MASTER, JRS_hard_queue_list, JRS_soft_queue_list);
      parse_list_hardsoft(cmdline, "-q", job, JRS_SCOPE_SLAVE, JRS_hard_queue_list, JRS_soft_queue_list);
      parse_list_hardsoft(cmdline, "-masterq", job, JRS_SCOPE_MASTER, JRS_hard_queue_list, 0);
      if (job_request_set_has_queue_requests(job)) {
         nm_set(job_field, JB_request_set_list);
      }

      if (job_get_master_hard_queue_list(job) != nullptr)
         nm_set(job_field, JB_request_set_list);
    
      while ((ep = cmdline.find_first<SPA_switch_val>("-r"))) {
         ocs::tom::set<JB_restart>(job, ocs::tom::get<SPA_argval_lIntT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_restart);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-w"))) {
         ocs::tom::set<JB_verify_suitable_queues>(job, ocs::tom::get<SPA_argval_lIntT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_verify_suitable_queues);
      }

      /* not needed in job struct - they are still used at this point */
      while ((ep = cmdline.find_first<SPA_switch_val>("-soft"))) {
         cmdline.erase(ep);
      }
      while ((ep = cmdline.find_first<SPA_switch_val>("-hard"))) {
         cmdline.erase(ep);
      }
      while ((ep = cmdline.find_first<SPA_switch_val>("-scope"))) {
         cmdline.erase(ep);
      }

      parse_list_simple(cmdline, "-S", job, JB_shell_list, 0, 0, FLG_LIST_APPEND);
      if (ocs::tom::get<JB_shell_list>(job))
         nm_set(job_field, JB_shell_list);

      
      while ((ep = cmdline.find_first<SPA_switch_val>("-wd"))) {
         ocs::tom::set<JB_cwd>(job, ocs::tom::get<SPA_argval_lStringT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_cwd);
      }

#ifdef WITH_EXTENSIONS
      while ((ep = cmdline.find_first<SPA_switch_val>("-when"))) {
         ocs::tom::set<JB_when>(job, ocs::tom::get<SPA_argval_lUlongT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_when);
      }
#endif

      /*
      ** to be processed in the original order, set -V equal to -v
      */
      while ((ep = cmdline.find_first<SPA_switch_val>("-V"))) {
         ocs::tom::set<SPA_switch_val>(ep, "-v");
      }
      parse_list_simple(cmdline, "-v", job, JB_env_list, VA_variable, VA_value, FLG_LIST_MERGE);
      if (ocs::tom::get<JB_env_list>(job))
         nm_set(job_field, JB_env_list);

      while ((ep = cmdline.find_first<SPA_switch_val>("-tc"))) {
         ocs::tom::set<JB_ja_task_concurrency>(job, ocs::tom::get<SPA_argval_lUlongT>(ep));
         cmdline.erase(ep);
         nm_set(job_field, JB_ja_task_concurrency);
      }

      if ((ep = cmdline.find_first<SPA_switch_val>("--"))) {
         cmdline.erase(ep);
         nm_set(job_field, JB_job_args);
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
            nm_set(job_field, JB_job_args);
         }
         ocs::tom::set<JB_job_args>(job, std::move(lp));
      }

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
            nm_set(job_field, JB_context_change);
         } else {
            ep = ep.next();
         }
      }
   }

   /* complain about unused options */
   for (const auto unknown : cmdline) {
      const char *cp;
      char str[1024];

      snprintf(str, sizeof(str), MSG_ANSWER_UNKNOWNOPTIONX_S,
         ocs::tom::get<SPA_switch_val>(unknown));
      cp = ocs::tom::get<SPA_switch_arg>(unknown);
      if (cp) {
         strcat(str, " ");
         strcat(str, cp);
      }
      strcat(str, "\n");
      answer_list_add(&answer, str, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
   }

   if ((ep = cmdline.find_first<SPA_switch_val>("-verify"))) {
      cmdline.erase(ep);
      verify = 1;
      nm_set(job_field, JB_env_list);
   } 
  
   if (job_field[1] == NoName) {
      answer_list_add(&answer, MSG_JOB_NOJOBATTRIBUTESELECTED, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

#if 0
   printf("=============== lWriteElemTo(job, stdout); ==================\n");
   lWriteElemTo(job, stdout);
#endif


   /* 
      STEP 2:
         make an gdi request for each jobid in the command line 
         using only these job fields that are in our job_field array 
   */

   if (!(what = lIntVector2What(JB_Type, job_field))) {
      answer_list_add(&answer, MSG_ANSWER_FAILDTOBUILDREDUCEDDESCRIPTOR, 
                        STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   rdp = nullptr;
   lReduceDescr(&rdp, JB_Type, what);
   if (rdp == nullptr) {
      answer_list_add(&answer, MSG_ANSWER_FAILDTOBUILDREDUCEDDESCRIPTOR, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   lFreeWhat(&what);

   /* if a user uses -u or -uall flag and does not enter jids
      we will add a dummy job to send other parameters to qmaster */
   if (users_flag && !ocs::tom::get<JB_job_identifier_list>(job)) {
      auto jid_list = ocs::tom::ListPtr<ocs::tom::ID>::create("");
      jid_list->emplace_back<ID_str>("*");
      ocs::tom::set<JB_job_identifier_list>(job, std::move(jid_list));
   }

   /* get next job id from cmd line */
   for (const auto jid : ocs::tom::get<JB_job_identifier_list>(job)) {
      // created with the first task, reduced as lAddElemUlong() created it
      ocs::tom::ListPtr<ocs::tom::JAT> task_list;
      ocs::tom::Element<ocs::tom::JB> rep;
      // built once and never released, so every use only takes a reference
      static const lDescr *const task_descr = ocs_cull_descr_reduce(JAT_Type, {JAT_task_number, JAT_hold});

      jobid = atol(ocs::tom::get<ID_str>(jid));
/*
      if ((all_or_jidlist == NOTINIT) && !strcmp(lGetString(ep, ID_str), "all")) {
         all_or_jidlist = ALL;
         (*all_jobs) = 1;
         DPRINTF("got \'all\' from parsing\n", jobid);
      } else */
      if (((all_or_jidlist == NOTINIT) || (all_or_jidlist == JOB)) /* && 
                  jobid != 0*/) { 
         all_or_jidlist = JOB;
         (*all_jobs) = 0; 
         DPRINTF("got job " sge_u32 " from parsing\n", jobid);
      } else {
         if (!strcmp(ocs::tom::get<ID_str>(jid), "all") || (all_or_jidlist == ALL)) {
            answer_list_add(&answer, MSG_ANSWER_ALLANDJOBIDSARENOTVALID, 
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ocs_cull_descr_release(&rdp);
            DRETURN(answer);
         } else if (!strcmp(ocs::tom::get<ID_str>(jid), "dummy")) {
            /* we will add a dummy-object to send parameters to qmaster */ 
         } else {
            answer_list_add(&answer, MSG_ANSWER_0ISNOTAVALIDJOBID,
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            ocs_cull_descr_release(&rdp);
            DRETURN(answer);
         }
      }
      if ((jobid != 0) || ((*all_jobs) == 1)){
         const char *job_name = ocs::tom::get<JB_job_name>(job);

         // the requests carry reduced jobs, created as lAddElemUlong() created them
         if (!requestlist) {
            requestlist = ocs::tom::ListPtr<ocs::tom::JB>::create("ulong_sublist", rdp);
         }
         auto new_rep = ocs::tom::ElementPtr<ocs::tom::JB>::create(rdp);
         ocs::tom::set<JB_job_number>(new_rep, jobid);
         rep = requestlist->push_back(std::move(new_rep));
         if (job_name != nullptr) {
            ocs::tom::set<JB_job_name>(rep, job_name);
         }
      }   
      else{
         char *name = nullptr;
         const char *job_name = ocs::tom::get<JB_job_name>(job);
         int size = strlen(ocs::tom::get<ID_str>(jid));
         if (job_name) {
            size += strlen(job_name); 
         }   
         size += 3;
          
         name = sge_malloc(size);
         /* format: <delimiter>old_name<delimiter>new_name */
         snprintf(name, size, "%s%s%s%s", JOB_NAME_DEL, ocs::tom::get<ID_str>(jid), JOB_NAME_DEL, job_name?job_name:"");
         // the requests carry reduced jobs, created as lAddElemStr() created them
         if (!requestlist) {
            requestlist = ocs::tom::ListPtr<ocs::tom::JB>::create("", rdp);
         }
         auto new_rep = ocs::tom::ElementPtr<ocs::tom::JB>::create(rdp);
         ocs::tom::set<JB_job_name>(new_rep, name);
         rep = requestlist->push_back(std::move(new_rep));
         sge_free(&name);
      }   

      if (!rep) {   
         answer_list_add_sprintf(&answer, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                                 MSG_MEM_MEMORYALLOCFAILED_S, __func__);
         ocs_cull_descr_release(&rdp);
         DRETURN(answer);
      }

      /* build task list from ID_Type from JB_job_identifier */
      
      if (is_hold_option && !ocs::tom::get<ID_ja_structure>(jid)) {
         task_list = ocs::tom::ListPtr<ocs::tom::JAT>::create("ulong_sublist", task_descr);
         auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create(task_descr);
         ocs::tom::set<JAT_task_number>(task, 0);
         ocs::tom::set<JAT_hold>(task, ocs::tom::get<ID_force>(jid));
         task_list->push_back(std::move(task));
      } else if (ocs::tom::get<ID_ja_structure>(jid)) {
         for (ocs::tom::ConstElement<ocs::tom::RN> range : ocs::tom::get<ID_ja_structure>(jid)) {
            uint32_t start = ocs::tom::get<RN_min>(range);
            uint32_t end = ocs::tom::get<RN_max>(range);
            uint32_t step = ocs::tom::get<RN_step>(range);
            for (;start<=end; start += step) {
               if (!task_list) {
                  task_list = ocs::tom::ListPtr<ocs::tom::JAT>::create("ulong_sublist", task_descr);
               }
               auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create(task_descr);
               ocs::tom::set<JAT_task_number>(task, start);
               ocs::tom::set<JAT_hold>(task, ocs::tom::get<ID_force>(jid));
               task_list->push_back(std::move(task));
            } 
         }
      }
      if (!ocs::tom::has<JB_ja_tasks>(rep) && task_list->size() > 0) {
         answer_list_add_sprintf(&answer, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 SFNMAX, MSG_OPTIONWORKSONLYONJOB);
         ocs_cull_descr_release(&rdp);
         DRETURN(answer);
      }
      ocs::tom::set<JB_ja_tasks>(job, std::move(task_list));
      ocs::tom::set<JB_ja_structure>(job, ocs::tom::get<ID_ja_structure>(jid).clone(""));

      /* fill in fields of the job; the field numbers come from tables and the request is reduced, so the
       * copy stays on the generic interface */
      {
         static int str_nm[] = {
            JB_account,
            JB_cwd,
            JB_checkpoint_name,
            JB_project,
            JB_pe,
            NoName
         };
         static int ulong_nm[] = {
            JB_mail_options,
            JB_priority,
            JB_jobshare,
            JB_override_tickets,
            JB_restart,
            JB_verify_suitable_queues,
            JB_ar,
            JB_ja_task_concurrency,
            JB_when,
            NoName
         };
         static int ulong64_nm[] = {
            JB_execution_time,
            JB_deadline,
            NoName
         };
         static int bool_nm[] = {
            JB_reserve,
            JB_merge_stderr,
            JB_notify,
            NoName
         };
         static int list_nm[] = {
            JB_stderr_path_list,
            JB_jid_request_list,
            JB_ja_ad_request_list,
            JB_request_set_list,
            JB_mail_list,
            JB_stdout_path_list,
            JB_stdin_path_list,
            JB_pe_range,
            JB_shell_list,
            JB_env_list,
            JB_job_args,
            JB_context,
            JB_context_change,
            JB_ja_tasks,
            JB_ja_structure,
            JB_user_list,
            NoName
         };
         static int object_nm[] = {
            JB_binding,
            NoName
         };

         /* copy all strings */
         for (i=0; str_nm[i]!=NoName; i++)
            if (lGetPosViaElem(job, str_nm[i], SGE_NO_ABORT) != -1 && lGetPosViaElem(rep, str_nm[i], SGE_NO_ABORT) != -1)
               lSetString(rep, str_nm[i], lGetString(job, str_nm[i]));

         /* copy all ulongs */
         for (i=0; ulong_nm[i]!=NoName; i++)
            if (lGetPosViaElem(job, ulong_nm[i], SGE_NO_ABORT) != -1 && lGetPosViaElem(rep, ulong_nm[i], SGE_NO_ABORT) != -1)
               lSetUlong(rep, ulong_nm[i], lGetUlong(job, ulong_nm[i]));

         /* copy all ulong64s */
         for (i=0; ulong64_nm[i]!=NoName; i++)
            if (lGetPosViaElem(job, ulong64_nm[i], SGE_NO_ABORT) != -1 && lGetPosViaElem(rep, ulong64_nm[i], SGE_NO_ABORT) != -1)
               lSetUlong64(rep, ulong64_nm[i], lGetUlong64(job, ulong64_nm[i]));

         /* copy all bools */
         for (i=0; bool_nm[i]!=NoName; i++)
            if (lGetPosViaElem(job, bool_nm[i], SGE_NO_ABORT) != -1 && lGetPosViaElem(rep, bool_nm[i], SGE_NO_ABORT) != -1)
               lSetBool(rep, bool_nm[i], lGetBool(job, bool_nm[i]));

         /* copy all lists */
         for (i=0; list_nm[i]!=NoName; i++)
            if (lGetPosViaElem(job, list_nm[i], SGE_NO_ABORT) != -1  && lGetPosViaElem(rep, list_nm[i], SGE_NO_ABORT) != -1)
               lSetList(rep, list_nm[i], lCopyList("", lGetList(job, list_nm[i])));

         /* copy all objects */
         for (i=0; object_nm[i]!=NoName; i++)
            if (lGetPosViaElem(job, object_nm[i], SGE_NO_ABORT) != -1  && lGetPosViaElem(rep, object_nm[i], SGE_NO_ABORT) != -1)
               lSetObject(rep, object_nm[i], lCopyElem(lGetObject(job, object_nm[i])));

      }
   }

   ocs_cull_descr_release(&rdp);
   DRETURN(answer);
}
