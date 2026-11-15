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
 * This code was generated from file source/libs/sgeobj/json/JB.json
 * DO NOT CHANGE
 */

/** @file
 * @brief job type: object type of the typed object model
 */

#include "tom/ocs_FieldTraits.h"
#include "sgeobj/cull/sge_boundaries.h"

/**
 * @brief The field numbers of JB, job type
 *
 * The enumeration is the type of every field number of JB, so a parameter
 * of this type takes the field numbers of JB only.
 */
enum JB_Field {
   /**
    * @brief Unique Job Number
    *
    * holds values in the range of 1..std::numeric_limits<uint32_t>::max()
    */
   JB_job_number = JB_LOWERBOUND,
   /**
    * @brief Job Name
    *
    * (qsub/qalter -N job_name)
    */
   JB_job_name,
   /**
    * @brief Job Version Number
    *
    * will be in increased when job is modified
    */
   JB_version,
   /**
    * @brief Job Requested Dependencies
    *
    * Dependencies as requested via qsub -hold_jid.
    * Can be job ids or job names, including the use of wildcards. (JRE_Type only JRE_job_name)
    */
   JB_jid_request_list,
   /**
    * @brief Predecessor Jobs
    *
    * list of job numbers of predecessor jobs (JRE_Type only JRE_job_name)
    */
   JB_jid_predecessor_list,
   /**
    * @brief Successor Jobs
    *
    * list of job numbers of successor jobs (JRE_Type only JRE_job_name)
    */
   JB_jid_successor_list,
   /**
    * @brief Job Requested Array Dependencies
    *
    * Job dependencies between jobs and their array tasks
    * requested via qsub -hold_jid_ad
    * e.g. when task 1 of job a has finished then task 1 of job b can start
    * Can be job ids or job names, including the use of wildcards. (JRE_Type only JRE_job_name)
    */
   JB_ja_ad_request_list,
   /**
    * @brief Job Array Predecessor Jobs
    *
    * List of job numbers of predecessor jobs for array job dependencies (JRE_Type only JRE_job_name)
    */
   JB_ja_ad_predecessor_list,
   /**
    * @brief Job Array Successor Jobs
    *
    * List of job numbers of successor jobs for array job dependencies (JRE_Type only JRE_job_name)
    */
   JB_ja_ad_successor_list,
   /**
    * @brief Jobs Session
    *
    * Jobs session (JAPI session tag for job event selection)
    */
   JB_session,
   /**
    * @brief Project Name
    *
    * Project name (qsub -P project_name)
    */
   JB_project,
   /**
    * @brief Department Name
    *
    * Department name. Set by schedd, saved (once) to qmaster.
    */
   JB_department,
   /**
    * @brief Command Prefix for Job Script
    *
    * Command prefix for jobscript ("qsub -C prefix") for parsing
    * special comments in the script file.
    */
   JB_directive_prefix,
   /**
    * @brief Executed File
    *
    * is the path to the locally spooled copy on the execution daemon,
    * it is script that actually gets executed,
    * In the case of a binary, is unused.
    */
   JB_exec_file,
   /**
    * @brief Script File Path
    *
    * is the path to the job as sent from the CLI, is the path on the submit host
    * In the case of a binary, is the path to the binary
    */
   JB_script_file,
   /**
    * @brief Script Size
    *
    * @todo really needed?
    */
   JB_script_size,
   /**
    * @brief Script in Memory
    *
    * the pointer to the character area of the jobscript
    */
   JB_script_ptr,
   /**
    * @brief Submission Time
    *
    * timestamp in microseconds since epoch
    */
   JB_submission_time,
   /**
    * @brief Earliest Execution Time
    *
    * When should the job start ("qsub/qalter -a date_time")
    * timestamp in microseconds since epoch
    */
   JB_execution_time,
   /**
    * @brief Deadline Time
    *
    * SGEEE. Deadline initiation time. (qsub -dl date_time)
    * timestamp in microseconds since epoch
    */
   JB_deadline,
   /**
    * @brief Job Owner
    *
    * user who submitted the job
    * @todo rename to JB_user to be consistent?
    */
   JB_owner,
   /**
    * @brief User Id
    *
    * user id of the job owner
    */
   JB_uid,
   /**
    * @brief Job Owner Group Name
    *
    * primary group name of the job owner
    */
   JB_group,
   /**
    * @brief Job Owner Group Id
    *
    * primary group id if the job owner
    */
   JB_gid,
   /**
    * @brief Account String
    *
    * Account string ("qsub/qalter -A account string")
    */
   JB_account,
   /**
    * @brief Current Working Directory
    *
    * Current working directory from qsub ("qsub -cwd" or "qsub -wd")
    */
   JB_cwd,
   /**
    * @brief Notify Job
    *
    * Notify job of impending kill/stop signal. ("qsub -notify")
    */
   JB_notify,
   /**
    * @brief Job Type
    *
    * Start job immediately or not at all. ("qsub -now")
    * @todo it could be a boolean, but is misused for other information!
    */
   JB_type,
   /**
    * @brief Reserve Resources
    *
    * Specifies if a reservation is desired by the user ("qsub -R y|n").
    * Available for non-immediate job submissions. Irrespective
    * of the users desire a job reservation is made
    *   o only in reservation scheduling mode
    *   o only until the maximum number of reservations during a
    *     scheduling run is not exceeded when the order comes at
    *     this job. The maximum number (SC_max_reservation) can be
    *     specified in sched_conf(5).
    *   o only for non-immediate jobs
    * Default is 'n'.
    */
   JB_reserve,
   /**
    * @brief Priority
    *
    * Priority ("qsub/qalter -p priority")
    */
   JB_priority,
   /**
    * @brief SGEE Job Share
    *
    * Priority ("qsub/qalter -js jobshare")
    */
   JB_jobshare,
   /**
    * @brief Shell List
    *
    * Command interpreter to be used (PN_Type).
    * ("qsub/qalter -S shell")
    */
   JB_shell_list,
   /**
    * @brief Verify
    *
    * Triggers "verify" messages. (qsub -verify)
    */
   JB_verify,
   /**
    * @brief Environment List
    *
    * Export these env variables (VA_Type). ("qsub -V").
    */
   JB_env_list,
   /**
    * @brief Job Context
    *
    * Custom attributes (name,val) pairs (VA_Type).
    * The result of the -ac, -sc and -dc switches, see JB_context_change.
    */
   JB_context,
   /**
    * @brief Job Arguments
    *
    * Job arguments (ST_Type).
    */
   JB_job_args,
   /**
    * @brief Checkpoint Attributes
    *
    * Checkpoint attributes ("qsub/qalter -c interval_flags")
    * @todo  merge all checkpointing stuff to one object?
    */
   JB_checkpoint_attr,
   /**
    * @brief Checkpoint Name
    *
    * Name of ckpt object ("qsub/qalter -ckpt ckpt_name")
    */
   JB_checkpoint_name,
   /**
    * @brief Checkpoint Object
    *
    * Ckpt object which will be sent from qmaster to execd.
    * @todo: meaning when we change it in qmaster it will not be updated in execd
    */
   JB_checkpoint_object,
   /**
    * @brief Checkpoint Interval
    *
    * Checkpoint frequency ("qsub/qalter -c seconds")
    */
   JB_checkpoint_interval,
   /**
    * @brief Rerunnable
    *
    * Is job rerunable? ("qsub/qalter -r y/n")
    * @todo it could be a boolean but is misused for other information!
    */
   JB_restart,
   /**
    * @brief stdout Path List
    *
    * Pathname for stdout (PN_Type). ("qsub/qalter -o path_name")
    */
   JB_stdout_path_list,
   /**
    * @brief stderr Path List
    *
    * Std error path streams (PN_Type). ("qsub/qalter -e path_name")
    */
   JB_stderr_path_list,
   /**
    * @brief stdin Path List
    *
    * Std input path streams (PN_Type). ("qsub/qalter -i path_name")
    */
   JB_stdin_path_list,
   /**
    * @brief Merge stderr
    *
    * Merge stdout and stderr? ("qsub/qalter -j y|n")
    */
   JB_merge_stderr,
   /**
    * @brief Request Set List
    *
    * List of request sets. 0 .. 3 request sets can exist: global, master, slave.
    * Sequential jobs can have a single job request set, the global one.
    * Parallel jobs can have up to 3 request sets: global requests,
    * requests for the master task, requests for the slave tasks.
    */
   JB_request_set_list,
   /**
    * @brief Mail options
    *
    * (qsub/qalter -m mail_options)
    */
   JB_mail_options,
   /**
    * @brief Mail list
    *
    * Mail recipiants (MR_Type).
    * (qsub/qalter -M mail_list)
    */
   JB_mail_list,
   /**
    * @brief Requested Parallel Environment
    *
    * Name of requested PE or wildcard expression for matching PEs
    * (qsub/qalter -pe pe-name slot_range)
    */
   JB_pe,
   /**
    * @brief Requested Slot Range for Parallel Environment
    *
    * PE slot range (RN_Type). Qmaster will ensure that it is ascending and normalized
    * (qsub/qalter -pe pe-name slot_range)
    */
   JB_pe_range,
   /**
    * @brief Kerberos Client TGB
    *
    * Kerberos client TGT
    */
   JB_tgt,
   /**
    * @brief DCE/Kerberos/TLS Credentials
    *
    * DCE/Kerberos credentials. Also used for TLS encryption to transport the credentials
    * from qrsh to the sge_execd, which will store it in the job's active_jobs directory.
    */
   JB_cred,
   /**
    * @brief Array Job Structure
    *
    * Elements describe task id range structure during the
    * submission time of a (array) job (RN_Type).
    * qsub -t tid_range
    */
   JB_ja_structure,
   /**
    * @brief Array Task IDs without Hold
    *
    * Just submitted array task without hold state (RN_Type).
    */
   JB_ja_n_h_ids,
   /**
    * @brief Array Task IDs with User Hold
    *
    * Just submitted and user hold applied (RN_Type).
    * qsub -h -t tid_range
    * qalter -h u/U jid.tid1-tid2:step
    */
   JB_ja_u_h_ids,
   /**
    * @brief Array Task IDs with System Hold
    *
    * Just submitted and system hold applied (RN_Type).
    * qalter -h s/S jid.tid1-tid2:step
    */
   JB_ja_s_h_ids,
   /**
    * @brief Array Task IDs with Operator Hold
    *
    * Just submitted and operator hold applied (RN_Type).
    * qalter -h o/O jid.tid1-tid2:step
    */
   JB_ja_o_h_ids,
   /**
    * @brief Array Task IDs with Array Hold
    *
    * Just submitted and array hold applied (RN_Type).
    * qalter -hold_jid_ad wc_job_list
    */
   JB_ja_a_h_ids,
   /**
    * @brief Template for new Tasks
    *
    * Template for new tasks. In SGEEE systems the schedd will
    * store initial tickets in this element. (JAT_Type)
    */
   JB_ja_template,
   /**
    * @brief List of Array Tasks
    *
    * List of array tasks (in case of array jobs) or one task
    * (in case of a job) (JAT_Type).
    */
   JB_ja_tasks,
   /**
    * @brief Host the job (array task) is executing on
    *
    * SGEEE - host job is executing on. Local to schedd.
    * Not spooled.
    * @todo still used?
    */
   JB_host,
   /**
    * @brief Category Reference
    *
    * Category string reference used in schedd.
    */
   JB_category,
   /**
    * @brief List of users for qalter
    *
    * List of usernames (qsub/qalter -u username_list).
    * qsub -u does not exist. Not part of a job, but only
    * userd for qalter request as where condition. Could most
    * probably be passed via lCondition.
    * @todo change qalter, remove this attribute from job
    */
   JB_user_list,
   /**
    * @brief Job Identifier List for qalter
    *
    * condition for qalter? @todo Then it should better be passed
    * via condition. (ID_Type)
    */
   JB_job_identifier_list,
   /**
    * @brief Verify Suitable Queues
    *
    * @todo used in qalter?
    */
   JB_verify_suitable_queues,
   /**
    * @brief Soft Wallclock GMT
    *
    * Timestamp (microseconds since epoch) when a soft wallclock limit will take effect
    */
   JB_soft_wallclock_gmt,
   /**
    * @brief Hard Wallclock GMT
    *
    * Timestamp (microseconds since epoch) when a hard wallclock limit will take effect
    */
   JB_hard_wallclock_gmt,
   /**
    * @brief Override Tickets
    *
    * SGEEE - override tickets assigned by admin.
    * (qalter -ot tickets).
    */
   JB_override_tickets,
   /**
    * @brief Path Aliases List
    *
    * Path aliases list (PA_Type).
    */
   JB_path_aliases,
   /**
    * @brief Urgency
    *
    * SGEEE. Absolute static urgency importance. The admin can use arbitrary
    * weighting factors in the formula used to determine this number. So any
    * value is possible. Needed only when scheduling code is run.
    * Not spooled.
    */
   JB_urg,
   /**
    * @brief Normalised Urgency
    *
    * SGEEE. Relative importance due to static urgency in the range between 0.0
    * and 1.0. No need to make this a per task attribute as long as waiting time
    * and deadline remain job attributes.
    * Not spooled.
    */
   JB_nurg,
   /**
    * @brief Normalised Posix Priority
    *
    * SGEEE. Relative importance due to Posix priority in the range between 0.0
    * and 1.0. No need to make this a per task attribute as long as the POSIX
    * priority remains a job attribute.
    * Not spooled.
    */
   JB_nppri,
   /**
    * @brief Relative Resource Contribution (?)
    *
    * SGEEE. Combined contribution to static urgency from all resources. This can
    * be any value. Actually this is a property of job category. This field is
    * needed only to provide it for diagnosis purposes it as per job information
    * via GDI.
    * Not spooled.
    */
   JB_rrcontr,
   /**
    * @brief Deadline Contribution
    *
    * SGEEE. Contribution to static urgency from waiting time. This can be any
    * value. No need to make this a per task attribute as long as waiting time
    * is a job attribute. Increases over time.
    * Not spooled.
    */
   JB_dlcontr,
   /**
    * @brief Waiting Time Contribution
    *
    * SGEEE. Contribution to static urgency from waiting time. This can be any
    * value. No need to make this a per task attribute as long as waiting time
    * is a job attribute. Increases over time.
    * Not spooled.
    */
   JB_wtcontr,
   /**
    * @brief Advance Reservation ID
    *
    * Unique advance reservation number.
    */
   JB_ar,
   /**
    * @brief Pty
    *
    * Interactive job should be started in a pty. 0=no, 1=yes, 2=use default.
    */
   JB_pty,
   /**
    * @brief Array Task Concurrency
    *
    * The number of concurrent array tasks executing at any given time.
    */
   JB_ja_task_concurrency,
   /**
    * @brief Binding Strategy
    *
    * Binding strategy for the job
    */
   JB_binding,
   /**
    * @brief Submission Command Line
    *
    * The submission command line as a string.
    * Arguments which contain whitespace or wildcards are enclosed in single quotes,
    * so it should be possible to copy/paste the command line into a shell.
    */
   JB_submission_command_line,
   /**
    * @brief Supplementary Group List
    *
    * list of supplementary groups and corresponding ID's
    */
   JB_grp_list,
   /**
    * @brief Joker
    *
    * Placeholder which can be used for arbitrary data.
    * Its purpose is to be able to add new attributes without changing the spooling format.
    * It is a list of arbitrary type and it is spooled.
    */
   JB_joker,
   /**
    * @brief sync options
    *
    * Bits that have been specified to the -sync switch.
    */
   JB_sync_options,
   /**
    * @brief Category ID (CAT_id)
    *
    * Category ID (CAT_id). Within the scheduler the field category refers to the category object.
    */
   JB_category_id,
   /**
    * @brief Value of -when qalter switch
    *
    * Value of the -when qalter switch (enum qalter_when_t).
    */
   JB_when,
   /**
    * @brief Job Context Change
    *
    * Instructions of the -ac, -sc and -dc switches (CX_Type), in the order given.
    * Sent by submit and qalter, applied to JB_context by the qmaster and not spooled.
    */
   JB_context_change,
};

