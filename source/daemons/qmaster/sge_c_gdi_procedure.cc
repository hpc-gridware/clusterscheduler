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
#include "procedure/qconf/objexport/ocs_QConfExportController.h"
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
#include "procedure/qconf/action/ocs_QConfActionServer.h"
#include "procedure/qconf/write/ocs_QConfWriteServer.h"

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

   /** @brief `qconf -S<obj>`: the export (CS-2826)
    *
    * The table-driven read with two differences, and both of them are in the
    * request rather than here: the export form of the field list, and a result
    * that is one text per object instead of one text. So it shares the model and
    * the views with the ordinary read and brings only its own controller, which
    * is what pairs each text with the name of the file it becomes.
    */
   struct QConfExportTraits {
      using Parameter = ocs::QConfParameter;
      using Model = ocs::QConfModelServer;
      using ViewBase = ocs::QConfViewBase;
      using Controller = ocs::QConfExportController;

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

   /** @brief What a procedure decided about itself, beside its output
    *
    * Everything a caller has to act on that is not the rendered text: how much
    * of it RBAC removed, and - for a procedure that writes - what the write
    * did. prepare_response() puts it into the response bundle.
    *
    * One struct rather than a growing list of out-parameters, because every one
    * of these has to be threaded through exec_procedure() and run_procedure()
    * whether the procedure at hand produces it or not.
    */
   struct ProcedureOutcome {
      uint32_t reduction = ocs::ProcedureParameter::REDUCTION_NONE;  ///< what RBAC removed
      ocs::QConfWriteResult write;                                   ///< what a write did
      bool has_write_result = false;                                 ///< whether @ref write was filled
      lList *export_objects = nullptr;   ///< what an export rendered, one entry per file (CS-2826)
      bool has_export_result = false;    ///< whether @ref export_objects was filled

      /** @brief Free what prepare_response() did not take
       *
       * The export list is the one member that owns memory. It is handed to the
       * response bundle when there is one to build, and the destructor catches
       * the path where there is not - a request that left exec_procedure() with
       * an error still rendered whatever it got that far with.
       */
      ~ProcedureOutcome() { lFreeList(&export_objects); }

      /* One owner, and the one that frees. Copying it would hand two of them the
       * same list, and the request path needs no copy: the struct is created in
       * run_procedure() and passed on by reference. */
      ProcedureOutcome() = default;
      ProcedureOutcome(const ProcedureOutcome &) = delete;
      ProcedureOutcome &operator=(const ProcedureOutcome &) = delete;
   };

   /** @brief The reduction a caller asked to have simulated (CS-2823)
    *
    * Turns ocs::ProcedureParameter::FORCE_REDUCTION_VARIABLE, which the client
    * shipped with the request, into the mask the response carries. See that
    * constant for why it exists and why it is harmless.
    *
    * An unknown token is ignored rather than refused: this is a test aid, and
    * failing a request over it would turn a mistyped variable into an outage.
    *
    * @param parameter the request
    * @return the forced mask, REDUCTION_NONE when nothing was asked for
    */
   uint32_t forced_reduction(const ocs::ProcedureParameter &parameter) {
      DENTER(TOP_LAYER);

      const char *value = parameter.get_variable(ocs::ProcedureParameter::FORCE_REDUCTION_VARIABLE);
      if (value == nullptr) {
         DRETURN(ocs::ProcedureParameter::REDUCTION_NONE);
      }

      uint32_t mask = ocs::ProcedureParameter::REDUCTION_NONE;
      std::string_view rest{value};
      while (true) {
         const auto comma = rest.find(',');
         const std::string_view token = rest.substr(0, comma);

         if (token == "masked") {
            mask |= ocs::ProcedureParameter::REDUCTION_MASKED;
         } else if (token == "hidden") {
            mask |= ocs::ProcedureParameter::REDUCTION_HIDDEN;
         } else if (!token.empty()) {
            DPRINTF("ignoring unknown " SFN " token " SFQ "\n",
                    ocs::ProcedureParameter::FORCE_REDUCTION_VARIABLE,
                    std::string{token}.c_str());
         }

         if (comma == std::string_view::npos) {
            break;
         }
         rest.remove_prefix(comma + 1);
      }
      DRETURN(mask);
   }

   template<typename Traits>
   void exec_procedure(ocs::gdi::Packet *packet, ocs::gdi::Task *task, monitoring_t *monitor,
                       std::ostringstream &os, ProcedureOutcome &outcome) {
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
      /* And which object type it writes. The dispatch entry names the target the
       * request must carry; the traits name the row of the qconf object table
       * that goes with it, because a target does not identify one - HGRP_LIST and
       * US_LIST each carry three rows. The provider compares the two, see
       * qconf_write_from_procedure(). */
      if constexpr (requires { Traits::write_object; }) {
         model.set_object(Traits::write_object);
      }

      const bool snapshot_ok = model.make_snapshot(&task->answer_list, parameter);

      /* Read before the verdict is acted on, not after: a write that the policy
       * refused still has to report how many items it could not read, because
       * that count is what -strict reports to the user. */
      if constexpr (requires { model.get_write_result(); }) {
         outcome.write = model.get_write_result();
         outcome.has_write_result = true;
      }

      if (!snapshot_ok) {
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

      /* CS-2826: an export renders one text per object and keeps them, because
       * the output stream has no way to say where one file ends and the next
       * begins. Taken from the controller the way a write result is read off
       * the model - detected rather than imposed, so no other controller needs
       * an empty accessor for it. */
      if constexpr (requires { controller.take_export_objects(); }) {
         outcome.export_objects = controller.take_export_objects();
         outcome.has_export_result = true;
      }

      /* CS-2821: where the model's verdict on RBAC reduction will be read once a
       * model can give one. Nothing reduces today, so the only non-zero mask is
       * one a caller asked to have simulated, which is what makes the refusal of
       * CS-2823 testable at all - see forced_reduction(). The seam stays a
       * parameter rather than a comment so that it is typed and visible at the
       * call site, and it is not an accessor on each of the five model
       * hierarchies because "always zero" is not worth five edits. */
      /* A template is not a stored object: the model built it for this answer
       * because the name does not exist (CS-2823), so there is nothing RBAC
       * could have removed from it and the response must not claim otherwise.
       * Reporting a reduction here would have the client refuse to open the
       * editor for an object that cannot have anything masked - which is how
       * "-a<obj>" would stop working the moment anything reduces. */
      bool is_template = false;
      if constexpr (requires { model.answered_with_template(); }) {
         is_template = model.answered_with_template();
      }
      outcome.reduction = is_template ? ocs::ProcedureParameter::REDUCTION_NONE
                                      : forced_reduction(parameter);

      DRETURN_VOID;
   }

   template<typename Traits>
   void prepare_response(ocs::gdi::Packet *packet, ocs::gdi::Task *task, std::ostringstream &os,
                         ProcedureOutcome &outcome) {
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
      lAddElemUlong(&reduction_list, ULNG_value, outcome.reduction, ULNG_Type);
      ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::REDUCTION, reduction_list);

      /* CS-2822: and what a write did, when this was one. Sent only then, so a
       * reading procedure's response is unchanged and the client can tell "no
       * write happened here" from "a write applied nothing". */
      if (outcome.has_write_result) {
         lList *applied_list = nullptr;
         lAddElemUlong(&applied_list, ULNG_value, outcome.write.applied, ULNG_Type);
         ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::WRITE_APPLIED,
                                                       applied_list);

         lList *failed_list = nullptr;
         lAddElemUlong(&failed_list, ULNG_value, outcome.write.parse_failed, ULNG_Type);
         ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::WRITE_PARSE_FAILED,
                                                       failed_list);

         lList *elements_list = nullptr;
         lAddElemUlong(&elements_list, ULNG_value, outcome.write.elements, ULNG_Type);
         ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::WRITE_ELEMENTS,
                                                       elements_list);
      }

      /* CS-2826: and the files an export rendered, as name/value pairs. Sent
       * whenever this was an export, empty list and all: the entry's presence is
       * what tells the caller that the export ran and found nothing from a
       * response that never carried the answer - see
       * ProcedureModel::get_export_objects(). The bundle takes the list, so the
       * outcome gives up its ownership here. */
      if (outcome.has_export_result) {
         ocs::ProcedureParameter::add_parameter_bundle(bundle, ocs::ProcedureParameter::EXPORT_OBJECTS,
                                                       outcome.export_objects);
         outcome.export_objects = nullptr;
      }
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
      ProcedureOutcome outcome;
      exec_procedure<Traits>(packet, task, monitor, out_ss, outcome);
      prepare_response<Traits>(packet, task, out_ss, outcome);

      DRETURN_VOID;
   }

   /** @brief `object_write`: the procedure that changes state (CS-2822)
    *
    * One procedure for every object type and any number of objects. The type is
    * a template argument rather than a field, so each registered type is its own
    * handler and the model learns which row of the qconf object table it writes
    * without the request having to say - a request that said so could lie.
    *
    * One view for all three factories: a write renders nothing, so XML and JSON
    * would wrap an empty text. What it has to say is in the answer list.
    *
    * @tparam OBJ the object type this entry writes
    */
   template<QConfObj OBJ>
   struct QConfWriteTraits {
      using Parameter = ocs::QConfWriteParameter;
      using Model = ocs::QConfWriteModelServer;
      using ViewBase = ocs::QConfWriteView;
      using Controller = ocs::QConfWriteController;

      static constexpr ProgName prog_number = QCONF;
      static constexpr QConfObj write_object = OBJ;   ///< read by exec_procedure()

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfWriteView>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfWriteView>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfWriteView>(parameter);
      }
   };

   /** @brief The traits of every qconf action (CS-2962)
    *
    * An action changes objects in qmaster without handing object texts over -
    * "-purge" (CS-2948), the access list membership switches (CS-2959). Which
    * function carries it out is looked up by the model from the action's name,
    * so all of them share these traits; what tells them apart in the table below
    * is the name and the object list each one is checked against. A write, so it
    * renders nothing, and it names no write_object.
    */
   struct QConfActionTraits {
      using Parameter = ocs::QConfActionParameter;
      using Model = ocs::QConfActionModelServer;
      using ViewBase = ocs::QConfActionView;
      using Controller = ocs::QConfActionController;

      static constexpr ProgName prog_number = QCONF;

      static std::unique_ptr<ViewBase> make_xml_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfActionView>(parameter);
      }

      static std::unique_ptr<ViewBase> make_plain_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfActionView>(parameter);
      }

      static std::unique_ptr<ViewBase> make_json_view(const Parameter &parameter) {
         return std::make_unique<ocs::QConfActionView>(parameter);
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
      {to_string_view(QCONF), ocs::QConfParameter::OBJECT_EXPORT, READS, &run_procedure<QConfExportTraits>},
      /* The entries that write. All of them name the same procedure and
       * sub-procedure and differ only in the object they write, which is why the
       * lookup below matches on the target as well - see sge_c_gdi_procedure().
       *
       * Each names the GDI target the request must be addressed to and the row
       * of the qconf object table that is written. The two have to agree, and
       * the provider checks that they do; it cannot be a static_assert because
       * the object table is not constexpr.
       *
       * One row per type, which is the point of templating the traits. The
       * eight types on the CS-2797 editor engine came first, the three whose
       * editor was written by hand followed when their read side had reached
       * the descriptor table (CS-2823), then the list types, and last the
       * configuration and the two singletons - the three that gdi_object_t does
       * not describe. A missing row here is a write refused as addressed
       * elsewhere, which is how the resource quota row was found to be missing:
       * by hand, after everything compiled. */
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::CAL_LIST), &run_procedure<QConfWriteTraits<QConfObj::CAL>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::CE_LIST), &run_procedure<QConfWriteTraits<QConfObj::CE>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::CK_LIST), &run_procedure<QConfWriteTraits<QConfObj::CK>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::CONF_LIST), &run_procedure<QConfWriteTraits<QConfObj::CONF>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::CQ_LIST), &run_procedure<QConfWriteTraits<QConfObj::CQ>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::EH_LIST), &run_procedure<QConfWriteTraits<QConfObj::EH>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::HGRP_LIST), &run_procedure<QConfWriteTraits<QConfObj::HGRP>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::PE_LIST), &run_procedure<QConfWriteTraits<QConfObj::PE>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::PR_LIST), &run_procedure<QConfWriteTraits<QConfObj::PR>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::RL_LIST), &run_procedure<QConfWriteTraits<QConfObj::RL>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::RQS_LIST), &run_procedure<QConfWriteTraits<QConfObj::RQS>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::SC_LIST), &run_procedure<QConfWriteTraits<QConfObj::SC>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::STN_LIST), &run_procedure<QConfWriteTraits<QConfObj::STN>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::US_LIST), &run_procedure<QConfWriteTraits<QConfObj::US>>},
      {to_string_view(QCONF), ocs::QConfWriteParameter::OBJECT_WRITE,
       modifies(ocs::gdi::Target::UU_LIST), &run_procedure<QConfWriteTraits<QConfObj::UU>>},
      /* CS-2948: "qconf -purge" changes one cluster queue in place, so it is
       * checked as a modify of the queue list - the same permission a "-mattr"
       * on the queue needs. */
      {to_string_view(QCONF), ocs::QConfActionParameter::QUEUE_PURGE,
       modifies(ocs::gdi::Target::CQ_LIST), &run_procedure<QConfActionTraits>},
      /* CS-2959: "-au"/"-du" and "-am"/"-ao"/"-dm"/"-do" change the entries of
       * access lists in place, so they are checked as a modify of the userset
       * list - what the client's GET and MOD were checked as before. The
       * manager-only rule of the reserved lists stays in userset_mod(). */
      {to_string_view(QCONF), ocs::QConfActionParameter::USERSET_MEMBERS,
       modifies(ocs::gdi::Target::US_LIST), &run_procedure<QConfActionTraits>},
      /* CS-2963: "-clearusage" clears the usage of users and projects alike. It
       * is checked as a modify of the user list; the project list carries the
       * same permission (both are manager-only in sge_c_gdi.cc). */
      {to_string_view(QCONF), ocs::QConfActionParameter::USAGE_CLEAR,
       modifies(ocs::gdi::Target::UU_LIST), &run_procedure<QConfActionTraits>},
      /* CS-2966: "-astnode"/"-mstnode"/"-dstnode" change nodes of the share
       * tree in place, checked as a modify of the tree - what the client's MOD
       * of the whole tree was checked as. */
      {to_string_view(QCONF), ocs::QConfActionParameter::SHARETREE_NODES,
       modifies(ocs::gdi::Target::STN_LIST), &run_procedure<QConfActionTraits>},
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
   /* The name pair alone no longer picks an entry: every object type the write
    * procedure covers registers the same procedure and sub-procedure and differs
    * only in the target it writes (CS-2822). So the addressing is part of the
    * key, and the first entry that matches the name pair is no longer
    * necessarily the right one.
    *
    * The two misses are told apart because they mean different things to the
    * caller. An unknown name is a procedure this daemon does not have; a known
    * name with the wrong command or target is a procedure that exists and was
    * addressed in a way it refuses - which is the guard itself, and the message
    * has to say so. */
   bool name_matched = false;
   for (const auto &[procedure_name, sub_procedure_name, requirements, handler]: procedure_dispatch_table) {
      if (procedure_name != name || sub_procedure_name != sub_name) {
         continue;
      }
      name_matched = true;

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
         continue;
      }

      handler(packet, task, monitor);
      DRETURN_VOID;
   }

   if (name_matched) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE,
               "stored procedure \"" SFN "/" SFN "\" cannot be executed as " SFN " on " SFN,
               name.c_str(), sub_name.c_str(), to_string(cmd).c_str(),
               to_string(task->target).c_str());
      answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);
      DRETURN_VOID;
   }

   // show an error if no method was found
   snprintf(SGE_EVENT, SGE_EVENT_SIZE, "requested stored procedure \"" SFN "/" SFN "\" is not available", name.c_str(), sub_name.c_str());
   answer_list_add(&(task->answer_list), SGE_EVENT, STATUS_ENOIMP, ANSWER_QUALITY_ERROR);

   DRETURN_VOID;
}
