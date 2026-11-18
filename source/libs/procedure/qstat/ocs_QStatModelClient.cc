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
 * @brief Client side model of `qstat`: fetches the lists over GDI
 */

#include <sstream>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_bootstrap_files.h"
#include "uti/sge_log.h"
#include "uti/sge_stdlib.h"

#include "cull/cull_what.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_schedd_conf.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CONF.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_Request.h"

#include "ocs_QStatParameter.h"
#include "ocs_QStatModelClient.h"


/** @brief The answer to one recorded GET request, as a typed list
 *
 * A GET of a target answers with a list of the target's type (reduced to the
 * selection), taken over as the typed get_response() takes it over.
 *
 * @tparam T the object type of the target
 * @param answer_list receives error messages
 * @param request the request the GET was recorded in, after wait()
 * @param target the target of the GET
 * @param id the id the GET was recorded with
 * @return the owning handle on the list, empty if the answer held none
 */
template <ocs::tom::ObjectType T>
static ocs::tom::ListPtr<T>
get_list_response(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const ocs::gdi::Request &request,
                  const ocs::gdi::Target target, const int id) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<T> list;
   request.get_response(answer_list, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, target, id, list);
   DRETURN(list);
}

/** @brief Fetch the lists from qmaster with one multi GDI request
 * @param answer_list receives error messages
 * @param parameter the parsed parameters
 * @return true when every list could be fetched
 */