namespace ocs::tom {

   struct BN;
   struct CK;
   struct CX;
   struct ID;
   struct JAT;
   struct JRE;
   struct JRS;
   struct MR;
   struct PA;
   struct PN;
   struct RN;
   struct ST;
   struct VA;

   /**
    * @brief job type
    *
    * JB_Type elements make only sense in conjunction with JAT_Type
    * elements.  One element of each type is necessary to hold all
    * data for the execution of one job. One JB_Type element an
    * x JAT_Type elements are needed to execute an array job with
    * x tasks.
    *  
    *          -----------       1:x        ------------
    *          | JB_Type |<---------------->| JAT_Type |
    *          -----------                  ------------
    *  
    * The relation between these two elements is defined in the
    * 'JB_ja_tasks' sublist of a 'JB_Type' element. This list will
    * contain all belonging JAT_Type elements.
    *  
    * The 'JAT_Type' CULL element containes all attributes in which
    * one array task may differ from another array task of the
    * same array job. The 'JB_Type' element defines all attributes
    * wich are equivalent for all tasks of an array job.
    * A job and an array job with one task are equivalent
    * concerning their data structures. Both consist of one 'JB_Type'
    * and one 'JAT_Type' element
    */
   struct JB {
      /** @brief The fields: number, name, type and flags, in the order of the field numbers */
      static constexpr FieldInfo fields[] = {
         {JB_job_number, "JB_job_number", FieldType::Uint32, Flag::PrimaryKey | Flag::Hash | Flag::Spool},
         {JB_job_name, "JB_job_name", FieldType::String, Flag::Spool},
         {JB_version, "JB_version", FieldType::Uint32, Flag::Spool},
         {JB_jid_request_list, "JB_jid_request_list", FieldType::List, Flag::Spool},
         {JB_jid_predecessor_list, "JB_jid_predecessor_list", FieldType::List, Flag::Spool},
         {JB_jid_successor_list, "JB_jid_successor_list", FieldType::List, Flag::None},
         {JB_ja_ad_request_list, "JB_ja_ad_request_list", FieldType::List, Flag::Spool},
         {JB_ja_ad_predecessor_list, "JB_ja_ad_predecessor_list", FieldType::List, Flag::Spool},
         {JB_ja_ad_successor_list, "JB_ja_ad_successor_list", FieldType::List, Flag::None},
         {JB_session, "JB_session", FieldType::String, Flag::Spool},
         {JB_project, "JB_project", FieldType::String, Flag::Spool},
         {JB_department, "JB_department", FieldType::String, Flag::Spool},
         {JB_directive_prefix, "JB_directive_prefix", FieldType::String, Flag::Spool},
         {JB_exec_file, "JB_exec_file", FieldType::String, Flag::Spool},
         {JB_script_file, "JB_script_file", FieldType::String, Flag::Spool},
         {JB_script_size, "JB_script_size", FieldType::Uint32, Flag::Spool},
         {JB_script_ptr, "JB_script_ptr", FieldType::String, Flag::None},
         {JB_submission_time, "JB_submission_time", FieldType::Uint64, Flag::Spool},
         {JB_execution_time, "JB_execution_time", FieldType::Uint64, Flag::Spool},
         {JB_deadline, "JB_deadline", FieldType::Uint64, Flag::Spool},
         {JB_owner, "JB_owner", FieldType::String, Flag::Spool},
         {JB_uid, "JB_uid", FieldType::Uint32, Flag::Spool},
         {JB_group, "JB_group", FieldType::String, Flag::Spool},
         {JB_gid, "JB_gid", FieldType::Uint32, Flag::Spool},
         {JB_account, "JB_account", FieldType::String, Flag::Spool},
         {JB_cwd, "JB_cwd", FieldType::String, Flag::Spool},
         {JB_notify, "JB_notify", FieldType::Bool, Flag::Spool},
         {JB_type, "JB_type", FieldType::Uint32, Flag::Spool},
         {JB_reserve, "JB_reserve", FieldType::Bool, Flag::Spool},
         {JB_priority, "JB_priority", FieldType::Uint32, Flag::Spool},
         {JB_jobshare, "JB_jobshare", FieldType::Uint32, Flag::Spool},
         {JB_shell_list, "JB_shell_list", FieldType::List, Flag::Spool},
         {JB_verify, "JB_verify", FieldType::Uint32, Flag::Spool},
         {JB_env_list, "JB_env_list", FieldType::List, Flag::Spool},
         {JB_context, "JB_context", FieldType::List, Flag::Spool},
         {JB_job_args, "JB_job_args", FieldType::List, Flag::Spool},
         {JB_checkpoint_attr, "JB_checkpoint_attr", FieldType::Uint32, Flag::Spool},
         {JB_checkpoint_name, "JB_checkpoint_name", FieldType::String, Flag::Spool},
         {JB_checkpoint_object, "JB_checkpoint_object", FieldType::Object, Flag::None},
         {JB_checkpoint_interval, "JB_checkpoint_interval", FieldType::Uint32, Flag::Spool},
         {JB_restart, "JB_restart", FieldType::Uint32, Flag::Spool},
         {JB_stdout_path_list, "JB_stdout_path_list", FieldType::List, Flag::Spool},
         {JB_stderr_path_list, "JB_stderr_path_list", FieldType::List, Flag::Spool},
         {JB_stdin_path_list, "JB_stdin_path_list", FieldType::List, Flag::Spool},
         {JB_merge_stderr, "JB_merge_stderr", FieldType::Bool, Flag::Spool},
         {JB_request_set_list, "JB_request_set_list", FieldType::List, Flag::Spool},
         {JB_mail_options, "JB_mail_options", FieldType::Uint32, Flag::Spool},
         {JB_mail_list, "JB_mail_list", FieldType::List, Flag::Spool},
         {JB_pe, "JB_pe", FieldType::String, Flag::Spool},
         {JB_pe_range, "JB_pe_range", FieldType::List, Flag::Spool},
         {JB_tgt, "JB_tgt", FieldType::String, Flag::None},
         {JB_cred, "JB_cred", FieldType::String, Flag::None},
         {JB_ja_structure, "JB_ja_structure", FieldType::List, Flag::Spool},
         {JB_ja_n_h_ids, "JB_ja_n_h_ids", FieldType::List, Flag::Spool},
         {JB_ja_u_h_ids, "JB_ja_u_h_ids", FieldType::List, Flag::Spool},
         {JB_ja_s_h_ids, "JB_ja_s_h_ids", FieldType::List, Flag::Spool},
         {JB_ja_o_h_ids, "JB_ja_o_h_ids", FieldType::List, Flag::Spool},
         {JB_ja_a_h_ids, "JB_ja_a_h_ids", FieldType::List, Flag::Spool},
         {JB_ja_template, "JB_ja_template", FieldType::List, Flag::Spool},
         {JB_ja_tasks, "JB_ja_tasks", FieldType::List, Flag::Spool},
         {JB_host, "JB_host", FieldType::Host, Flag::None},
         {JB_category, "JB_category", FieldType::Ref, Flag::NoTransfer},
         {JB_user_list, "JB_user_list", FieldType::List, Flag::None},
         {JB_job_identifier_list, "JB_job_identifier_list", FieldType::List, Flag::None},
         {JB_verify_suitable_queues, "JB_verify_suitable_queues", FieldType::Uint32, Flag::None},
         {JB_soft_wallclock_gmt, "JB_soft_wallclock_gmt", FieldType::Uint64, Flag::Spool},
         {JB_hard_wallclock_gmt, "JB_hard_wallclock_gmt", FieldType::Uint64, Flag::Spool},
         {JB_override_tickets, "JB_override_tickets", FieldType::Uint32, Flag::Spool},
         {JB_path_aliases, "JB_path_aliases", FieldType::List, Flag::Spool},
         {JB_urg, "JB_urg", FieldType::Double, Flag::None},
         {JB_nurg, "JB_nurg", FieldType::Double, Flag::None},
         {JB_nppri, "JB_nppri", FieldType::Double, Flag::None},
         {JB_rrcontr, "JB_rrcontr", FieldType::Double, Flag::None},
         {JB_dlcontr, "JB_dlcontr", FieldType::Double, Flag::None},
         {JB_wtcontr, "JB_wtcontr", FieldType::Double, Flag::None},
         {JB_ar, "JB_ar", FieldType::Uint32, Flag::Spool},
         {JB_pty, "JB_pty", FieldType::Uint32, Flag::Spool},
         {JB_ja_task_concurrency, "JB_ja_task_concurrency", FieldType::Uint32, Flag::Spool},
         {JB_binding, "JB_binding", FieldType::Object, Flag::Spool},
         {JB_submission_command_line, "JB_submission_command_line", FieldType::String, Flag::Spool},
         {JB_grp_list, "JB_grp_list", FieldType::List, Flag::Spool},
         {JB_joker, "JB_joker", FieldType::List, Flag::Spool},
         {JB_sync_options, "JB_sync_options", FieldType::Uint32, Flag::Spool},
         {JB_category_id, "JB_category_id", FieldType::Uint32, Flag::Hash},
         {JB_when, "JB_when", FieldType::Uint32, Flag::None},
         {JB_context_change, "JB_context_change", FieldType::List, Flag::None},
      };

