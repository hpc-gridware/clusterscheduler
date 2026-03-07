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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstring>
#include <math.h>
#include <sstream>
#include <iomanip>
#include <memory>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_log.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"
#include "uti/sge_io.h"
#include "uti/sge_string.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_feature.h"
#include "sgeobj/parse.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_cull_xml.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "basis_types.h"
#include "sig_handlers.h"
#include "ocs_qhost_print.h"
#include "ocs_client_parse.h"
#include "ocs_QHostParameter.h"
#include "ocs_QHostViewXML.h"
#include "ocs_QHostViewPlain.h"
#include "msg_common.h"
#include "msg_clients_common.h"
#include "msg_qhost.h"
#include "ocs_QHostModel.h"

extern char **environ;

/************************************************************************/
int main(int argc, char **argv)
{
   DENTER_MAIN(TOP_LAYER, "qhost");
   lList *alp = nullptr;
   int qhost_result = 0;

   sge_sig_handler_in_main_loop = 0;
   sge_setup_sig_handlers(QHOST);

   // install handlers for termination signals and unexpected exceptions
   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   // prepare gdi client and enroll at qmaster
   if (ocs::gdi::ClientBase::setup_and_enroll(QHOST, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(1);
   }

   ocs::QHostParameter qhost_parameter;
   if (!qhost_parameter.parse_parameters(&alp, argv, environ)) {
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(1);
   }


   // prepare data for output
   ocs::QHostModel model;
   bool lret = model.make_snapshot(&alp, qhost_parameter);
   if (!lret) {
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(1);
   }

   // prepare view to show output
   std::unique_ptr<ocs::QHostViewBase> report_handler;
   if (qhost_parameter.get_output_format() == ocs::QHostParameter::OutputFormat::XML) {
      report_handler = std::make_unique<ocs::QHostViewXML>(qhost_parameter);
   } else {
      report_handler = std::make_unique<ocs::QHostViewPlain>(qhost_parameter);
   }

   qhost_result = do_qhost(qhost_parameter, model, *report_handler);

   if (qhost_result != QHOST_SUCCESS) {
      answer_list_output(&alp);
      sge_prof_cleanup();
      sge_exit(1);
   }

   sge_prof_cleanup();
   sge_exit(0); /* 0 means ok - others are errors */
   DRETURN(0);
}




