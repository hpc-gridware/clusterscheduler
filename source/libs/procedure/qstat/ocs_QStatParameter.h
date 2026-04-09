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
#include <algorithm>
#include <limits>

#include "cull/cull.h"

#include "sgeobj/sge_qinstance_state.h"

#include "ocs_client_print.h"
#include "ocs_ProcedureParameter.h"

namespace ocs {
   class QStatParameter : public ProcedureParameter {
   public:
      // @todo cleanup: declare protected and provide access methods
      enum class OutputMode {
         QSELECT,
         QSTAT_GROUP,
         QSTAT_DEFAULT,
         JOB_INFO
      };
      OutputMode output_mode_ = OutputMode::QSTAT_DEFAULT; //< default | -j | -g c | qselect

      bool need_queues_ = false; ///< need to fetch queues from master
      bool need_job_list_ = true; ///< need to fetch job list from master

      uint32_t full_listing_ = QSTAT_DISPLAY_ALL; // similar to *show* in qhost
      bool state_filter_ = false; /// -s switch was used
      std::string state_filter_value_; ///< -s values
      uint32_t queue_state_ = std::numeric_limits<uint32_t>::max(); ///< -qs
      uint32_t explain_bits_ = QI_DEFAULT; ///< -explain
      uint32_t group_opt_ = 0; ///< -g
      int longest_queue_length = 30; ///< used to align the output of the queue name column

   protected:
      lList *resource_list_ = nullptr; ///< -l resource_request
      lList *q_resource_list_ = nullptr; ///< -F resource_request
      lList *queue_ref_list_ = nullptr; ///< -q queue_list
      lList *pe_ref_list_ = nullptr; ///< -pe pe_list
      lList *user_list_ = nullptr; ///< -u user_list - selects jobs
      lList *queue_user_list_ = nullptr; ///< -U user_list - selects queues
      lList *jid_list_ = nullptr; ///< -j argument list

   public:
      [[nodiscard]] const lList *get_q_resource_list() const { return q_resource_list_; }
      [[nodiscard]] const lList *get_queue_ref_list() const { return queue_ref_list_; }
      [[nodiscard]] const lList *get_user_list() const { return user_list_; }
      [[nodiscard]] lList *get_resource_list() const { return resource_list_; }
      [[nodiscard]] lList *get_pe_ref_list() const { return pe_ref_list_; }
      [[nodiscard]] lList *get_queue_user_list() const { return queue_user_list_; }
      [[nodiscard]] lList *get_jid_list() const { return jid_list_; }

   public:
      QStatParameter() : ProcedureParameter("") {}
      ~QStatParameter() override;

   };
}
