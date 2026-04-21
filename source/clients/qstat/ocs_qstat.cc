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

#include <memory>
#include <sstream>
#include <iostream>

#include "ocs_ProcedureController.h"
#include "ocs_ProcedureModel.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"

#include "gdi/ocs_gdi_Client.h"

#include "procedure/qstat/default/ocs_QStatDefaultController.h"
#include "procedure/qstat/default/ocs_QStatDefaultViewBase.h"
#include "procedure/qstat/default/ocs_QStatDefaultViewPlain.h"
#include "procedure/qstat/default/ocs_QStatDefaultViewXML.h"
#include "procedure/qstat/group/ocs_QStatGroupViewBase.h"
#include "procedure/qstat/group/ocs_QStatGroupViewPlain.h"
#include "procedure/qstat/group/ocs_QStatGroupViewXML.h"
#include "procedure/qstat/group/ocs_QStatGroupController.h"
#include "procedure/qstat/job/ocs_QStatJobController.h"
#include "procedure/qstat/job/ocs_QStatJobModel.h"
#include "procedure/qstat/job/ocs_QStatJobViewBase.h"
#include "procedure/qstat/job/ocs_QStatJobViewPlain.h"
#include "procedure/qstat/job/ocs_QStatJobViewXML.h"
#include "procedure/qstat/job/ocs_QStatJobViewJSON.h"
#include "procedure/qstat/select/ocs_QStatSelectViewBase.h"
#include "procedure/qstat/select/ocs_QStatSelectViewPlain.h"
#include "procedure/qstat/select/ocs_QStatSelectViewXML.h"
#include "procedure/qstat/select/ocs_QStatSelectController.h"
#include "procedure/qstat/ocs_QStatParameter.h"
#include "procedure/qstat/ocs_QStatModelBase.h"
#include "procedure/qstat/ocs_QStatModelClient.h"

#include "sig_handlers.h"
#include "qstat/ocs_QStatParameterClient.h"
#include "qstat/default/ocs_QStatDefaultViewJSON.h"
#include "qstat/group/ocs_QStatGroupViewJSON.h"
#include "qstat/select/ocs_QStatSelectViewJSON.h"

