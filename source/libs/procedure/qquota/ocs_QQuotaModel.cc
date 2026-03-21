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

#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"

#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_str.h"

#include "gdi/ocs_gdi_Request.h"

#include "ocs_QQuotaModel.h"
#include "ocs_QQuotaParameter.h"

void ocs::QQuotaModel::free_data() {
   DENTER(TOP_LAYER);
   lFreeList(&rqs_list);
   lFreeList(&centry_list);
   lFreeList(&userset_list);
   lFreeList(&hgroup_list);
   lFreeList(&exechost_list);
   DRETURN_VOID;
}

bool ocs::QQuotaModel::fetch_data(lList **answer_list, const QQuotaParameter &parameter) {
   DENTER(TOP_LAYER);

   gdi::Request gdi_multi{};

   // RQS
   lEnumeration *rqs_what = lWhat("%T(ALL)", RQS_Type);
   const int rqs_id = gdi_multi.request(answer_list, Mode::RECORD, gdi::Target::TargetValue::SGE_RQS_LIST,
                                        gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE,
                                        nullptr, nullptr, rqs_what, true);
   lFreeWhat(&rqs_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   // CEntry
   lEnumeration *ce_what = lWhat("%T(ALL)", CE_Type);
   const int ce_id = gdi_multi.request(answer_list, Mode::RECORD, gdi::Target::TargetValue::SGE_CE_LIST,
                                       gdi::Command::SGE_GDI_GET, ocs::gdi::SubCommand::SGE_GDI_SUB_NONE,
                                       nullptr, nullptr, ce_what, true);
   lFreeWhat(&ce_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   // User Set
   lEnumeration *userset_what = lWhat("%T(ALL)", US_Type);
   const int userset_id = gdi_multi.request(answer_list, Mode::RECORD, gdi::Target::SGE_US_LIST,
                                            gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE,
                                            nullptr, nullptr, userset_what, true);
   lFreeWhat(&userset_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   // Hostgroup
   lEnumeration *hgroup_what = lWhat("%T(ALL)", HGRP_Type);
   const int hgroup_id = gdi_multi.request(answer_list, Mode::RECORD, gdi::Target::SGE_HGRP_LIST,
                                           gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE,
                                           nullptr, nullptr, hgroup_what, true);
   lFreeWhat(&hgroup_what);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }


   // Hosts (fetch all specified + global host but not the template host)
   lCondition *where = nullptr;
   const lListElem *ep = nullptr;
   for_each_ep (ep, parameter.host_list) {
      lCondition *nw = lWhere("%T(%I h= %s)", EH_Type, EH_name, lGetString(ep, ST_name));
      if (!where)
         where = nw;
      else
         where = lOrWhere(where, nw);
   }
   if (where != nullptr) {
      lCondition *nw = lWhere("%T(%I == %s)", EH_Type, EH_name, SGE_GLOBAL_NAME);
      where = lOrWhere(where, nw);
   }
   lCondition *nw = lWhere("%T(%I != %s)", EH_Type, EH_name, SGE_TEMPLATE_NAME);
   if (where)
      where = lAndWhere(where, nw);
   else
      where = nw;

   lEnumeration *eh_what = lWhat("%T(%I %I %I %I)", EH_Type, EH_name, EH_load_list, EH_consumable_config_list, EH_resource_utilization);
   const int eh_id = gdi_multi.request(answer_list, Mode::SEND, gdi::Target::SGE_EH_LIST, gdi::Command::SGE_GDI_GET,
                                       gdi::SubCommand::SGE_GDI_SUB_NONE, nullptr, where, eh_what, true);
   gdi_multi.wait();
   lFreeWhat(&eh_what);
   lFreeWhere(&where);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /* --- resource quota sets */
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_RQS_LIST, rqs_id, &rqs_list);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   /* --- complex attribute */
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_CE_LIST, ce_id, &centry_list);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   /* --- usersets */
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_US_LIST, userset_id, &userset_list);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   /* --- hostgroups */
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_HGRP_LIST, hgroup_id, &hgroup_list);
   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }
   /* --- exec hosts*/
   lFreeList(answer_list);
   gdi_multi.get_response(answer_list, gdi::Command::SGE_GDI_GET, gdi::SubCommand::SGE_GDI_SUB_NONE, gdi::Target::SGE_EH_LIST, eh_id, &hgroup_list);

   if (answer_list_has_error(answer_list)) {
      DRETURN(false);
   }

   DRETURN(true);
}

bool ocs::QQuotaModel::make_snapshot(lList **answer_list, QQuotaParameter &parameter) {
   DENTER(TOP_LAYER);

   if (!fetch_data(answer_list, parameter)) {
      DRETURN(false);
   }

   DRETURN(true);
}
