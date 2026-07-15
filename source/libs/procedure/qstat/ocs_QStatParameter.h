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

#include "gdi/ocs_gdi_Packet.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_qinstance_state.h"

#include "ocs_ProcedureParameter.h"

#define QSTAT_DISPLAY_FULL (1 << 0)
#define QSTAT_DISPLAY_EXTENDED (1 << 1)
#define QSTAT_DISPLAY_RESOURCES (1 << 2)
#define QSTAT_DISPLAY_QRESOURCES (1 << 3)
#define QSTAT_DISPLAY_TASKS (1 << 4)
#define QSTAT_DISPLAY_NOEMPTYQ (1 << 5)
#define QSTAT_DISPLAY_PENDING (1 << 6)
#define QSTAT_DISPLAY_SUSPENDED (1 << 7)
#define QSTAT_DISPLAY_RUNNING (1 << 8)
#define QSTAT_DISPLAY_FINISHED (1 << 9)
#define QSTAT_DISPLAY_ALARMREASON (1 << 11)
#define QSTAT_DISPLAY_USERHOLD (1 << 12)
#define QSTAT_DISPLAY_SYSTEMHOLD (1 << 13)
#define QSTAT_DISPLAY_OPERATORHOLD (1 << 14)
#define QSTAT_DISPLAY_JOBARRAYHOLD (1 << 15)
#define QSTAT_DISPLAY_JOBHOLD (1 << 16)
#define QSTAT_DISPLAY_STARTTIMEHOLD (1 << 17)
#define QSTAT_DISPLAY_URGENCY (1 << 18)
#define QSTAT_DISPLAY_PRIORITY (1 << 19)
#define QSTAT_DISPLAY_PEND_REMAIN (1 << 20)

#define QHOST_DISPLAY_QUEUES     (1<<22)
#define QHOST_DISPLAY_JOBS       (1<<23)
#define QHOST_DISPLAY_RESOURCES  (1<<24)

#define QSTAT_DISPLAY_HOLD                                                                                             \
(QSTAT_DISPLAY_USERHOLD | QSTAT_DISPLAY_SYSTEMHOLD | QSTAT_DISPLAY_OPERATORHOLD | QSTAT_DISPLAY_JOBARRAYHOLD |      \
QSTAT_DISPLAY_JOBHOLD | QSTAT_DISPLAY_STARTTIMEHOLD)
#define QSTAT_DISPLAY_ALL                                                                                              \
(QSTAT_DISPLAY_PENDING | QSTAT_DISPLAY_SUSPENDED | QSTAT_DISPLAY_RUNNING | QSTAT_DISPLAY_FINISHED)

#define TAG_DEFAULT 0x00
#define TAG_SHOW_IT 0x01
#define TAG_FOUND_IT 0x02
#define TAG_SELECT_IT 0x04

namespace ocs {
   class QStatParameter : public ProcedureParameter {

#pragma region Constants

   public:
      static constexpr auto CQ_FORMAT = "cluster-queue-format";
      static constexpr auto DEFAULT_FORMAT = "default-format";
      static constexpr auto JOB_FORMAT = "job-format";

#pragma endregion

#pragma region Procedure Parameter

   private:
      static constexpr auto RESOURCE_LIST = "resource_list";
      static constexpr auto Q_RESOURCE_LIST = "q_resource_list";
      static constexpr auto QUEUE_REF_LIST = "queue_ref_list";
      static constexpr auto PE_REF_LIST = "pe_ref_list";
      static constexpr auto USER_LIST = "user_list";
      static constexpr auto QUEUE_USER_LIST = "queue_user_list";
      static constexpr auto JID_LIST = "jid_list";
      static constexpr auto SHOW = "show";
      static constexpr auto OUTPUT_MODE = "output_mode";
      static constexpr auto NEED_QUEUES = "need_queues";
      static constexpr auto NEED_JOBS = "need_jobs";
      static constexpr auto STATE_FILTER = "state_filter";
      static constexpr auto STATE_STRING = "state_string";
      static constexpr auto QUEUE_STATE = "queue_state";
      static constexpr auto EXPLAIN_BITS = "explain_bits";
      static constexpr auto GROUP_OPT = "group_opt";
      static constexpr auto LOAD_AVG_VARIABLE = "load_avg_variable";

   public:
      // @todo cleanup: declare protected and provide access methods
      /* CS-1908: default excludes QSTAT_DISPLAY_FINISHED so a bare `qstat`
       * behaves like `qstat -s a` (== -s prs). Before U7 removed the MORE_INFO
       * env-var gate in process_jobs_finished_state, the FINISHED bit in
       * QSTAT_DISPLAY_ALL was inert; without this override the default would
       * now spill the entire retention window on every plain qstat call.
       * Retention viewing stays opt-in via `-s f`. */
      uint32_t show_ = QSTAT_DISPLAY_PENDING | QSTAT_DISPLAY_RUNNING | QSTAT_DISPLAY_SUSPENDED;

      enum class OutputMode {
         QSELECT,
         QSTAT_GROUP,
         QSTAT_DEFAULT,
         JOB_INFO
      };
      OutputMode output_mode_ = OutputMode::QSTAT_DEFAULT; //< default | -j | -g c | qselect

      bool need_queues_ = false; ///< need to fetch queues from master
      bool need_job_list_ = true; ///< need to fetch job list from master

      bool state_filter_ = false; /// -s switch was used
      std::string state_filter_value_; ///< -s values

      uint32_t queue_state_ = std::numeric_limits<uint32_t>::max(); ///< -qs
      uint32_t explain_bits_ = QI_DEFAULT; ///< -explain
      uint32_t group_opt_ = 0; ///< -g

      /* Load variable shown in the "load_avg" column of `qstat -f`.
       * Resolved from SGE_QSTAT_LOAD_AVG on the client side (see
       * QStatParameterClient::parse_parameters); marshalled to the server via
       * get_bundle() / set_bundle() so server-rendered qstat (ExecContext::SERVER)
       * honours the caller's env var. Default is LOAD_ATTR_NP_LOAD_AVG on the
       * 9.2 branch (CS-2387 phase-2 flip); V91_BRANCH kept LOAD_ATTR_LOAD_AVG.
       */
      std::string load_avg_variable_ = LOAD_ATTR_NP_LOAD_AVG;

      [[nodiscard]] const char *get_load_avg_variable() const { return load_avg_variable_.c_str(); }
      void set_load_avg_variable(const char *value) { load_avg_variable_ = value != nullptr ? value : ""; }

#pragma endregion


#pragma region Data

   private:
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
      [[nodiscard]] int get_longest_queue_length() const { return longest_queue_length; }
      void set_longest_queue_length(const int value) { longest_queue_length = value; }

#pragma endregion


#pragma region Marshaling

   protected:
      void set_bundle(const lList *bundle) override;

   public:
      [[nodiscard]] lList *get_bundle() override;

#pragma endregion


#pragma region Constructors/Destructors

   public:
      explicit QStatParameter(const lList *bundle, gdi::Packet *packet);
      explicit QStatParameter(std::string procedure_name) : ProcedureParameter(std::move(procedure_name), nullptr) {}
      ~QStatParameter() override;

#pragma endregion

   };
}
