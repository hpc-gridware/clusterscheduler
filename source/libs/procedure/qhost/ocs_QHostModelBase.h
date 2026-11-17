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
 * @brief Base model of `qhost`: the lists the report is built from
 */

#include "cull/cull.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "ocs_QHostParameter.h"
#include "procedure/ocs_ProcedureModel.h"

namespace ocs {

   /** @brief Base model for qhost that holds all CULL data lists needed for rendering.
    *
    * Fetches execution host, queue, job, centry, PE, and ACL lists, then applies
    * resource-match filtering and sorts the result before handing it to the view.
    *
    * ## Client vs. server execution
    *
    * - **QHostModelClient** (`ExecContext::CLIENT`): issues GDI requests to qmaster
    *   to fetch all required lists and performs preparation locally.
    * - **QHostModelServer** (`ExecContext::SERVER`): reads directly from the in-process
    *   qmaster master lists, avoiding any network round-trip.
    *
    * ## Virtual pipeline inside make_snapshot()
    *
    * 1. `fetch_data()`   — populates the CULL list members (overridden per context).
    * 2. `prepare_data()` — post-processes fetched lists (e.g. resolves hostgroup references).
    * 3. `filter_data()`  — applies the resource-match filter from the parameters.
    * 4. `sort_data()`    — sorts the result list for deterministic view output.
    *
    * @ingroup libprocedure
    */
   class QHostModelBase {

#pragma region Data
   protected:
      bool is_manager_ = false;                         ///< True if the executing user has manager privileges
      ocs::tom::ListPtr<ocs::tom::US> acl_list_;         ///< Access control lists
      ocs::tom::ListPtr<ocs::tom::CE> centry_list_;      ///< Complex entries
      ocs::tom::ListPtr<ocs::tom::CONF> config_list_;    ///< The global configuration, client side only
      ocs::tom::ListPtr<ocs::tom::EH> exec_host_list_;   ///< Execution hosts
      ocs::tom::ListPtr<ocs::tom::JB> job_list_;         ///< Jobs, reduced to the fields of get_job_what()
      ocs::tom::ListPtr<ocs::tom::PE> pe_list_;          ///< Parallel environments
      ocs::tom::ListPtr<ocs::tom::CQ> queue_list_;       ///< Cluster queues with their instances
      /// Job load adjustments of the scheduler configuration, never an empty handle after a successful fetch_data()
      ocs::tom::ListPtr<ocs::tom::CE> job_load_adjustments_;

   public:
      /** @brief True if the executing user has manager privileges
       * @return true if the executing user has manager privileges
       */
      [[nodiscard]] virtual bool is_manager() const { return is_manager_; }
      /** @brief The cluster queues with their instances
       *
       * Not a const handle: the view sets the alarm states of the queue instances it reports.
       *
       * @return the cluster queue list
       */
      [[nodiscard]] virtual ocs::tom::List<ocs::tom::CQ> get_queue_list() const { return *queue_list_; }
      /** @brief The jobs
       *
       * Not a const handle: the view tags the array tasks it reports.
       *
       * @return the job list
       */
      [[nodiscard]] virtual ocs::tom::List<ocs::tom::JB> get_job_list() const { return *job_list_; }
      /** @brief The complex entries
       * @return the centry list
       */
      [[nodiscard]] virtual ocs::tom::ConstList<ocs::tom::CE> get_centry_list() const { return *centry_list_; }
      /** @brief The execution hosts, sorted for the output, the global host first
       * @return the exec host list
       */
      [[nodiscard]] virtual ocs::tom::ConstList<ocs::tom::EH> get_exec_host_list() const { return *exec_host_list_; }
      /** @brief The parallel environments
       * @return the PE list
       */
      [[nodiscard]] virtual ocs::tom::ConstList<ocs::tom::PE> get_pe_list() const { return *pe_list_; }
      /** @brief The job load adjustments of the scheduler configuration
       *
       * A copy taken with the snapshot, so the load values can be resolved after the data store lock was
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
      static lCondition *get_host_where(ocs::tom::ConstList<ocs::tom::ST> hostname_list);
      static lCondition *get_job_where(ocs::tom::ConstList<ocs::tom::ST> user_name_list, uint32_t show);

      static lEnumeration *get_host_what();

      static lEnumeration *get_queue_what();

      static lEnumeration *get_job_what();

      static lEnumeration *get_centry_what();

      static lEnumeration *get_pe_what();

      static lEnumeration *get_user_set_what();

      virtual bool fetch_data(lList **answer_list, ocs::tom::ConstList<ocs::tom::ST> hostname_list,
                              ocs::tom::ConstList<ocs::tom::ST> user_name_list, uint32_t show);

      virtual bool prepare_data(lList **answer_list, ocs::tom::List<ocs::tom::CE> resource_match_list,
                                uint32_t show) const;

      virtual void filter_data(ocs::tom::ConstList<ocs::tom::CE> resource_match_list);
      virtual void sort_data();
   public:
      virtual bool make_snapshot(lList **answer_list, QHostParameter &parameter);
#pragma endregion

#pragma region Constructors/Destructors
   public:
      QHostModelBase() = default;
      virtual ~QHostModelBase() = default;
#pragma endregion

   };
}

