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
 *  Portions of this code are Copyright 2011 Univa Inc.
 * 
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The qsub-style command line parser shared by every submit client
 */
#include <cstring>
#include <cstdlib>
#include <unistd.h>

#include "uti/config_file.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/ocs_BindingStrategy.h"
#include "sgeobj/ocs_BindingType.h"
#include "sgeobj/ocs_BindingStart.h"
#include "sgeobj/ocs_BindingStop.h"
#include "sgeobj/ocs_BindingUnit.h"
#include "sgeobj/ocs_BindingInstance.h"
#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_ckpt.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_var.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_jsv.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/ocs_Binding.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_PN.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_ULNG.h"
#include "sgeobj/tom/ocs_tom_VA.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "parse_job_cull.h"
#include "parse_qsub.h"
#include "sge_options.h"
#include "msg_common.h"

static ocs::tom::ListPtr<ocs::tom::VA> var_list_parse_from_environment(char **envp);
static int sge_parse_checkpoint_interval(const char *time_str);
static int set_yn_option(ocs::tom::ListPtr<ocs::tom::SPA> &opts, uint32_t opt, const char *arg, const char *value,
                         ocs::tom::ListPtr<ocs::tom::AN> *alpp);
static int ocs_parse_sync_switch(ocs::tom::ListPtr<ocs::tom::SPA> &opts, uint32_t opt, const char *arg,
                                 const char *value, ocs::tom::ListPtr<ocs::tom::AN> *alpp);


/** @brief Turn an argument vector into a list of options
 *
 * The first half of submitting: this recognises the switches and their
 * arguments without interpreting them, and #cull_parse_job_parameter then
 * builds the job from the result.
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param arg_list the arguments, not including `argv[0]`
 * @param envp the environment, for the options that read one
 * @param pcmdline the option list the parsed options are appended to; created if it is empty
 * @param flags any of
 *              - `FLG_USE_PSEUDOS`: the first non-switch token is the job script, what follows are job
 *                arguments; the tokens are marked as "script" or "jobarg" in `SPA_switch_val`. Without it a
 *                non-switch is added with `SPA_switch_val` "" (an argument to a non-existing switch)
 *              - `FLG_QALTER`: the non-options become the qalter-specific pseudo options (only together
 *                with `FLG_USE_PSEUDOS`), and `-h` is parsed for qalter
 *
 * @return `nullptr` when everything was fine, otherwise an answer list
 *         (`AN`) carrying one of
 *         - `STATUS_EUNKNOWN` - an internal error, a null pointer or out of memory
 *         - `STATUS_EEXIST` - an option was given more than once; a warning
 *         - `STATUS_ESEMANTIC` - an option that needs an argument had none
 *         - `STATUS_ESYNTAX` - an option argument could not be parsed
 *
 * @note MT-NOTE: cull_parse_cmdline() is MT safe
 */
