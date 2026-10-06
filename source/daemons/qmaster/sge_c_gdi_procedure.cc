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

/** @file
 * @brief Dispatching a procedure call that arrived over GDI
 */

#include <sstream>
#include <memory>
#include <array>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_log.h"
#include "uti/sge_component.h"

#include "sgeobj/sge_answer.h"

#include "gdi/ocs_gdi_Target.h"

#include "procedure/ocs_ProcedureParameter.h"
#include "procedure/calendar/ocs_CalendarModServer.h"

#include "procedure/qconf/ocs_QConfController.h"
#include "procedure/qconf/table/ocs_QConfTableController.h"
#include "procedure/qconf/table/ocs_QConfTableView.h"
#include "procedure/qconf/objl/ocs_QConfObjectFilterController.h"
#include "procedure/qconf/eventclient/ocs_QConfEventClientView.h"
#include "procedure/qconf/threadpool/ocs_QConfThreadPoolController.h"
#include "procedure/qconf/threadpool/ocs_QConfThreadPoolView.h"
#include "procedure/qconf/sharetree/ocs_QConfShareTreeNodesController.h"
#include "procedure/qconf/sharetree/ocs_QConfShareTreeTreeController.h"
#include "procedure/qconf/sharetree/ocs_QConfShareTreeView.h"
#include "procedure/qconf/processors/ocs_QConfProcessorController.h"
#include "procedure/qconf/processors/ocs_QConfProcessorView.h"
#include "procedure/qconf/eventclient/ocs_QConfEventClientController.h"
#include "procedure/qconf/objl/ocs_QConfObjectFilterView.h"
#include "procedure/qconf/sick/ocs_QConfSickController.h"
#include "procedure/qconf/sick/ocs_QConfSickView.h"
#include "procedure/qconf/cqueue/ocs_QConfCQueueShowController.h"
#include "procedure/qconf/cqueue/ocs_QConfCQueueShowView.h"
#include "procedure/qconf/hgroup/ocs_QConfHGroupResolvedController.h"
#include "procedure/qconf/hgroup/ocs_QConfHGroupShowController.h"
#include "procedure/qconf/hgroup/ocs_QConfHGroupShowViewJSON.h"
#include "procedure/qconf/hgroup/ocs_QConfHGroupStructureView.h"
#include "procedure/qconf/hgroup/ocs_QConfHGroupTreeController.h"
#include "procedure/qconf/hgroup/ocs_QConfHGroupWhyController.h"
#include "procedure/qconf/hgroup/ocs_QConfHGroupWhyView.h"
#include "procedure/qconf/ocs_QConfModelServer.h"
#include "procedure/qconf/ocs_QConfViewBase.h"
#include "procedure/qconf/ocs_QConfViewJSON.h"
#include "procedure/qconf/ocs_QConfViewPlain.h"
#include "procedure/qconf/ocs_QConfViewXML.h"

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

#include "qstat/default/ocs_QStatDefaultController.h"
#include "qstat/default/ocs_QStatDefaultViewBase.h"
#include "qstat/default/ocs_QStatDefaultViewJSON.h"
#include "qstat/default/ocs_QStatDefaultViewPlain.h"
#include "qstat/default/ocs_QStatDefaultViewXML.h"

#include "qstat/group/ocs_QStatGroupController.h"
#include "qstat/group/ocs_QStatGroupViewBase.h"
#include "qstat/group/ocs_QStatGroupViewJSON.h"
#include "qstat/group/ocs_QStatGroupViewPlain.h"
#include "qstat/group/ocs_QStatGroupViewXML.h"

#include "qstat/job/ocs_QStatJobController.h"
#include "qstat/job/ocs_QStatJobViewBase.h"
#include "qstat/job/ocs_QStatJobViewJSON.h"
#include "qstat/job/ocs_QStatJobViewPlain.h"
#include "qstat/job/ocs_QStatJobViewXML.h"

#include "qstat/select/ocs_QStatSelectController.h"
#include "qstat/select/ocs_QStatSelectViewBase.h"
#include "qstat/select/ocs_QStatSelectViewJSON.h"
#include "qstat/select/ocs_QStatSelectViewPlain.h"
#include "qstat/select/ocs_QStatSelectViewXML.h"

