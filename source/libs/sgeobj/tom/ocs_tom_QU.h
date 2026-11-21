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

/*
 * This code was generated from file source/libs/sgeobj/json/QU.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Queue Instance: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of QU, Queue Instance
 *
 * The enumeration is the type of every field number of QU, so a parameter
 * of this type takes the field numbers of QU only.
 */
enum QU_Field {
   /**
    * @brief Execution Host
    *
    * The host this queue instance runs on. Together with QU_qname it identifies the instance.
    */
   QU_qhostname = QU_LOWERBOUND,
   /**
    * @brief cluster queue name
    *
    * @todo add description
    */
   QU_qname,
   /**
    * @brief Full Name
    *
    * The instance name as users see it, "cluster_queue@host". Derived from QU_qname and QU_qhostname; do not set it directly.
    */
   QU_full_name,
   /**
    * @brief Modification Tag
    *
    * Scratch marker used while a cluster queue change is applied, holding SGE_QI_TAG_ADD, _MOD or _DEL for this instance. Not persistent.
    */
   QU_tag,
   /**
    * @brief Available At
    *
    * Earliest time this instance could run the job being scheduled, set during resource matching and used to order the candidates.
    */
   QU_available_at,
   /**
    * @brief Sequence Number
    *
    * Position of this queue in the scheduling order, used when the scheduler is configured to sort queues by sequence number rather than by load.
    */
   QU_seq_no,
   /**
    * @brief Suspend Count
    *
    * How many jobs are suspended at once when a suspend threshold is exceeded, and resumed at once when it falls back.
    */
   QU_nsuspend,
   /**
    * @brief Queue Type
    *
    * Whether the queue accepts batch jobs, interactive jobs, or both.
    */
   QU_qtype,
   /**
    * @brief Slots
    *
    * How many jobs may run in this instance at once. The scheduler compares it against the slots already used.
    */
   QU_job_slots,
   /**
    * @brief Temporary Directory
    *
    * Where the execution daemon creates the per-job temporary directory it advertises as $TMPDIR.
    */
   QU_tmpdir,
   /**
    * @brief Login Shell
    *
    * The shell a job script is started with, when the start mode calls for one.
    */
   QU_shell,
   /**
    * @brief Calendar
    *
    * The calendar that enables, disables or suspends this queue by time of day and day of week.
    */
   QU_calendar,
   /**
    * @brief Nice Value
    *
    * The nice value jobs in this queue are started with.
    */
   QU_priority,
   /**
    * @brief Processor Set
    *
    * The processor set jobs are bound to, on the platforms that support one.
    */
   QU_processors,
   /**
    * @brief Prolog
    *
    * A command run as a preamble before the job, in the job's environment. A non-zero exit puts the queue into error state.
    */
   QU_prolog,
   /**
    * @brief Epilog
    *
    * A command run after the job, in the job's environment, whatever the job's own exit status was.
    */
   QU_epilog,
   /**
    * @brief Shell Start Mode
    *
    * How the job script is handed to the shell: as a script argument, on stdin, or by exec.
    */
   QU_shell_start_mode,
   /**
    * @brief Starter Method
    *
    * Replaces the default way of starting the job, for jobs that need a wrapper.
    */
   QU_starter_method,
   /**
    * @brief Suspend Method
    *
    * Replaces the default SIGSTOP when the job is suspended.
    */
   QU_suspend_method,
   /**
    * @brief Resume Method
    *
    * Replaces the default SIGCONT when the job is resumed.
    */
   QU_resume_method,
   /**
    * @brief Terminate Method
    *
    * Replaces the default SIGKILL when the job is deleted.
    */
   QU_terminate_method,
   /**
    * @brief Initial State
    *
    * The state the instance takes when it is first created or when its execution daemon restarts: enabled, disabled, or whatever it was.
    */
   QU_initial_state,
   /**
    * @brief Rerunnable
    *
    * Default for jobs whose execution host crashed: rerun them elsewhere, or report them as failed. A job may override it with qsub -r.
    */
   QU_rerun,
   /**
    * @brief Soft Real Time Limit
    *
    * Wall clock time after which the job is warned with SIGUSR1 and then killed once QU_notify has passed.
    */
   QU_s_rt,
   /**
    * @brief Hard Real Time Limit
    *
    * Wall clock time after which the job is killed outright.
    */
   QU_h_rt,
   /**
    * @brief Soft CPU Limit
    *
    * Combined CPU time of all the job's processes after which it is sent SIGXCPU.
    */
   QU_s_cpu,
   /**
    * @brief Hard CPU Limit
    *
    * Combined CPU time after which the job is killed.
    */
   QU_h_cpu,
   /**
    * @brief Soft File Size Limit
    *
    * Largest file the job may create before it is warned.
    */
   QU_s_fsize,
   /**
    * @brief Hard File Size Limit
    *
    * Largest file the job may create.
    */
   QU_h_fsize,
   /**
    * @brief Soft Data Segment Limit
    *
    * Data segment size after which the job is warned.
    */
   QU_s_data,
   /**
    * @brief Hard Data Segment Limit
    *
    * Largest data segment the job may allocate.
    */
   QU_h_data,
   /**
    * @brief Soft Stack Limit
    *
    * Stack size after which the job is warned.
    */
   QU_s_stack,
   /**
    * @brief Hard Stack Limit
    *
    * Largest stack the job may use.
    */
   QU_h_stack,
   /**
    * @brief Soft Core File Limit
    *
    * Core file size after which the job is warned.
    */
   QU_s_core,
   /**
    * @brief Hard Core File Limit
    *
    * Largest core file the job may write.
    */
   QU_h_core,
   /**
    * @brief Soft Resident Set Limit
    *
    * Resident set size after which the job is warned.
    */
   QU_s_rss,
   /**
    * @brief Hard Resident Set Limit
    *
    * Largest resident set the job may hold.
    */
   QU_h_rss,
   /**
    * @brief Soft Virtual Memory Limit
    *
    * Virtual memory after which the job is warned. For a parallel job the limit applies per slot on the host.
    */
   QU_s_vmem,
   /**
    * @brief Hard Virtual Memory Limit
    *
    * Virtual memory after which the job is killed. For a parallel job the limit applies per slot on the host.
    */
   QU_h_vmem,
   /**
    * @brief Suspend Interval
    *
    * How long to wait between two suspend rounds, so a threshold is not acted on faster than the load can react.
    */
   QU_suspend_interval,
   /**
    * @brief Minimum Checkpoint Interval
    *
    * Shortest time between two automatic checkpoints, whatever shorter interval the checkpointing environment or the job asks for.
    */
   QU_min_cpu_interval,
   /**
    * @brief Notify Time
    *
    * How long to wait between the warning signal and the real suspend or kill signal, for jobs submitted with qsub -notify.
    */
   QU_notify,
   /**
    * @brief Checkpointing Environments
    *
    * The checkpointing environments a job may request here.
    */
   QU_ckpt_list,
   /**
    * @brief Parallel Environments
    *
    * The parallel environments a job may request here.
    */
   QU_pe_list,
   /**
    * @brief Owners
    *
    * Users allowed to suspend, resume and disable this queue without being managers or operators.
    */
   QU_owner_list,
   /**
    * @brief Access List
    *
    * The usersets allowed to run jobs here. Empty means everyone who is not in QU_xacl.
    */
   QU_acl,
   /**
    * @brief Excluded Access List
    *
    * The usersets not allowed to run jobs here. Checked before QU_acl.
    */
   QU_xacl,
   /**
    * @brief Projects
    *
    * The projects whose jobs may run here.
    */
   QU_projects,
   /**
    * @brief Excluded Projects
    *
    * The projects whose jobs may not run here.
    */
   QU_xprojects,
   /**
    * @brief Configured Consumables
    *
    * The consumable capacities of this instance, as configured. QU_resource_utilization tracks what is actually booked against them.
    */
   QU_consumable_config_list,
   /**
    * @brief Load Thresholds
    *
    * Load values above which the queue stops accepting jobs. Reaching one puts the instance into alarm state (the "a" in qstat).
    */
   QU_load_thresholds,
   /**
    * @brief Suspend Thresholds
    *
    * Load values above which jobs already running here are suspended, QU_nsuspend at a time.
    */
   QU_suspend_thresholds,
   /**
    * @brief Subordinate Queues
    *
    * Queues to suspend while this one is busy, either wholesale or per slot. What implements the "S" state on the other queue.
    */
   QU_subordinate_list,
   /**
    * @brief State Bits
    *
    * The QI_* state bits, and the letters qstat prints for them. Eleven independent flags; see sge_qinstance_state.cc.
    */
   QU_state,
   /**
    * @brief Queue Instance Tagged for Schedule
    *
    * used for parallel job scheduling only
    * 0 - can be used only as slave queue
    * 1 - can be used as slave queue for now assignment,
    *     however as master for reservation
    * 2 - can be used as master for now and reservation
    */
   QU_tagged4schedule,
   /**
    * @brief Unused
    *
    * @warning Read and written nowhere in either repository. Kept because removing an attribute changes the spooled format.
    */
   QU_cache_version,
   /**
    * @brief Pending Signal
    *
    * A signal sent to the queue that has not taken effect yet. Kept until the state changes, so it can be repeated.
    */
   QU_pending_signal,
   /**
    * @brief Pending Signal Retry Time
    *
    * When to send QU_pending_signal again. Spooled, so a qmaster restart does not lose an in-flight signal.
    */
   QU_pending_signal_delivery_time,
   /**
    * @brief Queue Version
    *
    * Incremented by qinstance_increase_qversion() on every change. A scheduler order carries the version it was computed against, and sge_follow() rejects the order if the queue has moved on.
    */
   QU_version,
   /**
    * @brief Subordinate Suspend Count
    *
    * How many superordinate queues currently demand that this one be suspended. A count, not a flag, so releasing one does not resume a queue another still holds down.
    */
   QU_suspended_on_subordinate,
   /**
    * @brief Last Suspend Threshold Check
    *
    * When the suspend thresholds were last evaluated, so they are not acted on more often than QU_suspend_interval.
    */
   QU_last_suspend_threshold_ckeck,
   /**
    * @brief Unused
    *
    * @warning Read and written nowhere in either repository. Kept because removing an attribute changes the spooled format.
    */
   QU_job_cnt,
   /**
    * @brief Unused
    *
    * @warning Read and written nowhere in either repository. Kept because removing an attribute changes the spooled format.
    */
   QU_pending_job_cnt,
   /**
    * @brief Soft Request Violations
    *
    * How many of the job's soft requests this instance cannot satisfy. The scheduler sorts candidates by this ascending, so a queue that grants more soft requests wins.
    */
   QU_soft_violation,
   /**
    * @brief Host Sequence Number
    *
    * The host's position in the scheduler's host sort order, copied onto the instance while tagging so queues on a better host sort first.
    */
   QU_host_seq_no,
   /**
    * @brief Resource Utilization
    *
    * What is actually booked against each consumable over time, including future bookings made by advance reservations. The counterpart of QU_consumable_config_list.
    */
   QU_resource_utilization,
   /**
    * @brief Messages
    *
    * Messages explaining the instance's state, shown by qstat -explain and qrstat.
    */
   QU_message_list,
   /**
    * @brief Deferred GDI Change
    *
    * Marks the instance as having a change that is applied after the current request rather than during it.
    */
   QU_gdi_do_later,
   /**
    * @brief New State Variables
    *
    * new state variables, these are needed for the scheduler, to dispach jobs into suitable queues
    */
   QU_state_changes,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   QU_joker,
};