      /** @brief The descriptor of the list library, derived from the field table */
      static constexpr auto descriptor = make_descriptor(fields);

      /** @brief The field names for the list library, derived from the field table */
      static constexpr auto names = field_names(fields);
   };

   /** @brief The field enumeration of JB belongs to JB */
   template <>
   struct TypeOf<JB_Field> {
      using type = JB;   ///< the object type
   };

   /** @brief JB_jid_request_list holds elements of JRE */
   template <>
   struct SubType<JB_jid_request_list> {
      using type = JRE;   ///< the sub-type
   };

   /** @brief JB_jid_predecessor_list holds elements of JRE */
   template <>
   struct SubType<JB_jid_predecessor_list> {
      using type = JRE;   ///< the sub-type
   };

   /** @brief JB_jid_successor_list holds elements of JRE */
   template <>
   struct SubType<JB_jid_successor_list> {
      using type = JRE;   ///< the sub-type
   };

   /** @brief JB_ja_ad_request_list holds elements of JRE */
   template <>
   struct SubType<JB_ja_ad_request_list> {
      using type = JRE;   ///< the sub-type
   };

   /** @brief JB_ja_ad_predecessor_list holds elements of JRE */
   template <>
   struct SubType<JB_ja_ad_predecessor_list> {
      using type = JRE;   ///< the sub-type
   };