#include "qstat/ocs_QStatModelServer.h"
#include "qstat/ocs_QStatParameter.h"
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

   /** @brief The `qconf` read path
    *
    * XML is named because the template asks every procedure for it; qconf has
    * only plain and json, and a call that requests XML is refused in
    * QConfModelBase::make_snapshot() before a view is ever chosen.
    */
   struct QConfTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfViewBase;
      using Controller = ocs::QConfController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfViewXML>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfViewPlain>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfViewJSON>(parameter);
      }
   };

   /** @brief `qconf -shgrp`: a sub-procedure, because the descriptor table cannot
    * express the JSON form (CS-2816). Shares QConfModelServer with the
    * table-driven read, the way the four qstat traits share theirs. */
   struct QConfHGroupShowTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfViewBase;
      using Controller = ocs::QConfHGroupShowController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfViewXML>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         /* the hand written plain form used HGRP_fields and qconf_sfi, which is
          * what the HGRP row holds, so the generic view serves it unchanged */
         return std::make_unique<ocs::QConfViewPlain>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfHGroupShowViewJSON>(parameter);
      }
   };

   /** @brief `qconf -shgrp_tree`. The three factories return the same class: the
    * hand written block ignored -fmt and printed plain text, and that is kept. */
   struct QConfHGroupTreeTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfHGroupStructureView;
      using Controller = ocs::QConfHGroupTreeController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -shgrp_resolved`, sharing the view with the tree */
   struct QConfHGroupResolvedTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfHGroupStructureView;
      using Controller = ocs::QConfHGroupResolvedController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -shgrp_why` */
   struct QConfHGroupWhyTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfHGroupWhyView;
      using Controller = ocs::QConfHGroupWhyController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -sc`: the complex entry table. One view for all three
    * factories, because the view switches on the output format itself - as the
    * hand written block did by passing qconf_opt_format to the writer. */
   struct QConfTableTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfTableView;
      using Controller = ocs::QConfTableController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -sq`: cluster queues, queue instances or a template. One view
    * for all three factories, as for the complex entry table - the view switches
    * on the output format itself. */
   struct QConfCQueueShowTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfCQueueShowView;
      using Controller = ocs::QConfCQueueShowController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -sds`: the queues whose configuration cannot take effect. One
    * view for all three factories - the report is plain text in every format. */
   struct QConfSickTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfSickView;
      using Controller = ocs::QConfSickController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -sobjl`: the names a filter matched. One view for all three
    * factories - the answer is a plain name list in every format until CS-2831. */
   struct QConfObjectFilterTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfObjectFilterView;
      using Controller = ocs::QConfObjectFilterController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -secl`: the registered event clients. One view for all three
    * factories - the view itself picks the table or the JSON array, because the
    * two share the fetched list and differ only in the writing. */
   struct QConfEventClientTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfEventClientView;
      using Controller = ocs::QConfEventClientController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -sep`: the processor table. One view for all three factories -
    * the switch never looked at the output format and still does not, which the
    * check pins; a JSON form is CS-2845. */
   struct QConfProcessorTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfProcessorView;
      using Controller = ocs::QConfProcessorController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -stl`: the qmaster thread pools. One view for all three
    * factories - it picks the table or the JSON array itself. */
   struct QConfThreadPoolTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfThreadPoolView;
      using Controller = ocs::QConfThreadPoolController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -sst` and `-bonsai`: the indented share tree view. */
   struct QConfShareTreeTreeTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfShareTreeView;
      using Controller = ocs::QConfShareTreeTreeController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
   };

   /** @brief `qconf -sstnode` and `-rsstnode`: one full path per node. Same view
    * as the tree form, a controller of its own because it renders the paths the
    * model took apart rather than the whole tree. */
   struct QConfShareTreeNodesTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfShareTreeView;
      using Controller = ocs::QConfShareTreeNodesController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
      }
      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ViewBase>(parameter);
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

   struct QStatSelectTraits {
      using Parameter = ocs::QStatParameter;
      using Model = ocs::QStatModelServer;
      using ViewBase = ocs::QStatSelectViewBase;
      using Controller = ocs::QStatSelectController;

      static constexpr ProgName prog_number = QSTAT;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatSelectViewXML>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatSelectViewPlain>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatSelectViewJSON>(parameter);
      }
   };

   struct QStatCQFormatTraits {
      using Parameter = ocs::QStatParameter;
      using Model = ocs::QStatModelServer;
      using ViewBase = ocs::QStatGroupViewBase;
      using Controller = ocs::QStatGroupController;

      static constexpr ProgName prog_number = QSTAT;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatGroupViewXML>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatGroupViewPlain>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatGroupViewJSON>(parameter);
      }
   };

   struct QStatDefaultTraits {
      using Parameter = ocs::QStatParameter;
      using Model = ocs::QStatModelServer;
      using ViewBase = ocs::QStatDefaultViewBase;
      using Controller = ocs::QStatDefaultController;

      static constexpr ProgName prog_number = QSTAT;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatDefaultViewXML>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatDefaultViewPlain>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatDefaultViewJSON>(parameter);
      }
   };

   struct QStatJobTraits {
      using Parameter = ocs::QStatParameter;
      using Model = ocs::QStatModelServer;
      using ViewBase = ocs::QStatJobViewBase;
      using Controller = ocs::QStatJobController;

      static constexpr ProgName prog_number = QSTAT;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatJobViewXML>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatJobViewPlain>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QStatJobViewJSON>(parameter);
      }
   };

   template<typename Traits>
   void exec_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                       std::ostringstream &os, uint32_t &reduction) {
      DENTER(TOP_LAYER);

      using Parameter = typename Traits::Parameter;
      using Model = typename Traits::Model;
      using ViewBase = typename Traits::ViewBase;
      using Controller = typename Traits::Controller;

      // Create parameter object containing the parsed switch information from the client
      Parameter parameter(task->data_list, packet);
      lFreeList(&task->data_list);

      // Create the server side model and make a snapshot of the required data
      Model model(packet, task);
      /* A model that modifies also needs the thread's monitoring structure,
       * because the write reaches qinstance_change_state_on_calendar_all()
       * through the object's on_success hook and that one monitors. Detected
       * rather than imposed: the reading models neither have nor need it, and
       * requiring the setter of all of them would be 22 empty overrides. */
      if constexpr (requires { model.set_monitor(monitor); }) {
         model.set_monitor(monitor);
      }
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

      /* CS-2821: where the model's verdict on RBAC reduction will be read once a
       * model can give one. Nothing reduces today, so it stays NONE - the seam is
       * a parameter rather than a comment so that it is typed and visible at the
       * call site, and it is not an accessor on each of the five model
       * hierarchies because "always zero" is not worth five edits. */
      reduction = ocs::ProcedureParameter::REDUCTION_NONE;

      DRETURN_VOID;
   }

   template<typename Traits>
   void prepare_response(ocs::gdi::Packet *packet, ocs::gdi::Task *task, std::ostringstream &os,
                         uint32_t reduction) {
      DENTER(TOP_LAYER);

      // if exec_procedure failed then the answer_list contains already an error
      // that is transferred via GDI. No need to send this separately
#if 0
      lWriteListTo(task->answer_list, stderr);
#endif

      // Prepare a response
      ocs::ProcedureParameter response(to_string(Traits::prog_number), packet);
      lList *bundle = response.get_bundle();

      // Add the procedures output to the bundle
      lList *output_list = nullptr;
      lAddElemStr(&output_list, ST_name, os.str().c_str(), ST_Type);
      ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::RESPONSE, output_list);

      /* CS-2821: and what RBAC removed from it. A separate entry rather than a
       * field of the response, because the response is a text and this is about
       * the text. The bundle's sublist is CULL_ANY_SUBTYPE, so a ULNG entry
       * round-trips without a wire format change. */
      lList *reduction_list = nullptr;
      lAddElemUlong(&reduction_list, ULNG_value, reduction, ULNG_Type);
      ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::REDUCTION, reduction_list);
