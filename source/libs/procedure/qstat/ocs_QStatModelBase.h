#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2023-2026 HPC-Gridware GmbH
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
 * @brief Base model of `qstat`: the lists the report is built from
 */

#include "cull/cull_list.h"

#include "ocs_QStatParameter.h"

namespace ocs {
   /** @brief Base model for qstat that holds all CULL data lists needed for rendering.
    *
    * Unlike the generic ProcedureModel (which delegates rendering to qmaster),
    * QStatModelBase fetches typed CULL lists, applies filtering, and exposes
    * the results directly to the view layer — in both client and server contexts.
    *
    * ## Client vs. server execution
    *
    * Two concrete subclasses implement the data-fetch step:
    *
    * - **QStatModelClient** (`ExecContext::CLIENT`): issues multiple GDI requests to
    *   qmaster to fetch queue, job, host, and auxiliary lists.
    *
    * - **QStatModelServer** (`ExecContext::SERVER`): reads directly from the in-process
    *   master lists inside qmaster, avoiding any network round-trip.
    *
    * Both subclasses share the filtering and preparation pipeline defined here.
    *
    * ## Virtual pipeline inside make_snapshot()
    *
    * 1. `prepare_filter()` — normalises parameter flags before data is fetched.
    * 2. `fetch_data()`     — populates the CULL list members (overridden per context).
    * 3. `prepare_data()`   — post-processes the fetched lists (e.g. resolves hostgroups).
    * 4. `filter_data()`    — applies user-specified filters (queue state, resource, PE …).
    *
    * @ingroup libprocedure
    */
   class QStatModelBase {
#pragma region Data
   public:
      // Data for the job view -j
      lList* ilp = nullptr;   ///< Scheduler job info (`SME_Type`): why the jobs are not running
      lList* jlp = nullptr;   ///< The jobs `qstat -j` was asked about
   protected:
      // data lists
      bool is_manager_ = false;   ///< Whether the executing user has manager privileges

      // Data for all views except for job view
      lList* queue_list_ = nullptr;      ///< Queue instances
      lList* centry_list_ = nullptr;     ///< Complex entries
      lList* exechost_list_ = nullptr;   ///< Execution hosts
      lList* schedd_config_ = nullptr;   ///< Scheduler configuration, for the priority columns
      lList* pe_list_ = nullptr;         ///< Parallel environments
      lList* ckpt_list_ = nullptr;       ///< Checkpointing environments
      lList* acl_list_ = nullptr;        ///< Access control lists
      lList* job_list_ = nullptr;        ///< Jobs
      lList* hgrp_list_ = nullptr;       ///< Host groups, to resolve `@group` in queue references
      lList* project_list_ = nullptr;    ///< Projects


   public:
      /** @brief Whether the executing user has manager privileges
       * @return true for a manager
       */
      [[nodiscard]] bool is_manager() const { return is_manager_; }

      /** @brief The queue instances
       * @return the queue list
       */
      [[nodiscard]] lList* get_queue_list() const { return queue_list_; }

      /** @brief The complex entries
       * @return the centry list
       */
      [[nodiscard]] lList* get_centry_list() const { return centry_list_; }

      /** @brief The execution hosts
       * @return the exec host list
       */
      [[nodiscard]] lList* get_exechost_list() const { return exechost_list_; }

      /** @brief The scheduler configuration
       * @return the scheduler configuration
       */
      [[nodiscard]] lList* get_schedd_config() const { return schedd_config_; }

      /** @brief The parallel environments
       * @return the PE list
       */
      [[nodiscard]] lList* get_pe_list() const { return pe_list_; }

      /** @brief The checkpointing environments
       * @return the checkpointing environment list
       */
      [[nodiscard]] lList* get_ckpt_list() const { return ckpt_list_; }

      /** @brief The access control lists
       * @return the ACL list
       */
      [[nodiscard]] lList* get_acl_list() const { return acl_list_; }

      /** @brief The jobs
       * @return the job list
       */
      [[nodiscard]] lList* get_job_list() const { return job_list_; }

      /** @brief The host groups
       * @return the hgroup list
       */
      [[nodiscard]] lList* get_hgrp_list() const { return hgrp_list_; }

      /** @brief The projects
       * @return the project list
       */
      [[nodiscard]] lList* get_project_list() const { return project_list_; }

#pragma endregion

#pragma region Data Retrieval
   private:
      static void apply_state_filter(QStatParameter &parameter);

      static lEnumeration *get_sub_job_filter(const QStatParameter &parameter);
      static lEnumeration *get_sub_ja_task_template_filter(const QStatParameter &parameter);
      static lEnumeration *get_sub_ja_task_filter(const QStatParameter &parameter);

   protected:
      static lEnumeration *get_queue_what();

      static lEnumeration *get_centry_what();

      static lCondition *get_ehost_where();

      static lEnumeration *get_ehost_what();

      static lEnumeration *get_pe_what();

      static lEnumeration *get_ckpt_what();

      static lEnumeration *get_uset_what();

      static lEnumeration *get_prj_what();

      static lEnumeration *get_sconf_what();

      static lEnumeration *get_hgrp_what();

      static lCondition *get_conf_where();

      static lEnumeration *get_conf_what();

      static lCondition *get_job_where(const QStatParameter &parameter);

      static lEnumeration *get_job_what(const QStatParameter &parameter);

      static lCondition *get_job_view_where(const lList *job_view_list);

      static lEnumeration *get_job_view_what();

      static lEnumeration *get_sme_what();

#pragma endregion

#pragma region Data Retrieval

   private:
      static int select_by_queue_state(uint32_t queue_states, lList *exechost_list, lList *queue_list, lList *centry_list);

      static int select_by_queue_user_list(lList *exechost_list, lList *cqueue_list, lList *queue_user_list, lList *acl_list, lList *project_list);

      static int select_by_pe_list(lList *queue_list, lList *peref_list, lList *pe_list);

      static int select_by_resource_list(lList *resource_list, lList *exechost_list, lList *queue_list, lList *centry_list, uint32_t empty_qs);

      static int select_by_qref_list(lList *cqueue_list, const lList *hgrp_list, const lList *qref_list);

      static bool is_cqueue_selected(lList *queue_list);

      void filter_jobs(const QStatParameter &parameter);

      int filter_queues(lList **answer_list, const QStatParameter &parameter) const;

   protected:
      virtual void prepare_filter(QStatParameter &parameter);

      virtual bool fetch_data(lList **answer_list, QStatParameter &parameter);

      virtual bool prepare_data(lList **answer_list, QStatParameter &parameter);

      virtual bool filter_data(lList **answer_list, QStatParameter &parameter);

   public:
      virtual bool make_snapshot(lList **answer_list, QStatParameter &parameter);

      void calc_longest_queue_length(QStatParameter &parameter) const;

#pragma endregion

#pragma region Constructor / Destructor

   public:
      QStatModelBase() = default;
      virtual ~QStatModelBase();

#pragma endregion

   };
}