extern char **environ;

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "qstat");
   lList *answer_list = nullptr;

   sge_sig_handler_in_main_loop = 0;
   sge_setup_sig_handlers(QSTAT);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QSTAT, MAIN_THREAD, &answer_list) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }


   // parse command line parameters and options
   const std::string procedure_name = to_cstr(QSTAT);
   ocs::QStatParameterClient parameter(procedure_name);
   if (!parameter.parse_parameters(&answer_list, argv, environ)) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }

   // fetch additional environment variables
   auto long_qnames_name = "SGE_LONG_QNAMES";
   if (const char *long_qnames_value = getenv(long_qnames_name); long_qnames_value != nullptr) {
      parameter.add_variable(long_qnames_name, long_qnames_value);
   }
   auto more_info_name = "MORE_INFO";
   if (const char *more_info_value = getenv(more_info_name); more_info_value != nullptr) {
      parameter.add_variable(more_info_name, more_info_value);
   }


   // start processing
   std::ostringstream out_ss;
   switch (parameter.output_mode_) {
      case ocs::QStatParameter::OutputMode::JOB_INFO: {
         std::unique_ptr<ocs::QStatModelBase> model;
         model = std::make_unique<ocs::QStatJobModel>();
         if (!model->make_snapshot(&answer_list, parameter)) {
            answer_list_output(&answer_list);
            sge_exit(1);
         }

         std::unique_ptr<ocs::QStatJobViewBase> view;
         if (parameter.get_output_format() == ocs::QStatParameter::OutputFormat::XML) {
            view = std::make_unique<ocs::QStatJobViewXML>(parameter);
         } else if (parameter.get_output_format() == ocs::QStatParameter::OutputFormat::JSON) {
            view = std::make_unique<ocs::QStatJobViewJSON>(parameter);
         } else {
            view = std::make_unique<ocs::QStatJobViewPlain>(parameter);
         }

         ocs::QStatJobController controller(out_ss);
         controller.process_request(parameter, dynamic_cast<ocs::QStatJobModel &>(*model), *view);
         break;
      }
      case ocs::QStatParameter::OutputMode::QSELECT: {
         parameter.set_sub_procedure_name(to_cstr(QSELECT));

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
            std::unique_ptr<ocs::QStatModelBase> model;
            model = std::make_unique<ocs::QStatModelClient>();
            if (!model->make_snapshot(&answer_list, parameter)) {
               answer_list_output(&answer_list);
               sge_exit(1);
            }

            std::unique_ptr<ocs::QStatSelectViewBase> view;
            if (parameter.get_output_format() == ocs::QStatParameter::OutputFormat::XML) {
               view = std::make_unique<ocs::QStatSelectViewXML>(parameter);
            } else if (parameter.get_output_format() == ocs::QStatParameter::OutputFormat::JSON) {
               view = std::make_unique<ocs::QStatSelectViewJSON>(parameter);
            } else {
               view = std::make_unique<ocs::QStatSelectViewPlain>(parameter);
            }

            ocs::QStatSelectController controller(out_ss);
            controller.process_request(parameter, dynamic_cast<ocs::QStatModelClient &>(*model), *view);
         }
         break;
      }
      case ocs::QStatParameter::OutputMode::QSTAT_GROUP: {
         parameter.set_sub_procedure_name(ocs::QStatParameter::CQ_FORMAT);

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
            std::unique_ptr<ocs::QStatModelBase> model;
            model = std::make_unique<ocs::QStatModelClient>();
            if (!model->make_snapshot(&answer_list, parameter)) {
               answer_list_output(&answer_list);
               sge_exit(1);
            }

            std::unique_ptr<ocs::QStatGroupViewBase> view;
            if (parameter.get_output_format() == ocs::QStatParameter::OutputFormat::XML) {
               view = std::make_unique<ocs::QStatGroupViewXML>(parameter);
            } else if (parameter.get_output_format() == ocs::QStatParameter::OutputFormat::JSON) {
               view = std::make_unique<ocs::QStatGroupViewJSON>(parameter);
            } else {
               view = std::make_unique<ocs::QStatGroupViewPlain>(parameter);
            }

            ocs::QStatGroupController controller(out_ss);
            controller.process_request(parameter, dynamic_cast<ocs::QStatModelClient &>(*model), *view);
         }
         break;
      }
      case ocs::QStatParameter::OutputMode::QSTAT_DEFAULT: {
         parameter.set_sub_procedure_name(ocs::QStatParameter::DEFAULT_FORMAT);

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
            std::unique_ptr<ocs::QStatModelBase> model;
            model = std::make_unique<ocs::QStatModelClient>();
            if (!model->make_snapshot(&answer_list, parameter)) {
               answer_list_output(&answer_list);
               sge_exit(1);
            }

            std::unique_ptr<ocs::QStatDefaultViewBase> view;
            if (parameter.get_output_format() == ocs::QStatParameter::OutputFormat::XML) {
               view = std::make_unique<ocs::QStatDefaultViewXML>(parameter);
            } else if (parameter.get_output_format() == ocs::QStatParameter::OutputFormat::JSON) {
               view = std::make_unique<ocs::QStatDefaultViewJSON>(parameter);
            } else {
               view = std::make_unique<ocs::QStatDefaultViewPlain>(parameter);
            }

            ocs::QStatDefaultController controller(out_ss);
            controller.process_request(parameter, dynamic_cast<ocs::QStatModelClient &>(*model), *view);
         }
         break;
      }
         // no default, to get compiler warning if a new output mode is added but not handled here
   }

   // Output to the console
   std::cout << out_ss.str();

   // call exit handler
   sge_exit(0);
   return 0;
}
