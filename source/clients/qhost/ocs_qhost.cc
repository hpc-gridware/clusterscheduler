/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#include <iostream>
#include <memory>
#include <sstream>

#include "ocs_ProcedureController.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/sge_answer.h"

#include "gdi/ocs_gdi_ClientBase.h"

#include "procedure/qhost/ocs_QHostParameter.h"
#include "procedure/qhost/ocs_QHostViewJSON.h"
#include "procedure/qhost/ocs_QHostViewXML.h"
#include "procedure/qhost/ocs_QHostViewPlain.h"
#include "procedure/qhost/ocs_QHostContoller.h"
#include "procedure/qhost/ocs_QHostModelClient.h"
#include "procedure/qhost/ocs_QHostParameterClient.h"

#include "procedure/ocs_ProcedureParameter.h"
#include "procedure/ocs_ProcedureModel.h"
#include "procedure/ocs_ProcedureView.h"

#include "sig_handlers.h"

extern char **environ;

int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "qhost");

   sge_sig_handler_in_main_loop = 0;
   sge_setup_sig_handlers(QHOST);

   // install handlers for termination signals and unexpected exceptions
   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   // prepare gdi client and enroll at qmaster
   lList *answer_list = nullptr;
   if (ocs::gdi::ClientBase::setup_and_enroll(QHOST, MAIN_THREAD, &answer_list) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }

   // parse command line parameters and options
   const std::string procedure_name = to_cstr(QHOST);
   ocs::QHostParameterClient parameter(procedure_name);
   if (!parameter.parse_parameters(&answer_list, argv, environ)) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }

   std::ostringstream out_ss;
   if (parameter.get_exec_context() == ocs::ProcedureParameter::ExecContext::SERVER) {
      // prepare data for output
      ocs::ProcedureModel model;
      if (!model.make_snapshot(&answer_list, parameter)) {
         answer_list_output(&answer_list);
         sge_exit(1);
      }

      ocs::ProcedureView view(parameter);
      ocs::ProcedureController controller(out_ss);
      controller.process_request(parameter, model, view);
   } else {
      // prepare data for output
      ocs::QHostModelClient model;
      if (!model.make_snapshot(&answer_list, parameter)) {
         answer_list_output(&answer_list);
         sge_exit(1);
      }

      // prepare view to show output
      std::unique_ptr<ocs::QHostViewBase> view;
      switch (parameter.get_output_format()) {
         case ocs::ProcedureParameter::OutputFormat::XML:
            view = std::make_unique<ocs::QHostViewXML>(parameter);
            break;
         case ocs::ProcedureParameter::OutputFormat::PLAIN:
            view = std::make_unique<ocs::QHostViewPlain>(parameter);
            break;
         case ocs::ProcedureParameter::OutputFormat::JSON:
            view = std::make_unique<ocs::QHostViewJSON>(parameter);
            break;
      }

      if (!view) {
         answer_list_add(&answer_list, "Unable to create view", STATUS_EUNKNOWN, ANSWER_QUALITY_CRITICAL);
         sge_exit(1);
      }

      // process request and show output
      ocs::QHostController controller(out_ss);
      controller.process_request(parameter, model, *view);
   }

   // Output to the console
   std::cout << out_ss.str();

   sge_exit(0);
   DRETURN(0);
}
