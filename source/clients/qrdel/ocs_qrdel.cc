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

#include "sgeobj/tom/ocs_tom_all.h"
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

static bool sge_parse_cmdline_qrdel(char **argv, char **envp, ocs::tom::ListPtr<ocs::tom::SPA> &cmdline,
                                    ocs::tom::ListPtr<ocs::tom::AN> *alpp);
static bool sge_parse_qrdel(ocs::tom::List<ocs::tom::SPA> cmdline, ocs::tom::ListPtr<ocs::tom::ID> &id_list,
                            ocs::tom::ListPtr<ocs::tom::AN> *alpp);

/** @brief The process environment, as the C library defines it
 *
 * Declared here rather than included, and passed to the command line parser so
 * that options which read an environment variable resolve it against the
 * submitting user's environment rather than against `getenv()`.
 */
extern char **environ;

/**
 * @brief qrdel: parse the command line and delete the advance reservations it names
 *
 * @param argc the number of arguments
 * @param argv the arguments
 *
 * @return 0 on success; on an error the process ends with sge_exit(1)
 */
int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "qrdel");
   ocs::tom::ListPtr<ocs::tom::SPA> pcmdline;
   ocs::tom::ListPtr<ocs::tom::ID> id_list;
   ocs::tom::ListPtr<ocs::tom::AN> alp;

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
   if (!sge_parse_cmdline_qrdel(++argv, environ, pcmdline, &alp)) {
      /*
      ** high level parsing error! show answer list
      */
      answer_list_output(&alp);
      pcmdline.reset();
      goto error_exit;
   }

   /*
   ** stage 2 of command line parsing
   */
   if (!sge_parse_qrdel(*pcmdline, id_list, &alp)) {
      answer_list_output(&alp);
      pcmdline.reset();
      goto error_exit;
   }

   pcmdline.reset();

   if (!id_list) {
      sge_usage(QRDEL, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      goto error_exit;
   }

   // the request list is replaced by the response, if any; both are freed
   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::AR_LIST, ocs::gdi::Command::DEL,
                                   ocs::gdi::SubCommand::NONE, id_list);
   id_list.reset();
   if (answer_list_has_error(&alp)) {
      answer_list_on_error_print_or_exit(&alp, stdout);
      goto error_exit;
   }
   answer_list_on_error_print_or_exit(&alp, stdout);

   alp.reset();
   ocs::gdi::ClientBase::shutdown();
   DRETURN(0);

error_exit:
   alp.reset();
   ocs::gdi::ClientBase::shutdown();
   sge_exit(1);
   DRETURN(1);
}

/**
 * @brief 'Stage 1' parsing of the qrdel options
 *
 * Parses the options with their arguments and stores them in @p cmdline.
 *
 * @param argv the arguments, without the program name
 * @param envp the process environment
 * @param cmdline receives the parsed options (`SPA`), created with the first option
 * @param alpp receives the parse errors
 * @return true on success, false on a parse error
 */
static bool sge_parse_cmdline_qrdel(char **argv, [[maybe_unused]] char **envp,
                                    ocs::tom::ListPtr<ocs::tom::SPA> &cmdline, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   char **sp;
   char **rp;

   rp = argv;
   while (*(sp=rp)) {
      /* -help */
      if ((rp = parse_noopt(sp, "-help", nullptr, cmdline, alpp)) != sp) {
         continue;
      }

      /* -f option */
      if ((rp = parse_noopt(sp, "-f", "--force", cmdline, alpp)) != sp) {
         continue;
      }

      /* -u option */
      if (!strcmp("-u", *sp)) {
         sp++;
         if (*sp) {
            auto user_list = str_list_parse_from_string(*sp, ",");

            const auto ep_opt = sge_add_arg(cmdline, 0, lListT, *(sp - 1), *sp);
            ocs::tom::set<SPA_argval_lListT>(ep_opt, std::move(user_list));
            sp++;
         }
         rp = sp;
         continue;
      }
      /* job id's */
      if ((rp = parse_param(sp, "ars", cmdline, alpp)) != sp) {
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
 * Gets the options from @p cmdline and puts the advance reservation ids, with
 * the user list and the force flag, in @p id_list.
 *
 * @param cmdline the parsed options (`SPA`); the options served are removed
 * @param id_list receives the advance reservation references (`ID`), created with the first one; freed on
 *                an error
 * @param alpp receives the errors
 * @return true on success, false on an error
 */
static bool sge_parse_qrdel(const ocs::tom::List<ocs::tom::SPA> cmdline, ocs::tom::ListPtr<ocs::tom::ID> &id_list,
                            ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);

   uint32_t pforce = 0;
   uint32_t helpflag;
   ocs::tom::ListPtr<ocs::tom::ST> user_list;
   bool ret = true;

   while (!cmdline.empty()) {
      if (parse_flag(cmdline, "-help",  alpp, &helpflag)) {
         sge_usage(QRDEL, stdout);
         sge_exit(0);
         break;
      }
      if (parse_flag(cmdline, "-f", alpp, &pforce)) {
         continue;
      }

      if (parse_multi_stringlist<ST_name>(cmdline, "-u", alpp, user_list)) {
         continue;
      }

      ocs::tom::ListPtr<ocs::tom::ST> ars;
      if (parse_multi_stringlist<ST_name>(cmdline, "ars", alpp, ars)) {
         for (const auto ep : *ars) {
            const char *id = ep.get<ST_name>();
            // a null id added nothing in the free emplace_back(), which created the list with the first one
            if (id == nullptr) {
               continue;
            }
            if (!id_list) {
               id_list = ocs::tom::ListPtr<ocs::tom::ID>::create("");
            }
            id_list->emplace_back<ID_str>(id);
         }
         continue;
      }

      for (const auto ep : cmdline) {
         answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                                 MSG_PARSE_INVALIDOPTIONARGUMENTX_S,
                                 ep.get<SPA_switch_val>());

      }
      break;
   }

   if (answer_list_has_error(alpp)) {
      ret = false;
      id_list.reset();
   } else {
      /* fill up ID list */
      if (user_list != nullptr) {
         if (id_list->empty()) {
            if (!id_list) {
               id_list = ocs::tom::ListPtr<ocs::tom::ID>::create("");
            }
            const auto id = id_list->emplace_back<ID_str>("0");
            id.set<ID_user_list>(user_list->clone(""));
         } else {
            for (const auto id : *id_list) {
               id.set<ID_user_list>(user_list->clone(""));
            }
         }
      }

      if (pforce != 0) {
         for (const auto id : *id_list) {
            id.set<ID_force>(pforce);
         }
      }
   }

   DRETURN(ret);
}
