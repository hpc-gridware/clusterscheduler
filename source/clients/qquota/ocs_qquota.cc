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
 *  Portions of this code are Copyright 2011 Univa Corporation.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <memory>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "procedure/qquota/ocs_QQuotaParameter.h"
#include "procedure/qquota/ocs_QQuotaModel.h"
#include "procedure/qquota/ocs_QQuotaViewBase.h"
#include "procedure/qquota/ocs_QQuotaViewPlain.h"
#include "procedure/qquota/ocs_QQuotaViewXML.h"
#include "procedure/qquota/ocs_QQuotaController.h"

#include "sig_handlers.h"

extern char **environ;
                                      
int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "qquota");
   lList *alp = nullptr;

   if (ocs::gdi::ClientBase::setup_and_enroll(QQUOTA, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   sge_setup_sig_handlers(QQUOTA);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   // parse command line parameters and options
   ocs::QQuotaParameter parameter;
   if (!parameter.parse_parameters(&alp, argv, environ)) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   // fetch and prepare data for output
   ocs::QQuotaModel model;
   if (!model.make_snapshot(&alp, parameter)) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   // create view that will display the output in correct format
   std::unique_ptr<ocs::QQuotaViewBase> view;
   if (parameter.is_xml) {
      view = std::make_unique<ocs::QQuotaViewXML>(parameter);
   } else {
      view = std::make_unique<ocs::QQuotaViewPlain>(parameter);
   }

   // start processing and show output
   ocs::QQuotaController controller;
   controller.process_request(parameter, model, *view);

   sge_exit(0);
   DRETURN(0);
}

/****** qquota/qquota_usage() **************************************************
*  NAME
*     qquota_usage() -- displays qquota help output
*
*  SYNOPSIS
*     static bool qquota_usage(FILE *fp) 
*
*  FUNCTION
*     displays qquota_usage for qlist client
*     note that the other clients use a common function
*     for this. output was adapted to a similar look.
*
*  INPUTS
*     FILE *fp - output file pointer
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: qquota_usage() is MT safe 
*
*******************************************************************************/

/****** qquota/sge_parse_from_file_qquota() ************************************
*  NAME
*     sge_parse_from_file_qquota() -- parse qquota command line options from
*                                     file
*
*  SYNOPSIS
*     static bool sge_parse_from_file_qquota(const char *file, lList 
*     **ppcmdline, lList **alpp) 
*
*  FUNCTION
*     parses the qquota command line options from file
*
*  INPUTS
*     const char *file  - file name
*     lList **ppcmdline - found command line options
*     lList **alpp      - answer list pointer
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: sge_parse_from_file_qquota() is MT safe 
*
*******************************************************************************/

/****** qquota/sge_parse_cmdline_qquota() **************************************
*  NAME
*     sge_parse_cmdline_qquota() -- ??? 
*
*  SYNOPSIS
*     static bool sge_parse_cmdline_qquota(char **argv, lList **ppcmdline, 
*     lList **alpp) 
*
*  FUNCTION
*     'stage 1' parsing of qquota-options. Parses options
*     with their arguments and stores them in ppcmdline.
*
*  INPUTS
*     char **argv       - argument list
*     lList **ppcmdline - found arguments
*     lList **alpp      - answer list pointer
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: sge_parse_cmdline_qquota() is MT safe 
*
*******************************************************************************/

/****** qquota/sge_parse_qquota() **********************************************
*  NAME
*     sge_parse_qquota() -- parse qquota options
*
*  SYNOPSIS
*     static bool sge_parse_qquota(lList **ppcmdline, lList **host_list, lList 
*     **resource_list, lList **user_list, lList **pe_list, lList 
*     **project_list, lList **cqueue_list, report_handler_t **report_handler, 
*     lList **alpp) 
*
*  FUNCTION
*     'stage 2' parsing of qquota-options. Gets the options from pcmdline
*
*  INPUTS
*     lList **ppcmdline                 - found command line options (from stage 1)
*     lList **host_list                 - parsed host list (-h option)
*     lList **resource_list             - parsed resource list (-l option)
*     lList **user_list                 - parsed user list (-u option)
*     lList **pe_list                   - parsed pe list (-pe option)
*     lList **project_list              - parsed project list (-P option)
*     lList **cqueue_list               - parsed queue list (-q option)
*     report_handler_t **report_handler - report handler for xml output
*     lList **alpp                      - answer list
*
*  RESULT
*     static bool - true on success
*                   false on error
*
*  NOTES
*     MT-NOTE: sge_parse_qquota() is MT safe 
*
*******************************************************************************/