ocs::tom::ListPtr<ocs::tom::AN> cull_parse_cmdline(const uint32_t prog_number, const char **arg_list, char **envp,
                                                   ocs::tom::ListPtr<ocs::tom::SPA> &pcmdline, const uint32_t flags) {
   DENTER(TOP_LAYER);

   const char **sp;
   ocs::tom::ListPtr<ocs::tom::AN> answer;
   char str[MAX_STRING_SIZE];
   ocs::tom::Element<ocs::tom::SPA> ep_opt;
   int i_ret;
   uint32_t is_qalter = flags & FLG_QALTER;
   bool is_hold_option = false;

   if (!arg_list) {
      answer_list_add(&answer, MSG_PARSE_NULLPOINTERRECEIVED, 
                      STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(answer);
   }

   sp = arg_list;

   /* reset hard/soft flag */
   int hard_soft_flag = 0;
   char scope_flag = JRS_SCOPE_GLOBAL;

   while (*sp) {
      DPRINTF("==> %s\n", *sp);
/*----------------------------------------------------------------------------*/
      /* "-a date_time */

      if (!strcmp("-a", *sp)) {
         uint32_t timeval;

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING, 
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field(s) is "date_time" */
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-a");

            DRETURN(answer);
         }

         if (!ulong_parse_date_time_from_string(&timeval, nullptr, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_WRONGTIMEFORMATEXSPECIFIEDTOAOPTION_S, *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, a_OPT, lUlong64T, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lUlong64T>(sge_gmt32_to_gmt64(timeval));

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-A account_string" */

      if (!strcmp("-A", *sp)) {

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING, 
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is account_string */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-A");
             DRETURN(answer);
         }

         DPRINTF("\"-A %s\"\n", *sp);

         ep_opt = sge_add_arg(pcmdline, A_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* either
       *    -ar ar_id
       * or
       *    -ar ar_id_list
       */
      if (prog_number == QRSTAT) {
         /* "-ar  advance_reservation */

         if (!strcmp("-ar", *sp)) {
            sp++;
            if (!*sp) {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, 
                                       "-ar");                
               DRETURN(answer);
            }

            DPRINTF("\"-ar %s\"\n", *sp);

            [[maybe_unused]] auto [ar_ids_parsed, ar_id_list] = ulong_list_parse_from_string(&answer, *sp, ",");

            ep_opt = sge_add_arg(pcmdline, ar_OPT, lListT, *(sp - 1), *sp);
            ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(ar_id_list));

            sp++;
            continue;
         }  
      } else {
         /* "-ar  advance_reservation */

         if (!strcmp("-ar", *sp)) {
            uint32_t ar_id;
            double ar_id_d;

            sp++;
            if (!*sp) {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, 
                                       ANSWER_QUALITY_ERROR, 
                                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, 
                                       "-ar");                
               DRETURN(answer);
            }

            if (!parse_ulong_val(&ar_id_d, nullptr, ocs::CEntry::Type::INT, *sp, nullptr, 0)) {
               answer_list_add(&answer, MSG_PARSE_INVALID_AR_MUSTBEUINT,
                                STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
               DRETURN(answer);
            }


            if (ar_id_d < 0) {
               answer_list_add(&answer, MSG_PARSE_INVALID_AR_MUSTBEUINT,
                                STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
               DRETURN(answer);
            }

            ar_id = ar_id_d;
            
            ep_opt = sge_add_arg(pcmdline, ar_OPT, lUlongT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lUlongT>(ar_id);

            sp++;
            continue;
         }  
      }
/*----------------------------------------------------------------------------*/
      /* "-ac context_list */
      if (!strcmp("-ac", *sp)) {
         /* next field is context_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     SFNMAX, MSG_PARSE_ACOPTIONMUSTHAVECONTEXTLISTLISTARGUMENT);
             DRETURN(answer);
         }

         DPRINTF("\"-ac %s\"\n", *sp);
         ocs::tom::ListPtr<ocs::tom::VA> variable_list;
         i_ret = var_list_parse_from_string(variable_list, *sp, 0);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                     MSG_ANSWER_WRONGCONTEXTLISTFORMATAC_S , *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, ac_OPT, lListT, *(sp - 1), *sp);
         auto lep = ocs::tom::ElementPtr<ocs::tom::VA>::create();
         lep->set<VA_variable>("+");
         variable_list->push_front(std::move(lep));
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(variable_list));

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
      /* "-b y|n" */

      if (!is_qalter && !strcmp("-b", *sp)) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-b");
             DRETURN(answer);
         }

         DPRINTF("\"-b %s\"\n", *sp);

         if (set_yn_option(pcmdline, b_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }

      /*----------------------------------------------------------------------------*/
      // -bstrategy linear | pack
      if (strcmp("-bstrategy", *sp) == 0) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-bstrategy");
            DRETURN(answer);
         }

         DPRINTF("\"-bstrategy %s\"\n", *sp);
         if (strcmp(ocs::BindingStrategy::to_string(ocs::BindingStrategy::PACKED).c_str(), *sp) == 0) {
            ep_opt = sge_add_arg(pcmdline, bstrategy_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(ocs::BindingStrategy::PACKED);
         } else {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_BSTRATEGY_INVALID_S, *sp);
            DRETURN(answer);
         }

         sp++;
         continue;
      }

      /*----------------------------------------------------------------------------*/
      // -binstance set | env | pe
      if (strcmp("-binstance", *sp) == 0) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-binstance");
            DRETURN(answer);
         }

         DPRINTF("\"-binstance %s\"\n", *sp);
         if (strcmp("set", *sp) == 0) {
            ep_opt = sge_add_arg(pcmdline, binstance_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(ocs::BindingInstance::SET);
         } else if (strcmp("env", *sp) == 0) {
            ep_opt = sge_add_arg(pcmdline, binstance_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(ocs::BindingInstance::ENV);
         } else if (strcmp("pe", *sp) == 0) {
            ep_opt = sge_add_arg(pcmdline, binstance_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(ocs::BindingInstance::PE);
         } else {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_BINSTANCE_INVALID_S, *sp);
            DRETURN(answer);
         }

         sp++;
         continue;
      }

      /*----------------------------------------------------------------------------*/
      /* "-bamount number */

      if (!strcmp("-bamount", *sp)) {
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-bamount" );
            DRETURN(answer);
         }

         int amount;
         if (!ulong_parse_binding_amount(&answer, &amount, *sp)) {
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, p_OPT, lIntT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lIntT>(amount);

         sp++;
         continue;
      }

      /*----------------------------------------------------------------------------*/
      // -bfilter <filter_string>
      if (strcmp("-bfilter", *sp) == 0) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-bfilter");
            DRETURN(answer);
         }
         DPRINTF("\"-bfilter %s\"\n", *sp);

         ep_opt = sge_add_arg(pcmdline, bfilter_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;
      }

      /*----------------------------------------------------------------------------*/
      // -bstart S | s | C | c | E | e
      if (strcmp("-bstart", *sp) == 0) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-bstart");
            DRETURN(answer);
         }

         DPRINTF("\"-bstart %s\"\n", *sp);
         ocs::BindingStart::Start bstart = ocs::BindingStart::from_string(*sp);
         if (bstart != ocs::BindingStart::UNINITIALIZED) {
            ep_opt = sge_add_arg(pcmdline, bstart_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(bstart);
         } else {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_BSTART_INVALID_S, *sp);
            DRETURN(answer);
         }

         sp++;
         continue;
      }

      /*----------------------------------------------------------------------------*/
      // -bstop s | c | e ...
      if (strcmp("-bstop", *sp) == 0) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-bstop");
            DRETURN(answer);
         }

         DPRINTF("\"-bstop %s\"\n", *sp);
         ocs::BindingStop::Stop bstop = ocs::BindingStop::from_string(*sp);
         if (bstop != ocs::BindingStop::UNINITIALIZED) {
            ep_opt = sge_add_arg(pcmdline, bstop_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(bstop);
         } else {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_BSTOP_INVALID_S, *sp);
            DRETURN(answer);
         }

         sp++;
         continue;
      }


      /*----------------------------------------------------------------------------*/
      // -bsort S | s | C | c | E | e
      if (strcmp("-bsort", *sp) == 0) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-bsort");
            DRETURN(answer);
         }

         DPRINTF("\"-bsort %s\"\n", *sp);
         std::string sort_arg = *sp;
         if (!ocs::TopologyString::contains_valid_node_names(sort_arg)) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_BSORT_INVALID_S, *sp);
            DRETURN(answer);
         }
         if (ocs::TopologyString::has_contradicting_sort_orders(sort_arg)) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_BSORT_CONTRA_S, *sp);
            DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, bsort_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);
         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      // -btype host | task
      if (strcmp("-btype", *sp) == 0) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-btype");
             DRETURN(answer);
         }

         DPRINTF("\"-btype %s\"\n", *sp);

         if (strcmp("host", *sp) == 0) {
            ep_opt = sge_add_arg(pcmdline, btype_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(ocs::BindingType::HOST);
         } else if (strcmp("slot", *sp) == 0) {
            ep_opt = sge_add_arg(pcmdline, btype_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(ocs::BindingType::SLOT);
         } else {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_BTYPE_INVALID_S, *sp);
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      // -bunit T|CT|ET|C|E|CS|ES
      if (strcmp("-bunit", *sp) == 0) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-bunit");
            DRETURN(answer);
         }

         DPRINTF("\"-bunit %s\"\n", *sp);
         ocs::BindingUnit::Unit bunit = ocs::BindingUnit::from_string(*sp);

         if (bunit != ocs::BindingUnit::UNINITIALIZED && bunit != ocs::BindingUnit::NONE) {
            ep_opt = sge_add_arg(pcmdline, bunit_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(bunit);
         } else {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_PARSE_BUNIT_INVALID_S, *sp);
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-c [op] interval */

      if (!strcmp("-c", *sp)) {
          int attr;
          long interval;

         /* next field is [op] interval */
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-c");  
            DRETURN(answer);
         }

         DPRINTF("\"%s %s\"\n", *(sp - 1), *sp);

         attr = sge_parse_checkpoint_attr(*sp);
         if (attr == 0) {
            interval = sge_parse_checkpoint_interval(*sp);
            if (!interval) {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, 
                                       SFNMAX, MSG_PARSE_CARGUMENTINVALID);
               DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, c_OPT, lLongT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lLongT>((long) interval);
         }
         else if (attr == -1) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    SFNMAX, MSG_PARSE_CSPECIFIERINVALID);
            DRETURN(answer);
         }
         else {
            ep_opt = sge_add_arg(pcmdline, c_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(attr);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-cell cell_name or -ckpt ckpt_object" */

      /* This is another small change that needed to be made to allow DRMAA to
       * resuse this method.  The -cat option doesn't actually exist for any
       * command line utility.  It is a qsub style representation of the
       * DRMAA_JOB_CATEGORY attribute.  It will be processed and removed by the
       * drmaa_job2sge_job method. */
      if (!strcmp ("-cat", *sp) || !strcmp("-cell", *sp) || !strcmp("-ckpt", *sp)) {

         /* next field is job_cat or cell_name or ckpt_object*/
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, *(sp - 1));
            DRETURN(answer);
         }

         DPRINTF("\"%s %s\"\n", *(sp - 1), *sp);

         ep_opt = sge_add_arg(pcmdline, 0, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-clear" */

      if (!strcmp("-clear", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         ep_opt = sge_add_noarg(pcmdline, clear_OPT, *sp, nullptr);

         sp++;
         continue;
      }

      /*-----------------------------------------------------------------------------*/
      /* "-explain" */

      if (!strcmp("-explain", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         ep_opt = sge_add_noarg(pcmdline, explain_OPT, *sp, nullptr);

         sp++;
         continue;
      }

      /*-----------------------------------------------------------------------------*/
      /* -ectx */
      if (!strcmp("-ectx", *sp)) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-ectx");
            DRETURN(answer);
         }

         DPRINTF("\"-ectx %s\"\n", *sp);
         ep_opt = sge_add_arg(pcmdline, ectx_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;
      }

      /*-----------------------------------------------------------------------------*/
      /* -fmt */
      if (!strcmp("-fmt", *sp)) {
          if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-fmt");
            DRETURN(answer);
         }

         DPRINTF("\"-fmt %s\"\n", *sp);
         ep_opt = sge_add_arg(pcmdline, fmt_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;
      }

      /*-----------------------------------------------------------------------------*/
      /* "-xml" */

      if (!strcmp("-xml", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         ep_opt = sge_add_noarg(pcmdline, xml_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-cwd" is mapped as -wd with nullptr path, this case is handled in the second parsing stage*/

      if (!strcmp("-cwd", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, wd_OPT, "-wd", nullptr);
         DPRINTF("\"%s\"\n", *sp);
         
         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-C directive_prefix" */
      if (!strcmp("-C", *sp)) {

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is directive_prefix */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                      MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-C");
             DRETURN(answer);
         }

         DPRINTF("\"-C %s\"\n", *sp);

         ep_opt = sge_add_arg(pcmdline, C_OPT, lStringT, *(sp - 1), *sp);

         if (strlen(*sp) > 0) {
            ep_opt.set<SPA_argval_lStringT>(*sp);
         }

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-d time */
      if (!strcmp("-d", *sp)) {
         uint32_t timeval;
         char tmp[1000];

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is "time" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-d");
             DRETURN(answer);
         }

         DPRINTF("\"-d %s\"\n", *sp);

         if (!parse_ulong_val(nullptr, &timeval, ocs::CEntry::Type::TIME, *sp, tmp, sizeof(tmp)-1)) {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_ANSWER_WRONGTIMEFORMATEXSPECIFIEDTODOPTION_S, *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, d_OPT, lUlong64T, *(sp-1), *sp);
         ep_opt.set<SPA_argval_lUlong64T>(sge_gmt32_to_gmt64(timeval));

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
      /* "-dc simple_context_list */
      if (!strcmp("-dc", *sp)) {
         /* next field is simple_context_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     SFNMAX, MSG_PARSE_DCOPTIONMUSTHAVESIMPLECONTEXTLISTARGUMENT);
             DRETURN(answer);
         }

         DPRINTF("\"-dc %s\"\n", *sp);
         ocs::tom::ListPtr<ocs::tom::VA> variable_list;
         i_ret = var_list_parse_from_string(variable_list, *sp, 0);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                     MSG_PARSE_WRONGCONTEXTLISTFORMATDC_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, dc_OPT, lListT, *(sp - 1), *sp);
         auto lep = ocs::tom::ElementPtr<ocs::tom::VA>::create();
         lep->set<VA_variable>("-");
         variable_list->push_front(std::move(lep));
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(variable_list));

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-dept name" */
      if (!strcmp("-dept", *sp)) {
         DTRACE;

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            snprintf(str, sizeof(str), MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
            answer_list_add(&answer, str, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
         }

         /* next field is the department name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-dept");
             DRETURN(answer);
         }

         DPRINTF("\"-dept %s\"\n", *sp);
         ep_opt = sge_add_arg(pcmdline, dept_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-display -- only for qsh " */

      if (!strcmp("-display", *sp)) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is display to redirect interactive jobs to */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     SFNMAX, MSG_PARSE_DISPLAYOPTIONMUSTHAVEARGUMENT);
             DRETURN(answer);
         }

         DPRINTF("\"-display %s\"\n", *sp);

         ep_opt = sge_add_arg(pcmdline, display_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;

      }

/*----------------------------------------------------------------------------*/
      /* "-dl date_time */

      if (!strcmp("-dl", *sp)) {
         uint32_t timeval;

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING, 
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field(s) is "date_time" */
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-dl");
            DRETURN(answer);
         }

         if (!ulong_parse_date_time_from_string(&timeval, nullptr, *sp)) {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_WRONGTIMEFORMATXSPECTODLOPTION_S, *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, dl_OPT, lUlongT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lUlong64T>(sge_gmt32_to_gmt64(timeval));

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-e path_name" */

      if (!strcmp("-e", *sp)) {
         if (prog_number == QRSUB) {
            if (pcmdline->find_first<SPA_switch_val>(*sp)) {
               answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                       MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
            }
         }

         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, 
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-e");
             DRETURN(answer);
         }

         DPRINTF("\"-e %s\"\n", *sp);

         if (prog_number == QRSUB) {
            uint32_t timeval;
            if (!ulong_parse_date_time_from_string(&timeval, nullptr, *sp)) {
               answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                       MSG_ANSWER_WRONGTIMEFORMATEXSPECIFIEDTOAOPTION_S, *sp);
               DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, e_OPT, lUlong64T, *(sp-1), *sp);
            ep_opt.set<SPA_argval_lUlong64T>(sge_gmt32_to_gmt64(timeval));
         } else {
            /* next field is path_name */
            auto [path_list_ret, path_list] = cull_parse_path_list(*sp);
            i_ret = path_list_ret;
            if (i_ret) {
                answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                        MSG_PARSE_WRONGPATHLISTFORMATXSPECTOEOPTION_S, *sp);
                DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, e_OPT, lListT, *(sp-1), *sp);
            ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(path_list));
         }

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-i path_name" */

      if (!strcmp("-i", *sp)) {
         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-i");
             DRETURN(answer);
         }

         DPRINTF("\"-i %s\"\n", *sp);

         auto [path_list_ret, path_list] = cull_parse_path_list(*sp);
         i_ret = path_list_ret;
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_WRONGPATHLISTFORMATXSPECTOEOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, i_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(path_list));

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-h [hold_list]" */

      if (!strcmp("-h", *sp)) {
            int hold;
            const char *cmd_switch;
            const char *cmd_arg = "";
         is_hold_option = true;
         cmd_switch = *sp;

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         if (is_qalter) {
            /* next field is hold_list */
            sp++;
            if (!*sp) {
                answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, 
                                        MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-h");
                DRETURN(answer);
            }
            hold = sge_parse_hold_list(*sp, prog_number);
            if (hold == -1) {
                answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                        MSG_PARSE_UNKNOWNHOLDLISTXSPECTOHOPTION_S, *sp);
                DRETURN(answer);
            }
            cmd_arg = *sp;
         }
         else {
            hold = MINUS_H_TGT_USER;
         }
         DPRINTF("\"-h %s\"\n", *sp);

         ep_opt = sge_add_arg(pcmdline, h_OPT, lIntT, cmd_switch, cmd_arg);
         ep_opt.set<SPA_argval_lIntT>(hold);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-hard" */

      if (!strcmp("-hard", *sp)) {

         DPRINTF("\"%s\"\n", *sp);

         hard_soft_flag = 1;
         ep_opt = sge_add_noarg(pcmdline, hard_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
     /*  -he y[es]|n[o] */

      if(!strcmp("-he", *sp)) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            snprintf(str, sizeof(str), MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
            answer_list_add(&answer, str, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
         }
         /* next field is yes/no switch */
         sp++;
         if(!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-he");
            DRETURN(answer);
         }
         
         DPRINTF("\"-he %s\"\n", *sp);
         
         if (set_yn_option(pcmdline, he_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-help" */

      if (!strcmp("-help", *sp)) {
         DPRINTF("\"%s\"\n", *sp);

         ep_opt = sge_add_noarg(pcmdline, help_OPT, *sp, nullptr);

         sp++;
         continue;

      }

/*-----------------------------------------------------------------------------*/
      /* "-hold_jid jid[,jid,...]" */

      if (!strcmp("-hold_jid", *sp)) {
         /* next field is hold_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                      MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-hold_jid");
             DRETURN(answer);
         }

         DPRINTF("\"-hold_jid %s\"\n", *sp);
         auto [jid_hold_list_ret, jid_hold_list] = cull_parse_jid_hold_list(*sp);
         i_ret = jid_hold_list_ret;
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                        MSG_PARSE_WRONGJIDHOLDLISTFORMATXSPECTOHOLDJIDOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, hold_jid_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(jid_hold_list));

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-hold_jid_ad jid[,jid,...]" */

      if (!strcmp("-hold_jid_ad", *sp)) {
         /* next field is hold_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                      MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-hold_jid_ad");
             DRETURN(answer);
         }

         DPRINTF("\"-hold_jid_ad %s\"\n", *sp);
         auto [jid_ad_hold_list_ret, jid_ad_hold_list] = cull_parse_jid_hold_list(*sp);
         i_ret = jid_ad_hold_list_ret;
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                        MSG_PARSE_WRONGJIDHOLDLISTFORMATXSPECTOHOLDJIDADOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, hold_jid_ad_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(jid_ad_hold_list));

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-j y|n" */

      if (!strcmp("-j", *sp)) {

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                   MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-j");
             DRETURN(answer);
         }

         DPRINTF("\"-j %s\"\n", *sp);

         if (set_yn_option(pcmdline, j_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-js jobshare */

      if (!strcmp("-js", *sp)) {
         uint32_t jobshare;
         double jobshare_d;

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-js");
            DRETURN(answer);
         }

         if (!parse_ulong_val(&jobshare_d, nullptr, ocs::CEntry::Type::INT, *sp, nullptr, 0)) {
            answer_list_add(&answer, MSG_PARSE_INVALIDJOBSHAREMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }


         if (jobshare_d < 0) {
            answer_list_add(&answer, MSG_PARSE_INVALIDJOBSHAREMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }

         jobshare = jobshare_d;
         
         ep_opt = sge_add_arg(pcmdline, js_OPT, lUlongT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lUlongT>(jobshare);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-jsv path_name" */

      if (!strcmp("-jsv", *sp)) {
         dstring input = DSTRING_INIT;
         dstring type = DSTRING_INIT;
         dstring user = DSTRING_INIT;
         dstring path = DSTRING_INIT;
         bool success;

         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-jsv" );
             DRETURN(answer);
         }

         DPRINTF("\"-jsv %s\"\n", *sp);

         sge_dstring_append(&input, *sp);
         success = jsv_url_parse(&input, &answer, &type, &user, &path, true);
         sge_dstring_free(&input);
         sge_dstring_free(&type);
         sge_dstring_free(&user);
         sge_dstring_free(&path);
         if (success) {
            auto path_list = ocs::tom::ListPtr<ocs::tom::PN>::create("");
            const auto elem = path_list->emplace_back<PN_path>(*sp);

            if (elem) {
               ep_opt = sge_add_arg(pcmdline, jsv_OPT, lListT, *(sp - 1), *sp);
               ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(path_list));
            } else {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                       SFNMAX, MSG_SGETEXT_NOMEM);
               DRETURN(answer);
            }
         } else {
            /* answer is filled in jsv_url_parse */
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*---------------------------------------------------------------------------*/
      /* "-l resource_list" */

      if (!strcmp("-l", *sp)) {
         /* next field is resource_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-l" );
             DRETURN(answer);
         }

         DPRINTF("\"-l %s\"\n", *sp);

         auto resource_list = centry_list_parse_from_string(ocs::tom::ListPtr<ocs::tom::CE>(), *sp, false);
         if (!resource_list) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_WRONGRESOURCELISTFORMATXSPECTOLOPTION_S, *sp);
             DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, l_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(resource_list));
         ep_opt.set<SPA_argval_lIntT>(hard_soft_flag);
         ep_opt.set<SPA_argval_lLongT>(scope_flag);

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
      /* "-m mail_options */

      if (!strcmp("-m", *sp)) {
         int mail_options;

         /* next field is mail_options */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-m");
             DRETURN(answer);
         }

         DPRINTF("\"-m %s\"\n", *sp);
         mail_options = sge_parse_mail_options(&answer, *sp, prog_number);
         if (!mail_options) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGMAILOPTIONSLISTFORMATXSPECTOMOPTION_S, *sp);
             DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, m_OPT, lIntT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lIntT>(mail_options);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-masterq destination_identifier_list" */

      if (!strcmp("-masterq", *sp)) {
         /* next field is destination_identifier */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEDESTIDLISTARGUMENT_S, "-masterq");
             DRETURN(answer);
         }

         DPRINTF("\"-masterq %s\"\n", *sp);
         lList *ids_out = nullptr;
         i_ret = cull_parse_destination_identifier_list(&ids_out, *sp);
         /* cull_parse_destination_identifier_list() creates the list with QR, so it is taken over unchecked */
         auto id_list = ocs::tom::ListPtr<ocs::tom::QR>::from_typed(ids_out);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_WRONGDESTIDLISTFORMATXSPECTOXOPTION_SS, *sp, "-masterq");
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, masterq_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(id_list));
         ep_opt.set<SPA_argval_lIntT>(0);
         ep_opt.set<SPA_argval_lLongT>(JRS_SCOPE_MASTER);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-M mail_list" */

      if (!strcmp("-M", *sp)) {
         /* next field is mail_list */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-M" );
             DRETURN(answer);
         }

         DPRINTF("\"-M %s\"\n", *sp);

         auto [mail_ret, mail_list] = mailrec_parse(*sp);
         i_ret = mail_ret;
          if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGMAILLISTFORMATXSPECTOMOPTION_S, *sp);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, M_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(mail_list));

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-N name" */

      if (!strcmp("-N", *sp)) {

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                  MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }

         /* next field is name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-N" );
             DRETURN(answer);
         }
         if (strchr(*sp, '/')) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                 SFNMAX, MSG_PARSE_ARGUMENTTONOPTIONMUSTNOTCONTAINBSL );
             DRETURN(answer);
         }

         if (**sp == '\0') {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    SFNMAX, MSG_PARSE_EMPTYSTRINGARGUMENTTONOPTIONINVALID);
             DRETURN(answer);
         }

         DPRINTF("\"-N %s\"\n", *sp);
         ep_opt = sge_add_arg(pcmdline, N_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-notify" */

      if (!strcmp("-notify", *sp)) {

         DPRINTF("\"-notify\"\n");

         ep_opt = sge_add_noarg(pcmdline, notify_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
     /*  -now y[es]|n[o] */

      if(!strcmp("-now", *sp)) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                     MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }
         /* next field is yes/no switch */
         sp++;
         if(!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                      MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-now" );
            DRETURN(answer);
         }
         
         DPRINTF("\"-now %s\"\n", *sp);
         
         if (set_yn_option(pcmdline, now_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
            
/*----------------------------------------------------------------------------*/
      /* "-o path_name" */

      if (!strcmp("-o", *sp)) {
         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-o" );
             DRETURN(answer);
         }

         DPRINTF("\"-o %s\"\n", *sp);
         auto [stdout_path_list_ret, stdout_path_list] = cull_parse_path_list(*sp);
         i_ret = stdout_path_list_ret;
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGSTDOUTPATHLISTFORMATXSPECTOOOPTION_S, *sp );
             DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, o_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(stdout_path_list));

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-ot override tickets */

      if (!strcmp("-ot", *sp)) {
         int otickets;
         double otickets_d;

         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-ot");
             DRETURN(answer);
         }

         if (!parse_ulong_val(&otickets_d, nullptr, ocs::CEntry::Type::INT, *sp, nullptr, 0)) {
            answer_list_add(&answer, MSG_PARSE_INVALIDOTICKETSMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }


         if (otickets_d < 0) {
            answer_list_add(&answer, MSG_PARSE_INVALIDOTICKETSMUSTBEUINT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }

         otickets = otickets_d;
         
         ep_opt = sge_add_arg(pcmdline, ot_OPT, lIntT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lUlongT>(otickets);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-p priority */

      if (!strcmp("-p", *sp)) {
         int priority;

         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-p" );
             DRETURN(answer);
         }

         if (!ulong_parse_priority(&answer, &priority, *sp)) {
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, p_OPT, lIntT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lIntT>(priority);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/

      /* "-P name" */

      if (!strcmp("-P", *sp)) {

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            snprintf(str, sizeof(str), MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
            answer_list_add(&answer, str, STATUS_EEXIST, ANSWER_QUALITY_WARNING);
         }

         /* next field is the projects name */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-P");
             DRETURN(answer);
         }

         DPRINTF("\"-P %s\"\n", *sp);
         ep_opt = sge_add_arg(pcmdline, P_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;
      }

      /*-----------------------------------------------------------------------------*/
      /* "-par" */

      if (strcmp("-par", *sp) == 0) {

         DPRINTF("\"%s\"\n", *sp);

         sp++;
         if (*sp == nullptr) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-par");
            DRETURN(answer);
         }

         // check if the given allocation rule (e.g., $pe_slots) is correct
         if (replace_params(*sp, nullptr, 0, pe_alloc_rule_variables) != 0) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_INVALIDOPTIONARGUMENT_SS, "-par", *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, par_OPT, lStringT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lLongT>(scope_flag);

         sp++;
         continue;
      }

      /*----------------------------------------------------------------------------*/
      if (!strcmp("-pe", *sp)) {
         dstring d_arg = DSTRING_INIT;

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }
DTRACE;
         /* next field must be the pe_name ... */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    SFNMAX, MSG_PARSE_PEOPTIONMUSTHAVEPENAMEARGUMENT );
             sge_dstring_free(&d_arg);
             DRETURN(answer);
         }

         /* ... followed by a range */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_PEOPTIONMUSTHAVERANGEAS2NDARGUMENT, 
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             sge_dstring_free(&d_arg);
             DRETURN(answer);
         }

         ocs::tom::ListPtr<ocs::tom::RN> pe_range;
         range_list_parse_from_string(pe_range, &answer, *sp, false, false, INF_ALLOWED);

         if (!pe_range) {
            sge_dstring_free(&d_arg);
            DRETURN(answer);
         }
         sge_dstring_append(&d_arg, *(sp-1));
         sge_dstring_append(&d_arg, " ");
         sge_dstring_append(&d_arg, *sp);
         ep_opt = sge_add_arg(pcmdline, pe_OPT, lStringT, *(sp - 2), sge_dstring_get_string(&d_arg));
         ep_opt.set<SPA_argval_lStringT>(*(sp - 1));
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(pe_range));

         sp++;

         sge_dstring_free(&d_arg);

         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-q destination_identifier_list" */

      if (!strcmp("-q", *sp)) {
         /* next field is destination_identifier */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEDESTIDLISTARGUMENT_S, "-q");
             DRETURN(answer);
         }

         DPRINTF("\"-q %s\"\n", *sp);
         lList *ids_out = nullptr;
         i_ret = cull_parse_destination_identifier_list(&ids_out, *sp);
         /* cull_parse_destination_identifier_list() creates the list with QR, so it is taken over unchecked */
         auto id_list = ocs::tom::ListPtr<ocs::tom::QR>::from_typed(ids_out);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGDESTIDLISTFORMATXSPECTOXOPTION_SS, *sp, "-q");
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, q_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(id_list));
         ep_opt.set<SPA_argval_lIntT>(hard_soft_flag);
         ep_opt.set<SPA_argval_lLongT>(scope_flag);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-r y|n" */

      if (!strcmp("-r", *sp)) {


         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-r" );
            DRETURN(answer);
         }

         DPRINTF("\"-r %s\"\n", *sp);

         if ((strcasecmp("y", *sp) == 0) || (strcasecmp("yes", *sp) == 0)) {
            ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(1);
         } else if ((strcasecmp ("n", *sp) == 0) || (strcasecmp ("no", *sp) == 0)) {
            ep_opt = sge_add_arg(pcmdline, r_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(2);
         } else {
            answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_INVALIDOPTIONARGUMENTRX_S, *sp );
            DRETURN(answer);
         }

         sp++;
         continue;

      }

/*-----------------------------------------------------------------------------*/
      /* "-R y|n" */

      if (!strcmp("-R", *sp)) {

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                 MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-R" );
             DRETURN(answer);
         }

         DPRINTF("\"-R %s\"\n", *sp);

         if (set_yn_option(pcmdline, R_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-sc variable_list" */
      /* set context */

      if (!strcmp("-sc", *sp)) {
         /* next field is context_list  [ == variable_list ] */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_SCOPTIONMUSTHAVECONTEXTLISTARGUMENT, 
                     STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-sc %s\"\n", *sp);
         ocs::tom::ListPtr<ocs::tom::VA> variable_list;
         i_ret = var_list_parse_from_string(variable_list, *sp, 0);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                          MSG_PARSE_WRONGCONTEXTLISTFORMATSCX_S, *sp );
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, sc_OPT, lListT, *(sp - 1), *sp);
         auto lep = ocs::tom::ElementPtr<ocs::tom::VA>::create();
         lep->set<VA_variable>("=");
         variable_list->push_front(std::move(lep));
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(variable_list));

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-scope" */

      if (strcmp("-scope", *sp) == 0) {

         DPRINTF("\"%s\"\n", *sp);

         sp++;
         if (*sp == nullptr) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-scope");
            DRETURN(answer);
         }

         if (!job_parse_scope_string(*sp, scope_flag)) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_INVALIDOPTIONARGUMENT_SS, "-scope", *sp);
            DRETURN(answer);
         }

         ep_opt = sge_add_noarg(pcmdline, scope_OPT, *(sp - 1), nullptr);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-soft" */

      if (!strcmp("-soft", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         hard_soft_flag = 2;
         ep_opt = sge_add_noarg(pcmdline, soft_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* "-shell y|n" */

      if (!is_qalter && !strcmp("-shell", *sp)) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp );
         }

         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-shell");
             DRETURN(answer);
         }

         DPRINTF("\"-shell %s\"\n", *sp);

         if (set_yn_option (pcmdline, shell_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }


/*----------------------------------------------------------------------------*/
     /*  -sync y[es]|n[o] */
     /*  -sync r|x|n */

      if (!strcmp("-sync", *sp)) {
         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer, STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S, *sp);
         }
         sp++;
         if(!*sp) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-sync");
            DRETURN(answer);
         }
         
         DPRINTF("\"-sync %s\"\n", *sp);
         
         if (ocs_parse_sync_switch(pcmdline, sync_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
            
/*----------------------------------------------------------------------------*/
      /* "-S path_name" */

      if (!strcmp("-S", *sp)) {
         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_SOPTIONMUSTHAVEPATHNAMEARGUMENT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-S %s\"\n", *sp);

         auto [shell_list_ret, shell_list] = cull_parse_path_list(*sp);
         i_ret = shell_list_ret;
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_WRONGSHELLLISTFORMATXSPECTOSOPTION_S, *sp );
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, S_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(shell_list));

         sp++;
         continue;
      }

/*----------------------------------------------------------------------------*/
      /* -t <TaskIdRange>
       */

      if (!strcmp("-t", *sp)) {
         /* next field is path_name */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_TOPTIONMUSTHAVEALISTOFTASKIDRANGES,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-t %s\"\n", *sp);

         ocs::tom::ListPtr<ocs::tom::RN> task_id_range_list;
         range_list_parse_from_string(task_id_range_list, &answer, *sp, false, true, INF_NOT_ALLOWED);
         if (!task_id_range_list) {
            DRETURN(answer);
         }

         range_list_sort_uniq_compress(*task_id_range_list, &answer, false);
         if (task_id_range_list->size() > 1) {
            answer_list_add(&answer, MSG_QCONF_ONLYONERANGE, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }


         ep_opt = sge_add_arg(pcmdline, t_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(task_id_range_list));

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-tc number */

      if (!strcmp("-tc", *sp)) {
         int number;

         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                       MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-tc" );
             DRETURN(answer);
         }

         if (!ulong_parse_task_concurrency(&answer, &number, *sp)) {
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, tc_OPT, lUlongT, *(sp - 1), *sp);
         ep_opt.set<SPA_argval_lUlongT>(number);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-terse" */

      if(!strcmp("-terse", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, terse_OPT, *sp, nullptr);
         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-u" */
      if (!strcmp("-u", *sp)) {
         /* next field is user_list */
         sp++;
         if (!*sp) {
            answer_list_add(&answer, MSG_PARSE_UOPTMUSTHAVEALISTUSERNAMES,
                  STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
            DRETURN(answer);
         }
 
         DPRINTF("\"-u %s\"\n", *sp);

         if (prog_number == QRSUB) {
            int rule[] = {ARA_name, 0};
            char **dest = nullptr;
            char *tmp;
            // cull_parse_string_list() works on any descriptor and has no typed form
            lList *user_list = nullptr;

            tmp = sge_strdup(nullptr, *sp);
            dest = string_list(tmp, ",", nullptr);
            cull_parse_string_list(dest, "user_list", ocs::tom::ARA::descriptor.e, rule, &user_list);
            sge_free(&tmp);
            sge_free(&dest);
            ep_opt = sge_add_arg(pcmdline, u_OPT, lListT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lListT>(user_list);
         } else {
            auto user_list = str_list_parse_from_string(*sp, ",");
            ep_opt = sge_add_arg(pcmdline, u_OPT, lListT, *(sp - 1), *sp);
            ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(user_list));
         }
         sp++;

         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-uall " */
      if (!strcmp("-uall", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, u_OPT, *sp, nullptr);

         DPRINTF("\"-uall \"\n");
         sp++;
  
         continue;
      }           

/*-----------------------------------------------------------------------------*/
      /* "-v variable_list" */

      if (!strcmp("-v", *sp)) {
         /* next field is variable_list */
         sp++;
         if (!*sp) {
             answer_list_add(&answer, MSG_PARSE_VOPTMUSTHAVEVARIABLELISTARGUMENT,
                             STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-v %s\"\n", *sp);

         ocs::tom::ListPtr<ocs::tom::VA> variable_list;
         i_ret = var_list_parse_from_string(variable_list, *sp, 1);
         if (i_ret) {
             answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                  MSG_PARSE_WRONGVARIABLELISTFORMATVORENVIRONMENTVARIABLENOTSET_S, *sp );
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, v_OPT, lListT, *(sp - 1), *sp);
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(variable_list));

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-verify" */

      if (!strcmp("-verify", *sp)) {

         DPRINTF("\"%s\"\n", *sp);
         ep_opt = sge_add_noarg(pcmdline, verify_OPT, *sp, nullptr);

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-V" */

      if (!strcmp("-V", *sp)) {
         DPRINTF("\"%s\"\n", *sp);
         auto env_list = var_list_parse_from_environment(envp);
         if (!env_list) {
             answer_list_add(&answer, MSG_PARSE_COULDNOTPARSEENVIRIONMENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }
         ep_opt = sge_add_arg(pcmdline, V_OPT, lListT, *sp, "");
         ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(env_list));

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-w e|w|n|v" */

      if (!strcmp("-w", *sp)) {
         int level;
         int lret;

         if (pcmdline->find_first<SPA_switch_val>(*sp)) {
            answer_list_add_sprintf(&answer,STATUS_EEXIST, ANSWER_QUALITY_WARNING,
                                    MSG_PARSE_XOPTIONALREADYSETOVERWRITINGSETING_S,
                                    *sp);
         }

         /* next field is "e|w|n|v|p" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, "-w");
             DRETURN(answer);
         }

         DPRINTF("\"-w %s\"\n", *sp);
         lret = job_parse_validation_level(&level, *sp, prog_number, &answer);
         if (!lret) {
            DRETURN(answer);
         } else {
            ep_opt = sge_add_arg(pcmdline, w_OPT, lIntT, *(sp - 1), *sp);
            ep_opt.set<SPA_argval_lIntT>(level);
         }

         sp++;
         continue;

      }
      /*-----------------------------------------------------------------------------*/
      /* "-wd" */

      if (!strcmp("-wd", *sp)) {

         sp++;
         if (!*sp) {
            answer_list_add_sprintf(&answer,STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                  MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, *(sp - 1));
            DRETURN(answer);
         }

         ep_opt = sge_add_arg(pcmdline, wd_OPT, lStringT, "-wd", *sp);
         ep_opt.set<SPA_argval_lStringT>(*sp);

         sp++;
         continue;
      }

      /*-----------------------------------------------------------------------------*/
      /* "-when" */

      if (strcmp("-when", *sp) == 0) {
         sp++;
         if (*sp == nullptr) {
            answer_list_add_sprintf(&answer,STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                  MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S, *(sp - 1));
            DRETURN(answer);
         }
         qalter_when_t when;
         if (!job_parse_when_string(*sp, when)) {
            answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                    MSG_PARSE_INVALIDOPTIONARGUMENT_SS, "-when", *sp);
            DRETURN(answer);
         } else {
            ep_opt = sge_add_arg(pcmdline, when_OPT, lUlongT, "-when", *sp);
            ep_opt.set<SPA_argval_lUlongT>(when);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-@" */
      /* reentrancy is built upon here */

      if (!strcmp("-@", *sp)) {
         int do_exit = 0;

         sp++;
         if (!*sp) {
             snprintf(str, sizeof(str), SFNMAX, MSG_PARSE_ATSIGNOPTIONMUSTHAVEFILEARGUMENT);
             answer_list_add(&answer, str, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
             DRETURN(answer);
         }

         DPRINTF("\"-@ %s\"\n", *sp);

         auto alp = parse_script_file(prog_number, *sp, "", pcmdline, envp, FLG_USE_NO_PSEUDOS); /* MT-NOTE: !!!! */
         if (answer_list_has_error(&alp)) {
            do_exit = 1;
         }
         // the answer list is usually still nullptr here: lAppendElem() refused to append to it, so the
         // errors of the options file were lost and the caller saw none (CS-3122)
         answer_list_append_list(&answer, &alp);

         if (do_exit) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* "-verbose" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-verbose", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, verbose_OPT, *sp, nullptr);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-inherit" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-inherit", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, inherit_OPT, *sp, nullptr);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-pty" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-pty", *sp)) {
         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-pty");
             DRETURN(answer);
         }

         DPRINTF("\"-pty %s\"\n", *sp);

         if (set_yn_option(pcmdline, pty_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-suspend_remote" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-suspend_remote", *sp)) {
         /* next field is "y|n" */
         sp++;
         if (!*sp) {
             answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                     MSG_PARSE_XOPTIONMUSTHAVEARGUMENT_S,"-suspend_remote");
             DRETURN(answer);
         }

         DPRINTF("\"-suspend_remote %s\"\n", *sp);

         if (set_yn_option(pcmdline, suspend_remote_OPT, *(sp - 1), *sp, &answer) != STATUS_OK) {
            DRETURN(answer);
         }

         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-nostdin" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-nostdin", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, nostdin_OPT, *sp, nullptr);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-noshell" - accept, but do nothing, must be handled by caller */

      if(!strcmp("-noshell", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, noshell_OPT, *sp, nullptr);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-X" — enable X11 forwarding for builtin IJS mode; handled by caller */

      if(!strcmp("-X", *sp)) {
         ep_opt = sge_add_noarg(pcmdline, x11_OPT, *sp, nullptr);
         sp++;
         continue;
      }
/*-----------------------------------------------------------------------------*/
      /* "-huh?" */

      if (!strncmp("-", *sp, 1) && strcmp("--", *sp)) {
         answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
              MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp );
         DRETURN(answer);
      }

/*-----------------------------------------------------------------------------*/
      /* - for read script from stdin */

      if (!strcmp("-", *sp)) {
         DPRINTF("\"%s\"\n", *sp);

         if ((flags & FLG_USE_PSEUDOS)) {
            if (!is_qalter) {
               ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_SCRIPT, nullptr);
               ep_opt.set<SPA_argval_lStringT>(*sp);
               for (sp++; *sp; sp++) {
                  ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_JOBARG, nullptr);
                  ep_opt.set<SPA_argval_lStringT>(*sp);
               }
            } else {
               answer_list_add_sprintf(&answer, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
               DRETURN(answer);
            }
            continue;
         }
         else {
            ep_opt = sge_add_noarg(pcmdline, 0, *sp, nullptr);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/
      /* -- separator for qalter between job ids and job args */

      if (!strcmp("--", *sp)) {
         DPRINTF("\"%s\"\n", *sp);

         if ((flags & FLG_USE_PSEUDOS)) {
            sp++;
            if (!*sp) {
               answer_list_add(&answer, MSG_PARSE_OPTIONMUSTBEFOLLOWEDBYJOBARGUMENTS,
                               STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
               DRETURN(answer);
            }
            for (; *sp; sp++) {
               ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_JOBARG, nullptr);
               ep_opt.set<SPA_argval_lStringT>(*sp);
            }
            continue;
         }
         else {
            ep_opt = sge_add_noarg(pcmdline, 0, *sp, nullptr);
         }

         sp++;
         continue;
      }

/*-----------------------------------------------------------------------------*/

      DPRINTF("===%s===\n", *sp);

      /*
      ** with qsub and qsh this can only
      ** be a script file, remember this
      ** in case of qalter we need jobids
      */
      if ((flags & FLG_USE_PSEUDOS)) {
         if (!is_qalter) {
            ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_SCRIPT, nullptr);
            ep_opt.set<SPA_argval_lStringT>(*sp);
            for (sp++; *sp; sp++) {
               ep_opt = sge_add_arg(pcmdline, 0, lStringT, STR_PSEUDO_JOBARG, nullptr);
               ep_opt.set<SPA_argval_lStringT>(*sp);
            }
         } else {
            if (!strcmp(*sp, "--")) {
               ep_opt = sge_add_noarg(pcmdline, 0, *sp, nullptr);
               break;
            }
            auto [jid_list_ret, jid_list] = cull_parse_jid_hold_list(*sp);
            i_ret = jid_list_ret;

            if (i_ret) {
               answer_list_add_sprintf(&answer, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                    MSG_PARSE_WRONGJOBIDLISTFORMATXSPECIFIED_S, *sp );
               DRETURN(answer);
            }
            ep_opt = sge_add_arg(pcmdline, 0, lListT, STR_PSEUDO_JOBID, *sp);
            ocs::tom::set<SPA_argval_lListT>(ep_opt.raw(), std::move(jid_list));
            sp++;
         }
         continue;
      } else {
         ep_opt = sge_add_arg(pcmdline, 0, lStringT, "", nullptr);
         ep_opt.set<SPA_argval_lStringT>(*sp);
      }

      sp++;
   }

   if (!is_hold_option) {
      if (prog_number == QHOLD) { 
         ep_opt = sge_add_arg(pcmdline, h_OPT, lIntT, "-h", "u");
         ep_opt.set<SPA_argval_lIntT>(MINUS_H_TGT_USER);

      } else if (prog_number == QRLS) {
         ep_opt = sge_add_arg(pcmdline, h_OPT, lIntT, "-h", "u");
         ep_opt.set<SPA_argval_lIntT>(MINUS_H_CMD_SUB | MINUS_H_TGT_USER);
      }
   }

   DRETURN(answer);
}


/***************************************************************************/
static int sge_parse_checkpoint_interval(
        const char *time_str
) {
   DENTER(TOP_LAYER);

   uint32_t seconds;

   DPRINTF("--------time_string: %s\n", time_str);
   if (!parse_ulong_val(nullptr, &seconds, ocs::CEntry::Type::TIME, time_str, nullptr, 0))
      seconds = 0;

     DPRINTF("-------- seconds: %d\n", (int) seconds);
   DRETURN(seconds);
}

/***************************************************************************/

/**
 * @brief Create a VA variable list from an environment array.
 *
 * Creates one VA element per "NAME=value" entry of @p envp. A name given
 * twice keeps one element with the last value (CS-3053). A degenerate entry
 * that carries no name (an all-'=' string such as "=") is skipped rather than
 * asserted on, so a crafted ambient environment cannot crash the parse.
 * MT-NOTE: MT safe.
 *
 * @param[in] envp NULL-terminated environment array ("NAME=value" entries)
 * @return the variable list, an empty handle if @p envp is nullptr
 */
static ocs::tom::ListPtr<ocs::tom::VA> var_list_parse_from_environment(char **envp) {
   DENTER(TOP_LAYER);

   if (envp == nullptr) {
      DRETURN(ocs::tom::ListPtr<ocs::tom::VA>());
   }

   auto env_list = ocs::tom::ListPtr<ocs::tom::VA>::create("env list");

   for (; *envp; envp++) {
      struct saved_vars_s *context = nullptr;

      char *env_entry = sge_strdup(nullptr, *envp);
      SGE_ASSERT((env_entry));

      char *env_name = sge_strtok_r(env_entry, "=", &context);
      // sge_strtok_r() returns nullptr for an all-delimiter entry (e.g. "="),
      // which the old SGE_ASSERT turned into an abort / NULL deref on a crafted
      // ambient environment (CS-2350, CWE-476/CWE-617). Skip such a nameless
      // entry instead of crashing the whole -V parse.
      if (env_name == nullptr) {
         sge_free(&env_entry);
         sge_free_saved_vars(context);
         continue;
      }
      auto ep = env_list->find_first<VA_variable>(env_name);
      if (ep == nullptr) {
         ep = env_list->emplace_back<VA_variable>(env_name);
      }

      char *env_description = sge_strtok_r((char *) 0, "\0", &context);
      ep.set<VA_value>(env_description);
      sge_free(&env_entry);
      sge_free_saved_vars(context);
   }

   DRETURN(env_list);
}

/**
 * @brief Sets the value of a y|n option
 *
 * Sets the value of the option element to TRUE or FALSE depending on whether
 * the option value is y[es] or n[o].
 *
 * @param opts The list of options to which to append the option element
 * @param opt The option code of the option
 * @param arg The option text
 * @param value The option value
 * @param alpp The answer list
 *
 * @return STATUS_OK if success, STATUS_ERROR1 otherwise
 *
 * @note MT-NOTES: set_yn_option() is MT safe
 */
static int set_yn_option(ocs::tom::ListPtr<ocs::tom::SPA> &opts, const uint32_t opt, const char *arg,
                         const char *value, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   ocs::tom::Element<ocs::tom::SPA> ep_opt;
   
   if ((strcasecmp("y", value) == 0) || (strcasecmp("yes", value) == 0)) {
      ep_opt = sge_add_arg(opts, opt, lIntT, arg, value);
      ep_opt.set<SPA_argval_lIntT>(true);
      ep_opt.set<SPA_argval_lUlongT>(true);
   }
   else if ((strcasecmp ("n", value) == 0) || (strcasecmp ("no", value) == 0)) {
      ep_opt = sge_add_arg(opts, opt, lIntT, arg, value);
      ep_opt.set<SPA_argval_lIntT>(false);
      ep_opt.set<SPA_argval_lUlongT>(false);
    }
   else {
       snprintf(SGE_EVENT, SGE_EVENT_SIZE,  MSG_PARSE_INVALIDOPTIONARGUMENT_SS, arg, value);
       answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
       
       return STATUS_ERROR1;
   }
   
   return STATUS_OK;
}

/** @brief Parse the -sync option switch
 *
 * Supports the old and new format of the -sync option switch.
 * Old format: -sync y(es)|n(o)
 * New format: -sync r|y|x|E|n
 *
 *    r - job start
 *    x or y - job end
 *    n - no sync
 *
 * @param opts list of options
 * @param opt option code
 * @param arg option argument
 * @param value option value
 * @param alpp answer list
 *
 * @return STATUS_OK if success, STATUS_ERROR1 otherwise
 */
static int
ocs_parse_sync_switch(ocs::tom::ListPtr<ocs::tom::SPA> &opts, const uint32_t opt, const char *arg, const char *value,
                      ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   uint32_t sync_bits = SYNC_UNINITIALIZED;

   // -sync yes|no (pre OCS format )
   if (strcasecmp("yes", value) == 0) {
      sync_bits = SYNC_JOB_END;
   } else if (strcasecmp ("no", value) == 0) {
      sync_bits = SYNC_NO;
   }

   // -sync r|y|x|E|n (new OCS format)
   if (sync_bits == SYNC_UNINITIALIZED) {
      const int len = strlen(value);
      for (int pos = 0; pos < len; pos++) {
         switch (value[pos]) {
         case 'r':
            sync_bits |= SYNC_JOB_START;
            break;
         case 'x':
         case 'y':
            sync_bits |= SYNC_JOB_END;
            break;
         case 'n':
            sync_bits |= SYNC_NO;
            break;
         default:
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_PARSE_INVALIDOPTIONARGUMENT_SS, arg, value);
            answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(STATUS_ERROR1);
         }
      }
   }

   // check if SYNC_NO was combined with other bits which might make no sense
   if ((sync_bits & SYNC_NO) != 0 && sync_bits != SYNC_NO) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_PARSE_INVALIDOPTIONARGUMENT_SS, arg, value);
      answer_list_add(alpp, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_ERROR1);
   }

   // transport bits via opts list
   auto ep_opt = sge_add_arg(opts, opt, lUlongT, arg, value);
   ep_opt.set<SPA_argval_lUlongT>(sync_bits);

   DRETURN(STATUS_OK);
}

/** @brief Express a path relative to the job owner's home directory
 *
 * Used for `-cwd`: the current directory on the submit host has to be named in
 * a way the execution host can resolve, and the home directory is the one
 * anchor both are assumed to share.
 *
 * @param pjob the job, for the owner whose home directory is used
 * @param path the path to rewrite
 * @param alpp used to return error messages
 * @return the rewritten path, allocated with strdup() for the caller to free; nullptr on an error
 *
 * @note Not thread safe, because it changes the working directory of the
 *       process. Acceptable because it is only reached through `-cwd`, which
 *       DRMAA can forbid.
 */
char *reroot_path(const ocs::tom::ConstElement<ocs::tom::JB> pjob, const char *path,
                  ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   const char *home = nullptr;
   char tmp_str[SGE_PATH_MAX + 1];
   char tmp_str2[SGE_PATH_MAX + 1];
   char tmp_str3[SGE_PATH_MAX + 1];

   home = job_get_env_string(pjob, VAR_PREFIX "O_HOME");
   strcpy(tmp_str, path);

   if (chdir(home) == 0) {
      /* If chdir() succeeds... */
      if (getcwd(tmp_str2, sizeof(tmp_str2)) == nullptr) {
         /* If getcwd() fails... */
         answer_list_add(alpp, MSG_ANSWER_GETCWDFAILED,
                         STATUS_EDISK, ANSWER_QUALITY_ERROR);
         DRETURN(nullptr);
      }

      if (chdir(tmp_str) == 0) {
         if (strncmp(tmp_str2, tmp_str, strlen(tmp_str2)) == 0) {
            /* If they are equal, build a new CWD using the value of the HOME
             * as the root instead of whatever that directory is called by
             * the -(c)wd path. */
            snprintf(tmp_str3, sizeof(tmp_str3), "%s%s", home, (char *) tmp_str + strlen(tmp_str2));
            strcpy(tmp_str, tmp_str3);
         }
      }
   }

   DRETURN(strdup(tmp_str));
}
