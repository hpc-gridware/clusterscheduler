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

#include "cull/cull.h"

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
      bool is_manager_ = false;           ///< True if the executing user has manager privileges
      lList *acl_list_ = nullptr;         ///< Access control lists
      lList *centry_list_ = nullptr;      ///< Complex entry lists
      lList *config_list_ = nullptr;      ///< Configuration lists
      lList *exec_host_list_ = nullptr;   ///< Execution host lists
      lList *job_list_ = nullptr;         ///< Job lists
      lList *pe_list_ = nullptr;          ///< Parallel environment lists
      lList *queue_list_ = nullptr;       ///< Queue lists

   public:
      [[nodiscard]] virtual bool is_manager() const { return is_manager_; }
      [[nodiscard]] virtual lList *get_queue_list() const { return queue_list_; }
      [[nodiscard]] virtual lList *get_job_list() const { return job_list_; }
      [[nodiscard]] virtual lList *get_centry_list() const { return centry_list_; }
      [[nodiscard]] virtual lList *get_exec_host_list() const { return exec_host_list_; }
      [[nodiscard]] virtual lList *get_pe_list() const { return pe_list_; }
      [[nodiscard]] virtual lList *get_acl_list() const { return acl_list_; }
#pragma endregion

#pragma region Data Retrieval
   protected:
      static lCondition *get_host_where(const lList *hostname_list);
      static lCondition *get_job_where(const lList *user_name_list, uint32_t show);
      static lEnumeration *get_host_what();
      static lEnumeration *get_queue_what();
      static lEnumeration *get_job_what();
      static lEnumeration *get_centry_what();
      static lEnumeration *get_pe_what();
      static lEnumeration *get_user_set_what();

      /** @brief Fetch raw CULL lists into the member variables.
       *
       * Overridden by QHostModelClient (GDI calls) and QHostModelServer (master lists).
       */
      virtual bool fetch_data(lList **answer_list, const lList *hostname_list, const lList *user_name_list, uint32_t show);
      /** @brief Post-process fetched lists (e.g. resolve hostgroup references). */
      virtual bool prepare_data(lList **answer_list, const lList *resource_match_list, uint32_t show) const;
      /** @brief Apply the resource-match filter to the exec host list. */
      virtual void filter_data(const lList *resource_match_list);
      /** @brief Sort the exec host list for deterministic view output. */
      virtual void sort_data();
   public:
      /** @brief Run the full pipeline: fetch_data → prepare_data → filter_data → sort_data.
       *
       * @param answer_list  Receives error messages on failure.
       * @param parameter    Parsed qhost parameters.
       * @return true if all pipeline steps succeeded.
       */
      virtual bool make_snapshot(lList **answer_list, QHostParameter &parameter);
#pragma endregion

#pragma region Constructors/Destructors
   public:
      QHostModelBase() = default;
      virtual ~QHostModelBase();
#pragma endregion

   };
}