   /** @brief JB_ja_ad_successor_list holds elements of JRE */
   template <>
   struct SubType<JB_ja_ad_successor_list> {
      using type = JRE;   ///< the sub-type
   };

   /** @brief JB_shell_list holds elements of PN */
   template <>
   struct SubType<JB_shell_list> {
      using type = PN;   ///< the sub-type
   };

   /** @brief JB_env_list holds elements of VA */
   template <>
   struct SubType<JB_env_list> {
      using type = VA;   ///< the sub-type
   };

   /** @brief JB_context holds elements of VA */
   template <>
   struct SubType<JB_context> {
      using type = VA;   ///< the sub-type
   };

   /** @brief JB_job_args holds elements of ST */
   template <>
   struct SubType<JB_job_args> {
      using type = ST;   ///< the sub-type
   };

   /** @brief JB_checkpoint_object holds elements of CK */
   template <>
   struct SubType<JB_checkpoint_object> {
      using type = CK;   ///< the sub-type
   };

   /** @brief JB_stdout_path_list holds elements of PN */
   template <>
   struct SubType<JB_stdout_path_list> {
      using type = PN;   ///< the sub-type
   };

   /** @brief JB_stderr_path_list holds elements of PN */
   template <>
   struct SubType<JB_stderr_path_list> {
      using type = PN;   ///< the sub-type
   };

