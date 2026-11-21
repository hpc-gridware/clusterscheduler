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
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_AR.h"
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
 * @return none
 */
void
ocs::QRStatController::process_request(QRStatParameter &parameter, QRStatModelBase &model, QRStatViewBase &view) {
   DENTER(TOP_LAYER);

   view.report_start(out_);
   for (const auto ar : model.get_ar_list()) {

      view.report_ar_start(out_);
      view.report_ar_node_ulong(out_, "id", ar.get<AR_id>());
      view.report_ar_node_string(out_, "name", ar.get<AR_name>());
      view.report_ar_node_string(out_, "owner", ar.get<AR_owner>());
      view.report_ar_node_state(out_, "state", ar.get<AR_state>());
      view.report_ar_node_time(out_, "start_time", ar.get<AR_start_time>());
      view.report_ar_node_time(out_, "end_time", ar.get<AR_end_time>());
      view.report_ar_node_duration(out_, "duration", ar.get<AR_duration>());

      if (parameter.is_explain() || !parameter.is_summary()) {
         for (ocs::tom::ConstElement<ocs::tom::QU> qinstance : ar.get<AR_reserved_queues>()) {
            for (ocs::tom::ConstElement<ocs::tom::QIM> qim : qinstance.get<QU_message_list>()) {
               const char *message = qim.get<QIM_message>();
               view.report_ar_node_string(out_, "message", message);
            }
         }
      }
      if (!parameter.is_summary()) {
         view.report_ar_node_time(out_, "submission_time", ar.get<AR_submission_time>());
         view.report_ar_node_string(out_, "group", ar.get<AR_group>());
         view.report_ar_node_string(out_, "account", ar.get<AR_account>());
         if (const char *project = ar.get<AR_project>(); project != nullptr) {
            view.report_ar_node_string(out_, "project", project);
         }

         if (const auto binding = ar.get<AR_binding>(); binding) {
            std::string binding_string;
            BindingIo::binding_print_to_string(binding, binding_string, false);
            view.report_ar_node_string(out_, "binding", binding_string.c_str());
         }

         if (ar.get<AR_resource_list>() != nullptr) {
            view.report_resource_list_start(out_);

            for (ocs::tom::ConstElement<ocs::tom::CE> resource : ar.get<AR_resource_list>()) {
               // get the dominant value of the resource for this host
               const auto type = static_cast<CEntry::Type>(resource.get<CE_valtype>());
               const bool as_string = type == CEntry::Type::STR || type == CEntry::Type::CSTR ||
                                      type == CEntry::Type::HOST || type == CEntry::Type::RESTR;
               const bool as_double = type == CEntry::Type::DOUBLE;
               const bool as_bool = type == CEntry::Type::BOOL;
               if (as_string) {
                  view.report_resource_list_node_str(out_, resource.get<CE_name>(),
                                                     resource.get<CE_stringval>());
               } else if (as_double) {
                  view.report_resource_list_node_double(out_, resource.get<CE_name>(),
                                                        resource.get<CE_doubleval>());
               } else if (as_bool) {
                  view.report_resource_list_node_bool(out_, resource.get<CE_name>(),
                                                      resource.get<CE_doubleval>() != 0);
               } else {
                  view.report_resource_list_node_uint64(out_, resource.get<CE_name>(),
                                                        static_cast<uint64_t>(resource.get<CE_doubleval>()));
               }
            }
            view.report_resource_list_finish(out_);
         }

         if (ar.get<AR_error_handling>() != 0) {
            view.report_ar_node_boolean(out_, "error_handling", true);
         }

         if (ar.get<AR_granted_resources_list>() != nullptr) {
            view.report_exec_binding_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::GRU> resource : ar.get<AR_granted_resources_list>()) {
               const char *hostname = resource.get<GRU_host>();
               for (ocs::tom::ConstElement<ocs::tom::ST> binding_str : resource.get<GRU_binding_inuse>()) {
                  TopologyString binding;

                  const char *binding_touse = binding_str.get<ST_name>();
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
         if (ar.get<AR_granted_resources_list>() != nullptr) {
            bool any = false;

            for (ocs::tom::ConstElement<ocs::tom::GRU> resource : ar.get<AR_granted_resources_list>()) {
               if (static_cast<GrantedResources::Type>(resource.get<GRU_type>()) !=
                   GrantedResources::Type::GRU_RESOURCE_MAP_TYPE) {
                  continue;
               }
               if (!any) {
                  view.report_granted_resource_list_start(out_);
                  any = true;
               }

               std::string text = std::string(resource.get<GRU_name>()) + "=" +
                                  std::to_string(static_cast<uint32_t>(resource.get<GRU_amount>()));
               ocs::tom::ConstList<ocs::tom::RESL> ids = resource.get<GRU_resource_map_list>();
               if (ids != nullptr && !ids.empty()) {
                  const char *sep = "(";
                  for (ocs::tom::ConstElement<ocs::tom::RESL> resl : ids) {
                     // the reader folds a repeated id into one element with a count, so
                     // write it out the way the configuration does
                     for (uint32_t i = resl.get<RESL_amount>(); i > 0; i--) {
                        text += sep;
                        text += resl.get<RESL_value>();
                        sep = " ";
                     }
                  }
                  text += ")";
               }
               view.report_granted_resource_list_node(out_, resource.get<GRU_host>(), text.c_str());
            }
            if (any) {
               view.report_granted_resource_list_finish(out_);
            }
         }

         if (ar.get<AR_granted_slots>() != nullptr) {
            view.report_exec_queue_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::JG> resource : ar.get<AR_granted_slots>()) {
               view.report_exec_queue_list_node(out_, resource.get<JG_qname>(),
                                                resource.get<JG_slots>());
            }
            view.report_exec_queue_list_finish(out_);
         }
         if (ar.get<AR_pe>() != nullptr) {
            dstring pe_range_string = DSTRING_INIT;

            range_list_print_to_string(ar.get<AR_pe_range>(), &pe_range_string, true, false, false);
            view.report_granted_parallel_environment_start(out_);
            view.report_granted_parallel_environment_node(out_, ar.get<AR_pe>(),
                                                          sge_dstring_get_string(&pe_range_string));
            view.report_granted_parallel_environment_finish(out_);
            sge_dstring_free(&pe_range_string);
         }
         if (ar.get<AR_allocation_rule>() != nullptr) {
            view.report_ar_node_string(out_, "allocation_rule", ar.get<AR_allocation_rule>());
         }
         if (ar.get<AR_master_queue_list>() != nullptr) {
            char tmp_buffer[MAX_STRING_SIZE];
            int fields[] = {QR_name, 0 };
            const char *delis[] = {" ", ",", ""};
            uni_print_list(nullptr, tmp_buffer, sizeof(tmp_buffer),
                           ar.get<AR_master_queue_list>(), fields, delis,
                           FLG_NO_DELIS_STRINGS);
            view.report_ar_node_string(out_, "master hard queue_list", tmp_buffer);
         }

         if (ar.get<AR_checkpoint_name>() != nullptr) {
            view.report_ar_node_string(out_, "checkpoint_name", ar.get<AR_checkpoint_name>());
         }

         if (ar.get<AR_mail_options>() != 0) {
            dstring mailopt = DSTRING_INIT;

            sge_dstring_append_mailopt(&mailopt, ar.get<AR_mail_options>());
            view.report_ar_node_string(out_, "mail_options", sge_dstring_get_string(&mailopt));

            sge_dstring_free(&mailopt);
         }

         if (ar.get<AR_mail_list>() != nullptr) {
            view.report_mail_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::MR> mail : ar.get<AR_mail_list>()) {
               const char *host=nullptr;
               host=mail.get<MR_host>();
               view.report_mail_list_node(out_, mail.get<MR_user>(), host?host:"NONE");
            }
            view.report_mail_list_finish(out_);
         }
         if (ar.get<AR_acl_list>() != nullptr) {
            view.report_acl_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::ARA> acl : ar.get<AR_acl_list>()) {
               view.report_acl_list_node(out_,acl.get<ARA_name>());
            }
            view.report_acl_list_finish(out_);
         }
         if (ar.get<AR_xacl_list>() != nullptr) {
            view.report_xacl_list_start(out_);
            for (ocs::tom::ConstElement<ocs::tom::ARA> xacl : ar.get<AR_xacl_list>()) {
                view.report_xacl_list_node(out_, xacl.get<ARA_name>());
            }
            view.report_xacl_list_finish(out_);
         }
      }
      view.report_ar_finish(out_);
   }

   view.report_finish(out_);
   DRETURN_VOID;
}
