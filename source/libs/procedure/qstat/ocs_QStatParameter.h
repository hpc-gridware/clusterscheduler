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
 * @brief Parameters of `qstat`: what the call was asked to report
 */

#include <string>
#include <algorithm>
#include <limits>

#include "cull/cull.h"

#include "gdi/ocs_gdi_Packet.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_qinstance_state.h"

#include "ocs_ProcedureParameter.h"

/** @brief `-f`: the full listing, queue by queue */
#define QSTAT_DISPLAY_FULL (1 << 0)
/** @brief `-ext`: the extra ticket and usage columns */
#define QSTAT_DISPLAY_EXTENDED (1 << 1)
/** @brief `-F`: the resources of each queue instance */
#define QSTAT_DISPLAY_RESOURCES (1 << 2)
/** @brief A resource selection was given to `-F` */
#define QSTAT_DISPLAY_QRESOURCES (1 << 3)
/** @brief `-t`: one line per array task */
#define QSTAT_DISPLAY_TASKS (1 << 4)
/** @brief `-ne`: skip queues that hold no job */
#define QSTAT_DISPLAY_NOEMPTYQ (1 << 5)
/** @brief `-s p`: the jobs that are waiting */
#define QSTAT_DISPLAY_PENDING (1 << 6)
/** @brief `-s s`: the jobs that are suspended */
#define QSTAT_DISPLAY_SUSPENDED (1 << 7)
/** @brief `-s r`: the jobs that are running */
#define QSTAT_DISPLAY_RUNNING (1 << 8)
/** @brief `-s f`: the jobs that have finished, from the retention window */
#define QSTAT_DISPLAY_FINISHED (1 << 9)
/** @brief `-explain a`: why a queue is in alarm */
#define QSTAT_DISPLAY_ALARMREASON (1 << 11)
/** @brief `-s hu`: the jobs held by their owner */
#define QSTAT_DISPLAY_USERHOLD (1 << 12)
/** @brief `-s hs`: the jobs held by the system */
#define QSTAT_DISPLAY_SYSTEMHOLD (1 << 13)
/** @brief `-s ho`: the jobs held by an operator */
#define QSTAT_DISPLAY_OPERATORHOLD (1 << 14)
/** @brief `-s ha`: the jobs held by an array dependency */
#define QSTAT_DISPLAY_JOBARRAYHOLD (1 << 15)
/** @brief `-s hj`: the jobs held by a job dependency */
#define QSTAT_DISPLAY_JOBHOLD (1 << 16)
/** @brief `-s hd`: the jobs held until their start time */
#define QSTAT_DISPLAY_STARTTIMEHOLD (1 << 17)
/** @brief `-urg`: the urgency columns */
#define QSTAT_DISPLAY_URGENCY (1 << 18)
/** @brief `-pri`: the priority columns */
#define QSTAT_DISPLAY_PRIORITY (1 << 19)
/** @brief Show how much of an array job is still pending */
#define QSTAT_DISPLAY_PEND_REMAIN (1 << 20)

/** @brief `qhost -q`: the queues of each host */
#define QHOST_DISPLAY_QUEUES     (1<<22)
/** @brief `qhost -j`: the jobs of each host */
#define QHOST_DISPLAY_JOBS       (1<<23)
/** @brief `qhost -F`: the resources of each host */
#define QHOST_DISPLAY_RESOURCES  (1<<24)

/** @brief Every hold state at once, for `-s h` */
#define QSTAT_DISPLAY_HOLD                                                                                             \
(QSTAT_DISPLAY_USERHOLD | QSTAT_DISPLAY_SYSTEMHOLD | QSTAT_DISPLAY_OPERATORHOLD | QSTAT_DISPLAY_JOBARRAYHOLD |      \
QSTAT_DISPLAY_JOBHOLD | QSTAT_DISPLAY_STARTTIMEHOLD)
/** @brief Every job state at once, for `-s a`
 *
 * `show_` does not default to this: a bare `qstat` leaves
 * #QSTAT_DISPLAY_FINISHED out so it does not print the whole retention
 * window. Finished jobs stay opt-in via `-s f` (CS-1908).
 */
#define QSTAT_DISPLAY_ALL                                                                                              \
(QSTAT_DISPLAY_PENDING | QSTAT_DISPLAY_SUSPENDED | QSTAT_DISPLAY_RUNNING | QSTAT_DISPLAY_FINISHED)

/** @name How the model marks a queue instance while it filters
 *
 * The filters run one after another over the same list, so each of them tags
 * what it kept rather than building a new list.
 * @{
 */
