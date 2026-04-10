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

#include "cull/cull_list.h"

#include "ocs_QStatParameter.h"

namespace ocs {
   class QStatModelBase {
#pragma region Data

   protected:
      // data lists
      bool is_manager_ = false;
      lList* queue_list_ = nullptr;
      lList* centry_list_ = nullptr;
      lList* exechost_list_ = nullptr;
      lList* schedd_config_ = nullptr;
      lList* pe_list_ = nullptr;
      lList* ckpt_list_ = nullptr;
      lList* acl_list_ = nullptr;
      lList* job_list_ = nullptr;
      lList* hgrp_list_ = nullptr;
      lList* project_list_ = nullptr;

   public:
      [[nodiscard]] lList* get_queue_list() const { return queue_list_; }
      [[nodiscard]] lList* get_centry_list() const { return centry_list_; }
      [[nodiscard]] lList* get_exechost_list() const { return exechost_list_; }
      [[nodiscard]] lList* get_schedd_config() const { return schedd_config_; }
      [[nodiscard]] lList* get_pe_list() const { return pe_list_; }
      [[nodiscard]] lList* get_ckpt_list() const { return ckpt_list_; }
      [[nodiscard]] lList* get_acl_list() const { return acl_list_; }
      [[nodiscard]] lList* get_job_list() const { return job_list_; }
      [[nodiscard]] lList* get_hgrp_list() const { return hgrp_list_; }
      [[nodiscard]] lList* get_project_list() const { return project_list_; }
      [[nodiscard]] bool is_manager() const { return is_manager_; }

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