   /** @brief JB_stdin_path_list holds elements of PN */
   template <>
   struct SubType<JB_stdin_path_list> {
      using type = PN;   ///< the sub-type
   };

   /** @brief JB_request_set_list holds elements of JRS */
   template <>
   struct SubType<JB_request_set_list> {
      using type = JRS;   ///< the sub-type
   };

   /** @brief JB_mail_list holds elements of MR */
   template <>
   struct SubType<JB_mail_list> {
      using type = MR;   ///< the sub-type
   };

   /** @brief JB_pe_range holds elements of RN */
   template <>
   struct SubType<JB_pe_range> {
      using type = RN;   ///< the sub-type
   };

   /** @brief JB_ja_structure holds elements of RN */
   template <>
   struct SubType<JB_ja_structure> {
      using type = RN;   ///< the sub-type
   };

   /** @brief JB_ja_n_h_ids holds elements of RN */
   template <>
   struct SubType<JB_ja_n_h_ids> {
      using type = RN;   ///< the sub-type
   };

   /** @brief JB_ja_u_h_ids holds elements of RN */
   template <>
   struct SubType<JB_ja_u_h_ids> {
      using type = RN;   ///< the sub-type
   };

   /** @brief JB_ja_s_h_ids holds elements of RN */
   template <>
   struct SubType<JB_ja_s_h_ids> {
      using type = RN;   ///< the sub-type
   };