bool ocs::QStatModelClient::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   gdi::Request gdi_multi{};

   if (!gdi::Client::sge_gdi_get_permission(answer_list, &is_manager_, nullptr, nullptr, nullptr)) {
      DRETURN(false);
   }

   // Data for the qstat -j view; the conditions and selections are built for the type of their target by the
   // base class

   const auto what_sme = ocs::tom::What<ocs::tom::SME>::from_typed(get_sme_what());
   const int sme_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::SME_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, ocs::tom::Where<ocs::tom::SME>(), what_sme);

   int job_view_id = -1;
   if (parameter.get_jid_list() != nullptr) {
      const auto job_view_where = ocs::tom::Where<ocs::tom::JB>::from_typed(
              get_job_view_where(parameter.get_jid_list(), parameter.get_user_list()));
      const auto job_view_what = ocs::tom::What<ocs::tom::JB>::from_typed(get_job_view_what());

      /* get job list for qtstat -j output */
      job_view_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::JB_LIST, gdi::Command::GET,
                                      gdi::SubCommand::NONE, nullptr, job_view_where, job_view_what);
   }

   // Data for all other views

   int q_id = -1;
   if (parameter.need_queues_) {
      const auto queue_what = ocs::tom::What<ocs::tom::CQ>::from_typed(get_queue_what());
      q_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::CQ_LIST, gdi::Command::GET,
                               gdi::SubCommand::NONE, nullptr, ocs::tom::Where<ocs::tom::CQ>(), queue_what);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   int j_id = -1;
   if (parameter.need_job_list_) {
      const auto job_where = ocs::tom::Where<ocs::tom::JB>::from_typed(get_job_where(parameter));
      const auto job_what = ocs::tom::What<ocs::tom::JB>::from_typed(get_job_what(parameter));
      j_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::JB_LIST, gdi::Command::GET,
                               gdi::SubCommand::NONE, nullptr, job_where, job_what);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   const auto centry_what = ocs::tom::What<ocs::tom::CE>::from_typed(get_centry_what());
   const int ce_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::CE_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, ocs::tom::Where<ocs::tom::CE>(), centry_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   const auto ehost_where = ocs::tom::Where<ocs::tom::EH>::from_typed(get_ehost_where());
   const auto ehost_what = ocs::tom::What<ocs::tom::EH>::from_typed(get_ehost_what());
   const int eh_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::EH_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, ehost_where, ehost_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   const auto pe_what = ocs::tom::What<ocs::tom::PE>::from_typed(get_pe_what());
   const int pe_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::PE_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, ocs::tom::Where<ocs::tom::PE>(), pe_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   const auto ckpt_what = ocs::tom::What<ocs::tom::CK>::from_typed(get_ckpt_what());
   const int ckpt_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::CK_LIST, gdi::Command::GET,
                                         gdi::SubCommand::NONE, nullptr, ocs::tom::Where<ocs::tom::CK>(), ckpt_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   const auto uset_what = ocs::tom::What<ocs::tom::US>::from_typed(get_uset_what());
   const int acl_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::US_LIST, gdi::Command::GET,
                                        gdi::SubCommand::NONE, nullptr, ocs::tom::Where<ocs::tom::US>(), uset_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   const auto prj_what = ocs::tom::What<ocs::tom::PR>::from_typed(get_prj_what());
   const int up_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::PR_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, ocs::tom::Where<ocs::tom::PR>(), prj_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   const auto sconf_what = ocs::tom::What<ocs::tom::SC>::from_typed(get_sconf_what());
   const int sc_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::SC_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, ocs::tom::Where<ocs::tom::SC>(), sconf_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   const auto hgrp_what = ocs::tom::What<ocs::tom::HGRP>::from_typed(get_hgrp_what());
   int hgrp_id = gdi_multi.request(answer_list, gdi::Mode::RECORD, gdi::Target::HGRP_LIST, gdi::Command::GET,
                                   gdi::SubCommand::NONE, nullptr, ocs::tom::Where<ocs::tom::HGRP>(), hgrp_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   const auto conf_where = ocs::tom::Where<ocs::tom::CONF>::from_typed(get_conf_where());
   const auto conf_what = ocs::tom::What<ocs::tom::CONF>::from_typed(get_conf_what());
   const int gc_id = gdi_multi.request(answer_list, gdi::Mode::SEND, gdi::Target::CONF_LIST, gdi::Command::GET,
                                       gdi::SubCommand::NONE, nullptr, conf_where, conf_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }


   gdi_multi.wait();

   // Start fetching the lists

   // Job view data (-j)
   ilp = get_list_response<ocs::tom::SME>(answer_list, gdi_multi, gdi::Target::SME_LIST, sme_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   if (parameter.get_jid_list() != nullptr) {
      jlp = get_list_response<ocs::tom::JB>(answer_list, gdi_multi, gdi::Target::JB_LIST, job_view_id);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   // Other views
   if (parameter.need_queues_) {
      cqueue_list_ = get_list_response<ocs::tom::CQ>(answer_list, gdi_multi, gdi::Target::CQ_LIST, q_id);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   if (parameter.need_job_list_) {
      job_list_ = get_list_response<ocs::tom::JB>(answer_list, gdi_multi, gdi::Target::JB_LIST, j_id);
      if (answer_list_has_error(answer_list)) {
         DRETURN(false);
      }
   }

   centry_list_ = get_list_response<ocs::tom::CE>(answer_list, gdi_multi, gdi::Target::CE_LIST, ce_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   exechost_list_ = get_list_response<ocs::tom::EH>(answer_list, gdi_multi, gdi::Target::EH_LIST, eh_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   pe_list_ = get_list_response<ocs::tom::PE>(answer_list, gdi_multi, gdi::Target::PE_LIST, pe_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   ckpt_list_ = get_list_response<ocs::tom::CK>(answer_list, gdi_multi, gdi::Target::CK_LIST, ckpt_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   acl_list_ = get_list_response<ocs::tom::US>(answer_list, gdi_multi, gdi::Target::US_LIST, acl_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   project_list_ = get_list_response<ocs::tom::PR>(answer_list, gdi_multi, gdi::Target::PR_LIST, up_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   schedd_config_ = get_list_response<ocs::tom::SC>(answer_list, gdi_multi, gdi::Target::SC_LIST, sc_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   // taken now: prepare_data() hands schedd_config_ over to the sconf module
   if (const auto sc = schedd_config_->front(); sc != nullptr) {
      job_load_adjustments_ = ocs::tom::get<SC_job_load_adjustments>(sc).clone("load_adj_copy");
   }
   // An empty list, not nullptr, when none are configured: the sched helpers read nullptr as "fetch them
   // from the data store of the calling thread" (LOAD_ADJUSTMENTS_LATE_FETCH in get_attribute()), which
   // after the early release of the reader lock would read the data store unprotected (CS-3008).
   if (job_load_adjustments_ == nullptr) {
      job_load_adjustments_ = ocs::tom::ListPtr<ocs::tom::CE>::create("load_adj_copy");
   }

   hgrp_list_ = get_list_response<ocs::tom::HGRP>(answer_list, gdi_multi, gdi::Target::HGRP_LIST, hgrp_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   const auto conf_l = get_list_response<ocs::tom::CONF>(answer_list, gdi_multi, gdi::Target::CONF_LIST, gc_id);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   if (const auto global_conf = conf_l->front(); global_conf != nullptr) {
      const char *cell_root = bootstrap_get_cell_root();
      const uint32_t progid = component_get_component_id();
      merge_configuration(nullptr, progid, cell_root, global_conf, nullptr);
   }

   // the access lists of the global configuration just merged
   config_user_lists_ = mconf_get_user_lists();
   config_xuser_lists_ = mconf_get_xuser_lists();

   DRETURN(true);
}

/** @brief Hand the scheduler configuration to the sconf module and prepare the complex entries
 *
 * On success the sconf module takes the configuration over and the model's
 * handle is empty afterwards; on failure it stays with the model.
 *
 * @param answer_list receives error messages
 * @param parameter the parsed parameters
 * @return true when the lists could be prepared
 */
bool ocs::QStatModelClient::prepare_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter) {
   DENTER(TOP_LAYER);

   // init the sconf module; it takes the list over on success and leaves it untouched otherwise
   const bool sconf_ok = sconf_set_config(schedd_config_, answer_list);
   if (!sconf_ok) {
      DRETURN(false);
   }

   // make the centry list ready to get used
   centry_list_init_double(*centry_list_);

   // do common client/server things
   DRETURN(QStatModelBase::prepare_data(answer_list, parameter));
}
