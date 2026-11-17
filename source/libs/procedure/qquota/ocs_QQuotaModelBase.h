#pragma once
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

#include "cull/cull.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_RQS.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "ocs_QQuotaParameter.h"

namespace ocs {
   /** @brief Base model for qquota that holds the CULL lists needed for resource quota rendering.
    *
    * Fetches resource quota sets (RQS), complex entries, user sets, host groups,
    * and execution hosts, then passes them to the view for rendering.
    *
    * ## Client vs. server execution
    *
    * - **QQuotaModelClient** (`ExecContext::CLIENT`): issues GDI requests to qmaster
    *   to fetch all required lists.
    * - **QQuotaModelServer** (`ExecContext::SERVER`): reads directly from the in-process
    *   qmaster master lists, avoiding any network round-trip.
    *
    * ## Virtual pipeline inside make_snapshot()
    *
    * 1. `fetch_data()` — populates the CULL list members (overridden per context).
    *
    * @ingroup libprocedure
    */
   class QQuotaModelBase {
#pragma region Data
   protected:
      ocs::tom::ListPtr<ocs::tom::CE> centry_list_;     ///< Complex entries, for the type of each limited resource
      ocs::tom::ListPtr<ocs::tom::US> user_set_list_;   ///< User sets, to resolve the `@group` filters of a rule
      ocs::tom::ListPtr<ocs::tom::HGRP> hgroup_list_;   ///< Host groups, to resolve the `@group` filters of a rule
      ocs::tom::ListPtr<ocs::tom::EH> exec_host_list_;  ///< Execution hosts, reduced to the fields of get_host_what()
      ocs::tom::ListPtr<ocs::tom::RQS> rqs_list_;       ///< The resource quota sets being reported
      /// Job load adjustments of the scheduler configuration, never an empty handle after a successful fetch_data()
      ocs::tom::ListPtr<ocs::tom::CE> job_load_adjustments_;

   public:
      /** @brief The resource quota sets being reported
       *
       * Not a const handle: the controller names the rules that have no name while it reports them.
       *
       * @return the RQS list
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::RQS> get_rqs_list() const { return *rqs_list_; }

      /** @brief The complex entries
       * @return the centry list
       */
      [[nodiscard]] ocs::tom::ConstList<ocs::tom::CE> get_centry_list() const { return *centry_list_; }

      /** @brief The user sets
       * @return the user set list
       */
      [[nodiscard]] ocs::tom::ConstList<ocs::tom::US> get_user_set_list() const { return *user_set_list_; }

      /** @brief The host groups
       * @return the hgroup list
       */
      [[nodiscard]] ocs::tom::ConstList<ocs::tom::HGRP> get_hgroup_list() const { return *hgroup_list_; }

      /** @brief The execution hosts
       * @return the exec host list
       */
      [[nodiscard]] ocs::tom::ConstList<ocs::tom::EH> get_exec_host_list() const { return *exec_host_list_; }

      /** @brief The job load adjustments of the scheduler configuration
       *
       * A copy taken with the snapshot, so dynamic limits can be resolved after the data store lock was
       * released (CS-3008). An empty list rather than an empty handle when none are configured, because an
       * empty handle makes the sched helpers fetch them from the data store.
       *
       * @return the load adjustments
       */
      [[nodiscard]] ocs::tom::ConstList<ocs::tom::CE> get_job_load_adjustments() const {
         return *job_load_adjustments_;
      }
#pragma endregion

#pragma region Data Retrieval
   protected:
      static lEnumeration *get_rqs_what();

      static lEnumeration *get_centry_what();

      static lEnumeration *get_user_set_what();

      static lEnumeration *get_hgroup_what();

      static lEnumeration *get_host_what();

      static lCondition *get_host_where(ocs::tom::ConstList<ocs::tom::ST> host_list);

      virtual bool fetch_data(lList **answer_list, ocs::tom::ConstList<ocs::tom::ST> host_list);
   public:
      virtual bool make_snapshot(lList **answer_list, QQuotaParameter &parameter);
#pragma endregion

#pragma region Constructors/Destructors
   public:
      QQuotaModelBase() = default;
      virtual ~QQuotaModelBase() = default;
#pragma endregion

   };
}
