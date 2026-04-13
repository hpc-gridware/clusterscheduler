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

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_str.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/ocs_GrantedResources.h"
#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_qref.h"

#include "qrstat/ocs_QRStatController.h"

#include <iostream>

#include "uti/sge_string.h"

void
ocs::QRStatController::process_request(QRStatParameter &parameter, QRStatModelBase &model, QRStatViewBase &view) {
   DENTER(TOP_LAYER);

   view.report_start(out_);
   for_each_ep_lv(ar, model.get_ar_list()) {

      view.report_ar_start(out_);
      view.report_ar_node_ulong(out_, "id", lGetUlong(ar, AR_id));
      view.report_ar_node_string(out_, "name", lGetString(ar, AR_name));
      view.report_ar_node_string(out_, "owner", lGetString(ar, AR_owner));
      view.report_ar_node_state(out_, "state", lGetUlong(ar, AR_state));
      view.report_ar_node_time(out_, "start_time", lGetUlong64(ar, AR_start_time));
      view.report_ar_node_time(out_, "end_time", lGetUlong64(ar, AR_end_time));
      view.report_ar_node_duration(out_, "duration", lGetUlong64(ar, AR_duration));

      if (parameter.is_explain() || !parameter.is_summary()) {
         for_each_ep_lv(qinstance, lGetList(ar, AR_reserved_queues)) {
            for_each_ep_lv(qim, lGetList(qinstance, QU_message_list)) {
               const char *message = lGetString(qim, QIM_message);
               view.report_ar_node_string(out_, "message", message);
            }
         }
      }
      if (!parameter.is_summary()) {
         view.report_ar_node_time(out_, "submission_time", lGetUlong64(ar, AR_submission_time));
         view.report_ar_node_string(out_, "group", lGetString(ar, AR_group));
         view.report_ar_node_string(out_, "account", lGetString(ar, AR_account));

         if (const lListElem *binding = lGetObject(ar, AR_binding); binding != nullptr) {
            std::string binding_string;
            BindingIo::binding_print_to_string(binding, binding_string, false);
            view.report_ar_node_string(out_, "binding", binding_string.c_str());
         }

         if (lGetList(ar, AR_resource_list) != nullptr) {
            view.report_resource_list_start(out_);

            for_each_ep_lv(resource, lGetList(ar, AR_resource_list)) {
               // get the dominant value of the resource for this host
               const auto type = static_cast<CEntry::Type>(lGetUlong(resource, CE_valtype));
               const bool as_string = type == CEntry::Type::STR || type == CEntry::Type::CSTR || type == CEntry::Type::HOST || type == CEntry::Type::RESTR || type == CEntry::Type::HOST;
               const bool as_double = type == CEntry::Type::DOUBLE;
               const bool as_bool = type == CEntry::Type::BOOL;
               if (as_string) {
                  view.report_resource_list_node_str(out_, lGetString(resource, CE_name), lGetString(resource, CE_stringval));
               } else if (as_double) {
                  view.report_resource_list_node_double(out_, lGetString(resource, CE_name), lGetDouble(resource, CE_doubleval));
               } else if (as_bool) {
                  view.report_resource_list_node_bool(out_, lGetString(resource, CE_name), lGetDouble(resource, CE_doubleval) != 0);
               } else {
                  view.report_resource_list_node_uint64(out_, lGetString(resource, CE_name), static_cast<uint64_t>(lGetDouble(resource, CE_doubleval)));
               }
            }
            view.report_resource_list_finish(out_);
         }

         if (lGetUlong(ar, AR_error_handling) != 0) {
            view.report_ar_node_boolean(out_, "error_handling", true);
         }

         if (lGetList(ar, AR_granted_resources_list) != nullptr) {
            view.report_exec_binding_list_start(out_);
            for_each_ep_lv(resource, lGetList(ar, AR_granted_resources_list)) {
               const char *hostname = lGetHost(resource, GRU_host);
               for_each_ep_lv(binding_str, lGetList(resource, GRU_binding_inuse)) {
                  TopologyString binding;

                  const char *binding_touse = lGetString(binding_str, ST_name);
                  if (binding_touse != nullptr) {
                     binding.reset_topology(binding_touse);
                  }

                  view.report_exec_binding_list_node(out_, hostname, binding.to_product_topology_string().c_str());
               }
            }
            view.report_exec_binding_list_finish(out_);
         }

         if (lGetList(ar, AR_granted_slots) != nullptr) {
            view.report_exec_queue_list_start(out_);
            for_each_ep_lv(resource, lGetList(ar, AR_granted_slots)) {
               view.report_exec_queue_list_node(out_,lGetString(resource, JG_qname), lGetUlong(resource, JG_slots));
            }
            view.report_exec_queue_list_finish(out_);
         }
         if (lGetString(ar, AR_pe) != nullptr) {
            dstring pe_range_string = DSTRING_INIT;

            range_list_print_to_string(lGetList(ar, AR_pe_range), &pe_range_string, true, false, false);
            view.report_granted_parallel_environment_start(out_);
            view.report_granted_parallel_environment_node(out_, lGetString(ar, AR_pe), sge_dstring_get_string(&pe_range_string));
            view.report_granted_parallel_environment_finish(out_);
            sge_dstring_free(&pe_range_string);
         }
         if (lGetList(ar, AR_master_queue_list) != nullptr) {
            char tmp_buffer[MAX_STRING_SIZE];
            int fields[] = {QR_name, 0 };
            const char *delis[] = {" ", ",", ""};
            uni_print_list(nullptr, tmp_buffer, sizeof(tmp_buffer), lGetList(ar, AR_master_queue_list),
                           fields, delis, FLG_NO_DELIS_STRINGS);
            view.report_ar_node_string(out_, "master hard queue_list", tmp_buffer);
         }

         if (lGetString(ar, AR_checkpoint_name) != nullptr) {
            view.report_ar_node_string(out_, "checkpoint_name", lGetString(ar, AR_checkpoint_name));
         }

         if (lGetUlong(ar, AR_mail_options) != 0) {
            dstring mailopt = DSTRING_INIT;

            sge_dstring_append_mailopt(&mailopt, lGetUlong(ar, AR_mail_options));
            view.report_ar_node_string(out_, "mail_options", sge_dstring_get_string(&mailopt));

            sge_dstring_free(&mailopt);
         }

         if (lGetList(ar, AR_mail_list) != nullptr) {
            view.report_mail_list_start(out_);
            for_each_ep_lv(mail, lGetList(ar, AR_mail_list)) {
               const char *host=nullptr;
               host=lGetHost(mail, MR_host);
               view.report_mail_list_node(out_, lGetString(mail, MR_user), host?host:"NONE");
            }
            view.report_mail_list_finish(out_);
         }
         if (lGetList(ar, AR_acl_list) != nullptr) {
            view.report_acl_list_start(out_);
            for_each_ep_lv(acl, lGetList(ar, AR_acl_list)) {
               view.report_acl_list_node(out_,lGetString(acl, ARA_name));
            }
            view.report_acl_list_finish(out_);
         }
         if (lGetList(ar, AR_xacl_list) != nullptr) {
            view.report_xacl_list_start(out_);
            for_each_ep_lv(xacl, lGetList(ar, AR_xacl_list)) {
                view.report_xacl_list_node(out_, lGetString(xacl, ARA_name));
            }
            view.report_xacl_list_finish(out_);
         }
      }
      view.report_ar_finish(out_);
   }

   view.report_finish(out_);
   DRETURN_VOID;
}
