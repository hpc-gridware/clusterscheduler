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

#include <memory>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_uidgid.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_str.h"

#include "gdi/ocs_gdi_Client.h"

#include "procedure/qrstat/ocs_QRStatParameter.h"
#include "procedure/qrstat/ocs_QRStatModel.h"
#include "procedure/qrstat/ocs_QRStatViewBase.h"
#include "procedure/qrstat/ocs_QRStatViewXML.h"
#include "procedure/qrstat/ocs_QRStatViewPlain.h"
#include "procedure/qrstat/ocs_QRStatController.h"

#include "sig_handlers.h"


/************************************************************************/
int main(int argc, const char **argv) {
   DENTER_MAIN(TOP_LAYER, "qrsub");
   lList *answer_list = nullptr;

   /* Set up the program information name */
   sge_setup_sig_handlers(QRSTAT);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   if (ocs::gdi::ClientBase::setup_and_enroll(QRSTAT, MAIN_THREAD, &answer_list) != ocs::gdi::AE_OK) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }

   // parse command line parameters and options
   ocs::QRStatParameter parameter;
   if (!parameter.parse_parameters(&answer_list, argv, environ)) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }

   // fetch and prepare data for output
   ocs::QRStatModel model;
   if (!model.make_snapshot(&answer_list, parameter)) {
      answer_list_output(&answer_list);
      sge_exit(1);
   }

   // create view that will display the output in correct format
   std::unique_ptr<ocs::QRStatViewBase> view;
   if (parameter.is_xml) {
      view = std::make_unique<ocs::QRStatViewXML>(parameter);
   } else {
      view = std::make_unique<ocs::QRStatViewPlain>(parameter);
   }

   // start processing and show output
   ocs::QRStatController controller;
   controller.process_request(parameter, model, *view);

   ocs::gdi::ClientBase::shutdown();
   DRETURN(0);
}

