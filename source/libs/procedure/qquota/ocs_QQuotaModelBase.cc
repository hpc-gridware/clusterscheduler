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
 * @brief Base model of `qquota`: the lists the report is built from
 */

#include "uti/sge_rmon_macros.h"
#include "uti/sge.h"

#include "sgeobj/sge_resource_quota.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_hgroup.h"
#include "sgeobj/sge_host.h"
#include "sgeobj/sge_str.h"

#include "ocs_QQuotaModelBase.h"
#include "ocs_QQuotaParameterClient.h"

ocs::QQuotaModelBase::~QQuotaModelBase() {
   DENTER(TOP_LAYER);
   lFreeList(&rqs_list_);
   lFreeList(&centry_list_);
   lFreeList(&user_set_list_);
   lFreeList(&hgroup_list_);
   lFreeList(&exec_host_list_);
   DRETURN_VOID;
}

/** @brief Fetch raw CULL lists into the member variables.
 *
 * Overridden by QQuotaModelClient (GDI calls) and QQuotaModelServer (master lists).
 *
 * @param answer_list receives error messages
 * @param host_list the hosts the user asked for; all hosts when empty
 * @return true when everything could be fetched
 */
bool ocs::QQuotaModelBase::fetch_data(lList **answer_list, const lList *host_list) {
   // Nothing to do here. Implemented in child classes.
   return true;
}

/** @brief Run the full pipeline: fetch_data, then hand off to the view.
 *
 * @param answer_list  Receives error messages on failure.
 * @param parameter    Parsed qquota parameters.
 * @return true if fetch_data succeeded.
 */
bool ocs::QQuotaModelBase::make_snapshot(lList **answer_list, QQuotaParameter &parameter) {
   DENTER(TOP_LAYER);

   if (!fetch_data(answer_list, parameter.get_host_list())) {
      DRETURN(false);
   }

   DRETURN(true);
}

/** @brief The resource quota set fields the model needs
 * @return the enumeration, which the caller owns
 */
lEnumeration *ocs::QQuotaModelBase::get_rqs_what() {
   return lWhat("%T(ALL)", RQS_Type);
}

/** @brief The complex entry fields the model needs
 * @return the enumeration, which the caller owns
 */
lEnumeration *ocs::QQuotaModelBase::get_centry_what() {
   return lWhat("%T(ALL)", CE_Type);
}

/** @brief The user set fields the model needs
 * @return the enumeration, which the caller owns
 */
lEnumeration *ocs::QQuotaModelBase::get_user_set_what() {
   return lWhat("%T(ALL)", US_Type);
}

/** @brief The host group fields the model needs
 * @return the enumeration, which the caller owns
 */
lEnumeration *ocs::QQuotaModelBase::get_hgroup_what() {
   return lWhat("%T(ALL)", HGRP_Type);
}

/** @brief The execution host fields the model needs
 * @return the enumeration, which the caller owns
 */
lEnumeration *ocs::QQuotaModelBase::get_host_what() {
   return lWhat("%T(%I %I %I %I)", EH_Type, EH_name, EH_load_list, EH_consumable_config_list, EH_resource_utilization);
}

/** @brief Build the CULL condition selecting the hosts to fetch
 * @param host_list the hosts the user asked for; all hosts when empty
 * @return the condition, which the caller owns
 */
lCondition *ocs::QQuotaModelBase::get_host_where(const lList *host_list) {
   lCondition *where = nullptr;

   for_each_ep_lv(ep, host_list) {
      lCondition *nw = lWhere("%T(%I h= %s)", EH_Type, EH_name, lGetString(ep, ST_name));
      if (!where) {
         where = nw;
      } else {
         where = lOrWhere(where, nw);
      }
   }
   if (where != nullptr) {
      const lCondition *nw = lWhere("%T(%I == %s)", EH_Type, EH_name, SGE_GLOBAL_NAME);
      where = lOrWhere(where, nw);
   }
   lCondition *nw = lWhere("%T(%I != %s)", EH_Type, EH_name, SGE_TEMPLATE_NAME);
   if (where) {
      where = lAndWhere(where, nw);
   } else {
      where = nw;
   }
   return where;
}
