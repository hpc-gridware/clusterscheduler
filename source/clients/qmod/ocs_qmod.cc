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
 * @brief qmod - enables, disables, suspends and clears queues and jobs
 */
#include <cstring>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_str.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "gdi/ocs_gdi_Client.h"

#include "sge_options.h"
#include "sig_handlers.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qmod.h"


static ocs::tom::ListPtr<ocs::tom::AN> sge_parse_cmdline_qmod(char **argv, char **envp,
                                                              ocs::tom::ListPtr<ocs::tom::SPA> &cmdline);
static ocs::tom::ListPtr<ocs::tom::AN> sge_parse_qmod(ocs::tom::List<ocs::tom::SPA> cmdline,
                                                      ocs::tom::ListPtr<ocs::tom::ID> &reflist, uint32_t *pforce);

static int qmod_usage(FILE *fp, char *what);

static bool answer_list_has_exit_code_error(ocs::tom::ListPtr<ocs::tom::AN> *answer_list);

/** @brief The process environment, as the C library defines it
 *
 * Declared here rather than included, and passed to the command line parser so
 * that options which read an environment variable resolve it against the
 * submitting user's environment rather than against `getenv()`.
 */
extern char **environ;

/**
 * @brief qmod: parse the command line and trigger the requested queue and job actions
 * @param argc the number of arguments
 * @param argv the arguments
 * @return 0 when no answer reports an error, 1 otherwise; the process ends with sge_exit()
 */
int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "qmod");
   uint32_t force = 0;
   ocs::tom::ListPtr<ocs::tom::ID> ref_list;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ListPtr<ocs::tom::SPA> pcmdline;
   bool answ_list_has_err = false;

   sge_setup_sig_handlers(QMOD);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QMOD, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   /*
   ** static func for parsing all qmod specific switches
   ** here we get action, force, ref_list
   */
   alp = sge_parse_cmdline_qmod(++argv, environ, pcmdline);
   if(alp) {
      /*
      ** high level parsing error! show answer list
      */
      for (const auto aep : *alp) {
         fprintf(stderr, "%s\n", aep.get<AN_text>());
      }
      alp.reset();
      pcmdline.reset();
      sge_exit(1);
   }

   alp = sge_parse_qmod(*pcmdline, ref_list, &force);

   if(alp) {
      /*
      ** low level parsing error! show answer list
      */
      for (const auto aep : *alp) {
         fprintf(stderr, "%s\n", aep.get<AN_text>());
      }
      alp.reset();
      pcmdline.reset();
      ref_list.reset();
      sge_exit(1);
   }

   for (const auto idep : *ref_list) {
      idep.set<ID_force>(force);
   }

   if (ref_list) {
      // the request list is replaced by the response, if any, freed with the handle
      alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::CQ_LIST, ocs::gdi::Command::TRIGGER,
                                      ocs::gdi::SubCommand::NONE, ref_list);
   }

   answ_list_has_err = answer_list_has_exit_code_error(&alp); 

   /*
   ** show answer list
   */
   for (const auto aep : *alp) {
      fprintf(stdout, "%s\n", aep.get<AN_text>());
   }

   alp.reset();
   ref_list.reset();
   pcmdline.reset();

   if(answ_list_has_err) {
      sge_exit(1);
   }
   else {
      sge_exit(0);
   }
   DRETURN(0);
}


/**
 * @brief Returns if there was a critical error
 *
 * Checks the answer list if there was any critical error or in the other
 * cases if there was a status other than ok.
 *
 * @param answer_list AN list
 *
 * @return "true" if an error is found "false" otherwise
 *
 * @note MT-NOTE: answer_list_has_exit_code_error() is not MT safe
 */
static bool answer_list_has_exit_code_error(ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   bool ret = false;

   if (answer_list_has_quality(answer_list, ANSWER_QUALITY_CRITICAL)) {
      ret = true;
   } else {
      /* check each ERROR if the status is really != 1 (STATUS_OK) */
      uint32_t status;
      for (const auto answer : **answer_list) {
         if (answer_has_quality(answer, ANSWER_QUALITY_ERROR)) {
            status = answer_get_status(answer);       
            if (status != STATUS_OK) {
               ret = true;
            }
         }   
      }
  } 

   DRETURN(ret);
}


/**
 * @brief 'Stage 1' parsing of the qmod options
 *
 * Parses the options with their arguments and stores them in @p cmdline.
 *
 * @param argv the arguments, without the program name
 * @param envp the process environment
 * @param cmdline receives the parsed options (`SPA`), created with the first option
 * @return the answer list with the parse errors, nullptr if there were none
 */
