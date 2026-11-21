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
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cerrno>
#include <sstream>
#include <vector>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

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
#include "tom/ocs_Raw.h"
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
   for (const auto aep : *alp) {
      printf("%s\n", aep.get<AN_text>());
      if (ret == STATUS_OK) {
         ret = aep.get<AN_status>();
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
 * @brief Copy one field of the parsed job into a request, if both hold it
 *
 * The request is reduced to the fields the options selected, the parsed job is
 * complete; lGetPosViaElem() != -1 on both was the test before. A list or an
 * object is copied, as lCopyList("")/lCopyElem() did (nullptr stays nullptr).
 *
 * @tparam F the field of the job
 * @param job the parsed job
 * @param rep the request element, reduced
 */
template <auto F>
static void
ocs_qalter_copy_one(const ocs::tom::ConstElement<ocs::tom::JB> job, const ocs::tom::Element<ocs::tom::JB> rep) {
   if (job.has<F>() && rep.has<F>()) {
      if constexpr (ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::List) {
         rep.set<F>(job.get<F>().clone(""));
      } else if constexpr (ocs::tom::FieldTraits<F>::type == ocs::tom::FieldType::Object) {
         rep.set<F>(job.get<F>().clone());
      } else {
         rep.set<F>(job.get<F>());
      }
   }
}

/**
 * @brief Copy the fields of the parsed job into a request, each one only if both hold it
 * @tparam Fs the fields of the job, copied in this order
 * @param job the parsed job
 * @param rep the request element, reduced
 */
template <auto... Fs>
static void
ocs_qalter_copy_present(const ocs::tom::ConstElement<ocs::tom::JB> job, const ocs::tom::Element<ocs::tom::JB> rep) {
   DENTER(TOP_LAYER);
   (ocs_qalter_copy_one<Fs>(job, rep), ...);
   DRETURN_VOID;
}

/**
 * @brief Turn the parsed command line of qalter, qhold, qrls or qresub into job modify requests
 *
 * First all options are parsed into a dummy job, then a request is built for
 * each job id of the command line from the options of the dummy job and added
 * to the request list.
 *
 * @param me_who the program, QALTER, QHOLD, QRLS or QRESUB
 * @param cmdline the parsed command line (SPA), or an empty handle for the defaults; the options used
 *                are removed
 * @param requestlist receives the job modify requests for ocs::gdi::Client::sge_gdi (reduced JB),
 *                    created with the first request
 * @param all_jobs set to 1 if the requests address all jobs, 0 if they name jobs
 * @param all_users not used
 * @return nullptr on success, else an answer list (AN) with STATUS_EUNKNOWN for
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
   ocs::tom::ListPtr<ocs::tom::AN> answer;
   enum {NOTINIT, JOB, ALL} all_or_jidlist = NOTINIT;
   int users_flag = 0;

   // the job fields the options select for modification, each once, in the order they were selected
   std::vector<int> job_fields;
   const auto select_field = [&job_fields](const int nm) {
      if (std::find(job_fields.begin(), job_fields.end(), nm) == job_fields.end()) {
         job_fields.push_back(nm);
      }
   };
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
   select_field(JB_job_number);
   select_field(JB_job_name);

   /* don't do verification of schedulability per default */
   job.set<JB_verify_suitable_queues>(SKIP_VERIFY);
   select_field(JB_verify_suitable_queues);


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
         job.set<JB_execution_time>(ep.get<SPA_argval_lUlong64T>());
         cmdline.erase(ep);
         select_field(JB_execution_time);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-A"))) {
         job.set<JB_account>(ep.get<SPA_argval_lStringT>());
         cmdline.erase(ep);
         select_field(JB_account);
      }
      
      while ((ep = cmdline.find_first<SPA_switch_val>("-ar"))) {
         job.set<JB_ar>(ep.get<SPA_argval_lUlongT>());
         cmdline.erase(ep);
         select_field(JB_ar);
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
         select_field(JB_binding);
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
         select_field(JB_binding);
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
         select_field(JB_binding);
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
         select_field(JB_binding);
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
         select_field(JB_binding);
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
         select_field(JB_binding);
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
         select_field(JB_binding);
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
         select_field(JB_binding);
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
         select_field(JB_binding);
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
         job.set<JB_cwd>(tmp_str);
         cmdline.erase(ep);
         select_field(JB_cwd);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-c"))) {
         if (ep.get<SPA_argtype>() == lLongT) {
            job.set<JB_checkpoint_interval>(
                        ep.get<SPA_argval_lLongT>());
            select_field(JB_checkpoint_interval);
         }
         if (ep.get<SPA_argtype>() == lIntT) { 
            job.set<JB_checkpoint_attr>(ep.get<SPA_argval_lIntT>());
            select_field(JB_checkpoint_attr);
         }
         cmdline.erase(ep);
      }


      while ((ep = cmdline.find_first<SPA_switch_val>("-ckpt"))) {
         job.set<JB_checkpoint_name>(ep.get<SPA_argval_lStringT>());
         cmdline.erase(ep);
         select_field(JB_checkpoint_name);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-dept"))) {
         job.set<JB_department>(ep.get<SPA_argval_lStringT>());
         cmdline.erase(ep);
         select_field(JB_department);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-dl"))) {
         job.set<JB_deadline>(ep.get<SPA_argval_lUlong64T>());
         cmdline.erase(ep);
         select_field(JB_deadline);
      }

      parse_list_append<JB_stderr_path_list>(cmdline, "-e", job);
      if (job.get<JB_stderr_path_list>())
         select_field(JB_stderr_path_list);
      
      parse_list_append<JB_stdin_path_list>(cmdline, "-i", job);
      if (job.get<JB_stdin_path_list>())
         select_field(JB_stdin_path_list);
   }

   /* STR_PSEUDO_JOBID */
   if (cmdline.find_first<SPA_switch_val>(STR_PSEUDO_JOBID)) {
      ocs::tom::ListPtr<ocs::tom::ID> jid_list;
      const bool parsed = parse_multi_jobtaskslist(cmdline, STR_PSEUDO_JOBID, &answer, jid_list, true, 0);
      if (!parsed) {
         DRETURN(answer);
      }                                                 
      job.set<JB_job_identifier_list>(std::move(jid_list));
   } 

   if (me_who != QRESUB) {
      while ((ep = cmdline.find_first<SPA_switch_val>("-u"))) {
         auto jid_list = ocs::tom::ListPtr<ocs::tom::ID>::create("");
         jid_list->emplace_back<ID_str>("*");
         job.set<JB_job_identifier_list>(std::move(jid_list));

         // SPA_argval_lListT carries lists of several types and has no typed exchange; the parser fills the
         // argument of -u with an ST user list
         lList *lp = nullptr;
         ocs::tom::exchange<SPA_argval_lListT>(ep, &lp);
         job.set<JB_user_list>(ocs::tom::ListPtr<ocs::tom::ST>::from_typed(lp));
         cmdline.erase(ep);
         select_field(JB_user_list);
         users_flag = 1;
      }
 
   }                    
   
   while ((ep = cmdline.find_first<SPA_switch_val>("-h"))) {
      is_hold_option = true;

      for (ocs::tom::Element<ocs::tom::ID> jid : job.get<JB_job_identifier_list>()) {
         jid.set<ID_force>((uint32_t) ep.get<SPA_argval_lIntT>());
      }
      cmdline.erase(ep);
      select_field(JB_ja_tasks);
      select_field(JB_ja_structure);
   }

   if (me_who != QRESUB) {
      /* -hold_jid */
      if (cmdline.find_first<SPA_switch_val>("-hold_jid")) {
         // created with the first reference, as the free emplace_back() created it
         ocs::tom::ListPtr<ocs::tom::JRE> jref_list;
         while ((ep = cmdline.find_first<SPA_switch_val>("-hold_jid"))) {
            // the parser fills the argument with an ST name list
            for (const auto sep : ocs::tom::ConstList<ocs::tom::ST>::from_typed(ep.get<SPA_argval_lListT>())) {
               const char *name = sep.get<ST_name>();
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
         job.set<JB_jid_request_list>(std::move(jref_list));
         select_field(JB_jid_request_list);
         select_field(JB_jid_predecessor_list);
      }

      /* -hold_jid_ad */
      if (cmdline.find_first<SPA_switch_val>("-hold_jid_ad")) {
         // created with the first reference, as the free emplace_back() created it
         ocs::tom::ListPtr<ocs::tom::JRE> jref_list;
         while ((ep = cmdline.find_first<SPA_switch_val>("-hold_jid_ad"))) {
            // the parser fills the argument with an ST name list
            for (const auto sep : ocs::tom::ConstList<ocs::tom::ST>::from_typed(ep.get<SPA_argval_lListT>())) {
               const char *name = sep.get<ST_name>();
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
         job.set<JB_ja_ad_request_list>(std::move(jref_list));
         select_field(JB_ja_ad_request_list);
         select_field(JB_ja_ad_predecessor_list);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-R"))) {
         job.set<JB_reserve>(ep.get<SPA_argval_lIntT>());
         cmdline.erase(ep);
         select_field(JB_reserve);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-j"))) {
         job.set<JB_merge_stderr>(ep.get<SPA_argval_lIntT>());
         cmdline.erase(ep);
         select_field(JB_merge_stderr);
      }

      parse_list_hardsoft<JRS_hard_resource_list, JRS_soft_resource_list>(cmdline, "-l", job, JRS_SCOPE_GLOBAL);
      parse_list_hardsoft<JRS_hard_resource_list, JRS_soft_resource_list>(cmdline, "-l", job, JRS_SCOPE_MASTER);
      parse_list_hardsoft<JRS_hard_resource_list, JRS_soft_resource_list>(cmdline, "-l", job, JRS_SCOPE_SLAVE);

      if (job_request_set_remove_duplicates(job)) {
         select_field(JB_request_set_list);
      }

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
         select_field(JB_mail_options);
      }

      parse_list_merge<JB_mail_list, MR_host, MR_user>(cmdline, "-M", job);
      if (job.get<JB_mail_list>())
         select_field(JB_mail_list);

      while ((ep = cmdline.find_first<SPA_switch_val>("-N"))) {
         job.set<JB_job_name>(ep.get<SPA_argval_lStringT>());
         cmdline.erase(ep);
         select_field(JB_job_name); 
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-notify"))) {
         job.set<JB_notify>(true);
         cmdline.erase(ep);
         select_field(JB_notify);
      }

      parse_list_append<JB_stdout_path_list>(cmdline, "-o", job);
      if (job.get<JB_stdout_path_list>())
         select_field(JB_stdout_path_list);

      while ((ep = cmdline.find_first<SPA_switch_val>("-p"))) {
         job.set<JB_priority>(
            BASE_PRIORITY + ep.get<SPA_argval_lIntT>());
         cmdline.erase(ep);
         select_field(JB_priority);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-js"))) {
         job.set<JB_jobshare>(ep.get<SPA_argval_lUlongT>());
         cmdline.erase(ep);
         select_field(JB_jobshare);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-P"))) {
         job.set<JB_project>(ep.get<SPA_argval_lStringT>());
         cmdline.erase(ep);
         select_field(JB_project);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-par"))) {
         const char *allocation_rule = ep.get<SPA_switch_arg>();
         uint32_t scope = ep.get<SPA_argval_lLongT>();
         const auto jrs = job_get_or_create_request_setRW(job, scope);
         if (jrs) {
            jrs.set<JRS_allocation_rule>(allocation_rule);
         }
         cmdline.erase(ep);
         select_field(JB_request_set_list);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-pe"))) {
         job.set<JB_pe>(ep.get<SPA_argval_lStringT>());
         /* put sublist from parsing into job */
         // SPA_argval_lListT carries lists of several types; at -pe the parser fills it with a slot range, so the
         // list is taken out raw and adopted as RN here; the range held before is freed by the set
         job.set<JB_pe_range>(ocs::tom::adopt<ocs::tom::RN>(
            ocs::tom::exchange(ep, SPA_argval_lListT, ocs::tom::RawListPtr())));
         cmdline.erase(ep);

         /* cannot address fields separately in command line but in gdi interface */ 
         select_field(JB_pe);
         select_field(JB_pe_range);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-ot"))) {
         job.set<JB_override_tickets>(ep.get<SPA_argval_lUlongT>());
         cmdline.erase(ep);
         select_field(JB_override_tickets);
      }

      parse_list_hardsoft<JRS_hard_queue_list, JRS_soft_queue_list>(cmdline, "-q", job, JRS_SCOPE_GLOBAL);
      parse_list_hardsoft<JRS_hard_queue_list, JRS_soft_queue_list>(cmdline, "-q", job, JRS_SCOPE_MASTER);
      parse_list_hardsoft<JRS_hard_queue_list, JRS_soft_queue_list>(cmdline, "-q", job, JRS_SCOPE_SLAVE);
      parse_list_hardsoft<JRS_hard_queue_list>(cmdline, "-masterq", job, JRS_SCOPE_MASTER);
      if (job_request_set_has_queue_requests(job)) {
         select_field(JB_request_set_list);
      }

      if (job_get_master_hard_queue_list(job) != nullptr)
         select_field(JB_request_set_list);
    
      while ((ep = cmdline.find_first<SPA_switch_val>("-r"))) {
         job.set<JB_restart>(ep.get<SPA_argval_lIntT>());
         cmdline.erase(ep);
         select_field(JB_restart);
      }

      while ((ep = cmdline.find_first<SPA_switch_val>("-w"))) {
         job.set<JB_verify_suitable_queues>(ep.get<SPA_argval_lIntT>());
         cmdline.erase(ep);
         select_field(JB_verify_suitable_queues);
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

      parse_list_append<JB_shell_list>(cmdline, "-S", job);
      if (job.get<JB_shell_list>())
         select_field(JB_shell_list);

      
      while ((ep = cmdline.find_first<SPA_switch_val>("-wd"))) {
         job.set<JB_cwd>(ep.get<SPA_argval_lStringT>());
         cmdline.erase(ep);
         select_field(JB_cwd);
      }

#ifdef WITH_EXTENSIONS
      while ((ep = cmdline.find_first<SPA_switch_val>("-when"))) {
         job.set<JB_when>(ep.get<SPA_argval_lUlongT>());
         cmdline.erase(ep);
         select_field(JB_when);
      }
#endif

      /*
      ** to be processed in the original order, set -V equal to -v
      */
      while ((ep = cmdline.find_first<SPA_switch_val>("-V"))) {
         ep.set<SPA_switch_val>("-v");
      }
      parse_list_merge<JB_env_list, VA_variable, VA_value>(cmdline, "-v", job);
      if (job.get<JB_env_list>())
         select_field(JB_env_list);

      while ((ep = cmdline.find_first<SPA_switch_val>("-tc"))) {
         job.set<JB_ja_task_concurrency>(ep.get<SPA_argval_lUlongT>());
         cmdline.erase(ep);
         select_field(JB_ja_task_concurrency);
      }

      if ((ep = cmdline.find_first<SPA_switch_val>("--"))) {
         cmdline.erase(ep);
         select_field(JB_job_args);
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
            select_field(JB_job_args);
         }
         job.set<JB_job_args>(std::move(lp));
      }

      /* context switches are sensitive to order */
      ep = cmdline.front();
      while (ep) {
         if (!strcmp(ep.get<SPA_switch_val>(), "-ac") ||
             !strcmp(ep.get<SPA_switch_val>(), "-dc") ||
             !strcmp(ep.get<SPA_switch_val>(), "-sc")) {
            // the switches are concatenated in their order, as instructions for the qmaster (CS-3058);
            // ocs_context_change_add() has no typed form for the change list yet
            lList *change = nullptr;
            ocs::tom::exchange<JB_context_change>(job, &change);
            // the parser fills the argument of the context switches with a VA list
            ocs_context_change_add(&change, ocs::tom::ConstList<ocs::tom::VA>::from_typed(
                                               ep.get<SPA_argval_lListT>()));
            ocs::tom::exchange<JB_context_change>(job, &change);
            const auto temp = ep.next();
            cmdline.erase(ep);
            ep = temp;
            select_field(JB_context_change);
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
         unknown.get<SPA_switch_val>());
      cp = unknown.get<SPA_switch_arg>();
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
      select_field(JB_env_list);
   } 
  
   // job number and name are always selected, so this never holds; kept as the old test of the second slot
   if (job_fields.size() < 2) {
      answer_list_add(&answer, MSG_JOB_NOJOBATTRIBUTESELECTED, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   /* 
      STEP 2:
         make an gdi request for each jobid in the command line 
         using only these job fields that are in our job_fields vector
   */

   // the requests carry the job fields the options selected, known only at run time; every request and the
   // request list are created like an empty job of that layout, so the reduction is built once, here, where a
   // failure (only for lack of memory) gives the answer the old lReduceDescr() call gave
   ocs::tom::ElementPtr<ocs::tom::JB> request_like;
   try {
      const auto what = ocs::tom::what<ocs::tom::JB>(job_fields);
      // the element keeps its own reference on the reduction
      request_like = ocs::tom::ElementPtr<ocs::tom::JB>::create(what);
   } catch (const ocs::RuntimeException &) {
      answer_list_add(&answer, MSG_ANSWER_FAILDTOBUILDREDUCEDDESCRIPTOR, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   } catch (const ocs::LogicException &) {
      answer_list_add(&answer, MSG_ANSWER_FAILDTOBUILDREDUCEDDESCRIPTOR, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   /* if a user uses -u or -uall flag and does not enter jids
      we will add a dummy job to send other parameters to qmaster */
   if (users_flag && !job.get<JB_job_identifier_list>()) {
      auto jid_list = ocs::tom::ListPtr<ocs::tom::ID>::create("");
      jid_list->emplace_back<ID_str>("*");
      job.set<JB_job_identifier_list>(std::move(jid_list));
   }

   /* get next job id from cmd line */
   for (const auto jid : job.get<JB_job_identifier_list>()) {
      // created with the first task, reduced as lAddElemUlong() created it
      ocs::tom::ListPtr<ocs::tom::JAT> task_list;
      ocs::tom::Element<ocs::tom::JB> rep;
      // built once for the life of the process
      const auto task_layout = ocs::tom::layout<JAT_task_number, JAT_hold>();

      jobid = atol(jid.get<ID_str>());
      if (((all_or_jidlist == NOTINIT) || (all_or_jidlist == JOB)) /* && 
                  jobid != 0*/) { 
         all_or_jidlist = JOB;
         (*all_jobs) = 0; 
         DPRINTF("got job " sge_u32 " from parsing\n", jobid);
      } else {
         if (!strcmp(jid.get<ID_str>(), "all") || (all_or_jidlist == ALL)) {
            answer_list_add(&answer, MSG_ANSWER_ALLANDJOBIDSARENOTVALID, 
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         } else if (!strcmp(jid.get<ID_str>(), "dummy")) {
            /* we will add a dummy-object to send parameters to qmaster */ 
         } else {
            answer_list_add(&answer, MSG_ANSWER_0ISNOTAVALIDJOBID,
                            STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }
      }
      if ((jobid != 0) || ((*all_jobs) == 1)){
         const char *job_name = job.get<JB_job_name>();

         // the requests carry reduced jobs, created as lAddElemUlong() created them
         if (!requestlist) {
            requestlist = ocs::tom::ListPtr<ocs::tom::JB>::create_like("ulong_sublist", *request_like);
         }
         auto new_rep = ocs::tom::ElementPtr<ocs::tom::JB>::create_like(*request_like);
         new_rep->set<JB_job_number>(jobid);
         rep = requestlist->push_back(std::move(new_rep));
         if (job_name != nullptr) {
            rep.set<JB_job_name>(job_name);
         }
      }   
      else{
         char *name = nullptr;
         const char *job_name = job.get<JB_job_name>();
         int size = strlen(jid.get<ID_str>());
         if (job_name) {
            size += strlen(job_name); 
         }   
         size += 3;
          
         name = sge_malloc(size);
         /* format: <delimiter>old_name<delimiter>new_name */
         snprintf(name, size, "%s%s%s%s", JOB_NAME_DEL, jid.get<ID_str>(), JOB_NAME_DEL, job_name?job_name:"");
         // the requests carry reduced jobs, created as lAddElemStr() created them
         if (!requestlist) {
            requestlist = ocs::tom::ListPtr<ocs::tom::JB>::create_like("", *request_like);
         }
         auto new_rep = ocs::tom::ElementPtr<ocs::tom::JB>::create_like(*request_like);
         new_rep->set<JB_job_name>(name);
         rep = requestlist->push_back(std::move(new_rep));
         sge_free(&name);
      }   

      if (!rep) {   
         answer_list_add_sprintf(&answer, STATUS_EMALLOC, ANSWER_QUALITY_ERROR,
                                 MSG_MEM_MEMORYALLOCFAILED_S, __func__);
         DRETURN(answer);
      }

      /* build task list from ID from JB_job_identifier */
      
      if (is_hold_option && !jid.get<ID_ja_structure>()) {
         task_list = ocs::tom::ListPtr<ocs::tom::JAT>::create("ulong_sublist", task_layout);
         auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create(task_layout);
         task->set<JAT_task_number>(0);
         task->set<JAT_hold>(jid.get<ID_force>());
         task_list->push_back(std::move(task));
      } else if (jid.get<ID_ja_structure>()) {
         for (ocs::tom::ConstElement<ocs::tom::RN> range : jid.get<ID_ja_structure>()) {
            uint32_t start = range.get<RN_min>();
            uint32_t end = range.get<RN_max>();
            uint32_t step = range.get<RN_step>();
            for (;start<=end; start += step) {
               if (!task_list) {
                  task_list = ocs::tom::ListPtr<ocs::tom::JAT>::create("ulong_sublist", task_layout);
               }
               auto task = ocs::tom::ElementPtr<ocs::tom::JAT>::create(task_layout);
               task->set<JAT_task_number>(start);
               task->set<JAT_hold>(jid.get<ID_force>());
               task_list->push_back(std::move(task));
            } 
         }
      }
      if (!rep.has<JB_ja_tasks>() && task_list->size() > 0) {
         answer_list_add_sprintf(&answer, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR,
                                 SFNMAX, MSG_OPTIONWORKSONLYONJOB);
         DRETURN(answer);
      }
      job.set<JB_ja_tasks>(std::move(task_list));
      job.set<JB_ja_structure>(jid.get<ID_ja_structure>().clone(""));

      /* fill in the fields of the job that the reduced request holds: strings, ulongs, ulong64s, bools,
       * lists and objects, in this order */
      ocs_qalter_copy_present<JB_account, JB_cwd, JB_checkpoint_name, JB_project, JB_pe,
                              JB_mail_options, JB_priority, JB_jobshare, JB_override_tickets, JB_restart,
                              JB_verify_suitable_queues, JB_ar, JB_ja_task_concurrency, JB_when,
                              JB_execution_time, JB_deadline,
                              JB_reserve, JB_merge_stderr, JB_notify,
                              JB_stderr_path_list, JB_jid_request_list, JB_ja_ad_request_list, JB_request_set_list,
                              JB_mail_list, JB_stdout_path_list, JB_stdin_path_list, JB_pe_range, JB_shell_list,
                              JB_env_list, JB_job_args, JB_context, JB_context_change, JB_ja_tasks,
                              JB_ja_structure, JB_user_list,
                              JB_binding>(job, rep);
   }

   DRETURN(answer);
}
