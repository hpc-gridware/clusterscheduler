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
 * This code was generated from file source/libs/sgeobj/json/JAT.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Job Array Task: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/tom/ocs_tom_boundaries.h"

/**
 * @brief The field numbers of JAT, Job Array Task
 *
 * The enumeration is the type of every field number of JAT, so a parameter
 * of this type takes the field numbers of JAT only.
 */
enum JAT_Field {
   /**
    * @brief Task Number
    *
    * Unique task number assigned during task creation.
    */
   JAT_task_number = JAT_LOWERBOUND,
   /**
    * @brief Status
    *
    * First part of the state (see also JAT_hold, JAT_state).
    * @todo merge status, state and hold
    */
   JAT_status,
   /**
    * @brief Start Time
    *
    * Start time of the array task in microseconds since epoch.
    */
   JAT_start_time,
   /**
    * @brief End Time
    *
    * End time of the array task in microseconds since epoch.
    * Only used in sge_execd to simulate job runs.
    */
   JAT_end_time,
   /**
    * @brief Hold
    *
    * Second part of the state (user, operator, system hold)
    * @todo merge status, state and hold
    */
   JAT_hold,
   /**
    * @brief Granted PE
    *
    * Name of a granted parallel environment (in case of parallel jobs).
    */
   JAT_granted_pe,
   /**
    * @brief Job Restarted
    *
    * Was the task restarted (due to reschedule/migrate)?
    * @todo it is no boolean, but misused for other information!
    */
   JAT_job_restarted,
   /**
    * @brief Granted Destination Identifier List
    *
    * Granted destination identifier list (JG).
    * Has one element per queue instance the job is running on.
    */
   JAT_granted_destin_identifier_list,
   /**
    * @brief Granted Resources
    *
    * List of granted resources, currently these are granted RSMAPs only.
    */
   JAT_granted_resources_list,
   /**
    * @brief Master Queue
    *
    * Name of the master queue.
    * @todo redundant information, it is the name of the first element in the GDIL.
    */
   JAT_master_queue,
   /**
    * @brief State
    *
    * Third part of state (see also JAT_hold, JAT_status).
    * @todo merge status, state and hold
    */
   JAT_state,
   /**
    * @brief PVM Checkpoint Pid
    *
    * Pid of a previous, checkpointed run of the job/ja_task.
    * @todo was apparently required for checkpointing/restart of PVM jobs - still needed?
    */
   JAT_pvm_ckpt_pid,
   /**
    * @brief Pending Signal
    *
    * Pending signal (not yet acknowledged by execd).
    */
   JAT_pending_signal,
   /**
    * @brief Pending Signal Delivery Time
    *
    * Time when a signal shall be delivered by sge_qmaster to sge_execd (repeated) in µs since epoch.
    * Also used in sge_execd to repeat sending of signals.
    */
   JAT_pending_signal_delivery_time,
   /**
    * @brief Pid
    *
    * Pid of the sge_shepherd of a job (child of sge_execd). Used for signal delivery.
    */
   JAT_pid,
   /**
    * @brief OS Job Id
    *
    * Unique id which applies to all os processes started
    * on behalf of this task. Set during the startup phase of the
    * job. Meaning depends on the architecture of the
    * host were the task is started.
    * SOLARIS/LINUX: additional group id
    * Could be / was a OS supported job id on no longer supported platforms
    * like Cray, Nec, Irix.
    */
   JAT_osjobid,
   /**
    * @brief Systemd Scope
    *
    * In case a job is started in a systemd scope, this is the
    * scope unit name.
    * Only used in sge_execd, it is set during the startup phase of the
    * job.
    */
   JAT_systemd_scope,
   /**
    * @brief Systemd Slice
    *
    * In case of tightly integrated parallel jobs, the job's tasks (master and slave)
    * are started within a slice. We only store it if we have a tightly integrated parallel job,
    * and this slice needs to be deleted after the job is finished.
    * Only used in sge_execd, it is set during the startup phase of the
    * job.
    */
   JAT_systemd_slice,
   /**
    * @brief Usage Collection Mode
    *
    * Usage collection mode of a pe task.
    * One of 0 (FALSE), 1 (PDC), 2 (HYBRID), 3 (TRUE).
    */
   JAT_usage_collection,
   /**
    * @brief Usage List
    *
    * Raw usage from data collector. Scaled by sge_qmaster to JAT_scaled_usage_list.
    * Scheduling is using the scaled usage.
    */
   JAT_usage_list,
   /**
    * @brief Scaled Usage List
    *
    * Scaled usage set by qmaster, used by scheduler.
    * Usage scaling is defined in the exechost objects, see sge_host_conf.5
    */
   JAT_scaled_usage_list,
   /**
    * @brief Reported Usage List
    *
    * Usage which has already been reported in intermediate accounting records
    * in the reporting file. For long running jobs, written around midnight
    * to provide daily accounting information.
    */
   JAT_reported_usage_list,
   /**
    * @brief Functional Share
    *
    * Functional shares associated with the job.
    */
   JAT_fshare,
   /**
    * @brief Tickets
    *
    * Total number of tickets.
    */
   JAT_tix,
   /**
    * @brief Override Tickets
    *
    * Override tickets.
    */
   JAT_oticket,
   /**
    * @brief Functional Tickets
    *
    * Functional tickets.
    */
   JAT_fticket,
   /**
    * @brief Sharetree Tickets
    *
    * Sharetree tickets.
    */
   JAT_sticket,
   /**
    * @brief Share
    *
    * Job targeted proportion set by scheduler.
    */
   JAT_share,
   /**
    * @brief Suitable
    *
    * Tag used in filtering in client job output (qstat, qhost).
    */
   JAT_suitable,
   /**
    * @brief Task List
    *
    * List of running parallel tasks.
    */
   JAT_task_list,
   /**
    * @brief Finished Task List
    *
    * List of finished parallel tasks - contains only task names.
    */
   JAT_finished_task_list,
   /**
    * @brief Previous Usage List
    *
    * Copy of the scaled usage list created when a job gets rescheduled.
    * @todo If a job is rescheduled multiple times, should probably be aggregated.
    */
   JAT_previous_usage_list,
   /**
    * @brief PE Object
    *
    * PE object granted to this task (PE) when it was scheduled.
    */
   JAT_pe_object,
   /**
    * @brief Next PE Task Id
    *
    * Used locally in execd to store next pe task id for this jatask on this execd.
    */
   JAT_next_pe_task_id,
   /**
    * @brief Stop Initiate Time
    *
    * The time in µs since epoch when the action to initiate a job finish
    * (i.e. job termination, or checkpoint creation) was triggered or re-triggered.
    */
   JAT_stop_initiate_time,
   /**
    * @brief Priority
    *
    * The priority derived from weighted normalized tickets and weighted normalized
    * static urgency. Changes with task due to GEEE ticket dependency.
    */
   JAT_prio,
   /**
    * @brief Normalized Tickets
    *
    * Relative importance due to JAT_tix amount in the range between 0.0 and 1.0.
    */
   JAT_ntix,
   /**
    * @brief Wallclock Limit
    *
    * Wallclock_limit. Either requested (qsub -l h_rt=...) or the minimum of queue h_rt
    * in microseconds since epoch
    */
   JAT_wallclock_limit,
   /**
    * @brief Message List
    *
    * Task specific messages.
    * Currently only used for storing the reason why a job is in error state.
    */
   JAT_message_list,
   /**
    * @brief Deleted By
    *
    * Name of the user who deleted the array task (e.g. via qdel),
    * or the component which triggered the deletion (sge_qmaster, sge_execd).
    * Used for accounting and reporting purposes.
    */
   JAT_deleted_by,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   JAT_joker,
};