static ocs::tom::ListPtr<ocs::tom::AN> sge_parse_cmdline_qmod(char **argv, [[maybe_unused]] char **envp,
                                                              ocs::tom::ListPtr<ocs::tom::SPA> &cmdline) {
   DENTER(TOP_LAYER);

char **sp;
char **rp;
ocs::tom::ListPtr<ocs::tom::AN> alp;

   rp = argv;

   if (*rp == nullptr) {
      /* no command line argument: print help on error */
      qmod_usage(stderr, nullptr);
      answer_list_add_sprintf(&alp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, SFNMAX, MSG_PARSE_NOOPTIONARGUMENT);
   }

   while(*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", "--help", cmdline, &alp)) != sp)
         continue;

      /* -f option */
      if ((rp = parse_noopt(sp, "-f", "--force", cmdline, &alp)) != sp)
         continue;

      /* -c option */
      if ((rp = parse_until_next_opt(sp, "-c", "--clear", cmdline, &alp)) != sp)
         continue;

      /* -cj option */
      if ((rp = parse_until_next_opt(sp, "-cj", "--clearjob", cmdline, &alp)) != sp)
         continue;

      /* -cq option */
      if ((rp = parse_until_next_opt(sp, "-cq", "--clearqueue", cmdline, &alp)) != sp)
         continue;

      /* -s option */
      if ((rp = parse_until_next_opt(sp, "-s", "--suspend", cmdline, &alp)) != sp)
         continue;

      /* -sj option */
      if ((rp = parse_until_next_opt(sp, "-sj", "--suspendjob", cmdline, &alp)) != sp)
         continue;

      /* -sq option */
      if ((rp = parse_until_next_opt(sp, "-sq", "--suspendqueue", cmdline, &alp)) != sp)
         continue;

      /* -us option */
      if ((rp = parse_until_next_opt(sp, "-us", "--unsuspend", cmdline, &alp)) != sp)
         continue;

      /* -usj option */
      if ((rp = parse_until_next_opt(sp, "-usj", "--unsuspendjob", cmdline, &alp)) != sp)
         continue;

      /* -usq option */
      if ((rp = parse_until_next_opt(sp, "-usq", "--unsuspendqueue", cmdline, &alp)) != sp)
         continue;

      /* -d option */
      if ((rp = parse_until_next_opt(sp, "-d", "--disable", cmdline, &alp)) != sp)
         continue;

      /* -rj option */
      if ((rp = parse_until_next_opt(sp, "-rj", "--reschedulejob", cmdline, &alp)) != sp)
         continue;

      /* -rq option */
      if ((rp = parse_until_next_opt(sp, "-rq", "--reschedulequeue", cmdline, &alp)) != sp)
         continue;

      /* -r option */
      if ((rp = parse_until_next_opt(sp, "-r", "--reschedule", cmdline, &alp)) != sp)
         continue;

      /* -e option */
      if ((rp = parse_until_next_opt(sp, "-e", "--enable", cmdline, &alp)) != sp)
         continue;

      /* -t */
      if (!strcmp("-t", *sp)) {
         /* next field is path_name */
         sp++;
         if (*sp == nullptr) {
             answer_list_add(&alp, MSG_PARSE_TOPTIONMUSTHAVEALISTOFTASKIDRANGES, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             goto error;
         }

         DPRINTF("\"-t %s\"\n", *sp);

         ocs::tom::ListPtr<ocs::tom::RN> task_id_ranges;
         range_list_parse_from_string(task_id_ranges, &alp, *sp, false, true, INF_NOT_ALLOWED);
         if (!task_id_ranges) {
            goto error;
         }

         range_list_sort_uniq_compress(*task_id_ranges, &alp, true);
         if (task_id_ranges->size() > 1) {
            answer_list_add(&alp, MSG_QCONF_ONLYONERANGE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            goto error;
         }

         const auto ep_opt = sge_add_arg(cmdline, t_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt, std::move(task_id_ranges));

         sp++;
         rp = sp;
         continue;
      }

#ifdef __SGE_QINSTANCE_STATE_DEBUG__
      if ((rp = parse_until_next_opt(sp, "-_e", "--_error", cmdline, &alp)) != sp)
         continue;
      if ((rp = parse_until_next_opt(sp, "-_o", "--_orphaned", cmdline, &alp)) != sp)
         continue;
      if ((rp = parse_until_next_opt(sp, "-_do", "--_dorphaned", cmdline, &alp)) != sp)
         continue;
      if ((rp = parse_until_next_opt(sp, "-_u", "--_unknown", cmdline, &alp)) != sp)
         continue;
      if ((rp = parse_until_next_opt(sp, "-_du", "--_dunknown", cmdline, &alp)) != sp)
         continue;
      if ((rp = parse_until_next_opt(sp, "-_c", "--_confambiguous", cmdline, &alp)) != sp)
         continue;
      if ((rp = parse_until_next_opt(sp, "-_dc", "--_dconfambiguous", cmdline, &alp)) != sp)
         continue;
#endif

      /* oops */
      answer_list_add_sprintf(&alp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
error:
      qmod_usage(stderr, nullptr);
      DRETURN(alp);
   }
   DRETURN(alp);
}

/**
 * @brief 'Stage 2' parsing of the qmod options
 *
 * Gets the options from @p cmdline, sets the force and action flags and puts
 * the queue/job names/numbers in @p reflist.
 *
 * @param cmdline the parsed options (`SPA`); the options served are removed
 * @param reflist receives the queue and job references (`ID`)
 * @param pforce receives the -f flag
 * @return the answer list with the errors, nullptr if there were none
 */
static ocs::tom::ListPtr<ocs::tom::AN> sge_parse_qmod(const ocs::tom::List<ocs::tom::SPA> cmdline,
                                                      ocs::tom::ListPtr<ocs::tom::ID> &reflist, uint32_t *pforce) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   uint32_t helpflag;
   int usageshowed = 0;

   /* Loop over all options. Only valid options can be in the
      cmdline list. Except f_OPT all options are exclusive.
   */
   while (!cmdline.empty()) {
      ocs::tom::Element<ocs::tom::SPA> ep;
      static const char *options[] = {
         "-c",
         "-cj",
         "-cq",
         "-d",
         "-r",
         "-rj",
         "-rq",
         "-e",
         "-s",
         "-sj",
         "-sq",
         "-us",
         "-usj",
         "-usq",
#ifdef __SGE_QINSTANCE_STATE_DEBUG__
         "-_e",
         "-_o",
         "-_do",
         "-_u",
         "-_du",
         "-_c",
         "-_dc",
#endif
         nullptr
      };
      static const uint32_t transitions[] = {
         QI_DO_CLEARERROR,
         QI_DO_CLEARERROR | JOB_DO_ACTION,
         QI_DO_CLEARERROR | QUEUE_DO_ACTION,
         QI_DO_DISABLE | QUEUE_DO_ACTION,
         QI_DO_RESCHEDULE,
         QI_DO_RESCHEDULE | JOB_DO_ACTION,
         QI_DO_RESCHEDULE | QUEUE_DO_ACTION,
         QI_DO_ENABLE | QUEUE_DO_ACTION,
         QI_DO_SUSPEND,
         QI_DO_SUSPEND | JOB_DO_ACTION,
         QI_DO_SUSPEND | QUEUE_DO_ACTION,
         QI_DO_UNSUSPEND,
         QI_DO_UNSUSPEND | JOB_DO_ACTION,
         QI_DO_UNSUSPEND | QUEUE_DO_ACTION,
#ifdef __SGE_QINSTANCE_STATE_DEBUG__
         QI_DO_SETERROR,
         QI_DO_SETORPHANED,
         QI_DO_CLEARORPHANED,
         QI_DO_SETUNKNOWN,
         QI_DO_CLEARUNKNOWN,
         QI_DO_SETAMBIGUOUS,
         QI_DO_CLEARAMBIGUOUS,
#endif
         QI_DO_NOTHING
      };
      int i;

      if (parse_flag(cmdline, "-help",  &alp, &helpflag)) {
         usageshowed = qmod_usage(stdout, nullptr);
         break;
      }

      if (parse_flag(cmdline, "-f", &alp, pforce)) {
         continue;
      }

      i = 0;
      while (options[i] != nullptr) {
         if ((transitions[i] & QUEUE_DO_ACTION) == 0) {
            parse_multi_jobtaskslist(cmdline, options[i], &alp, reflist, true, transitions[i]);
         } else {
            ocs::tom::ListPtr<ocs::tom::ST> queues;
            if (parse_multi_stringlist<ST_name>(cmdline, options[i], &alp, queues)) {
               id_list_build_from_str_list(reflist, &alp, *queues, transitions[i], *pforce);
            }
         }
         i++;
      }

      /* we get to this point, than there are -t options without job names. We have to write an error message */
      if ((ep = cmdline.find_first<SPA_switch_val>("-t"))) {
         answer_list_add_sprintf(&alp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                 MSG_JOB_LONELY_TOPTION_S, ep.get<SPA_switch_arg>());

         break;
      }

   }

   if (!cmdline.empty()) {
      if (!usageshowed) {
         qmod_usage(stderr, nullptr);
      }
      answer_list_add_sprintf(&alp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              SFNMAX, MSG_PARSE_TOOMANYOPTIONS);
   }

   DRETURN(alp);
}

/****
 **** qmod_usage (static)
 ****
 **** displays usage of qmod on file fp.
 **** Is what nullptr, full usage will be displayed.
 ****
 **** Returns always 1.
 ****
 **** If what is a pointer to an option-string,
 **** only usage for that option will be displayed.
 ****   ** not implemented yet! **
 ****/
static int qmod_usage(
FILE *fp,
char *what
) {
   dstring ds;
   char buffer[256];

   sge_dstring_init(&ds, buffer, sizeof(buffer));

   fprintf(fp, "%s\n", feature_get_product_name(FS_SHORT_VERSION, &ds));

   if(!what) {
      /* display full usage */
      fprintf(fp,"%s qmod [options]\n", MSG_SRC_USAGE);
      fprintf(fp, "   [-c job_wc_queue_list]  %s\n", MSG_QMOD_c_OPT_USAGE);
      fprintf(fp, "   [-cj job_list]          %s\n", MSG_QMOD_c_OPT_USAGE_J);
      fprintf(fp, "   [-cq wc_queue_list]     %s\n", MSG_QMOD_c_OPT_USAGE_Q);

      fprintf(fp, "   [-d wc_queue_list]      %s\n", MSG_QMOD_d_OPT_USAGE);
      fprintf(fp, "   [-e wc_queue_list]      %s\n", MSG_QMOD_e_OPT_USAGE);
      fprintf(fp, "   [-f]                    %s\n", MSG_QMOD_f_OPT_USAGE);
      fprintf(fp, "   [-help]                 %s\n", MSG_COMMON_help_OPT_USAGE);
      fprintf(fp, "   [-r job_wc_queue_list]  %s\n", MSG_QMOD_r_OPT_USAGE);
      fprintf(fp, "   [-rj job_list]          %s\n", MSG_QMOD_r_OPT_USAGE_J);
      fprintf(fp, "   [-rq wc_queue_list]     %s\n", MSG_QMOD_r_OPT_USAGE_Q);

      fprintf(fp, "   [-s job_wc_queue_list]  %s\n", MSG_QMOD_s_OPT_USAGE);
      fprintf(fp, "   [-sj job_list]          %s\n", MSG_QMOD_s_OPT_USAGE_J);
      fprintf(fp, "   [-sq wc_queue_list]     %s\n", MSG_QMOD_s_OPT_USAGE_Q);
      fprintf(fp, "   [-us job_wc_queue_list] %s\n", MSG_QMOD_us_OPT_USAGE);
      fprintf(fp, "   [-usj job_list]         %s\n", MSG_QMOD_us_OPT_USAGE_J);
      fprintf(fp, "   [-usq wc_queue_list]    %s\n", MSG_QMOD_us_OPT_USAGE_Q);

#ifdef __SGE_QINSTANCE_STATE_DEBUG__
      fprintf(fp, "   [-_e queue_list]        %s\n", MSG_QMOD_err_OPT_ISAGE);
      fprintf(fp, "   [-_o queue_list]        %s\n", MSG_QMOD_o_OPT_ISAGE);
      fprintf(fp, "   [-_do queue_list]       %s\n", MSG_QMOD_do_OPT_ISAGE);
      fprintf(fp, "   [-_u queue_list]        %s\n", MSG_QMOD_u_OPT_ISAGE);
      fprintf(fp, "   [-_du queue_list]       %s\n", MSG_QMOD_du_OPT_ISAGE);
      fprintf(fp, "   [-_c queue_list]        %s\n", MSG_QMOD_c_OPT_ISAGE);
      fprintf(fp, "   [-_dc queue_list]       %s\n", MSG_QMOD_dc_OPT_ISAGE);
#endif
      fprintf(fp, "\n");
      fprintf(fp, "job_wc_queue_list          {job_tasks|wc_queue}[{','|' '}{job_tasks|wc_queue}[{','|' '}...]]\n");
      fprintf(fp, "job_list                   {job_tasks}[{','|' '}job_tasks[{','|' '}...]]\n");
      fprintf(fp, "job_tasks                  {{job_id'.'task_id_range}|job_name|pattern}[' -t 'task_id_range]\n");
      fprintf(fp, "task_id_range              task_id['-'task_id[':'step]]\n");
      fprintf(fp, "wc_cqueue                  %s\n", MSG_QSTAT_HELP_WCCQ);
      fprintf(fp, "wc_host                    %s\n", MSG_QSTAT_HELP_WCHOST);
      fprintf(fp, "wc_hostgroup               %s\n", MSG_QSTAT_HELP_WCHG);
      fprintf(fp, "wc_qinstance               wc_cqueue@wc_host\n");
      fprintf(fp, "wc_qdomain                 wc_cqueue@wc_hostgroup\n");
      fprintf(fp, "wc_queue                   wc_cqueue|wc_qdomain|wc_qinstance\n");
      fprintf(fp, "wc_queue_list              wc_queue[','wc_queue[','...]]\n");
   } else {
      /* display option usage */
      fprintf(fp, MSG_QDEL_not_available_OPT_USAGE_S,what);
      fprintf(fp, "\n");
   }
   return 1;
}

