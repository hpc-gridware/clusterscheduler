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
 * This code was generated from file source/libs/sgeobj/json/CQ.json
 * DO NOT CHANGE
 */

/** @file
 * @brief Cluster Queue: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of CQ, Cluster Queue
 *
 * The enumeration is the type of every field number of CQ, so a parameter
 * of this type takes the field numbers of CQ only.
 */
enum CQ_Field {
   /**
    * @brief Name
    *
    * The name of the cluster queue
    */
   CQ_name = CQ_LOWERBOUND,
   /**
    * @brief Host List
    *
    * The list of hosts / host groups on which queue instances for the cluster queue shall be created.
    */
   CQ_hostlist,
   /**
    * @brief Queue Instances
    *
    * List of queue instances of the cluster queue.
    */
   CQ_qinstances,
   /**
    * @brief Sequence Number
    *
    * Queue sequence number, used together with queue_sort_method seqno.
    */
   CQ_seq_no,
   /**
    * @brief NSuspend
    *
    * Number of jobs to suspend on suspend_threshold per time interval.
    */
   CQ_nsuspend,
   /**
    * @brief Job Slots
    *
    * Number of slots per queue instance.
    */
   CQ_job_slots,
   /**
    * @brief Rerun
    *
    * Are jobs running in the queue re-runnable?
    */
   CQ_rerun,
   /**
    * @brief Soft Filesize Limit
    *
    * Soft filesize limit.
    */
   CQ_s_fsize,
   /**
    * @brief Hard Filesize Limit
    *
    * Hard filesize limit.
    */
   CQ_h_fsize,
   /**
    * @brief Soft Data Limit
    *
    * Soft data limit.
    */
   CQ_s_data,
   /**
    * @brief Hard Data Limit
    *
    * Hard data limit.
    */
   CQ_h_data,
   /**
    * @brief Soft Stack Limit
    *
    * Soft stack size limit.
    */
   CQ_s_stack,
   /**
    * @brief Hard Stack Limit
    *
    * Hard stack size limit.
    */
   CQ_h_stack,
   /**
    * @brief Soft Core Limit
    *
    * Soft core size limit.
    */
   CQ_s_core,
   /**
    * @brief Hard Core Limit
    *
    * Hard core size limit.
    */
   CQ_h_core,
   /**
    * @brief Soft RSS Limit
    *
    * Soft RSS limit.
    */
   CQ_s_rss,
   /**
    * @brief Hard RSS Limit
    *
    * Hard RSS limit.
    */
   CQ_h_rss,
   /**
    * @brief Soft VMEM Limit
    *
    * Soft VMEM limit.
    */
   CQ_s_vmem,
   /**
    * @brief Hard VMEM Limit
    *
    * Hard VMEM limit.
    */
   CQ_h_vmem,
   /**
    * @brief Soft Runtime Limit
    *
    * Soft runtime limit.
    */
   CQ_s_rt,
   /**
    * @brief Hard Runtime Limit
    *
    * Hard runtime limit.
    */
   CQ_h_rt,
   /**
    * @brief Soft CPU Limit
    *
    * Soft CPU time limit.
    */
   CQ_s_cpu,
   /**
    * @brief Hard CPU Limit
    *
    * Hard CPU time limit.
    */
   CQ_h_cpu,
   /**
    * @brief Suspend Interval
    *
    * Once every interval nsuspend jobs can be suspended by suspend_threshold.
    */
   CQ_suspend_interval,
   /**
    * @brief Min CPU Interval
    *
    * Time between two automatic checkpoints.
    */
   CQ_min_cpu_interval,
   /**
    * @brief Notify
    *
    * Notify jobs before signalling them?
    */
   CQ_notify,
   /**
    * @brief Tmp Directory
    *
    * Toplevel tmp directory holding per job tmp directories on the execution side.
    */
   CQ_tmpdir,
   /**
    * @brief Shell
    *
    * Default shell used when starting jobs.
    */
   CQ_shell,
   /**
    * @brief Calendar
    *
    * Queue calendar.
    */
   CQ_calendar,
   /**
    * @brief Priority
    *
    * Nice value applied to jobs running in the queue.
    */
   CQ_priority,
   /**
    * @brief Processors
    *
    * Solaris processor set
    * @todo is it still used at all? We now have hwloc on Solaris as well.
    */
   CQ_processors,
   /**
    * @brief Prolog
    *
    * Prolog script started before a job.
    */
   CQ_prolog,
   /**
    * @brief Epilog
    *
    * Epilog script started after a job.
    */
   CQ_epilog,
   /**
    * @brief Shell Start Mode
    *
    * How to start jobs (unix_behavior or posix_compliant, see sge_queue_conf.5 man page.
    */
   CQ_shell_start_mode,
   /**
    * @brief Starter Method
    *
    * Job starter script.
    */
   CQ_starter_method,
   /**
    * @brief Suspend Method
    *
    * Signal to be sent to suspend a job or path to a script to be executed when a job shall be suspended.
    */
   CQ_suspend_method,
   /**
    * @brief Resume Method
    *
    * Signal to be sent to resume a job or path to a script to be executed when a job shall be resumed.
    */
   CQ_resume_method,
   /**
    * @brief Terminate Method
    *
    * Signal to be sent to terminate a job or path to a script to be executed when a job shall be terminated.
    */
   CQ_terminate_method,
   /**
    * @brief Initial State
    *
    * Initial state of a newly created queue instance (default, enabled, disabled).
    */
   CQ_initial_state,
   /**
    * @brief PE List
    *
    * List of parallel environments available for a queue instance.
    */
   CQ_pe_list,
   /**
    * @brief Checkpoint List
    *
    * List of  checkpointing environments available for a queue instance.
    */
   CQ_ckpt_list,
   /**
    * @brief Owner List
    *
    * List of queue owners (has administrative rights like disable/enable, suspend/resume.
    */
   CQ_owner_list,
   /**
    * @brief Access List
    *
    * ACL defining whose jobs may run in the queue.
    */
   CQ_acl,
   /**
    * @brief X Access List
    *
    * ACL defining whose jobs may not run in the queue.
    */
   CQ_xacl,
   /**
    * @brief Projects
    *
    * List of projects whose jobs may run in the queue.
    */
   CQ_projects,
   /**
    * @brief X Projects
    *
    * List of projects whose jobs may not run in the queue.
    */
   CQ_xprojects,
   /**
    * @brief Load Thresholds
    *
    * List of load thresholds which will disable queue instances when exceeded.
    */
   CQ_load_thresholds,
   /**
    * @brief Suspend Thresholds
    *
    * List of load thresholds which will suspend queue instances (jobs running in the qinstance) when exceeded.
    */
   CQ_suspend_thresholds,
   /**
    * @brief Consumable Config List
    *
    * Capacities of consumable resources.
    */
   CQ_consumable_config_list,
   /**
    * @brief Subordinate List
    *
    * List of subordinate queues.
    */
   CQ_subordinate_list,
   /**
    * @brief Queue Type
    *
    * Queue type (batch or interactive).
    */
   CQ_qtype,
   /**
    * @brief Tag
    *
    * Queue tag used in scheduler thread to mark certain queues.
    */
   CQ_tag,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of VA type and it is spooled.
    */
   CQ_joker,
};

