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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

#include "uti/ocs_Pattern.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/sge_dstring.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"
#include "uti/sge_unistd.h"
#include "uti/sge_bootstrap_files.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_job.h"

#include "sched/sge_schedd_text.h"

#include "gdi/ocs_gdi_Client.h"

#include "ocs_QStatDefaultController.h"
#include "ocs_QStatDefaultViewBase.h"
#include "ocs_QStatDefaultViewPlain.h"
#include "ocs_QStatDefaultViewXML.h"
#include "ocs_QStatGroupViewBase.h"
#include "ocs_QStatGroupViewPlain.h"
#include "ocs_QStatGroupViewXML.h"
#include "ocs_QStatGroupController.h"
#include "ocs_QStatGenericModel.h"
#include "ocs_QStatJobController.h"
#include "ocs_QStatJobModel.h"
#include "ocs_QStatJobViewBase.h"
#include "ocs_QStatJobViewPlain.h"
#include "ocs_QStatJobViewXML.h"
#include "ocs_QStatParameter.h"
#include "ocs_QStatSelectViewBase.h"
#include "ocs_QStatSelectViewPlain.h"
#include "ocs_QStatSelectViewXML.h"
#include "ocs_QStatSelectController.h"
#include "ocs_QStatModelBase.h"
#include "sig_handlers.h"
#include "ocs_client_job.h"
#include "ocs_client_print.h"
#include "basis_types.h"
#include "ocs_qstat_xml.h"
#include "msg_qstat.h"


#define FORMAT_I_20 "%I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I %I "
#define FORMAT_I_10 "%I %I %I %I %I %I %I %I %I %I "
#define FORMAT_I_5 "%I %I %I %I %I "
#define FORMAT_I_2 "%I %I "
#define FORMAT_I_1 "%I "

/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
/*-------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "qstat");
   lList *alp = nullptr;

   sge_sig_handler_in_main_loop = 0;
   sge_setup_sig_handlers(QSTAT);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QSTAT, MAIN_THREAD, &alp) != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   // parse command line parameters and options
   ocs::QStatParameter parameter;
   if (!parameter.parse_parameters(&alp, argv, environ)) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   // create model according to output mode and fetch data
   std::unique_ptr<ocs::QStatModelBase> model;
   if (parameter.output_mode_ == ocs::QStatParameter::OutputMode::JOB_INFO) {
      model = std::make_unique<ocs::QStatJobModel>();
   } else {
      model = std::make_unique<ocs::QStatGenericModel>();
   }
   if (!model->make_snapshot(&alp, parameter)) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   // start processing
   switch (parameter.output_mode_) {
      case ocs::QStatParameter::OutputMode::JOB_INFO: {
         std::unique_ptr<ocs::QStatJobViewBase> view;
         if (parameter.output_format_== ocs::QStatParameter::OutputFormat::XML) {
            view = std::make_unique<ocs::QStatJobViewXML>();
         } else {
            view = std::make_unique<ocs::QStatJobViewPlain>();
         }

         ocs::QStatJobController controller;
         controller.process_request(parameter, dynamic_cast<ocs::QStatJobModel &>(*model), *view);
         break;
      }
      case ocs::QStatParameter::OutputMode::QSELECT: {
         std::unique_ptr<ocs::QStatSelectViewBase> view;
         if (parameter.output_format_== ocs::QStatParameter::OutputFormat::XML) {
            view = std::make_unique<ocs::QStatSelectViewXML>(parameter);
         } else {
            view = std::make_unique<ocs::QStatSelectViewPlain>(parameter);
         }

         ocs::QStatSelectController controller;
         controller.process_request(parameter, dynamic_cast<ocs::QStatGenericModel &>(*model), *view);
         break;
      }
      case ocs::QStatParameter::OutputMode::QSTAT_GROUP: {
         std::unique_ptr<ocs::QStatGroupViewBase> view;
         if (parameter.output_format_== ocs::QStatParameter::OutputFormat::XML) {
            view = std::make_unique<ocs::QStatGroupViewXML>();
         } else {
            view = std::make_unique<ocs::QStatGroupViewPlain>();
         }

         ocs::QStatGroupController controller;
         controller.process_request(parameter, dynamic_cast<ocs::QStatGenericModel &>(*model), *view);
         break;
      }
      case ocs::QStatParameter::OutputMode::QSTAT_DEFAULT: {
         std::unique_ptr<ocs::QStatDefaultViewBase> view;
         if (parameter.output_format_== ocs::QStatParameter::OutputFormat::XML) {
            view = std::make_unique<ocs::QStatDefaultViewXML>();
         } else {
            view = std::make_unique<ocs::QStatDefaultViewPlain>();
         }

         ocs::QStatDefaultController controller;
         controller.process_request(parameter, dynamic_cast<ocs::QStatGenericModel &>(*model), *view);
         break;
      }
      // no default, to get compiler warning if a new output mode is added but not handled here
   }

   // call exit handler
   sge_exit(0);
   return 0;
}


