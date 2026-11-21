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

#include "tom/ocs_Types.h"

#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CE.h"
#include "sgeobj/tom/ocs_tom_CONF.h"
#include "sgeobj/tom/ocs_tom_CK.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_EH.h"
#include "sgeobj/tom/ocs_tom_HGRP.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_PE.h"
#include "sgeobj/tom/ocs_tom_PR.h"
#include "sgeobj/tom/ocs_tom_QR.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "sgeobj/tom/ocs_tom_SC.h"
#include "sgeobj/tom/ocs_tom_SME.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_US.h"

#include "ocs_QStatParameter.h"
#include "sgeobj/sge_answer.h"

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
      ocs::tom::ListPtr<ocs::tom::SME> ilp;   ///< Scheduler job info: why the jobs are not running
      ocs::tom::ListPtr<ocs::tom::JB> jlp;    ///< The jobs `qstat -j` was asked about
   protected:
      // data lists
      bool is_manager_ = false;   ///< Whether the executing user has manager privileges

      // Data for all views except for job view
      /// Cluster queues with their instances; for the default listing filter_data() moves the instances into
      /// #qinstance_list_ and frees this list
      ocs::tom::ListPtr<ocs::tom::CQ> cqueue_list_;
      ocs::tom::ListPtr<ocs::tom::QU> qinstance_list_;   ///< Queue instances of the default listing, see #cqueue_list_
      ocs::tom::ListPtr<ocs::tom::CE> centry_list_;      ///< Complex entries
      ocs::tom::ListPtr<ocs::tom::EH> exechost_list_;    ///< Execution hosts
      ocs::tom::ListPtr<ocs::tom::SC> schedd_config_;    ///< Scheduler configuration, for the priority columns
      /// Job load adjustments of the scheduler configuration, never an empty handle after a successful fetch_data()
      ocs::tom::ListPtr<ocs::tom::CE> job_load_adjustments_;
      ocs::tom::ListPtr<ocs::tom::US> config_user_lists_;    ///< `user_lists` of the global configuration, for `-U`
      ocs::tom::ListPtr<ocs::tom::US> config_xuser_lists_;   ///< `xuser_lists` of the global configuration, for `-U`
      ocs::tom::ListPtr<ocs::tom::PE> pe_list_;          ///< Parallel environments
      ocs::tom::ListPtr<ocs::tom::CK> ckpt_list_;        ///< Checkpointing environments
      ocs::tom::ListPtr<ocs::tom::US> acl_list_;         ///< Access control lists
      ocs::tom::ListPtr<ocs::tom::JB> job_list_;         ///< Jobs
      ocs::tom::ListPtr<ocs::tom::HGRP> hgrp_list_;      ///< Host groups, to resolve `@group` in queue references
      ocs::tom::ListPtr<ocs::tom::PR> project_list_;     ///< Projects


   public:
      /** @brief Whether the executing user has manager privileges
       * @return true for a manager
       */
      [[nodiscard]] bool is_manager() const { return is_manager_; }

      /** @brief The cluster queues with their instances
       *
       * Filled for the `-g c` listing and for qselect; the default listing reads get_qinstance_list().
       *
       * @return the cluster queue list, an empty handle when no queues were fetched
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::CQ> get_cqueue_list() const { return *cqueue_list_; }

      /** @brief The queue instances of the default listing, sorted for the output
       * @return the queue instance list, an empty handle before filter_data()
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::QU> get_qinstance_list() const { return *qinstance_list_; }

      /** @brief The complex entries
       * @return the centry list
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::CE> get_centry_list() const { return *centry_list_; }

      /** @brief The execution hosts
       * @return the exec host list
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::EH> get_exechost_list() const { return *exechost_list_; }

      /** @brief The scheduler configuration
       * @return the scheduler configuration, an empty handle on the client once the sconf module took it over
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::SC> get_schedd_config() const { return *schedd_config_; }

      /** @brief The job load adjustments of the scheduler configuration
       *
       * A copy taken with the snapshot, so the load values can be corrected after the data store lock
       * was released (CS-3008). An empty list rather than an empty handle when none are configured,
       * because an empty handle makes the sched helpers fetch them from the data store.
       *
       * @return the load adjustments
       */
      [[nodiscard]] ocs::tom::ConstList<ocs::tom::CE> get_job_load_adjustments() const {
         return *job_load_adjustments_;
      }

      /** @brief The parallel environments
       * @return the PE list
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::PE> get_pe_list() const { return *pe_list_; }

      /** @brief The checkpointing environments
       * @return the checkpointing environment list
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::CK> get_ckpt_list() const { return *ckpt_list_; }

      /** @brief The access control lists
       * @return the ACL list
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::US> get_acl_list() const { return *acl_list_; }

      /** @brief The jobs
       * @return the job list
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::JB> get_job_list() const { return *job_list_; }

      /** @brief The host groups
       * @return the hgroup list
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::HGRP> get_hgrp_list() const { return *hgrp_list_; }

      /** @brief The projects
       * @return the project list
       */
      [[nodiscard]] ocs::tom::List<ocs::tom::PR> get_project_list() const { return *project_list_; }

