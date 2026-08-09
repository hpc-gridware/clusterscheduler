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
 * @brief Base view of plain `qstat`, and the interface the three output formats implement
 */

#include <cinttypes>
#include <algorithm>
#include <string>

#include "ocs_ProcedureView.h"
#include "../ocs_QStatParameter.h"
#include "../ocs_QStatModelClient.h"

namespace ocs {
   /** @brief Base view for plain `qstat`, and the interface the three formats implement
    *
    * This is the listing `qstat` prints without `-j`: the queue instances, then
    * the jobs in them, then the jobs that are still waiting. The controller
    * walks that structure and reports it through these hooks;
    * #QStatDefaultViewPlain, #QStatDefaultViewXML and #QStatDefaultViewJSON
    * turn the events into their own syntax.
    *
    * The hooks do not take CULL elements but the `*_summary_t` structs below.
    * The controller works out what a line says - which of the many optional
    * columns apply, what the state string is, whether a value is present at all
    * - once, and all three formats print the same answer.
    *
    * @ingroup libprocedure
    */
   class QStatDefaultViewBase : public ProcedureView {
   public:
      /** @brief Which extra column a `report_additional_info()` call is about */
      enum job_additional_info_t {
         JOB_ADDITIONAL_INFO_ERROR = 0,   ///< Not a column; the value could not be determined
         CHECKPOINT_ENV  = 1,             ///< The checkpointing environment
         MASTER_QUEUE = 2,                ///< The master queue of a parallel job
         FULL_JOB_NAME = 3,               ///< The job name, untruncated
         REQUESTED_PE = 4,                ///< The parallel environment that was asked for
         GRANTED_PE = 5                   ///< The parallel environment that was given
      };

      /** @brief One queue instance, reduced to what its header line shows */
      struct queue_summary_t {
         // Owning storage: queue_type/arch/state used to be const char* aliasing
         // stack locals of the controller, a latent use-after-scope if the
         // summary ever outlived that frame (CS-2366, LOW-QSTAT-003). std::string
         // makes them own their data; an empty string means "absent". load_avg_str
         // points at a string literal and stays const char*.
         std::string queue_type;   ///< The queue type letters, e.g. `BIP`

         uint32_t    used_slots;    ///< Slots in use
         uint32_t    resv_slots;    ///< Slots reserved by advance reservations
         uint32_t    total_slots;   ///< Slots configured

         std::string arch;    ///< The host architecture; empty when it is not known
         std::string state;   ///< The queue state letters; empty when the queue is in no special state

         const char* load_avg_str;   ///< What to print instead of the load, e.g. `-NA-`; a string literal
         bool has_load_value;   ///< Whether a load value is known at all
         bool has_load_value_from_object;   ///< Whether the load came from the host rather than being derived
         double load_avg;   ///< The load average, valid only when #has_load_value

      };

      /** @brief One job, reduced to what its line shows
    *
    * The `has_*` flags exist because a job that has not started has no usage,
    * and a missing value prints as an empty column rather than as zero.
    */
      struct job_summary_t {
         bool print_jobid;            ///< Whether the job number still has to be printed on this line
         int priority;                ///< The priority the job was submitted with
         double nurg;                 ///< Normalised urgency
         double urg;                  ///< Urgency
         double nppri;                ///< Normalised POSIX priority
         double nprior;               ///< Normalised total priority
         double ntckts;               ///< Normalised tickets
         double rrcontr;              ///< Resource contribution to the urgency
         double wtcontr;              ///< Waiting time contribution to the urgency
         double dlcontr;              ///< Deadline contribution to the urgency
         const char* name;            ///< The job name
         const char* user;            ///< The submitting user
         const char* project;         ///< The project the job is accounted to
         const char* department;      ///< The department the submitting user belongs to
         char state[8];               ///< The state letters
         uint64_t submit_time;        ///< When the job was submitted
         uint64_t start_time;         ///< When the job started; 0 while it waits
         uint64_t deadline;           ///< The deadline of a deadline job

         bool   has_cpu_usage;        ///< Whether CPU usage is known
         uint32_t cpu_usage;          ///< CPU seconds used, valid only when #has_cpu_usage
         bool   has_mem_usage;        ///< Whether memory usage is known
         double mem_usage;            ///< Integral memory usage, valid only when #has_mem_usage
         bool   has_io_usage;         ///< Whether I/O usage is known
         double io_usage;             ///< Data transferred, valid only when #has_io_usage

