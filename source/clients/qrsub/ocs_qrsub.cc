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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_profiling.h"

#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"

#include "gdi/sge_gdi.h"
#include "gdi/ocs_gdi_client.h"

#include "ocs_client_parse.h"
#include "parse_qsub.h"
#include "parse_job_cull.h"
#include "usage.h"
#include "sig_handlers.h"
#include "ocs_qrsub_parse.h"
#include "msg_clients_common.h"

extern char **environ;


/************************************************************************/
int main(int argc, const char **argv) {
   lList *pcmdline = nullptr;
   lList *alp = nullptr;
   lList *ar_lp = nullptr;

   lListElem *ar = nullptr;

   DENTER_MAIN(TOP_LAYER, "qrsub");

   /* Set up the program information name */
   sge_setup_sig_handlers(QRSUB);

   log_state_set_log_gui(1);

   if (gdi_client_setup_and_enroll(QRSUB, MAIN_THREAD, &alp) != AE_OK) {
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
      if ((alp = parse_script_file(QRSUB, sge_dstring_get_string(&file), "", &pcmdline, environ, 
         FLG_HIGHER_PRIOR | FLG_IGN_NO_FILE)) == nullptr) {
         /* arguments from $HOME/.sge_ar_request file */
         if (get_user_home_file_path(&file, SGE_HOME_DEF_AR_REQ_FILE, user, &alp)) {
            lFreeList(&alp);
            alp = parse_script_file(QRSUB, sge_dstring_get_string(&file), "", &pcmdline, environ, 
            FLG_HIGHER_PRIOR | FLG_IGN_NO_FILE);
         }
      }
      sge_dstring_free(&file); 

      if (alp) {
         answer_list_output(&alp);
         lFreeList(&pcmdline);
         goto error_exit;
      }
   }
   
   alp = cull_parse_cmdline(QRSUB, argv+1, environ, &pcmdline, FLG_USE_PSEUDOS);

   if (answer_list_print_err_warn(&alp, nullptr, "qrsub: ", MSG_WARNING) > 0) {
      lFreeList(&pcmdline);
      goto error_exit;
   }
   
   if (pcmdline == nullptr) {
      /* no command line option is present: print help to stderr */
      sge_usage(QRSUB, stderr);
      fprintf(stderr, "%s\n", MSG_PARSE_NOOPTIONARGUMENT);
      goto error_exit;
   }

   /*
   ** stage 2 of command line parsing
   */
   ar = lCreateElem(AR_Type);

   if (!sge_parse_qrsub(pcmdline, &alp, &ar)) {
      answer_list_output(&alp);
      lFreeList(&pcmdline);
      goto error_exit;
   }

   lFreeList(&pcmdline);

   ar_lp = lCreateList(nullptr, AR_Type);
   lAppendElem(ar_lp, ar);

   alp = sge_gdi(SGE_AR_LIST, SGE_GDI_ADD | SGE_GDI_RETURN_NEW_VERSION, &ar_lp, nullptr, nullptr);
   lFreeList(&ar_lp);
   answer_list_on_error_print_or_exit(&alp, stdout);
   if (answer_list_has_error(&alp)) {
      gdi_client_shutdown();
      sge_prof_cleanup();
      if (answer_list_has_status(&alp, STATUS_NOTOK_DOAGAIN)) {
         lFreeList(&alp);
         DRETURN(25);
      } else {
         lFreeList(&alp);
         DRETURN(1);
      }
   }

   lFreeList(&alp);
   gdi_client_shutdown();
   sge_prof_cleanup();
   DRETURN(0);

error_exit:
   gdi_client_shutdown();
   sge_prof_cleanup();
   sge_exit(1);
   DRETURN(1);
}