#pragma endregion

#pragma region Data Retrieval
   private:
      static void apply_state_filter(QStatParameter &parameter);

      static ocs::tom::What<ocs::tom::JB> get_sub_job_filter(const QStatParameter &parameter);
      static ocs::tom::What<ocs::tom::JAT> get_sub_ja_task_template_filter(const QStatParameter &parameter);
      static ocs::tom::What<ocs::tom::JAT> get_sub_ja_task_filter(const QStatParameter &parameter);

   protected:
      static ocs::tom::What<ocs::tom::CQ> get_queue_what();

      static ocs::tom::What<ocs::tom::CE> get_centry_what();

      static ocs::tom::Where<ocs::tom::EH> get_ehost_where();

      static ocs::tom::What<ocs::tom::EH> get_ehost_what();

      static ocs::tom::What<ocs::tom::PE> get_pe_what();

      static ocs::tom::What<ocs::tom::CK> get_ckpt_what();

      static ocs::tom::What<ocs::tom::US> get_uset_what();

      static ocs::tom::What<ocs::tom::PR> get_prj_what();

      static ocs::tom::What<ocs::tom::SC> get_sconf_what();

      static ocs::tom::What<ocs::tom::HGRP> get_hgrp_what();

      static ocs::tom::Where<ocs::tom::CONF> get_conf_where();

      static ocs::tom::What<ocs::tom::CONF> get_conf_what();

      static ocs::tom::Where<ocs::tom::JB> get_job_where(const QStatParameter &parameter);

      static ocs::tom::What<ocs::tom::JB> get_job_what(const QStatParameter &parameter);

      static ocs::tom::Where<ocs::tom::JB> get_job_owner_where(ocs::tom::ConstList<ocs::tom::ST> user_list);

      static ocs::tom::Where<ocs::tom::JB> get_job_view_where(ocs::tom::ConstList<ocs::tom::ST> job_view_list,
                                                              ocs::tom::ConstList<ocs::tom::ST> user_list);

      static ocs::tom::What<ocs::tom::JB> get_job_view_what();

      static ocs::tom::What<ocs::tom::SME> get_sme_what();

#pragma endregion

#pragma region Data Retrieval

   private:
      static int select_by_queue_state(uint32_t queue_states, ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                                       ocs::tom::List<ocs::tom::CQ> queue_list,
                                       ocs::tom::ConstList<ocs::tom::CE> centry_list,
                                       ocs::tom::ConstList<ocs::tom::CE> load_adjustments);

      static int select_by_queue_user_list(ocs::tom::ConstList<ocs::tom::EH> exechost_list,
                                           ocs::tom::List<ocs::tom::CQ> cqueue_list,
                                           ocs::tom::ConstList<ocs::tom::ST> queue_user_list,
                                           ocs::tom::ConstList<ocs::tom::US> acl_list,
                                           ocs::tom::ConstList<ocs::tom::PR> project_list,
                                           ocs::tom::ConstList<ocs::tom::US> config_acl,
                                           ocs::tom::ConstList<ocs::tom::US> config_xacl);

      static int select_by_pe_list(ocs::tom::List<ocs::tom::CQ> queue_list,
                                   ocs::tom::ConstList<ocs::tom::ST> peref_list,
                                   ocs::tom::ConstList<ocs::tom::PE> pe_list);

      static int select_by_resource_list(ocs::tom::List<ocs::tom::CE> resource_list,
                                         ocs::tom::List<ocs::tom::EH> exechost_list,
                                         ocs::tom::List<ocs::tom::CQ> queue_list,
                                         ocs::tom::ConstList<ocs::tom::CE> centry_list, uint32_t empty_qs);

      static int select_by_qref_list(ocs::tom::List<ocs::tom::CQ> cqueue_list,
                                     ocs::tom::ConstList<ocs::tom::HGRP> hgrp_list,
                                     ocs::tom::ConstList<ocs::tom::QR> qref_list);

      static bool is_cqueue_selected(ocs::tom::List<ocs::tom::CQ> queue_list);

      void filter_jobs(const QStatParameter &parameter);

      int filter_queues(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const QStatParameter &parameter) const;

   protected:
      virtual void prepare_filter(QStatParameter &parameter);

      virtual bool fetch_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter);

      virtual bool prepare_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter);

      virtual bool filter_data(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter);

   public:
      virtual bool make_snapshot(ocs::tom::ListPtr<ocs::tom::AN> *answer_list, QStatParameter &parameter);

      void calc_longest_queue_length(QStatParameter &parameter) const;

#pragma endregion

#pragma region Constructor / Destructor

   public:
      QStatModelBase() = default;
      virtual ~QStatModelBase();

#pragma endregion

   };
}