         u_long override_tickets;     ///< Tickets a manager granted the job
         bool   is_queue_assigned;    ///< Whether the job has been assigned to a queue instance
         u_long tickets;              ///< Total tickets
         u_long otickets;             ///< Override tickets
         u_long ftickets;             ///< Functional tickets
         u_long stickets;             ///< Share tree tickets

         double share;                ///< The share of the cluster the job is entitled to
         const char* queue;           ///< The queue instance the job runs in
         const char* master;          ///< The master queue of a parallel job
         uint32_t slots;              ///< Slots the job holds
         bool is_array;               ///< Whether the job is an array job
         bool is_running;             ///< Whether the job is running
         const char* task_id;         ///< The task range, pre-rendered

      };

      /** @brief One task of an array job, reduced to what its line shows */
      struct task_summary_t {
         const char* task_id;       ///< The task number
         const char* state;         ///< The state letters
         bool has_cpu_usage;        ///< Whether CPU usage is known
         double cpu_usage;          ///< CPU seconds used, valid only when #has_cpu_usage
         bool has_mem_usage;        ///< Whether memory usage is known
         double mem_usage;          ///< Integral memory usage, valid only when #has_mem_usage
         bool has_io_usage;         ///< Whether I/O usage is known
         double io_usage;           ///< Data transferred, valid only when #has_io_usage
         bool is_running;           ///< Whether the task is running
         bool has_exit_status;      ///< Whether the task has finished and left an exit status
         uint32_t exit_status;      ///< The exit status, valid only when #has_exit_status
      };

      /** @brief Build a view for one `qstat` call
       * @param parameter the call's parameters
       */
      explicit QStatDefaultViewBase(const ProcedureParameter &parameter) : ProcedureView(parameter) {};
      ~QStatDefaultViewBase() override = default;

      // region General report handling
      /** @brief Report the header line of one queue instance
       * @param os stream to write to
       * @param qname the queue instance
       * @param summary the queue instance, already reduced to what is printed
       * @param parameter the call's parameters
       */
      virtual void report_queue_summary(std::ostream &os, const char* qname,  queue_summary_t *summary, QStatParameter &parameter) = 0;
      /** @brief Begin the report
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_started(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief End the report
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_finished(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief Begin the queue section
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_queue_section_started(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief End the queue section
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_queue_section_finished(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief Begin one queue instance
       * @param os stream to write to
       * @param qname the queue instance
       * @param parameter the call's parameters
       */
      virtual void report_queue_started(std::ostream &os, const char* qname, QStatParameter &parameter) = 0;
      /** @brief Report that the queue instance is in load alarm
       * @param os stream to write to
       * @param qname the queue instance
       * @param reason why the alarm fired
       */
      virtual void report_queue_load_alarm(std::ostream &os, const char* qname, const char* reason) = 0;
      /** @brief Report that the queue instance is in suspend alarm
       * @param os stream to write to
       * @param qname the queue instance
       * @param reason why the alarm fired
       */
      virtual void report_queue_suspend_alarm(std::ostream &os, const char* qname, const char* reason) = 0;
      /** @brief Report a message attached to the queue instance
       * @param os stream to write to
       * @param qname the queue instance
       * @param message the message
       */
      virtual void report_queue_message(std::ostream &os, const char* qname, const char *message) = 0;
      /** @brief Begin the resources of one queue instance
       * @param os stream to write to
       * @param name the queue instance
       */
      virtual void report_queue_resource_started(std::ostream &os, const char* name) = 0;
      /** @brief End the resource list
       * @param os stream to write to
       * @param name the queue instance
       */
      virtual void report_queue_resource_finished(std::ostream &os, const char* name) = 0;
      /** @brief Report one resource of the queue instance
       * @param os stream to write to
       * @param resource the complex entry the value belongs to
       * @param dom where the value comes from, e.g. `hl` for a host load value
       * @param name the resource
       * @param value its value
       * @param details the value's origin, printed when the user asked for it
       */
      virtual void report_queue_resource(std::ostream &os, const lListElem *resource, const char* dom, const char* name, const char* value, const char *details) = 0;
      /** @brief End the current queue instance
       * @param os stream to write to
       * @param qname the queue instance
       * @param parameter the call's parameters
       */
      virtual void report_queue_finished(std::ostream &os, const char* qname, QStatParameter &parameter) = 0;
      /** @brief Begin the jobs running in the queue instance
       * @param os stream to write to
       * @param qname the queue instance
       * @param parameter the call's parameters
       */
      virtual void report_queue_jobs_started(std::ostream &os, const char* qname, QStatParameter &parameter) = 0;
      /** @brief End the jobs of the queue instance
       * @param os stream to write to
       * @param qname the queue instance
       * @param parameter the call's parameters
       */
      virtual void report_queue_jobs_finished(std::ostream &os, const char* qname, QStatParameter &parameter) = 0;
      /** @brief Begin the section of jobs that are still waiting
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_pending_jobs_started(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief End the pending job section
       * @param os stream to write to
       */
      virtual void report_pending_jobs_finished(std::ostream &os) = 0;
      /** @brief Begin the section of jobs that have finished
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_finished_jobs_started(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief End the finished job section
       * @param os stream to write to
       */
      virtual void report_finished_jobs_finished(std::ostream &os) = 0;
      /** @brief Begin the section of jobs in error state
       * @param os stream to write to
       * @param parameter the call's parameters
       */
      virtual void report_error_jobs_started(std::ostream &os, QStatParameter &parameter) = 0;
      /** @brief End the error job section
       * @param os stream to write to
       */
      virtual void report_error_jobs_finished(std::ostream &os) = 0;
      // endregion