#if 0
      DPRINTF("response:\n %s", os.str().c_str());
#endif

      // Pass responsibility for the bundle to gdi
      task->data_list = bundle;

      DRETURN_VOID;
   }

   template<typename Traits>
   void run_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor) {
      DENTER(TOP_LAYER);

      std::ostringstream out_ss;
      uint32_t reduction = ocs::ProcedureParameter::REDUCTION_NONE;
      exec_procedure<Traits>(packet, task, monitor, out_ss, reduction);
      prepare_response<Traits>(packet, task, out_ss, reduction);

      DRETURN_VOID;
   }

   /** @brief `calendar_mod`: the first procedure that changes state (CS-2822)
    *
    * One view for all three factories - the response is a confirmation line
    * with nothing to structure, so XML and JSON would add a wrapper and no
    * information.
    */
   struct CalendarModTraits {
      using Parameter = ocs::CalendarModParameter;
      using Model = ocs::CalendarModModelServer;
      using ViewBase = ocs::CalendarModView;
      using Controller = ocs::CalendarModController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::CalendarModView>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::CalendarModView>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::CalendarModView>(parameter);
      }
   };

   using ProcedureHandler = void (*)(ocs::gdi::Packet *, ocs::gdi::Task *, monitoring_t *);

   /** @brief What a caller must satisfy before a procedure may run
    *
    * Both halves exist because the dispatch below used to look at the procedure
    * name alone and ignore how the request had been addressed. Permission is
    * decided before the dispatch, from the command and the target:
    * sge_c_gdi_check_execution_permission() picks the read or the modify branch
    * by command, and the modify branch asks about task->target. A client could
    * therefore name a modifying procedure in a GET_PROCEDURE request and have it
    * checked as a read - no modify permission, no admin host - or name an object
    * it does not touch and be checked against that one instead. The entry
    * declares both, the dispatch refuses a request that disagrees, and a new
    * procedure cannot be registered without saying what it writes.
    */
   struct ProcedureRequirements {
      ocs::gdi::Command command;  ///< the command the request must carry
      ocs::gdi::Target target;    ///< the object list it acts on, for the permission check
   };

   /* A procedure that only reads, addressed as Target::PROCEDURE. That target
    * owns no permission of its own, which is right for a read: the GET branch
    * checks the host through sge_task_check_get_perm_host() and does not look at
    * an object list at all. A write cannot use it - sge_chck_mod_perm_user()
    * does not know it and refuses through its default - which is why a
    * modifying entry names the list it writes instead. */
   constexpr ProcedureRequirements READS{ocs::gdi::Command::GET_PROCEDURE, ocs::gdi::Target::PROCEDURE};

   /** @brief A procedure that modifies @p target
    *
    * The request must be addressed to that same list, so the permission checks
    * of the modify branch ask about the object the procedure really writes, and
    * so get_gdi_object() hands the dispatch the row carrying its modifier,
    * writer and on_success hooks.
    *
    * @param target the object list the procedure writes
    * @return its requirements
    */
   constexpr ProcedureRequirements modifies(const ocs::gdi::Target target) {
      return {ocs::gdi::Command::MOD_PROCEDURE, target};
   }

   struct ProcedureDispatchEntry {
      std::string_view procedure_name;
      std::string_view sub_procedure_name;
      ProcedureRequirements requirements;
      ProcedureHandler handler;
   };

   /* The size is deduced, not written down: it used to be a literal, so adding an
    * entry without also bumping it was a compile error at best - and the same
    * coupling in the 9.1 request limits, where the fill was a loop rather than a
    * brace list, silently overran the array instead (CS-2829). */
   constexpr auto procedure_dispatch_table = std::to_array<ProcedureDispatchEntry>({
      {to_string_view(QCONF), "", READS, &run_procedure<QConfTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::HGROUP_SHOW, READS, &run_procedure<QConfHGroupShowTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::HGROUP_TREE, READS, &run_procedure<QConfHGroupTreeTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::HGROUP_RESOLVED, READS, &run_procedure<QConfHGroupResolvedTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::HGROUP_WHY, READS, &run_procedure<QConfHGroupWhyTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::CENTRY_LIST, READS, &run_procedure<QConfTableTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::CATEGORY_LIST, READS, &run_procedure<QConfTableTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::QUEUE_SICK, READS, &run_procedure<QConfSickTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::OBJECT_FILTER, READS, &run_procedure<QConfObjectFilterTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::EVENT_CLIENT_LIST, READS, &run_procedure<QConfEventClientTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::PROCESSOR_LIST, READS, &run_procedure<QConfProcessorTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::THREAD_POOL_LIST, READS, &run_procedure<QConfThreadPoolTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::SHARETREE_TREE, READS, &run_procedure<QConfShareTreeTreeTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::SHARETREE_NODES, READS, &run_procedure<QConfShareTreeNodesTraits>},
      {to_string_view(QCONF), ocs::QConfParameter::CQUEUE_SHOW, READS, &run_procedure<QConfCQueueShowTraits>},
      /* The only entry that writes. It names CAL_LIST, which is what the
       * request must be addressed to and what the modify permission is checked
       * against - see ProcedureRequirements above. */
      {to_string_view(QCONF), ocs::CalendarModParameter::CALENDAR_MOD,
       modifies(ocs::gdi::Target::CAL_LIST), &run_procedure<CalendarModTraits>},
      {to_string_view(QHOST), "", READS, &run_procedure<QHostTraits>},
      {to_string_view(QQUOTA), "", READS, &run_procedure<QQuotaTraits>},
      {to_string_view(QRSTAT), "", READS, &run_procedure<QRStatTraits>},
      {to_string_view(QSTAT), to_string_view(QSELECT), READS, &run_procedure<QStatSelectTraits>},
      {to_string_view(QSTAT), ocs::QStatParameter::DEFAULT_FORMAT, READS, &run_procedure<QStatDefaultTraits>},
      {to_string_view(QSTAT), ocs::QStatParameter::CQ_FORMAT, READS, &run_procedure<QStatCQFormatTraits>},
      {to_string_view(QSTAT), ocs::QStatParameter::JOB_FORMAT, READS, &run_procedure<QStatJobTraits>},
   });
} // namespace

