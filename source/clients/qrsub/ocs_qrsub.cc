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
 * @brief qrsub - requests an advance reservation
 */

#include <cstdarg>
#include <cstdlib>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_profiling.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/tom/ocs_tom_AR.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_client_parse.h"
#include "parse_qsub.h"
#include "ocs_parse_job.h"
#include "usage.h"
#include "sig_handlers.h"
#include "ocs_qrsub_parse.h"
#include "msg_clients_common.h"

/** @brief The process environment, as the C library defines it
 *
 * Declared here rather than included, and passed to the command line parser so
 * that options which read an environment variable resolve it against the
 * submitting user's environment rather than against `getenv()`.
 */
extern char **environ;

/**
 * @brief qrsub: parse the command line and request the advance reservation it describes
 * @param argc the number of arguments
 * @param argv the arguments
 * @return 0 on success, 25 when qmaster asks to try again, 1 otherwise
 */
int main(int argc, const char **argv) {
   DENTER_MAIN(TOP_LAYER, "qrsub");
   ocs::tom::ListPtr<ocs::tom::SPA> pcmdline;
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::tom::ElementPtr<ocs::tom::AR> ar;

   /* Set up the program information name */
   sge_setup_sig_handlers(QRSUB);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QRSUB, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      goto error_exit;
   }

   /*
   ** stage 1 of commandline parsing
   */
   {
      dstring file = DSTRING_INIT;
      const char *user = component_get_username();
      const char *cell_root = bootstrap_get_cell_root();

      /* arguments from SGE_ROOT/common/sge_ar_request file */
      get_root_file_path(&file, cell_root, SGE_COMMON_DEF_AR_REQ_FILE);
      if ((alp = parse_script_file(QRSUB, sge_dstring_get_string(&file), "", pcmdline, environ,
         FLG_HIGHER_PRIOR | FLG_IGN_NO_FILE)) == nullptr) {
         /* arguments from $HOME/.sge_ar_request file */
         if (get_user_home_file_path(&file, SGE_HOME_DEF_AR_REQ_FILE, user, &alp)) {
            alp.reset();
            alp = parse_script_file(QRSUB, sge_dstring_get_string(&file), "", pcmdline, environ,
            FLG_HIGHER_PRIOR | FLG_IGN_NO_FILE);
         }
      }
      sge_dstring_free(&file);

      if (alp) {
         answer_list_output(&alp);
         pcmdline.reset();
         goto error_exit;
      }

      alp = ocs_parse_cmdline(QRSUB, argv+1, environ, pcmdline, FLG_USE_PSEUDOS);
   }

   if (answer_list_print_err_warn(&alp, nullptr, "qrsub: ", MSG_WARNING) > 0) {
      pcmdline.reset();
      goto error_exit;
   }

   if (!pcmdline) {
      /* no command line option is present: print help to stderr */
      sge_usage(QRSUB, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      goto error_exit;
   }

   /*
   ** stage 2 of command line parsing
   */
   ar = ocs::tom::ElementPtr<ocs::tom::AR>::create();

   if (!sge_parse_qrsub(*pcmdline, &alp, ar.get())) {
      answer_list_output(&alp);
      pcmdline.reset();
      goto error_exit;
   }

   pcmdline.reset();

   {
      // the request list is replaced by the response; both are freed at the end of the block
      auto ars = ocs::tom::ListPtr<ocs::tom::AR>::create(nullptr);
      ars->push_back(std::move(ar));
      alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::AR_LIST, ocs::gdi::Command::ADD,
                                      ocs::gdi::SubCommand::RETURN_NEW_VERSION, ars);
   }
   answer_list_on_error_print_or_exit(&alp, stdout);
   if (answer_list_has_error(&alp)) {
      ocs::gdi::ClientBase::shutdown();
      if (answer_list_has_status(&alp, STATUS_NOTOK_DOAGAIN)) {
         alp.reset();
         DRETURN(25);
      } else {
         alp.reset();
         DRETURN(1);
      }
   }

   alp.reset();
   ocs::gdi::ClientBase::shutdown();
   DRETURN(0);

error_exit:
   ocs::gdi::ClientBase::shutdown();
   sge_exit(1);
   DRETURN(1);
}