   /** @brief JB_ja_o_h_ids holds elements of RN */
   template <>
   struct SubType<JB_ja_o_h_ids> {
      using type = RN;   ///< the sub-type
   };

   /** @brief JB_ja_a_h_ids holds elements of RN */
   template <>
   struct SubType<JB_ja_a_h_ids> {
      using type = RN;   ///< the sub-type
   };

   /** @brief JB_ja_template holds elements of JAT */
   template <>
   struct SubType<JB_ja_template> {
      using type = JAT;   ///< the sub-type
   };

   /** @brief JB_ja_tasks holds elements of JAT */
   template <>
   struct SubType<JB_ja_tasks> {
      using type = JAT;   ///< the sub-type
   };

   /** @brief JB_user_list holds elements of ST */
   template <>
   struct SubType<JB_user_list> {
      using type = ST;   ///< the sub-type
   };

   /** @brief JB_job_identifier_list holds elements of ID */
   template <>
   struct SubType<JB_job_identifier_list> {
      using type = ID;   ///< the sub-type
   };

   /** @brief JB_path_aliases holds elements of PA */
   template <>
   struct SubType<JB_path_aliases> {
      using type = PA;   ///< the sub-type
   };

   /** @brief JB_binding holds elements of BN */
   template <>
   struct SubType<JB_binding> {
      using type = BN;   ///< the sub-type
   };

   /** @brief JB_grp_list holds elements of ST */
   template <>
   struct SubType<JB_grp_list> {
      using type = ST;   ///< the sub-type
   };

   /** @brief JB_joker holds elements of VA */
   template <>
   struct SubType<JB_joker> {
      using type = VA;   ///< the sub-type
   };

   /** @brief JB_context_change holds elements of CX */
   template <>
   struct SubType<JB_context_change> {
      using type = CX;   ///< the sub-type
   };

} // namespace ocs::tom

/** @brief The descriptor of JB for the list library: a synonym for ocs::tom::JB::descriptor */
inline constexpr const lDescr *JB_Type = ocs::tom::JB::descriptor.e;
