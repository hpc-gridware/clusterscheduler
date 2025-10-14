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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>
#include <string>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_BindingIo.h"
#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/ocs_GrantedResources.h"
#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_range.h"
#include "sgeobj/sge_mailrec.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qref.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/cull_parse_util.h"
#include "sgeobj/sge_str.h"


#include "basis_types.h"
#include "ocs_qrstat_report_handler.h"

bool
qrstat_print(lList **answer_list, qrstat_report_handler_t *handler, qrstat_env_t *qrstat_env) {
   bool ret = true;

   DENTER(TOP_LAYER);

   {
      const lListElem *ar = nullptr;

      handler->report_start(handler, answer_list);
      if (!qrstat_env->is_summary &&
          lGetNumberOfElem(qrstat_env->ar_list) == 0) {

          ret = false;

          for_each_ep(ar, qrstat_env->ar_id_list) {
            handler->report_start_unknown_ar(handler, qrstat_env, answer_list);
            handler->report_ar_node_ulong_unknown(handler, qrstat_env, answer_list, "id", lGetUlong(ar, ULNG_value));
            handler->report_finish_unknown_ar(handler, answer_list);
          }
          handler->report_newline(handler, answer_list);
      } else {
         for_each_ep(ar, qrstat_env->ar_list) {

            handler->report_start_ar(handler, qrstat_env, answer_list);
            handler->report_ar_node_ulong(handler, qrstat_env, answer_list, "id", lGetUlong(ar, AR_id));
            handler->report_ar_node_string(handler, answer_list, "name", lGetString(ar, AR_name));
            handler->report_ar_node_string(handler, answer_list, "owner", lGetString(ar, AR_owner));
            handler->report_ar_node_state(handler, answer_list, "state", lGetUlong(ar, AR_state));         
            handler->report_ar_node_time(handler, answer_list, "start_time", lGetUlong64(ar, AR_start_time));
            handler->report_ar_node_time(handler, answer_list, "end_time", lGetUlong64(ar, AR_end_time));
            handler->report_ar_node_duration(handler, answer_list, "duration", lGetUlong64(ar, AR_duration));

            if (qrstat_env->is_explain || !handler->show_summary) {
               const lListElem *qinstance;
               for_each_ep(qinstance, lGetList(ar, AR_reserved_queues)) {
                  const lListElem *qim = nullptr;
                  for_each_ep(qim, lGetList(qinstance, QU_message_list)) {
                     const char *message = lGetString(qim, QIM_message);
                     handler->report_ar_node_string(handler, answer_list, "message", message);
                  }
               }
            }
            if (!handler->show_summary) {
               handler->report_ar_node_time(handler, answer_list, "submission_time", lGetUlong64(ar, AR_submission_time));
               handler->report_ar_node_string(handler, answer_list, "group", lGetString(ar, AR_group));
               handler->report_ar_node_string(handler, answer_list, "account", lGetString(ar, AR_account));

               if (const lListElem *binding = lGetObject(ar, AR_binding); binding != nullptr) {
                  std::string binding_string;
                  ocs::BindingIo::binding_print_to_string(binding, binding_string, false);
                  handler->report_ar_node_string(handler, answer_list, "binding", binding_string.c_str());
               }

               if (lGetList(ar, AR_resource_list) != nullptr) {
                  const lListElem *resource = nullptr;

                  handler->report_start_resource_list(handler, answer_list);            
                  for_each_ep(resource, lGetList(ar, AR_resource_list)) {
                     dstring string_value = DSTRING_INIT;

                     if (lGetString(resource, CE_stringval)) {
                        sge_dstring_append(&string_value, lGetString(resource, CE_stringval));
                     } else {
                        sge_dstring_sprintf(&string_value, "%f", lGetDouble(resource, CE_doubleval));
                     }

                     handler->report_resource_list_node(handler, answer_list,
                                                        lGetString(resource, CE_name),
                                                        sge_dstring_get_string(&string_value));
                     sge_dstring_free(&string_value);
                  }
                  handler->report_finish_resource_list(handler, answer_list);
               }
               
               if (lGetUlong(ar, AR_error_handling) != 0) {
                  handler->report_ar_node_boolean(handler, answer_list, "error_handling", true);
               }

               if (lGetList(ar, AR_granted_resources_list) != nullptr) {
                  handler->report_start_exec_binding_list(handler, answer_list);
                  const lListElem *resource = nullptr;

                  for_each_ep(resource, lGetList(ar, AR_granted_resources_list)) {
                     const char *hostname = lGetHost(resource, GRU_host);
                     const lListElem *binding_str = nullptr;
                     for_each_ep(binding_str, lGetList(resource, GRU_binding_inuse)) {
                        ocs::TopologyString binding;
                        const char *binding_touse = lGetString(binding_str, ST_name);
                        if (binding_touse != nullptr) {
                           binding.reset_topology(binding_touse);
                        }

                        handler->report_exec_binding_list_node(handler, answer_list, hostname, binding.to_product_topology_string().c_str());
                     }
                  }
                  handler->report_finish_exec_binding_list(handler, answer_list);
               }

               if (lGetList(ar, AR_granted_slots) != nullptr) {
                  const lListElem *resource = nullptr;

                  handler->report_start_exec_queue_list(handler, answer_list);
                  for_each_ep(resource, lGetList(ar, AR_granted_slots)) {
                     handler->report_exec_queue_list_node(handler, answer_list,
                                                         lGetString(resource, JG_qname),
                                                          lGetUlong(resource, JG_slots));
                  }
                  handler->report_finish_exec_queue_list(handler, answer_list);
               }
               if (lGetString(ar, AR_pe) != nullptr) {
                  dstring pe_range_string = DSTRING_INIT;

                  range_list_print_to_string(lGetList(ar, AR_pe_range), &pe_range_string, true, false, false);
                  handler->report_start_granted_parallel_environment(handler, answer_list);
                  handler->report_granted_parallel_environment_node(handler, answer_list,
                                                                    lGetString(ar, AR_pe),
                                                                    sge_dstring_get_string(&pe_range_string));
                  handler->report_finish_granted_parallel_environment(handler, answer_list);
                  sge_dstring_free(&pe_range_string);
               }
               if (lGetList(ar, AR_master_queue_list) != nullptr) {
                  char tmp_buffer[MAX_STRING_SIZE];
                  int fields[] = {QR_name, 0 };
                  const char *delis[] = {" ", ",", ""};
                  uni_print_list(nullptr, tmp_buffer, sizeof(tmp_buffer), lGetList(ar, AR_master_queue_list),
                                 fields, delis, FLG_NO_DELIS_STRINGS);
                  handler->report_ar_node_string(handler, answer_list, "master hard queue_list", tmp_buffer);
               }

               if (lGetString(ar, AR_checkpoint_name) != nullptr) {
                  handler->report_ar_node_string(handler, answer_list, "checkpoint_name",
                                                 lGetString(ar, AR_checkpoint_name));
               }

               if (lGetUlong(ar, AR_mail_options) != 0) {
                  dstring mailopt = DSTRING_INIT;

                  sge_dstring_append_mailopt(&mailopt, lGetUlong(ar, AR_mail_options));
                  handler->report_ar_node_string(handler, answer_list, "mail_options",
                                                 sge_dstring_get_string(&mailopt));

                  sge_dstring_free(&mailopt);
               }

               if (lGetList(ar, AR_mail_list) != nullptr) {
                  const lListElem *mail = nullptr;

                  handler->report_start_mail_list(handler, answer_list);
                  for_each_ep(mail, lGetList(ar, AR_mail_list)) {
                     const char *host=nullptr;
                     host=lGetHost(mail, MR_host);
                     handler->report_mail_list_node(handler, answer_list,
                                                    lGetString(mail, MR_user),
                                                    host?host:"NONE");
                  }
                  handler->report_finish_mail_list(handler, answer_list);
               }
               if (lGetList(ar, AR_acl_list) != nullptr) {
                  const lListElem *acl = nullptr;

                  handler->report_start_acl_list(handler, answer_list);
                  for_each_ep(acl, lGetList(ar, AR_acl_list)) {
                     handler->report_acl_list_node(handler, answer_list,
                                                    lGetString(acl, ARA_name));
                  }
                  handler->report_finish_acl_list(handler, answer_list);
               }
               if (lGetList(ar, AR_xacl_list) != nullptr) {
                  const lListElem *xacl = nullptr;

                  handler->report_start_xacl_list(handler, answer_list);
                  for_each_ep(xacl, lGetList(ar, AR_xacl_list)) {
                     handler->report_xacl_list_node(handler, answer_list,
                                                    lGetString(xacl, ARA_name));
                  }
                  handler->report_finish_xacl_list(handler, answer_list);
               }
            }
            handler->report_finish_ar(handler, answer_list);
         }
      }

      handler->report_finish(handler, answer_list);
   }
   DRETURN(ret);
}