/** @brief Not tagged */
#define TAG_DEFAULT 0x00
/** @brief The queue instance is reported */
#define TAG_SHOW_IT 0x01
/** @brief The queue instance matched a filter */
#define TAG_FOUND_IT 0x02
/** @brief The queue instance survived every filter */
#define TAG_SELECT_IT 0x04
/** @} */

namespace ocs {
   /** @brief Everything one `qstat` or `qselect` call was asked to report
    *
    * One parameter object serves all four output modes - the default listing,
    * `-g c`, `-j` and `qselect` - because they share the same filters and only
    * differ in what they print. #output_mode_ says which of them is running.
    *
    * Built from the command line on the client and from the marshalled bundle
    * on the server, so that both sides run the same model against the same
    * parameters.
    *
    * @ingroup libprocedure
    */
   class QStatParameter : public ProcedureParameter {

#pragma region Constants

   public:
      /** @name Names the sub-procedure is dispatched on
       *
       * The server picks the controller and the view from this name, so it has
       * to mean the same on both sides.
       * @{
       */
      static constexpr auto CQ_FORMAT = "cluster-queue-format";   ///< `qstat -g c`
      static constexpr auto DEFAULT_FORMAT = "default-format";    ///< Plain `qstat`, and `qselect`
      static constexpr auto JOB_FORMAT = "job-format";            ///< `qstat -j`
      /** @} */

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
      uint32_t show_ = QSTAT_DISPLAY_PENDING | QSTAT_DISPLAY_RUNNING | QSTAT_DISPLAY_SUSPENDED;   ///< What to report, as a `QSTAT_DISPLAY_*` bitmask

      /** @brief Which of the four listings this call produces */
      enum class OutputMode {
         QSELECT,         ///< `qselect`: nothing but the names of the matching queue instances
         QSTAT_GROUP,     ///< `qstat -g c`: one line per cluster queue
         QSTAT_DEFAULT,   ///< Plain `qstat`: the queue instances and the jobs in them
         JOB_INFO         ///< `qstat -j`: one job in full detail
      };
      OutputMode output_mode_ = OutputMode::QSTAT_DEFAULT; ///< Which listing this call produces

      bool need_queues_ = false; ///< need to fetch queues from master
      bool need_job_list_ = true; ///< need to fetch job list from master

      bool state_filter_ = false; ///< -s switch was used
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
      std::string load_avg_variable_ = LOAD_ATTR_NP_LOAD_AVG;   ///< Which load value the `load_avg` column shows

      /** @brief The load value shown in the `load_avg` column
       * @return the load attribute's name
       */
      [[nodiscard]] const char *get_load_avg_variable() const { return load_avg_variable_.c_str(); }

      /** @brief Choose the load value shown in the `load_avg` column
       * @param value the load attribute's name; nullptr clears it
       */
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
      /** @brief The resources whose values the listing shows, from `-F`
       * @return the requested resources
       */
      [[nodiscard]] const lList *get_q_resource_list() const { return q_resource_list_; }

      /** @brief The queues to report on, from `-q`
       * @return the queue references, empty when all queues were requested
       */
      [[nodiscard]] const lList *get_queue_ref_list() const { return queue_ref_list_; }

      /** @brief The users whose jobs to report, from `-u`
       * @return the user names
       */
      [[nodiscard]] const lList *get_user_list() const { return user_list_; }

      /** @brief The resources a queue must offer to be reported, from `-l`
       * @return the requested resources
       */
      [[nodiscard]] lList *get_resource_list() const { return resource_list_; }

      /** @brief The parallel environments a queue must offer to be reported, from `-pe`
       * @return the requested parallel environments
       */
      [[nodiscard]] lList *get_pe_ref_list() const { return pe_ref_list_; }

      /** @brief The users a queue must be usable by to be reported, from `-U`
       * @return the user names
       */
      [[nodiscard]] lList *get_queue_user_list() const { return queue_user_list_; }

      /** @brief The jobs `qstat -j` was asked about
       * @return the job ids and patterns as they were given
       */
      [[nodiscard]] lList *get_jid_list() const { return jid_list_; }

      /** @brief The width of the queue name column
       * @return the width, in characters
       */
      [[nodiscard]] int get_longest_queue_length() const { return longest_queue_length; }

      /** @brief Set the width of the queue name column
       *
       * Called by QStatModelBase::calc_longest_queue_length() once the reported
       * queues are known.
       *
       * @param value the width, in characters
       */
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

      /** @brief Build empty parameters, to be filled in by the client
       * @param procedure_name the command being run
       */
      explicit QStatParameter(std::string procedure_name) : ProcedureParameter(std::move(procedure_name), nullptr) {}
      ~QStatParameter() override;

#pragma endregion

   };
}
