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

#include "cull/cull_list.h"

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_str.h"

#include "gdi/ocs_gdi_Client.h"

#include "qrstat/ocs_QRStatModel.h"
#include "qrstat/ocs_QRStatParameter.h"

void ocs::QRStatModel::free_data() {
   DENTER(TOP_LAYER);
   lFreeWhat(&what_AR_Type);
   lFreeWhere(&where_AR_Type);
   lFreeList(&ar_list);
   DRETURN_VOID;
}

void ocs::QRStatModel::qrstat_filter_add_core_attributes(QRStatParameter& parameter) {
   lEnumeration *what = nullptr;
   constexpr int nm_AR_Type[] = {
      AR_id,
      AR_name,
      AR_owner,
      AR_start_time,
      AR_end_time,
      AR_duration,
      AR_state,
      AR_reserved_queues,
      AR_reserved_hosts,
      NoName
   };

   what = lIntVector2What(AR_Type, nm_AR_Type);
   lMergeWhat(&what_AR_Type, &what);
}

void ocs::QRStatModel::qrstat_filter_add_ar_attributes(QRStatParameter& parameter) {
   lEnumeration *what = nullptr;
   constexpr int nm_AR_Type[] = {
      AR_account,
      AR_owner,
      AR_group,
      AR_submission_time,
      AR_verify,
      AR_error_handling,
      AR_checkpoint_name,
      AR_resource_list,
      AR_resource_utilization,
      AR_queue_list,
      AR_granted_slots,
      AR_mail_options,
      AR_mail_list,
      AR_pe,
      AR_pe_range,
      AR_master_queue_list,
      AR_acl_list,
      AR_xacl_list,
      AR_type,
      AR_reserved_queues,
      AR_reserved_hosts,
      AR_binding,
      AR_granted_resources_list,
      NoName
   };

   what = lIntVector2What(AR_Type, nm_AR_Type);
   lMergeWhat(&what_AR_Type, &what);
}

void ocs::QRStatModel::qrstat_filter_add_xml_attributes(QRStatParameter& parameter) {
   lEnumeration *what = nullptr;
   constexpr int nm_AR_Type[] = {
      AR_account,
      AR_owner,
      AR_group,
      AR_submission_time,
      AR_verify,
      AR_error_handling,
      AR_checkpoint_name,
      AR_resource_list,
      AR_resource_utilization,
      AR_queue_list,
      AR_granted_slots,
      AR_mail_options,
      AR_mail_list,
      AR_pe,
      AR_pe_range,
      AR_acl_list,
      AR_xacl_list,
      AR_type,
      NoName
   };

   what = lIntVector2What(AR_Type, nm_AR_Type);
   lMergeWhat(&what_AR_Type, &what);
}

void ocs::QRStatModel::qrstat_filter_add_explain_attributes(QRStatParameter& parameter) {
   lEnumeration *what = nullptr;
   constexpr int nm_AR_Type[] = {
      AR_error_handling,
      NoName
   };

   what = lIntVector2What(AR_Type, nm_AR_Type);
   lMergeWhat(&what_AR_Type, &what);
}

void ocs::QRStatModel::qrstat_filter_add_ar_where(QRStatParameter& parameter) {
   lCondition *where = nullptr;
   const lListElem *elem = nullptr; /* ULNG_Type */

   DENTER(TOP_LAYER);
   for_each_ep(elem, parameter.ar_id_list) {
      lCondition *tmp_where = nullptr;
      u_long32 value = lGetUlong(elem, ULNG_value);

      tmp_where = lWhere("%T(%I == %u)", AR_Type, AR_id, value);
      if (tmp_where != nullptr) {
         if (where == nullptr) {
            where = tmp_where;
         } else {
            where = lOrWhere(where, tmp_where);
         }
      }
   }
   if (where != nullptr) {
      if (where_AR_Type == nullptr) {
         where_AR_Type = where;
      } else {
         where_AR_Type = lAndWhere(where_AR_Type, where);
      }
   }
   DRETURN_VOID;
}

void ocs::QRStatModel::qrstat_filter_add_u_where(QRStatParameter& parameter) {
   lCondition *where = nullptr;
   const lListElem *elem = nullptr; /* ST_Type */

   for_each_ep(elem, parameter.user_list) {
      lCondition *tmp_where = nullptr;
      const char *name = lGetString(elem, ST_name);

      tmp_where = lWhere("%T(%I p= %s)", AR_Type, AR_owner, name);
      if (tmp_where != nullptr) {
         if (where == nullptr) {
            where = tmp_where;
         } else {
            where = lOrWhere(where, tmp_where);
         }
      }
   }
   if (where != nullptr) {
      if (where_AR_Type == nullptr) {
         where_AR_Type = where;
      } else {
         where_AR_Type = lAndWhere(where_AR_Type, where);
      }
   }
}

bool ocs::QRStatModel::fetch_data(lList **answer_list, QRStatParameter& parameter) {
   DENTER(TOP_LAYER);

   qrstat_filter_add_core_attributes(parameter);
   if (parameter.is_explain) {
      qrstat_filter_add_explain_attributes(parameter);
   }
   if (parameter.is_xml) {
      qrstat_filter_add_xml_attributes(parameter);
   }
   if (!parameter.is_summary) {
      qrstat_filter_add_ar_attributes(parameter);
      qrstat_filter_add_ar_where(parameter);
   } else {
      str_list_transform_user_list(&parameter.user_list, answer_list, parameter.user);
      qrstat_filter_add_u_where(parameter);
   }

   *answer_list = gdi::Client::sge_gdi(gdi::Target::TargetValue::SGE_AR_LIST, gdi::Command::SGE_GDI_GET,
                                       gdi::SubCommand::SGE_GDI_SUB_NONE, &ar_list, where_AR_Type, what_AR_Type);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   lFreeList(answer_list);

   if (!parameter.is_summary && lGetNumberOfElem(ar_list) == 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_WARNING, "Requested AR does not exist");
      DRETURN(false);
   }

   DRETURN(true);
}

bool ocs::QRStatModel::make_snapshot(lList **answer_list, QRStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (!fetch_data(answer_list, parameter)) {
      DRETURN(false);
   }

   DRETURN(true);
}
