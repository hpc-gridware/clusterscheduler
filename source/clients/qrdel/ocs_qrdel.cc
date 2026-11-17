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
 * @brief qrdel - deletes advance reservations
 */

#include <cstring>
#include <cinttypes>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/tom/ocs_tom_ID.h"
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "gdi/ocs_gdi_Client.h"

#include "usage.h"
#include "sig_handlers.h"
#include "msg_common.h"
#include "msg_clients_common.h"

static bool sge_parse_cmdline_qrdel(char **argv, char **envp, lList **ppcmdline, lList **alpp);
static bool sge_parse_qrdel(lList **ppcmdline, lList **ppid_list, lList **alpp);

/** @brief The process environment, as the C library defines it
 *
 * Declared here rather than included, and passed to the command line parser so
 * that options which read an environment variable resolve it against the
 * submitting user's environment rather than against `getenv()`.
 */
extern char **environ;

/************************************************************************/
int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "qrdel");
   lList *pcmdline = nullptr, *id_list = nullptr;
   lList *alp = nullptr;

   /* Set up the program information name */
   sge_setup_sig_handlers(QRDEL);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QRDEL, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      goto error_exit;
   }

   /*
   ** stage 1 of commandline parsing
   */
   if (!sge_parse_cmdline_qrdel(++argv, environ, &pcmdline, &alp)) {
      /*
      ** high level parsing error! show answer list
      */
      answer_list_output(&alp);
      lFreeList(&pcmdline);
      goto error_exit;
   }

   /*
   ** stage 2 of command line parsing
   */
   if (!sge_parse_qrdel(&pcmdline, &id_list, &alp)) {
      answer_list_output(&alp);
      lFreeList(&pcmdline);
      goto error_exit;
   }

   lFreeList(&pcmdline);

   if (!id_list) {
      sge_usage(QRDEL, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      goto error_exit;
   }

   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::AR_LIST, ocs::gdi::Command::DEL,
                 ocs::gdi::SubCommand::NONE, &id_list, nullptr, nullptr);
   lFreeList(&id_list);
   if (answer_list_has_error(&alp)) {
      answer_list_on_error_print_or_exit(&alp, stdout);
      goto error_exit;
   }
   answer_list_on_error_print_or_exit(&alp, stdout);

   lFreeList(&alp);
   ocs::gdi::ClientBase::shutdown();
   DRETURN(0);

error_exit:
   lFreeList(&alp);
   ocs::gdi::ClientBase::shutdown();
   sge_exit(1);
   DRETURN(1);
}

/**
 * @brief 'Stage 1' parsing of the qrdel options
 *
 * Parses the options with their arguments and stores them in @p ppcmdline.
 *
 * @param argv the arguments, without the program name
 * @param envp the process environment
 * @param ppcmdline receives the parsed options (`SPA_Type`)
 * @param alpp receives the parse errors
 * @return true on success, false on a parse error
 */
static bool sge_parse_cmdline_qrdel(char **argv, char **envp, lList **ppcmdline, lList **alpp) {
   DENTER(TOP_LAYER);

   char **sp;
   char **rp;

   rp = argv;
   while (*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, ppcmdline, alpp)) != sp)
         continue;

      /* -f option */
      if ((rp = parse_noopt(sp, "-f", "--force", ppcmdline, alpp)) != sp)
         continue;

      /* -u option */
      if (!strcmp("-u", *sp)) {
         sp++;
         if (*sp) {
            auto user_list = str_list_parse_from_string(*sp, ",");

            lListElem *ep_opt = sge_add_arg(ppcmdline, 0, lListT, *(sp - 1), *sp);
            ocs::tom::set<SPA_argval_lListT>(ep_opt, std::move(user_list));
            sp++;
         }
         rp = sp;
         continue;
      }
      /* job id's */
      if ((rp = parse_param(sp, "ars", ppcmdline, alpp)) != sp) {
         continue;
      }
      /* oops */
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              MSG_PARSE_INVALIDOPTIONARGUMENTX_S, *sp);
      sge_usage(QRDEL, stderr);
      DRETURN(false);
   }

   DRETURN(true);
}

/**
 * @brief 'Stage 2' parsing of the qrdel options
 *
 * Gets the options from @p ppcmdline and puts the advance reservation ids, with
 * the user list and the force flag, in @p ppid_list.
 *
 * @param ppcmdline the parsed options (`SPA_Type`); the options served are removed
 * @param ppid_list receives the advance reservation references (`ID_Type`); freed on an error
 * @param alpp receives the errors
 * @return true on success, false on an error
 */
static bool sge_parse_qrdel(lList **ppcmdline, lList **ppid_list, lList **alpp) {
   DENTER(TOP_LAYER);

   uint32_t pforce = 0;
   uint32_t helpflag;
   ocs::tom::ListPtr<ocs::tom::ST> user_list;
   bool ret = true;

   while (lGetNumberOfElem(*ppcmdline)) {
      if (parse_flag(ppcmdline, "-help",  alpp, &helpflag)) {
         sge_usage(QRDEL, stdout);
         sge_exit(0);
         break;
      }
      if (parse_flag(ppcmdline, "-f", alpp, &pforce))
         continue;

      if (parse_multi_stringlist<ST_name>(ocs::tom::List<ocs::tom::SPA>(*ppcmdline), "-u", alpp, user_list)) {
         continue;
      }

      ocs::tom::ListPtr<ocs::tom::ST> ars;
      if (parse_multi_stringlist<ST_name>(ocs::tom::List<ocs::tom::SPA>(*ppcmdline), "ars", alpp, ars)) {
         for (const auto ep : *ars) {
            const char *id = ocs::tom::get<ST_name>(ep);
            ocs::tom::emplace_back<ID_str>(ppid_list, id);
         }
         continue;
      }

      /* the options are created by sge_add_arg() and friends with SPA_Type */
      for (const auto ep : ocs::tom::ConstList<ocs::tom::SPA>::from_typed(*ppcmdline)) {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDOPTIONARGUMENTX_S,
                                 ocs::tom::get<SPA_switch_val>(ep));

      }
      break;
   }

   if (answer_list_has_error(alpp)) {
      ret = false;
      lFreeList(ppid_list);
   } else {
      /* fill up ID list; emplace_back<ID_str>() creates it with ID_Type, so it is wrapped unchecked */
      if (user_list != nullptr) {
         if (ocs::tom::List<ocs::tom::ID>::from_typed(*ppid_list).empty()) {
            ocs::tom::Element<ocs::tom::ID> id = ocs::tom::emplace_back<ID_str>(ppid_list, "0");
            ocs::tom::set<ID_user_list>(id, user_list->clone(""));
         } else {
            for (const auto id : ocs::tom::List<ocs::tom::ID>::from_typed(*ppid_list)) {
               ocs::tom::set<ID_user_list>(id, user_list->clone(""));
            }
         }
      }

      if (pforce != 0) {
         for (const auto id : ocs::tom::List<ocs::tom::ID>::from_typed(*ppid_list)) {
            ocs::tom::set<ID_force>(id, pforce);
         }
      }
   }

   DRETURN(ret);
}
