/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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

#include <sstream>
#include <memory>
#include <array>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"
#include "uti/sge_component.h"

#include "sgeobj/sge_answer.h"

#include "procedure/ocs_ProcedureParameter.h"

#include "procedure/qhost/ocs_QHostContoller.h"
#include "procedure/qhost/ocs_QHostModelServer.h"
#include "procedure/qhost/ocs_QHostViewBase.h"
#include "procedure/qhost/ocs_QHostViewJSON.h"
#include "procedure/qhost/ocs_QHostViewPlain.h"
#include "procedure/qhost/ocs_QHostViewXML.h"

#include "procedure/qquota/ocs_QQuotaController.h"
#include "procedure/qquota/ocs_QQuotaModelServer.h"
#include "procedure/qquota/ocs_QQuotaViewBase.h"
#include "procedure/qquota/ocs_QQuotaViewJSON.h"
#include "procedure/qquota/ocs_QQuotaViewPlain.h"
#include "procedure/qquota/ocs_QQuotaViewXML.h"

#include "procedure/qrstat/ocs_QRStatController.h"
#include "procedure/qrstat/ocs_QRStatModelServer.h"
#include "procedure/qrstat/ocs_QRStatViewBase.h"
#include "procedure/qrstat/ocs_QRStatViewJSON.h"
#include "procedure/qrstat/ocs_QRStatViewPlain.h"
#include "procedure/qrstat/ocs_QRStatViewXML.h"

#include "sge_c_gdi_procedure.h"

namespace {
   struct QHostTraits {
      using Parameter = ocs::QHostParameter;
      using Model = ocs::QHostModelServer;
      using ViewBase = ocs::QHostViewBase;
      using Controller = ocs::QHostController;

      static constexpr ProgName prog_number = QHOST;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QHostViewXML>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QHostViewPlain>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QHostViewJSON>(parameter);
      }
   };

   struct QQuotaTraits {
      using Parameter = ocs::QQuotaParameter;
      using Model = ocs::QQuotaModelServer;
      using ViewBase = ocs::QQuotaViewBase;
      using Controller = ocs::QQuotaController;

      static constexpr ProgName prog_number = QQUOTA;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QQuotaViewXML>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QQuotaViewPlain>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QQuotaViewJSON>(parameter);
      }
   };

   struct QRStatTraits {
      using Parameter = ocs::QRStatParameter;
      using Model = ocs::QRStatModelServer;
      using ViewBase = ocs::QRStatViewBase;
      using Controller = ocs::QRStatController;

      static constexpr ProgName prog_number = QRSTAT;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QRStatViewXML>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QRStatViewPlain>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QRStatViewJSON>(parameter);
      }
   };

   template<typename Traits>
   void exec_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, std::ostringstream &os) {
      DENTER(TOP_LAYER);

      using Parameter = typename Traits::Parameter;
      using Model = typename Traits::Model;
      using ViewBase = typename Traits::ViewBase;
      using Controller = typename Traits::Controller;

      // Create parameter object containing the parsed switch information from the client
      Parameter parameter(&task->data_list);

      // Create the server side model and make a snapshot of the required data
      Model model(packet, task);
      if (!model.make_snapshot(&task->answer_list, parameter)) {
         // error was written by make_snapshot()
         DRETURN_VOID;
      }

      // Prepare view to show output
      std::unique_ptr<ViewBase> view;
      switch (parameter.get_output_format()) {
         case ocs::ProcedureParameter::OutputFormat::XML:
            view = Traits::make_xml_view(parameter);
            break;
         case ocs::ProcedureParameter::OutputFormat::PLAIN:
            view = Traits::make_plain_view(parameter);
            break;
         case ocs::ProcedureParameter::OutputFormat::JSON:
            view = Traits::make_json_view(parameter);
            break;
         default:
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, "unsupported output format requested in " SFQ, __func__);
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
            DRETURN_VOID;
      }

      // Process request and show output
      Controller controller(os);
      controller.process_request(parameter, model, *view);

      DRETURN_VOID;
   }

   template<typename Traits>
   void prepare_response(ocs::gdi::Task *task, std::ostringstream &os) {
      DENTER(TOP_LAYER);

      // Prepare a response
      ocs::ProcedureParameter response(to_string(Traits::prog_number));
      lList *bundle = response.get_bundle();

      // Add the procedures output to the bundle
      lList *output_list = nullptr;
      lAddElemStr(&output_list, ST_name, os.str().c_str(), ST_Type);
      ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::RESPONSE, output_list);

      // Pass responsibility for the bundle to gdi
      task->data_list = bundle;

      DRETURN_VOID;
   }

   template<typename Traits>
   void run_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task) {
      DENTER(TOP_LAYER);

      std::ostringstream out_ss;
      exec_procedure<Traits>(packet, task, out_ss);
      prepare_response<Traits>(task, out_ss);

      DRETURN_VOID;
   }

   using ProcedureHandler = void (*)(ocs::gdi::Packet *, ocs::gdi::Task *);

   struct ProcedureDispatchEntry {
      std::string_view name;
      ProcedureHandler handler;
   };

   const std::array<ProcedureDispatchEntry, 3> procedure_dispatch_table{
      {
         {to_string_view(QHOST), &run_procedure<QHostTraits>},
         {to_string_view(QQUOTA), &run_procedure<QQuotaTraits>},
         {to_string_view(QRSTAT), &run_procedure<QRStatTraits>}
      }
   };
} // namespace

void sge_c_gdi_procedure(gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::gdi::Command cmd,
                         ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // get the name of the procedure that should be called
   const std::string procedure_name = ocs::ProcedureParameter::get_procedure_from_bundle(task->data_list);

   // Find and trigger the procedures handler
   for (const auto &[name, handler]: procedure_dispatch_table) {
      if (procedure_name == name) {
         handler(packet, task);
         DRETURN_VOID;
      }
   }

   // show an error if no method was found
   snprintf(SGE_EVENT, SGE_EVENT_SIZE, "requested stored procedure " SFQ " is not available", procedure_name.c_str());
   answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);

   DRETURN_VOID;
}