      // region Job handling
      /** @brief Report one job
       * @param os stream to write to
       * @param jid the job number
       * @param summary the job, already reduced to what is printed
       * @param parameter the call's parameters
       * @param model the fetched lists
       */
      virtual void report_job(std::ostream &os, uint32_t jid, job_summary_t *summary, QStatParameter &parameter, QStatModelBase &model) = 0;
      /** @brief Begin the tasks of the current job
       * @param os stream to write to
       */
      virtual void report_sub_tasks_started(std::ostream &os) = 0;
      /** @brief Report one task of the current job
       * @param os stream to write to
       * @param summary the task, already reduced to what is printed
       */
      virtual void report_sub_task(std::ostream &os, task_summary_t *summary) = 0;
      /** @brief End the task list
       * @param os stream to write to
       */
      virtual void report_sub_tasks_finished(std::ostream &os) = 0;
      /** @brief Report one of the extra columns the user asked for
       * @param os stream to write to
       * @param name which extra column this is
       * @param value its value
       */
      virtual void report_additional_info(std::ostream &os, job_additional_info_t name, const char* value) = 0;
      /** @brief Report the parallel environment the job asked for
       * @param os stream to write to
       * @param pe_name the parallel environment
       * @param pe_range the slot range that was requested
       */
      virtual void report_requested_pe(std::ostream &os, const char* pe_name, const char* pe_range) = 0;
      /** @brief Report the parallel environment the job was given
       * @param os stream to write to
       * @param pe_name the parallel environment
       * @param pe_slots the slots that were granted
       */
      virtual void report_granted_pe(std::ostream &os, const char* pe_name, int pe_slots) = 0;
      /** @brief Begin the requests that came from the cluster defaults rather than the command line
       * @param os stream to write to
       */
      virtual void report_default_request_started(std::ostream &os) = 0;
      /** @brief End the default requests
       * @param os stream to write to
       */
      virtual void report_default_request_finished(std::ostream &os) = 0;
      /** @brief Report one default request
       * @param os stream to write to
       * @param name the resource
       * @param value its value
       */
      virtual void report_default_request(std::ostream &os, const char* name, const char* value) = 0;
      /** @brief Begin the hard resource requests of one scope
       * @param os stream to write to
       * @param scope which request scope the request belongs to: global, master or slave
       */
      virtual void report_hard_resources_started(std::ostream &os, int scope) = 0;
      /** @brief Report one hard resource request
       * @param os stream to write to
       * @param scope which request scope the request belongs to: global, master or slave
       * @param resource the complex entry the value belongs to
       * @param name the resource
       * @param value the requested value
       * @param uc the urgency contribution of the request
       */
      virtual void report_hard_resource(std::ostream &os, int scope, const lListElem *resource, const char* name, const char* value, double uc) = 0;
      /** @brief Begin the soft resource requests of one scope
       * @param os stream to write to
       * @param scope which request scope the request belongs to: global, master or slave
       */
      virtual void report_soft_resources_started(std::ostream &os, int scope) = 0;
      /** @brief End the hard resource requests
       * @param os stream to write to
       */
      virtual void report_hard_resources_finished(std::ostream &os) = 0;
      /** @brief Report one soft resource request
       * @param os stream to write to
       * @param scope which request scope the request belongs to: global, master or slave
       * @param resource the complex entry the value belongs to
       * @param name the resource
       * @param value the requested value
       * @param uc the urgency contribution of the request
       */
      virtual void report_soft_resource(std::ostream &os, int scope, const lListElem *resource, const char* name, const char* value, double uc) = 0;
      /** @brief End the soft resource requests
       * @param os stream to write to
       */
      virtual void report_soft_resources_finished(std::ostream &os) = 0;
      /** @brief Begin the queues the job insisted on
       * @param os stream to write to
       * @param scope which request scope the request belongs to: global, master or slave
       */
      virtual void report_hard_requested_queues_started(std::ostream &os, int scope) = 0;
      /** @brief Report one queue the job insisted on
       * @param os stream to write to
       * @param scope which request scope the request belongs to: global, master or slave
       * @param name the queue
       */
      virtual void report_hard_requested_queue(std::ostream &os, int scope, const char* name) = 0;
      /** @brief End the hard queue requests
       * @param os stream to write to
       */
      virtual void report_hard_requested_queues_finished(std::ostream &os) = 0;
      /** @brief Begin the queues the job preferred
       * @param os stream to write to
       * @param scope which request scope the request belongs to: global, master or slave
       */
      virtual void report_soft_requested_queues_started(std::ostream &os, int scope) = 0;
      /** @brief Report one queue the job preferred
       * @param os stream to write to
       * @param scope which request scope the request belongs to: global, master or slave
       * @param name the queue
       */
      virtual void report_soft_requested_queue(std::ostream &os, int scope, const char* name) = 0;
      /** @brief End the soft queue requests
       * @param os stream to write to
       */
      virtual void report_soft_requested_queues_finished(std::ostream &os) = 0;
      /** @brief Begin the job dependencies as they were named at submission
       * @param os stream to write to
       */
      virtual void report_predecessors_requested_started(std::ostream &os) = 0;
      /** @brief Report one requested predecessor
       * @param os stream to write to
       * @param name the job, by name or number as it was given
       */
      virtual void report_predecessor_requested(std::ostream &os, const char* name) = 0;
      /** @brief End the requested predecessors
       * @param os stream to write to
       */
      virtual void report_predecessors_requested_finished(std::ostream &os) = 0;
      /** @brief Begin the jobs this one is still waiting for
       * @param os stream to write to
       */
      virtual void report_predecessors_started(std::ostream &os) = 0;
      /** @brief Report one job this one is still waiting for
       * @param os stream to write to
       * @param jid the job number
       */
      virtual void report_predecessor(std::ostream &os, uint32_t jid) = 0;
      /** @brief End the predecessors
       * @param os stream to write to
       */
      virtual void report_predecessors_finished(std::ostream &os) = 0;
      /** @brief Begin the array dependencies as they were named at submission
       * @param os stream to write to
       */
      virtual void report_ad_predecessors_requested_started(std::ostream &os) = 0;
      /** @brief Report one requested array predecessor
       * @param os stream to write to
       * @param name the job, by name or number as it was given
       */
      virtual void report_ad_predecessor_requested(std::ostream &os, const char* name) = 0;
      /** @brief End the requested array predecessors
       * @param os stream to write to
       */
      virtual void report_ad_predecessors_requested_finished(std::ostream &os) = 0;
      /** @brief Begin the array jobs whose tasks this one is waiting for
       * @param os stream to write to
       */
      virtual void report_ad_predecessors_started(std::ostream &os) = 0;
      /** @brief Report one array predecessor
       * @param os stream to write to
       * @param jid the job number
       */
      virtual void report_ad_predecessor(std::ostream &os, uint32_t jid) = 0;
      /** @brief End the array predecessors
       * @param os stream to write to
       */
      virtual void report_ad_predecessors_finished(std::ostream &os) = 0;
      /** @brief Begin the core binding of the job
       * @param os stream to write to
       */
      virtual void report_binding_started(std::ostream &os) = 0;
      /** @brief Report the binding as it was requested
       * @param os stream to write to
       * @param binding the binding as one string
       */
      virtual void report_binding(std::ostream &os, const char *binding) = 0;
      /** @brief Report a string valued part of the binding request
       * @param os stream to write to
       * @param name the attribute
       * @param value its value
       */
      virtual void report_binding_attribute(std::ostream &os, const char *name, const char *value) = 0;
      /** @brief Report a numeric part of the binding request
       * @param os stream to write to
       * @param name the attribute
       * @param value its value
       */
      virtual void report_binding_attribute(std::ostream &os, const char *name, uint32_t value) = 0;
      /** @brief End the core binding
       * @param os stream to write to
       */
      virtual void report_binding_finished(std::ostream &os) = 0;
      /** @brief End the current job
       * @param os stream to write to
       * @param jid the job number
       */
      virtual void report_job_finished(std::ostream &os, uint32_t jid) = 0;
      // endregion

   };
}

