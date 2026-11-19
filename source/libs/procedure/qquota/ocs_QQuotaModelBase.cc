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
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "ocs_QQuotaModelBase.h"
#include "ocs_QQuotaParameterClient.h"
#include "sgeobj/sge_answer.h"

/** @brief Fetch raw CULL lists into the member variables.
 *
 * Overridden by QQuotaModelClient (GDI calls) and QQuotaModelServer (master lists).
 *
 * @param answer_list receives error messages
 * @param host_list the hosts the user asked for; all hosts when empty
 * @return true when everything could be fetched
 */
bool ocs::QQuotaModelBase::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                      const ocs::tom::ConstList<ocs::tom::ST> host_list) {
   DENTER(TOP_LAYER);
   // Nothing to do here. Implemented in child classes.
   DRETURN(true);
}

/** @brief Run the full pipeline: fetch_data, then hand off to the view.
 *
 * @param answer_list  Receives error messages on failure.
 * @param parameter    Parsed qquota parameters.
 * @return true if fetch_data succeeded.
 */
bool ocs::QQuotaModelBase::make_snapshot(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QQuotaParameter &parameter) {
   DENTER(TOP_LAYER);

   if (!fetch_data(answer_list, parameter.get_host_list())) {
      DRETURN(false);
   }

   DRETURN(true);
}

/** @brief The resource quota set fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::RQS> ocs::QQuotaModelBase::get_rqs_what() {
   return ocs::tom::what_all<ocs::tom::RQS>();
}

/** @brief The complex entry fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::CE> ocs::QQuotaModelBase::get_centry_what() {
   return ocs::tom::what_all<ocs::tom::CE>();
}

/** @brief The user set fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::US> ocs::QQuotaModelBase::get_user_set_what() {
   return ocs::tom::what_all<ocs::tom::US>();
}

/** @brief The host group fields the model needs
 * @return the selection of every field
 */
ocs::tom::What<ocs::tom::HGRP> ocs::QQuotaModelBase::get_hgroup_what() {
   return ocs::tom::what_all<ocs::tom::HGRP>();
}

/** @brief The execution host fields the model needs
 * @return the selection
 */
ocs::tom::What<ocs::tom::EH> ocs::QQuotaModelBase::get_host_what() {
   return ocs::tom::what<EH_name, EH_load_list, EH_consumable_config_list, EH_resource_utilization>();
}

/** @brief Build the condition selecting the hosts to fetch
 * @param host_list the hosts the user asked for; all hosts when empty
 * @return the condition: the hosts named and the global host, never the template host
 */
ocs::tom::Where<ocs::tom::EH> ocs::QQuotaModelBase::get_host_where(const ocs::tom::ConstList<ocs::tom::ST> host_list) {
   DENTER(TOP_LAYER);
   ocs::tom::Where<ocs::tom::EH> where;

   for (const auto ep : host_list) {
      where |= ocs::tom::field<EH_name>().same_host(ocs::tom::get<ST_name>(ep));
   }
   if (where) {
      where |= ocs::tom::field<EH_name>() == SGE_GLOBAL_NAME;
   }
   where &= ocs::tom::field<EH_name>() != SGE_TEMPLATE_NAME;
   DRETURN(where);
}