namespace ocs::tom {

   struct HR;
   struct QU;
   struct VA;

   /**
    * @brief Cluster Queue
    *
    * An object of this type defines a cluster queue.
    * A cluster queue defines the attributes of queue instances (cluster queue on host)
    * It contains attribute lists per attribute, which are used to define the value of an attribute
    * per host and/or per host group
    * @see man page sge_queue_conf.5
    * @todo the attribute lists (starting below with seq_no) have a certain type, e.g. AULNG_Type, but
    *       but their SubClassName and SubCullPrefix are all ANY.
    */
   struct CQ {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {CQ_name, "CQ_name", FieldType::String, Flag::PrimaryKey | Flag::Unique | Flag::Hash | Flag::Spool},
         {CQ_hostlist, "CQ_hostlist", FieldType::List, Flag::None},
         {CQ_qinstances, "CQ_qinstances", FieldType::List, Flag::None},
         {CQ_seq_no, "CQ_seq_no", FieldType::List, Flag::Spool},
         {CQ_nsuspend, "CQ_nsuspend", FieldType::List, Flag::Spool},
         {CQ_job_slots, "CQ_job_slots", FieldType::List, Flag::Spool},
         {CQ_rerun, "CQ_rerun", FieldType::List, Flag::Spool},
         {CQ_s_fsize, "CQ_s_fsize", FieldType::List, Flag::Spool},
         {CQ_h_fsize, "CQ_h_fsize", FieldType::List, Flag::Spool},
         {CQ_s_data, "CQ_s_data", FieldType::List, Flag::Spool},
         {CQ_h_data, "CQ_h_data", FieldType::List, Flag::Spool},
         {CQ_s_stack, "CQ_s_stack", FieldType::List, Flag::Spool},
         {CQ_h_stack, "CQ_h_stack", FieldType::List, Flag::Spool},
         {CQ_s_core, "CQ_s_core", FieldType::List, Flag::Spool},
         {CQ_h_core, "CQ_h_core", FieldType::List, Flag::Spool},
         {CQ_s_rss, "CQ_s_rss", FieldType::List, Flag::Spool},
         {CQ_h_rss, "CQ_h_rss", FieldType::List, Flag::Spool},
         {CQ_s_vmem, "CQ_s_vmem", FieldType::List, Flag::Spool},
         {CQ_h_vmem, "CQ_h_vmem", FieldType::List, Flag::Spool},
         {CQ_s_rt, "CQ_s_rt", FieldType::List, Flag::Spool},
         {CQ_h_rt, "CQ_h_rt", FieldType::List, Flag::Spool},
         {CQ_s_cpu, "CQ_s_cpu", FieldType::List, Flag::Spool},
         {CQ_h_cpu, "CQ_h_cpu", FieldType::List, Flag::Spool},
         {CQ_suspend_interval, "CQ_suspend_interval", FieldType::List, Flag::Spool},
         {CQ_min_cpu_interval, "CQ_min_cpu_interval", FieldType::List, Flag::Spool},
         {CQ_notify, "CQ_notify", FieldType::List, Flag::Spool},
         {CQ_tmpdir, "CQ_tmpdir", FieldType::List, Flag::Spool},
         {CQ_shell, "CQ_shell", FieldType::List, Flag::Spool},
         {CQ_calendar, "CQ_calendar", FieldType::List, Flag::Spool},
         {CQ_priority, "CQ_priority", FieldType::List, Flag::Spool},
         {CQ_processors, "CQ_processors", FieldType::List, Flag::Spool},
         {CQ_prolog, "CQ_prolog", FieldType::List, Flag::Spool},
         {CQ_epilog, "CQ_epilog", FieldType::List, Flag::Spool},
         {CQ_shell_start_mode, "CQ_shell_start_mode", FieldType::List, Flag::Spool},
         {CQ_starter_method, "CQ_starter_method", FieldType::List, Flag::Spool},
         {CQ_suspend_method, "CQ_suspend_method", FieldType::List, Flag::Spool},
         {CQ_resume_method, "CQ_resume_method", FieldType::List, Flag::Spool},
         {CQ_terminate_method, "CQ_terminate_method", FieldType::List, Flag::Spool},
         {CQ_initial_state, "CQ_initial_state", FieldType::List, Flag::Spool},
         {CQ_pe_list, "CQ_pe_list", FieldType::List, Flag::Spool},
         {CQ_ckpt_list, "CQ_ckpt_list", FieldType::List, Flag::Spool},
         {CQ_owner_list, "CQ_owner_list", FieldType::List, Flag::Spool},
         {CQ_acl, "CQ_acl", FieldType::List, Flag::Spool},
         {CQ_xacl, "CQ_xacl", FieldType::List, Flag::Spool},
         {CQ_projects, "CQ_projects", FieldType::List, Flag::Spool},
         {CQ_xprojects, "CQ_xprojects", FieldType::List, Flag::Spool},
         {CQ_load_thresholds, "CQ_load_thresholds", FieldType::List, Flag::Spool},
         {CQ_suspend_thresholds, "CQ_suspend_thresholds", FieldType::List, Flag::Spool},
         {CQ_consumable_config_list, "CQ_consumable_config_list", FieldType::List, Flag::Spool},
         {CQ_subordinate_list, "CQ_subordinate_list", FieldType::List, Flag::Spool},
         {CQ_qtype, "CQ_qtype", FieldType::List, Flag::Spool},
         {CQ_tag, "CQ_tag", FieldType::Uint32, Flag::None},
         {CQ_joker, "CQ_joker", FieldType::List, Flag::Spool},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of CQ belongs to CQ */
   template <>
   struct TypeOf<CQ_Field> {
      using type = CQ;   ///< the object type
   };

   /** @brief CQ_hostlist holds elements of HR */
   template <>
   struct SubType<CQ_hostlist> {
      using type = HR;   ///< the sub-type
   };

   /** @brief CQ_qinstances holds elements of QU */
   template <>
   struct SubType<CQ_qinstances> {
      using type = QU;   ///< the sub-type
   };

   /** @brief CQ_joker holds elements of VA */
   template <>
   struct SubType<CQ_joker> {
      using type = VA;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of CQ for the list library: a synonym for ocs::tom::CQ::descriptor */
inline constexpr const lDescr *CQ_Type = ocs::tom::CQ::descriptor.e;