/** @brief Run a procedure call that arrived over GDI
 *
 * @param ao the table entry for the object being acted on
 * @param packet the client request
 * @param task the GDI task being answered
 * @param cmd the command being executed
 * @param sub_command what kind of modification this is
 * @param monitor for monitoring qmaster threads
 */
void sge_c_gdi_procedure(gdi_object_t *ao, ocs::gdi::Packet *packet, ocs::gdi::Task *task, ocs::gdi::Command cmd,
                         ocs::gdi::SubCommand sub_command, monitoring_t *monitor) {
   DENTER(TOP_LAYER);

   // get the name of the procedure that should be called
   const std::string name = ocs::ProcedureParameter::get_procedure_from_bundle(task->data_list);
   const std::string sub_name = ocs::ProcedureParameter::get_sub_procedure_from_bundle(task->data_list);
   DPRINTF("requested procedure: \"" SFN "/" SFN "\"\n", name.c_str(), sub_name.c_str());

   // Find and trigger the procedures handler
   for (const auto &[procedure_name, sub_procedure_name, requirements, handler]: procedure_dispatch_table) {
      if (procedure_name == name && sub_procedure_name == sub_name) {
         /* The request said which permission checks ran before we got here:
          * the command chose between the read and the modify branch of
          * sge_c_gdi_check_execution_permission(), and the target chose which
          * object list those checks asked about. Neither is believed - both
          * must match what this procedure's entry declares, or a caller could
          * have a modifying procedure checked as a read, or have it checked
          * against an object it does not touch. Refused here rather than in the
          * permission path because this is the function the worker dispatch
          * reaches, so the guard must not depend on a caller having looked
          * first. */
         if (cmd != requirements.command || task->target != requirements.target) {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE,
                     "stored procedure \"" SFN "/" SFN "\" cannot be executed as " SFN " on " SFN,
                     name.c_str(), sub_name.c_str(), to_string(cmd).c_str(),
                     to_string(task->target).c_str());
            answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
            DRETURN_VOID;
         }
         handler(packet, task, monitor);
         DRETURN_VOID;
      }
   }

   // show an error if no method was found
   snprintf(SGE_EVENT, SGE_EVENT_SIZE, "requested stored procedure \"" SFN "/" SFN "\" is not available", name.c_str(), sub_name.c_str());
   answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);

   DRETURN_VOID;
}
