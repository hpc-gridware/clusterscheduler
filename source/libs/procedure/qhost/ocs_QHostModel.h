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


namespace ocs {
   class QHostModel {
      bool is_manager_ = false;
      lList *acl_list_ = nullptr;
      lList *centry_list_ = nullptr;
      lList *config_list_ = nullptr;
      lList *exechost_list_ = nullptr;
      lList *job_list_ = nullptr;
      lList *pe_list_ = nullptr;
      lList *queue_list_ = nullptr;

      bool fetch_data(lList **answer_list, const lList *hostname_list, const lList *user_name_list, u_long32 show);
      bool prepare_data(lList **answer_list, const lList *resource_match_list, u_long32 show) const;
      void filter_data(const lList *resource_match_list);
      void sort_data();

      void free_data();
   public:
      QHostModel() = default;
      virtual ~QHostModel() { free_data(); }

      bool make_snapshot(lList **answer_list, QHostParameter &parameter);

      [[nodiscard]] bool is_manager() const { return is_manager_; }
      [[nodiscard]] lList *get_queue_list() const { return queue_list_; }
      [[nodiscard]] lList *get_job_list() const { return job_list_; }
      [[nodiscard]] lList *get_centry_list() const { return centry_list_; }
      [[nodiscard]] lList *get_exechost_list() const { return exechost_list_; }
      [[nodiscard]] lList *get_pe_list() const { return pe_list_; }
      [[nodiscard]] lList *get_acl_list() const { return acl_list_; }
   };
}


