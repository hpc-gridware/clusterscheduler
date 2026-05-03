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
#include <iostream>
#include <memory>

#include "ocs_ProcedureController.h"
#include "ocs_ProcedureModel.h"
#include "ocs_ProcedureView.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_object.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "procedure/qquota/ocs_QQuotaParameterClient.h"
#include "procedure/qquota/ocs_QQuotaModelBase.h"
#include "procedure/qquota/ocs_QQuotaViewBase.h"
#include "procedure/qquota/ocs_QQuotaViewPlain.h"
#include "procedure/qquota/ocs_QQuotaViewXML.h"
#include "procedure/qquota/ocs_QQuotaController.h"

#include "sig_handlers.h"
#include "qquota/ocs_QQuotaModelClient.h"
#include "qquota/ocs_QQuotaViewJSON.h"

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
   const std::string procedure_name = to_cstr(QQUOTA);
   ocs::QQuotaParameterClient parameter(procedure_name);
   if (!parameter.parse_parameters(&alp, argv, environ)) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   std::ostringstream out_ss;
   if (parameter.get_exec_context() == ocs::ProcedureParameter::ExecContext::SERVER) {
      // prepare data for output
      ocs::ProcedureModel model;
      if (!model.make_snapshot(&alp, parameter)) {
         answer_list_output(&alp);
         sge_exit(1);
      }

      ocs::ProcedureView view(parameter);
      ocs::ProcedureController controller(out_ss);
      controller.process_request(parameter, model, view);
   } else {
      // fetch and prepare data for output
      ocs::QQuotaModelClient model;
      if (!model.make_snapshot(&alp, parameter)) {
         answer_list_output(&alp);
         sge_exit(1);
      }

      // create view that will display the output in correct format
      std::unique_ptr<ocs::QQuotaViewBase> view;
      switch (parameter.get_output_format()) {
         case ocs::QQuotaParameterClient::OutputFormat::JSON:
            view = std::make_unique<ocs::QQuotaViewJSON>(parameter);
            break;
         case ocs::QQuotaParameterClient::OutputFormat::XML:
            view = std::make_unique<ocs::QQuotaViewXML>(parameter);
            break;
         case ocs::QQuotaParameterClient::OutputFormat::PLAIN:
            view = std::make_unique<ocs::QQuotaViewPlain>(parameter);
            break;
      }

      // start processing and show output
      ocs::QQuotaController controller(out_ss);
      controller.process_request(parameter, model, *view);
   }

   std::cout << out_ss.str();

   sge_exit(0);
   DRETURN(0);
}
