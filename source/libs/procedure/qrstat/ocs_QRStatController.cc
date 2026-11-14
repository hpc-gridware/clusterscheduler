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
 * @brief Controller of `qrstat`: runs the request and drives the view
 */

#include <sstream>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/ocs_GrantedResources.h"
#include "sgeobj/cull/sge_host_RESL_L.h"
#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_qref.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_ARA.h"
#include "sgeobj/tom/ocs_tom_BN.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_MR.h"
#include "sgeobj/tom/ocs_tom_QIM.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_RESL.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "sgeobj/tom/ocs_tom_ST.h"

#include "qrstat/ocs_QRStatController.h"

#include <iostream>

#include "uti/sge_string.h"

/** @brief Run the request
 * @param parameter the parsed parameters
 * @param model the model, client or server side
 * @param view the view for the requested output format
 */
void
ocs::QRStatController::process_request(QRStatParameter &parameter, QRStatModelBase &model, QRStatViewBase &view) {
   DENTER(TOP_LAYER);

   view.report_start(out_);
   for_each_ep_lv(ar, model.get_ar_list()) {

      view.report_ar_start(out_);
      view.report_ar_node_ulong(out_, "id", ocs::tom::get<AR_id>(ar));
      view.report_ar_node_string(out_, "name", ocs::tom::get<AR_name>(ar));
      view.report_ar_node_string(out_, "owner", ocs::tom::get<AR_owner>(ar));
      view.report_ar_node_state(out_, "state", ocs::tom::get<AR_state>(ar));
      view.report_ar_node_time(out_, "start_time", ocs::tom::get<AR_start_time>(ar));
      view.report_ar_node_time(out_, "end_time", ocs::tom::get<AR_end_time>(ar));
      view.report_ar_node_duration(out_, "duration", ocs::tom::get<AR_duration>(ar));

      if (parameter.is_explain() || !parameter.is_summary()) {
         for (ocs::tom::ConstElement<ocs::tom::QU> qinstance : ocs::tom::get<AR_reserved_queues>(ar)) {
            for (ocs::tom::ConstElement<ocs::tom::QIM> qim : ocs::tom::get<QU_message_list>(qinstance)) {
               const char *message = ocs::tom::get<QIM_message>(qim);
               view.report_ar_node_string(out_, "message", message);
            }
         }
      }
      if (!parameter.is_summary()) {
         view.report_ar_node_time(out_, "submission_time", ocs::tom::get<AR_submission_time>(ar));
         view.report_ar_node_string(out_, "group", ocs::tom::get<AR_group>(ar));
         view.report_ar_node_string(out_, "account", ocs::tom::get<AR_account>(ar));
         if (const char *project = ocs::tom::get<AR_project>(ar); project != nullptr) {
            view.report_ar_node_string(out_, "project", project);
         }

         if (const lListElem *binding = ocs::tom::get<AR_binding>(ar); binding != nullptr) {
            std::string binding_string;
            BindingIo::binding_print_to_string(binding, binding_string, false);
            view.report_ar_node_string(out_, "binding", binding_string.c_str());
         }

         if (static_cast<const lList *>(ocs::tom::get<AR_resource_list>(ar)) != nullptr) {
            view.report_resource_list_start(out_);

            for (ocs::tom::ConstElement<ocs::tom::CE> resource : ocs::tom::get<AR_resource_list>(ar)) {
               // get the dominant value of the resource for this host
               const auto type = static_cast<CEntry::Type>(ocs::tom::get<CE_valtype>(resource));
               const bool as_string = type == CEntry::Type::STR || type == CEntry::Type::CSTR || type == CEntry::Type::HOST || type == CEntry::Type::RESTR;
               const bool as_double = type == CEntry::Type::DOUBLE;
               const bool as_bool = type == CEntry::Type::BOOL;
               if (as_string) {
                  view.report_resource_list_node_str(out_, ocs::tom::get<CE_name>(resource), ocs::tom::get<CE_stringval>(resource));
               } else if (as_double) {
                  view.report_resource_list_node_double(out_, ocs::tom::get<CE_name>(resource), ocs::tom::get<CE_doubleval>(resource));
               } else if (as_bool) {
                  view.report_resource_list_node_bool(out_, ocs::tom::get<CE_name>(resource), ocs::tom::get<CE_doubleval>(resource) != 0);
               } else {
                  view.report_resource_list_node_uint64(out_, ocs::tom::get<CE_name>(resource), static_cast<uint64_t>(ocs::tom::get<CE_doubleval>(resource)));
               }
            }
            view.report_resource_list_finish(out_);
         }

         if (ocs::tom::get<AR_error_handling>(ar) != 0) {
            view.report_ar_node_boolean(out_, "error_handling", true);
         }

         if (static_cast<const lList *>(ocs::tom::get<AR_granted_resources_list>(ar)) != nullptr) {
            view.report_exec_binding_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::GRU> resource : ocs::tom::get<AR_granted_resources_list>(ar)) {
               const char *hostname = ocs::tom::get<GRU_host>(resource);
               for (ocs::tom::ConstElement<ocs::tom::ST> binding_str : ocs::tom::get<GRU_binding_inuse>(resource)) {
                  TopologyString binding;

                  const char *binding_touse = ocs::tom::get<ST_name>(binding_str);
                  if (binding_touse != nullptr) {
                     binding.reset_topology(binding_touse);
                  }

                  view.report_exec_binding_list_node(out_, hostname, binding.to_product_topology_string().c_str());
               }
            }
            view.report_exec_binding_list_finish(out_);
         }

         // the resource maps the reservation holds, per host, with the granted ids. The
         // binding above comes from the same list but is reported on its own, so only
         // resource map entries are of interest here.
         if (static_cast<const lList *>(ocs::tom::get<AR_granted_resources_list>(ar)) != nullptr) {
            bool any = false;

            for (ocs::tom::ConstElement<ocs::tom::GRU> resource : ocs::tom::get<AR_granted_resources_list>(ar)) {
               if (static_cast<GrantedResources::Type>(ocs::tom::get<GRU_type>(resource)) !=
                   GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
                  continue;
               }
               if (!any) {
                  view.report_granted_resource_list_start(out_);
                  any = true;
               }

               std::string text = std::string(ocs::tom::get<GRU_name>(resource)) + "=" +
                                  std::to_string(static_cast<uint32_t>(ocs::tom::get<GRU_amount>(resource)));
               ocs::tom::ConstList<ocs::tom::RESL> ids = ocs::tom::get<GRU_resource_map_list>(resource);
               if (ids != nullptr && lGetNumberOfElem(ids) > 0) {
                  const char *sep = "(";
                  for (ocs::tom::ConstElement<ocs::tom::RESL> resl : ids) {
                     // the reader folds a repeated id into one element with a count, so
                     // write it out the way the configuration does
                     for (uint32_t i = ocs::tom::get<RESL_amount>(resl); i > 0; i--) {
                        text += sep;
                        text += ocs::tom::get<RESL_value>(resl);
                        sep = " ";
                     }
                  }
                  text += ")";
               }
               view.report_granted_resource_list_node(out_, ocs::tom::get<GRU_host>(resource), text.c_str());
            }
            if (any) {
               view.report_granted_resource_list_finish(out_);
            }
         }

         if (static_cast<const lList *>(ocs::tom::get<AR_granted_slots>(ar)) != nullptr) {
            view.report_exec_queue_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::JG> resource : ocs::tom::get<AR_granted_slots>(ar)) {
               view.report_exec_queue_list_node(out_,ocs::tom::get<JG_qname>(resource), ocs::tom::get<JG_slots>(resource));
            }
            view.report_exec_queue_list_finish(out_);
         }
         if (ocs::tom::get<AR_pe>(ar) != nullptr) {
            dstring pe_range_string = DSTRING_INIT;

            range_list_print_to_string(ocs::tom::get<AR_pe_range>(ar), &pe_range_string, true, false, false);
            view.report_granted_parallel_environment_start(out_);
            view.report_granted_parallel_environment_node(out_, ocs::tom::get<AR_pe>(ar), sge_dstring_get_string(&pe_range_string));
            view.report_granted_parallel_environment_finish(out_);
            sge_dstring_free(&pe_range_string);
         }
         if (ocs::tom::get<AR_allocation_rule>(ar) != nullptr) {
            view.report_ar_node_string(out_, "allocation_rule", ocs::tom::get<AR_allocation_rule>(ar));
         }
         if (static_cast<const lList *>(ocs::tom::get<AR_master_queue_list>(ar)) != nullptr) {
            char tmp_buffer[MAX_STRING_SIZE];
            int fields[] = {QR_name, 0 };
            const char *delis[] = {" ", ",", ""};
            uni_print_list(nullptr, tmp_buffer, sizeof(tmp_buffer), static_cast<const lList *>(ocs::tom::get<AR_master_queue_list>(ar)),
                           fields, delis, FLG_NO_DELIS_STRINGS);
            view.report_ar_node_string(out_, "master hard queue_list", tmp_buffer);
         }

         if (ocs::tom::get<AR_checkpoint_name>(ar) != nullptr) {
            view.report_ar_node_string(out_, "checkpoint_name", ocs::tom::get<AR_checkpoint_name>(ar));
         }

         if (ocs::tom::get<AR_mail_options>(ar) != 0) {
            dstring mailopt = DSTRING_INIT;

            sge_dstring_append_mailopt(&mailopt, ocs::tom::get<AR_mail_options>(ar));
            view.report_ar_node_string(out_, "mail_options", sge_dstring_get_string(&mailopt));

            sge_dstring_free(&mailopt);
         }

         if (static_cast<const lList *>(ocs::tom::get<AR_mail_list>(ar)) != nullptr) {
            view.report_mail_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::MR> mail : ocs::tom::get<AR_mail_list>(ar)) {
               const char *host=nullptr;
               host=ocs::tom::get<MR_host>(mail);
               view.report_mail_list_node(out_, ocs::tom::get<MR_user>(mail), host?host:"NONE");
            }
            view.report_mail_list_finish(out_);
         }
         if (static_cast<const lList *>(ocs::tom::get<AR_acl_list>(ar)) != nullptr) {
            view.report_acl_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::ARA> acl : ocs::tom::get<AR_acl_list>(ar)) {
               view.report_acl_list_node(out_,ocs::tom::get<ARA_name>(acl));
            }
            view.report_acl_list_finish(out_);
         }
         if (static_cast<const lList *>(ocs::tom::get<AR_xacl_list>(ar)) != nullptr) {
            view.report_xacl_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::ARA> xacl : ocs::tom::get<AR_xacl_list>(ar)) {
                view.report_xacl_list_node(out_, ocs::tom::get<ARA_name>(xacl));
            }
            view.report_xacl_list_finish(out_);
         }
      }
      view.report_ar_finish(out_);
   }

   view.report_finish(out_);
   DRETURN_VOID;
}