namespace ocs::tom {

   struct FPET;
   struct GRU;
   struct JG;
   struct PE;
   struct PET;
   struct QIM;
   struct UA;
   struct VA;

   /**
    * @brief Job Array Task
    *
    * JAT elements are a sub element of JB elements.
    * One element of each type is necessary to hold all data for the execution of one job.
    * One JB element and x JAT elements are needed to execute an array job with x tasks.
    *  
    *          -----------       1:x        ------------
    *          | JB |<---------------->| JAT |
    *          -----------                  ------------
    *  
    * The relation between these two elements is defined in the
    * 'JB_ja_tasks' sublist of a 'JB' element. This list will
    * contain all belonging JAT elements.
    *  
    * The 'JAT' element contains all attributes in which
    * one array task may differ from another array task of the
    * same array job. The 'JB' element defines all attributes
    * which are equivalent for all tasks of an array job.
    * A job and an array job with one task are equivalent
    * concerning their data structures. Both consist of one 'JB'
    * and one 'JAT' element.
    *  
    * 'JAT' elements contain dynamic data which accrue during the
    * execution of a job. Therefore it is not necessary to create
    * these elements during the submition of a (array) job but
    * after the job has been dispatched.
    */
   struct JAT {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JAT_task_number, "JAT_task_number", FieldType::Uint32, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::SpoolSublist},
         {JAT_status, "JAT_status", FieldType::Uint32, Flag::SpoolSublist},
         {JAT_start_time, "JAT_start_time", FieldType::Uint64, Flag::SpoolSublist},
         {JAT_end_time, "JAT_end_time", FieldType::Uint64, Flag::SpoolSublist},
         {JAT_hold, "JAT_hold", FieldType::Uint32, Flag::SpoolSublist},
         {JAT_granted_pe, "JAT_granted_pe", FieldType::String, Flag::SpoolSublist},
         {JAT_job_restarted, "JAT_job_restarted", FieldType::Uint32, Flag::SpoolSublist},
         {JAT_granted_destin_identifier_list, "JAT_granted_destin_identifier_list", FieldType::List, Flag::SpoolSublist},
         {JAT_granted_resources_list, "JAT_granted_resources_list", FieldType::List, Flag::Spool},
         {JAT_master_queue, "JAT_master_queue", FieldType::String, Flag::SpoolSublist},
         {JAT_state, "JAT_state", FieldType::Uint32, Flag::SpoolSublist},
         {JAT_pvm_ckpt_pid, "JAT_pvm_ckpt_pid", FieldType::Uint32, Flag::SpoolSublist},
         {JAT_pending_signal, "JAT_pending_signal", FieldType::Uint32, Flag::SpoolSublist},
         {JAT_pending_signal_delivery_time, "JAT_pending_signal_delivery_time", FieldType::Uint64, Flag::SpoolSublist},
         {JAT_pid, "JAT_pid", FieldType::Uint32, Flag::SpoolSublist},
         {JAT_osjobid, "JAT_osjobid", FieldType::String, Flag::SpoolSublist},
         {JAT_systemd_scope, "JAT_systemd_scope", FieldType::String, Flag::SpoolSublist},
         {JAT_systemd_slice, "JAT_systemd_slice", FieldType::String, Flag::SpoolSublist},
         {JAT_usage_collection, "JAT_usage_collection", FieldType::Uint32, Flag::None},
         {JAT_usage_list, "JAT_usage_list", FieldType::List, Flag::SpoolSublist},
         {JAT_scaled_usage_list, "JAT_scaled_usage_list", FieldType::List, Flag::SpoolSublist},
         {JAT_reported_usage_list, "JAT_reported_usage_list", FieldType::List, Flag::SpoolSublist},
         {JAT_fshare, "JAT_fshare", FieldType::Uint32, Flag::SpoolSublist},
         {JAT_tix, "JAT_tix", FieldType::Double, Flag::SpoolSublist},
         {JAT_oticket, "JAT_oticket", FieldType::Double, Flag::SpoolSublist},
         {JAT_fticket, "JAT_fticket", FieldType::Double, Flag::SpoolSublist},
         {JAT_sticket, "JAT_sticket", FieldType::Double, Flag::SpoolSublist},
         {JAT_share, "JAT_share", FieldType::Double, Flag::SpoolSublist},
         {JAT_suitable, "JAT_suitable", FieldType::Uint32, Flag::None},
         {JAT_task_list, "JAT_task_list", FieldType::List, Flag::SpoolSublist},
         {JAT_finished_task_list, "JAT_finished_task_list", FieldType::List, Flag::SpoolSublist},
         {JAT_previous_usage_list, "JAT_previous_usage_list", FieldType::List, Flag::None},
         {JAT_pe_object, "JAT_pe_object", FieldType::Object, Flag::SpoolSublist},
         {JAT_next_pe_task_id, "JAT_next_pe_task_id", FieldType::Uint32, Flag::None},
         {JAT_stop_initiate_time, "JAT_stop_initiate_time", FieldType::Uint64, Flag::SpoolSublist},
         {JAT_prio, "JAT_prio", FieldType::Double, Flag::None},
         {JAT_ntix, "JAT_ntix", FieldType::Double, Flag::None},
         {JAT_wallclock_limit, "JAT_wallclock_limit", FieldType::Uint64, Flag::Spool},
         {JAT_message_list, "JAT_message_list", FieldType::List, Flag::Spool},
         {JAT_deleted_by, "JAT_deleted_by", FieldType::String, Flag::SpoolSublist},
         {JAT_joker, "JAT_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JAT belongs to JAT */
   template <>
   struct TypeOf<JAT_Field> {
      using type = JAT;   ///< the object type
   };

   /** @brief JAT_granted_destin_identifier_list holds elements of JG */
   template <>
   struct SubType<JAT_granted_destin_identifier_list> {
      using type = JG;   ///< the sub-type
   };

   /** @brief JAT_granted_resources_list holds elements of GRU */
   template <>
   struct SubType<JAT_granted_resources_list> {
      using type = GRU;   ///< the sub-type
   };

   /** @brief JAT_usage_list holds elements of UA */
   template <>
   struct SubType<JAT_usage_list> {
      using type = UA;   ///< the sub-type
   };

   /** @brief JAT_scaled_usage_list holds elements of UA */
   template <>
   struct SubType<JAT_scaled_usage_list> {
      using type = UA;   ///< the sub-type
   };

   /** @brief JAT_reported_usage_list holds elements of UA */
   template <>
   struct SubType<JAT_reported_usage_list> {
      using type = UA;   ///< the sub-type
   };

   /** @brief JAT_task_list holds elements of PET */
   template <>
   struct SubType<JAT_task_list> {
      using type = PET;   ///< the sub-type
   };

   /** @brief JAT_finished_task_list holds elements of FPET */
   template <>
   struct SubType<JAT_finished_task_list> {
      using type = FPET;   ///< the sub-type
   };

   /** @brief JAT_previous_usage_list holds elements of UA */
   template <>
   struct SubType<JAT_previous_usage_list> {
      using type = UA;   ///< the sub-type
   };

   /** @brief JAT_pe_object holds elements of PE */
   template <>
   struct SubType<JAT_pe_object> {
      using type = PE;   ///< the sub-type
   };

   /** @brief JAT_message_list holds elements of QIM */
   template <>
   struct SubType<JAT_message_list> {
      using type = QIM;   ///< the sub-type
   };

   /** @brief JAT_joker holds elements of VA */
   template <>
   struct SubType<JAT_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom
