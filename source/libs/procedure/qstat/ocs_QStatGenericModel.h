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

#include <string>

#include "cull/cull.h"
#include "cull/cull_list.h"

#include "ocs_client_print.h"

#include "ocs_QStatModelBase.h"
#include "ocs_QStatParameter.h"

namespace ocs {
   class QStatGenericModel : public QStatModelBase {
   public:
      bool is_manager_ = false;

      // data filter
      lEnumeration *what_JB_Type = nullptr;
      lEnumeration *what_JAT_Type_template = nullptr;
      lEnumeration *what_JAT_Type_list = nullptr;

      lCondition *qstat_get_JB_Type_selection(lList *user_list, u_long32 show);
      lEnumeration *qstat_get_JB_Type_filter();

      // data lists
      lList* queue_list = nullptr;
      lList* centry_list = nullptr;
      lList* exechost_list = nullptr;
      lList* schedd_config = nullptr;
      lList* pe_list = nullptr;
      lList* ckpt_list = nullptr;
      lList* acl_list = nullptr;
      lList* job_list = nullptr;
      lList* hgrp_list = nullptr;
      lList* project_list = nullptr;

      void calc_longest_queue_length(QStatParameter &parameter);
   private:
      void qstat_filter_add_core_attributes(QStatParameter &parameter);
      int build_job_state_filter(lList **alpp, QStatParameter &parameter);
      void qstat_filter_add_l_attributes();
      void qstat_filter_add_ext_attributes();
      void qstat_filter_add_urg_attributes();
      void qstat_filter_add_pri_attributes();
      void qstat_filter_add_r_attributes();
      void qstat_filter_add_t_attributes();
      void qstat_filter_add_U_attributes();
      void qstat_filter_add_pe_attributes();
      void qstat_filter_add_q_attributes();
      bool prepare_filter(lList **answer_list, QStatParameter &parameter);

      int qstat_env_filter_queues(lList **alpp, QStatParameter &parameter);
      int filter_jobs(QStatParameter &parameter);

      bool fetch_data(lList **answer_list, QStatParameter &parameter);
      bool prepare_data(lList **alpp);
      bool filter_data(lList **alpp, QStatParameter &parameter);
      void free_data();
   public:
      QStatGenericModel() : QStatModelBase() {};
      ~QStatGenericModel() override { free_data(); }

      bool make_snapshot(lList **answer_list, QStatParameter &parameter) override;

      [[nodiscard]] bool is_manager() const { return is_manager_; }
   };
}