namespace ocs::tom {

   struct CE;
   struct CQU;
   struct PR;
   struct QIM;
   struct RUE;
   struct SO;
   struct ST;
   struct US;
   struct VA;

   /**
    * @brief Queue Instance
    *
    * Queue Instance: Instance of a cluster queue on an execution host
    */
   struct QU {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {QU_qhostname, "QU_qhostname", FieldType::Host, Flag::Hash | Flag::Spool},
         {QU_qname, "QU_qname", FieldType::String, Flag::Hash | Flag::Spool},
         {QU_full_name, "QU_full_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash},
         {QU_tag, "QU_tag", FieldType::Uint32, Flag::None},
         {QU_available_at, "QU_available_at", FieldType::Uint64, Flag::None},
         {QU_seq_no, "QU_seq_no", FieldType::Uint32, Flag::Configure},
         {QU_nsuspend, "QU_nsuspend", FieldType::Uint32, Flag::Configure},
         {QU_qtype, "QU_qtype", FieldType::Uint32, Flag::Configure},
         {QU_job_slots, "QU_job_slots", FieldType::Uint32, Flag::Configure},
         {QU_tmpdir, "QU_tmpdir", FieldType::String, Flag::Configure},
         {QU_shell, "QU_shell", FieldType::String, Flag::Configure},
         {QU_calendar, "QU_calendar", FieldType::String, Flag::Configure},
         {QU_priority, "QU_priority", FieldType::String, Flag::Configure},
         {QU_processors, "QU_processors", FieldType::String, Flag::Configure},
         {QU_prolog, "QU_prolog", FieldType::String, Flag::Configure},
         {QU_epilog, "QU_epilog", FieldType::String, Flag::Configure},
         {QU_shell_start_mode, "QU_shell_start_mode", FieldType::String, Flag::Configure},
         {QU_starter_method, "QU_starter_method", FieldType::String, Flag::Configure},
         {QU_suspend_method, "QU_suspend_method", FieldType::String, Flag::Configure},
         {QU_resume_method, "QU_resume_method", FieldType::String, Flag::Configure},
         {QU_terminate_method, "QU_terminate_method", FieldType::String, Flag::Configure},
         {QU_initial_state, "QU_initial_state", FieldType::String, Flag::Configure},
         {QU_rerun, "QU_rerun", FieldType::Bool, Flag::Configure},
         {QU_s_rt, "QU_s_rt", FieldType::String, Flag::Configure},
         {QU_h_rt, "QU_h_rt", FieldType::String, Flag::Configure},
         {QU_s_cpu, "QU_s_cpu", FieldType::String, Flag::Configure},
         {QU_h_cpu, "QU_h_cpu", FieldType::String, Flag::Configure},
         {QU_s_fsize, "QU_s_fsize", FieldType::String, Flag::Configure},
         {QU_h_fsize, "QU_h_fsize", FieldType::String, Flag::Configure},
         {QU_s_data, "QU_s_data", FieldType::String, Flag::Configure},
         {QU_h_data, "QU_h_data", FieldType::String, Flag::Configure},
         {QU_s_stack, "QU_s_stack", FieldType::String, Flag::Configure},
         {QU_h_stack, "QU_h_stack", FieldType::String, Flag::Configure},
         {QU_s_core, "QU_s_core", FieldType::String, Flag::Configure},
         {QU_h_core, "QU_h_core", FieldType::String, Flag::Configure},
         {QU_s_rss, "QU_s_rss", FieldType::String, Flag::Configure},
         {QU_h_rss, "QU_h_rss", FieldType::String, Flag::Configure},
         {QU_s_vmem, "QU_s_vmem", FieldType::String, Flag::Configure},
         {QU_h_vmem, "QU_h_vmem", FieldType::String, Flag::Configure},
         {QU_suspend_interval, "QU_suspend_interval", FieldType::String, Flag::Configure},
         {QU_min_cpu_interval, "QU_min_cpu_interval", FieldType::String, Flag::Configure},
         {QU_notify, "QU_notify", FieldType::String, Flag::Configure},
         {QU_ckpt_list, "QU_ckpt_list", FieldType::List, Flag::Configure},
         {QU_pe_list, "QU_pe_list", FieldType::List, Flag::Configure},
         {QU_owner_list, "QU_owner_list", FieldType::List, Flag::Configure},
         {QU_acl, "QU_acl", FieldType::List, Flag::Configure},
         {QU_xacl, "QU_xacl", FieldType::List, Flag::Configure},
         {QU_projects, "QU_projects", FieldType::List, Flag::Configure},
         {QU_xprojects, "QU_xprojects", FieldType::List, Flag::Configure},
         {QU_consumable_config_list, "QU_consumable_config_list", FieldType::List, Flag::Configure},
         {QU_load_thresholds, "QU_load_thresholds", FieldType::List, Flag::Configure},
         {QU_suspend_thresholds, "QU_suspend_thresholds", FieldType::List, Flag::Configure},
         {QU_subordinate_list, "QU_subordinate_list", FieldType::List, Flag::Configure},
         {QU_state, "QU_state", FieldType::Uint32, Flag::Spool},
         {QU_tagged4schedule, "QU_tagged4schedule", FieldType::Uint32, Flag::None},
         {QU_cache_version, "QU_cache_version", FieldType::Uint32, Flag::None},
         {QU_pending_signal, "QU_pending_signal", FieldType::Uint32, Flag::Spool},
         {QU_pending_signal_delivery_time, "QU_pending_signal_delivery_time", FieldType::Uint64, Flag::Spool},
         {QU_version, "QU_version", FieldType::Uint32, Flag::Spool},
         {QU_suspended_on_subordinate, "QU_suspended_on_subordinate", FieldType::Uint32, Flag::None},
         {QU_last_suspend_threshold_ckeck, "QU_last_suspend_threshold_ckeck", FieldType::Uint64, Flag::None},
         {QU_job_cnt, "QU_job_cnt", FieldType::Uint32, Flag::None},
         {QU_pending_job_cnt, "QU_pending_job_cnt", FieldType::Uint32, Flag::None},
         {QU_soft_violation, "QU_soft_violation", FieldType::Uint32, Flag::None},
         {QU_host_seq_no, "QU_host_seq_no", FieldType::Uint32, Flag::None},
         {QU_resource_utilization, "QU_resource_utilization", FieldType::List, Flag::None},
         {QU_message_list, "QU_message_list", FieldType::List, Flag::Spool},
         {QU_gdi_do_later, "QU_gdi_do_later", FieldType::Uint32, Flag::None},
         {QU_state_changes, "QU_state_changes", FieldType::List, Flag::None},
         {QU_joker, "QU_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of QU belongs to QU */
   template <>
   struct TypeOf<QU_Field> {
      using type = QU;   ///< the object type
   };

   /** @brief QU_ckpt_list holds elements of ST */
   template <>
   struct SubType<QU_ckpt_list> {
      using type = ST;   ///< the sub-type
   };

   /** @brief QU_pe_list holds elements of ST */
   template <>
   struct SubType<QU_pe_list> {
      using type = ST;   ///< the sub-type
   };

   /** @brief QU_owner_list holds elements of US */
   template <>
   struct SubType<QU_owner_list> {
      using type = US;   ///< the sub-type
   };

   /** @brief QU_acl holds elements of US */
   template <>
   struct SubType<QU_acl> {
      using type = US;   ///< the sub-type
   };

   /** @brief QU_xacl holds elements of US */
   template <>
   struct SubType<QU_xacl> {
      using type = US;   ///< the sub-type
   };

   /** @brief QU_projects holds elements of PR */
   template <>
   struct SubType<QU_projects> {
      using type = PR;   ///< the sub-type
   };

   /** @brief QU_xprojects holds elements of PR */
   template <>
   struct SubType<QU_xprojects> {
      using type = PR;   ///< the sub-type
   };

   /** @brief QU_consumable_config_list holds elements of CE */
   template <>
   struct SubType<QU_consumable_config_list> {
      using type = CE;   ///< the sub-type
   };

   /** @brief QU_load_thresholds holds elements of CE */
   template <>
   struct SubType<QU_load_thresholds> {
      using type = CE;   ///< the sub-type
   };

   /** @brief QU_suspend_thresholds holds elements of CE */
   template <>
   struct SubType<QU_suspend_thresholds> {
      using type = CE;   ///< the sub-type
   };

   /** @brief QU_subordinate_list holds elements of SO */
   template <>
   struct SubType<QU_subordinate_list> {
      using type = SO;   ///< the sub-type
   };

   /** @brief QU_resource_utilization holds elements of RUE */
   template <>
   struct SubType<QU_resource_utilization> {
      using type = RUE;   ///< the sub-type
   };

   /** @brief QU_message_list holds elements of QIM */
   template <>
   struct SubType<QU_message_list> {
      using type = QIM;   ///< the sub-type
   };

   /** @brief QU_state_changes holds elements of CQU */
   template <>
   struct SubType<QU_state_changes> {
      using type = CQU;   ///< the sub-type
   };

   /** @brief QU_joker holds elements of VA */
   template <>
   struct SubType<QU_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
