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
 * @brief Base model of `qrstat`: the lists the report is built from
 */

#include "tom/ocs_Types.h"

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_advance_reservation.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_str.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

#include "gdi/ocs_gdi_Client.h"

#include "qrstat/ocs_QRStatModelBase.h"
#include "qrstat/ocs_QRStatParameterClient.h"
#include "sgeobj/sge_answer.h"

/** @brief The advance reservation fields the report needs
 *
 * Which fields those are depends on the parameters - a summary needs far
 * fewer than a full listing.
 *
 * @param parameter the parsed parameters
 * @return the selection
 */
ocs::tom::What<ocs::tom::AR> ocs::QRStatModelBase::get_ar_what(QRStatParameter& parameter) {
   // Core attributes
   auto what = ocs::tom::what<AR_id, AR_name, AR_owner, AR_start_time, AR_end_time, AR_duration, AR_state,
                              AR_reserved_queues, AR_reserved_hosts>();

   // for XML or JSON output
   if (parameter.get_output_format() != QRStatParameterClient::OutputFormat::PLAIN) {
      what = std::move(what).merge(
         ocs::tom::what<AR_account, AR_project, AR_owner, AR_group, AR_submission_time, AR_verify,
                        AR_error_handling, AR_checkpoint_name, AR_resource_list, AR_resource_utilization,
                        AR_queue_list, AR_granted_slots, AR_mail_options, AR_mail_list, AR_pe, AR_pe_range,
                        AR_allocation_rule, AR_acl_list, AR_xacl_list, AR_type>());
   }

   // required for full AR output
   if (!parameter.is_summary()) {
      what = std::move(what).merge(
         ocs::tom::what<AR_account, AR_project, AR_owner, AR_group, AR_submission_time, AR_verify,
                        AR_error_handling, AR_checkpoint_name, AR_resource_list, AR_resource_utilization,
                        AR_queue_list, AR_granted_slots, AR_mail_options, AR_mail_list, AR_pe, AR_pe_range,
                        AR_allocation_rule, AR_master_queue_list, AR_acl_list, AR_xacl_list, AR_type,
                        AR_reserved_queues, AR_reserved_hosts, AR_binding, AR_granted_resources_list>());
   }

   // required for -explain
   if (parameter.is_explain()) {
      what = std::move(what).merge(ocs::tom::what<AR_error_handling>());
   }
   return what;
}

/** @brief Build the condition selecting the reservations to report
 * @param parameter the parsed parameters, holding the id and user filters
 * @return the condition; empty when nothing was filtered
 */
ocs::tom::Where<ocs::tom::AR> ocs::QRStatModelBase::get_ar_where(QRStatParameter& parameter) {
   DENTER(TOP_LAYER);
   ocs::tom::Where<ocs::tom::AR> where;

   // Summay or full view
   if (!parameter.is_summary()) {
      for (const auto elem : parameter.get_ar_id_list()) {
         where |= ocs::tom::field<AR_id>() == ocs::tom::get<ULNG_value>(elem);
      }
   } else {
      // filter users
      parameter.transform_user_list();

      for (const auto elem : parameter.get_user_list()) {
         where |= ocs::tom::field<AR_owner>().like(ocs::tom::get<ST_name>(elem));
      }
   }
   DRETURN(where);
}

/** @brief Fetch the AR list into ar_list_.
 *
 * Overridden by QRStatModelClient (GDI call) and QRStatModelServer (master list).
 *
 * @param answer_list receives error messages
 * @param parameter the parsed parameters
 * @return true when the list could be fetched
 */
bool ocs::QRStatModelBase::fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QRStatParameter& parameter) {
   DENTER(TOP_LAYER);
   // has to be overwritten by child classes
   DRETURN(true);
}

/** @brief Run the full pipeline: fetch_data, then hand off to the view.
 *
 * @param answer_list  Receives error messages on failure.
 * @param parameter    Parsed qrstat parameters.
 * @return true if fetch_data succeeded.
 */
bool ocs::QRStatModelBase::make_snapshot(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QRStatParameter &parameter) {
   DENTER(TOP_LAYER);

   if (!fetch_data(answer_list, parameter)) {
      DRETURN(false);
   }

   DRETURN(true);
}
