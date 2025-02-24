#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2024-2025 HPC-Gridware GmbH
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

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

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
*
*    SGE_ULONG(JB_job_number) - Unique Job Number
*    holds values in the range of 1..U_LONG32_MAX
*
*    SGE_STRING(JB_job_name) - Job Name
*    (qsub/qalter -N job_name)
*
*    SGE_ULONG(JB_version) - Job Version Number
*    will be in increased when job is modified
*
*    SGE_LIST(JB_jid_request_list) - Job Requested Dependencies
*    Dependencies as requested via qsub -hold_jid.
*    Can be job ids or job names, including the use of wildcards. (JRE_Type only JRE_job_name)
*
*    SGE_LIST(JB_jid_predecessor_list) - Predecessor Jobs
*    list of job numbers of predecessor jobs (JRE_Type only JRE_job_name)
*
*    SGE_LIST(JB_jid_successor_list) - Successor Jobs
*    list of job numbers of successor jobs (JRE_Type only JRE_job_name)
*
*    SGE_LIST(JB_ja_ad_request_list) - Job Requested Array Dependencies
*    Job dependencies between jobs and their array tasks
*    requested via qsub -hold_jid_ad
*    e.g. when task 1 of job a has finished then task 1 of job b can start
*    Can be job ids or job names, including the use of wildcards. (JRE_Type only JRE_job_name)
*
*    SGE_LIST(JB_ja_ad_predecessor_list) - Job Array Predecessor Jobs
*    List of job numbers of predecessor jobs for array job dependencies (JRE_Type only JRE_job_name)
*
*    SGE_LIST(JB_ja_ad_successor_list) - Job Array Successor Jobs
*    List of job numbers of successor jobs for array job dependencies (JRE_Type only JRE_job_name)
*
*    SGE_STRING(JB_session) - Jobs Session
*    Jobs session (JAPI session tag for job event selection)
*
*    SGE_STRING(JB_project) - Project Name
*    Project name (qsub -P project_name)
*
*    SGE_STRING(JB_department) - Department Name
*    Department name. Set by schedd, saved (once) to qmaster.
*
*    SGE_STRING(JB_directive_prefix) - Command Prefix for Job Script
*    Command prefix for jobscript ("qsub -C prefix") for parsing
*    special comments in the script file.
*
*    SGE_STRING(JB_exec_file) - Executed File
*    is the path to the locally spooled copy on the execution daemon,
*    it is script that actually gets executed,
*    In the case of a binary, is unused.
*
*    SGE_STRING(JB_script_file) - Script File Path
*    is the path to the job as sent from the CLI, is the path on the submit host
*    In the case of a binary, is the path to the binary
*
*    SGE_ULONG(JB_script_size) - Script Size
*    @todo really needed?
*
*    SGE_STRING(JB_script_ptr) - Script in Memory
*    the pointer to the character area of the jobscript
*
*    SGE_ULONG64(JB_submission_time) - Submission Time
*    timestamp in microseconds since epoch
*
*    SGE_ULONG64(JB_execution_time) - Earliest Execution Time
*    When should the job start ("qsub/qalter -a date_time")
*    timestamp in microseconds since epoch
*
*    SGE_ULONG64(JB_deadline) - Deadline Time
*    SGEEE. Deadline initiation time. (qsub -dl date_time)
*    timestamp in microseconds since epoch
*
*    SGE_STRING(JB_owner) - Job Owner
*    user who submitted the job
*    @todo rename to JB_user to be consistent?
*
*    SGE_ULONG(JB_uid) - User Id
*    user id of the job owner
*
*    SGE_STRING(JB_group) - Job Owner Group Name
*    primary group name of the job owner
*
*    SGE_ULONG(JB_gid) - Job Owner Group Id
*    primary group id if the job owner
*
*    SGE_STRING(JB_account) - Account String
*    Account string ("qsub/qalter -A account string")
*
*    SGE_STRING(JB_cwd) - Current Working Directory
*    Current working directory from qsub ("qsub -cwd" or "qsub -wd")
*
*    SGE_BOOL(JB_notify) - Notify Job
*    Notify job of impending kill/stop signal. ("qsub -notify")
*
*    SGE_ULONG(JB_type) - Job Type
*    Start job immediately or not at all. ("qsub -now")
*    @todo it could be a boolean, but is misused for other information!
*
*    SGE_BOOL(JB_reserve) - Reserve Resources
*    Specifies if a reservation is desired by the user ("qsub -R y|n").
*    Available for non-immediate job submissions. Irrespective
*    of the users desire a job reservation is made
*      o only in reservation scheduling mode
*      o only until the maximum number of reservations during a
*        scheduling run is not exceeded when the order comes at
*        this job. The maximum number (SC_max_reservation) can be
*        specified in sched_conf(5).
*      o only for non-immediate jobs
*    Default is 'n'.
*
*    SGE_ULONG(JB_priority) - Priority
*    Priority ("qsub/qalter -p priority")
*
*    SGE_ULONG(JB_jobshare) - SGEE Job Share
*    Priority ("qsub/qalter -js jobshare")
*
*    SGE_LIST(JB_shell_list) - Shell List
*    Command interpreter to be used (PN_Type).
*    ("qsub/qalter -S shell")
*
*    SGE_ULONG(JB_verify) - Verify
*    Triggers "verify" messages. (qsub -verify)
*
*    SGE_LIST(JB_env_list) - Environment List
*    Export these env variables (VA_Type). ("qsub -V").
*
*    SGE_LIST(JB_context) - Job Context
*    Custom attributes (name,val) pairs (VA_Type).
*    ("qsub/qalter -ac/-dc context_list")
*
*    SGE_LIST(JB_job_args) - Job Arguments
*    Job arguments (ST_Type).
*
*    SGE_ULONG(JB_checkpoint_attr) - Checkpoint Attributes
*    Checkpoint attributes ("qsub/qalter -c interval_flags")
*    @todo  merge all checkpointing stuff to one object?
*
*    SGE_STRING(JB_checkpoint_name) - Checkpoint Name
*    Name of ckpt object ("qsub/qalter -ckpt ckpt_name")
*
*    SGE_OBJECT(JB_checkpoint_object) - Checkpoint Object
*    Ckpt object which will be sent from qmaster to execd.
*    @todo: meaning when we change it in qmaster it will not be updated in execd
*
*    SGE_ULONG(JB_checkpoint_interval) - Checkpoint Interval
*    Checkpoint frequency ("qsub/qalter -c seconds")
*
*    SGE_ULONG(JB_restart) - Rerunnable
*    Is job rerunable? ("qsub/qalter -r y/n")
*    @todo it could be a boolean but is misused for other information!
*
*    SGE_LIST(JB_stdout_path_list) - stdout Path List
*    Pathname for stdout (PN_Type). ("qsub/qalter -o path_name")
*
*    SGE_LIST(JB_stderr_path_list) - stderr Path List
*    Std error path streams (PN_Type). ("qsub/qalter -e path_name")
*
*    SGE_LIST(JB_stdin_path_list) - stdin Path List
*    Std input path streams (PN_Type). ("qsub/qalter -i path_name")
*
*    SGE_BOOL(JB_merge_stderr) - Merge stderr
*    Merge stdout and stderr? ("qsub/qalter -j y|n")
*
*    SGE_LIST(JB_request_set_list) - Request Set List
*    List of request sets. 0 .. 3 request sets can exist: global, master, slave.
*    Sequential jobs can have a single job request set, the global one.
*    Parallel jobs can have up to 3 request sets: global requests,
*    requests for the master task, requests for the slave tasks.
*
*    SGE_ULONG(JB_mail_options) - Mail options
*    (qsub/qalter -m mail_options)
*
*    SGE_LIST(JB_mail_list) - Mail list
*    Mail recipiants (MR_Type).
*    (qsub/qalter -M mail_list)
*
*    SGE_STRING(JB_pe) - Requested Parallel Environment
*    Name of requested PE or wildcard expression for matching PEs
*    (qsub/qalter -pe pe-name slot_range)
*
*    SGE_LIST(JB_pe_range) - Requested Slot Range for Parallel Environment
*    PE slot range (RN_Type). Qmaster will ensure that it is ascending and normalized
*    (qsub/qalter -pe pe-name slot_range)
*
*    SGE_STRING(JB_tgt) - Kerberos Client TGB
*    Kerberos client TGT
*
*    SGE_STRING(JB_cred) - DCE/Kerberos Credentials
*    DCE/Kerberos credentials
*
*    SGE_LIST(JB_ja_structure) - Array Job Structure
*    Elements describe task id range structure during the
*    submission time of a (array) job (RN_Type).
*    qsub -t tid_range
*
*    SGE_LIST(JB_ja_n_h_ids) - Array Task IDs without Hold
*    Just submitted array task without hold state (RN_Type).
*
*    SGE_LIST(JB_ja_u_h_ids) - Array Task IDs with User Hold
*    Just submitted and user hold applied (RN_Type).
*    qsub -h -t tid_range
*    qalter -h u/U jid.tid1-tid2:step
*
*    SGE_LIST(JB_ja_s_h_ids) - Array Task IDs with System Hold
*    Just submitted and system hold applied (RN_Type).
*    qalter -h s/S jid.tid1-tid2:step
*
*    SGE_LIST(JB_ja_o_h_ids) - Array Task IDs with Operator Hold
*    Just submitted and operator hold applied (RN_Type).
*    qalter -h o/O jid.tid1-tid2:step
*
*    SGE_LIST(JB_ja_a_h_ids) - Array Task IDs with Array Hold
*    Just submitted and array hold applied (RN_Type).
*    qalter -hold_jid_ad wc_job_list
*
*    SGE_LIST(JB_ja_z_ids) - Zombie Task IDs
*    Zombie task ids (RN_Type).
*    @todo still used?
*
*    SGE_LIST(JB_ja_template) - Template for new Tasks
*    Template for new tasks. In SGEEE systems the schedd will
*    store initial tickets in this element. (JAT_Type)
*
*    SGE_LIST(JB_ja_tasks) - List of Array Tasks
*    List of array tasks (in case of array jobs) or one task
*    (in case of a job) (JAT_Type).
*
*    SGE_HOST(JB_host) - Host the job (array task) is executing on
*    SGEEE - host job is executing on. Local to schedd.
*    Not spooled.
*    @todo still used?
*
*    SGE_REF(JB_category) - Category Reference
*    Category string reference used in schedd.
*
*    SGE_LIST(JB_user_list) - List of users for qalter
*    List of usernames (qsub/qalter -u username_list).
*    qsub -u does not exist. Not part of a job, but only
*    userd for qalter request as where condition. Could most
*    probably be passed via lCondition.
*    @todo change qalter, remove this attribute from job
*
*    SGE_LIST(JB_job_identifier_list) - Job Identifier List for qalter
*    condition for qalter? @todo Then it should better be passed
*    via condition. (ID_Type)
*
*    SGE_ULONG(JB_verify_suitable_queues) - Verify Suitable Queues
*    @todo used in qalter?
*
*    SGE_ULONG64(JB_soft_wallclock_gmt) - Soft Wallclock GMT
*    Timestamp (microseconds since epoch) when a soft wallclock limit will take effect
*
*    SGE_ULONG64(JB_hard_wallclock_gmt) - Hard Wallclock GMT
*    Timestamp (microseconds since epoch) when a hard wallclock limit will take effect
*
*    SGE_ULONG(JB_override_tickets) - Override Tickets
*    SGEEE - override tickets assigned by admin.
*    (qalter -ot tickets).
*
*    SGE_LIST(JB_qs_args) - Queuing System Arguments
*    Arguments for foreign queuing system (qsi?) (ST_Type).
*    @todo Either delete it, or recycle it to be used with starter_method.
*
*    SGE_LIST(JB_path_aliases) - Path Aliases List
*    Path aliases list (PA_Type).
*
*    SGE_DOUBLE(JB_urg) - Urgency
*    SGEEE. Absolute static urgency importance. The admin can use arbitrary
*    weighting factors in the formula used to determine this number. So any
*    value is possible. Needed only when scheduling code is run.
*    Not spooled.
*
*    SGE_DOUBLE(JB_nurg) - Normalised Urgency
*    SGEEE. Relative importance due to static urgency in the range between 0.0
*    and 1.0. No need to make this a per task attribute as long as waiting time
*    and deadline remain job attributes.
*    Not spooled.
*
*    SGE_DOUBLE(JB_nppri) - Normalised Posix Priority
*    SGEEE. Relative importance due to Posix priority in the range between 0.0
*    and 1.0. No need to make this a per task attribute as long as the POSIX
*    priority remains a job attribute.
*    Not spooled.
*
*    SGE_DOUBLE(JB_rrcontr) - Relative Resource Contribution (?)
*    SGEEE. Combined contribution to static urgency from all resources. This can
*    be any value. Actually this is a property of job category. This field is
*    needed only to provide it for diagnosis purposes it as per job information
*    via GDI.
*    Not spooled.
*
*    SGE_DOUBLE(JB_dlcontr) - Deadline Contribution
*    SGEEE. Contribution to static urgency from waiting time. This can be any
*    value. No need to make this a per task attribute as long as waiting time
*    is a job attribute. Increases over time.
*    Not spooled.
*
*    SGE_DOUBLE(JB_wtcontr) - Waiting Time Contribution
*    SGEEE. Contribution to static urgency from waiting time. This can be any
*    value. No need to make this a per task attribute as long as waiting time
*    is a job attribute. Increases over time.
*    Not spooled.
*
*    SGE_ULONG(JB_ar) - Advance Reservation ID
*    Unique advance reservation number.
*
*    SGE_ULONG(JB_pty) - Pty
*    Interactive job should be started in a pty. 0=no, 1=yes, 2=use default.
*
*    SGE_ULONG(JB_ja_task_concurrency) - Array Task Concurrency
*    The number of concurrent array tasks executing at any given time.
*
*    SGE_LIST(JB_binding) - Binding Strategy
*    Binding strategy for execution host (and later scheduler)
*
*    SGE_STRING(JB_submission_command_line) - Submission Command Line
*    The submission command line as a string.
*    Arguments which contain whitespace or wildcards are enclosed in single quotes,
*    so it should be possible to copy/paste the command line into a shell.
*
*    SGE_LIST(JB_grp_list) - Supplementary Group List
*    list of supplementary groups and corresponding ID's
*
*    SGE_LIST(JB_joker) - Joker
*    Placeholder which can be used for arbitrary data.
*    Its purpose is to be able to add new attributes without changing the spooling format.
*    It is a list of arbitrary type and it is spooled.
*
*    SGE_ULONG(JB_sync_options) - sync options
*    Bits that have been specified to the -sync switch.
*
*/

enum {
   JB_job_number = JB_LOWERBOUND,
   JB_job_name,
   JB_version,
   JB_jid_request_list,
   JB_jid_predecessor_list,
   JB_jid_successor_list,
   JB_ja_ad_request_list,
   JB_ja_ad_predecessor_list,
   JB_ja_ad_successor_list,
   JB_session,
   JB_project,
   JB_department,
   JB_directive_prefix,
   JB_exec_file,
   JB_script_file,
   JB_script_size,
   JB_script_ptr,
   JB_submission_time,
   JB_execution_time,
   JB_deadline,
   JB_owner,
   JB_uid,
   JB_group,
   JB_gid,
   JB_account,
   JB_cwd,
   JB_notify,
   JB_type,
   JB_reserve,
   JB_priority,
   JB_jobshare,
   JB_shell_list,
   JB_verify,
   JB_env_list,
   JB_context,
   JB_job_args,
   JB_checkpoint_attr,
   JB_checkpoint_name,
   JB_checkpoint_object,
   JB_checkpoint_interval,
   JB_restart,
   JB_stdout_path_list,
   JB_stderr_path_list,
   JB_stdin_path_list,
   JB_merge_stderr,
   JB_request_set_list,
   JB_mail_options,
   JB_mail_list,
   JB_pe,
   JB_pe_range,
   JB_tgt,
   JB_cred,
   JB_ja_structure,
   JB_ja_n_h_ids,
   JB_ja_u_h_ids,
   JB_ja_s_h_ids,
   JB_ja_o_h_ids,
   JB_ja_a_h_ids,
   JB_ja_z_ids,
   JB_ja_template,
   JB_ja_tasks,
   JB_host,
   JB_category,
   JB_user_list,
   JB_job_identifier_list,
   JB_verify_suitable_queues,
   JB_soft_wallclock_gmt,
   JB_hard_wallclock_gmt,
   JB_override_tickets,
   JB_qs_args,
   JB_path_aliases,
   JB_urg,
   JB_nurg,
   JB_nppri,
   JB_rrcontr,
   JB_dlcontr,
   JB_wtcontr,
   JB_ar,
   JB_pty,
   JB_ja_task_concurrency,
   JB_binding,
   JB_submission_command_line,
   JB_grp_list,
   JB_joker,
   JB_sync_options
};

LISTDEF(JB_Type)
   SGE_ULONG(JB_job_number, CULL_PRIMARY_KEY | CULL_HASH | CULL_SPOOL)
   SGE_STRING(JB_job_name, CULL_SPOOL)
   SGE_ULONG(JB_version, CULL_SPOOL)
   SGE_LIST(JB_jid_request_list, JRE_Type, CULL_SPOOL)
   SGE_LIST(JB_jid_predecessor_list, JRE_Type, CULL_SPOOL)
   SGE_LIST(JB_jid_successor_list, JRE_Type, CULL_DEFAULT)
   SGE_LIST(JB_ja_ad_request_list, JRE_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_ad_predecessor_list, JRE_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_ad_successor_list, JRE_Type, CULL_DEFAULT)
   SGE_STRING(JB_session, CULL_SPOOL)
   SGE_STRING(JB_project, CULL_SPOOL)
   SGE_STRING(JB_department, CULL_SPOOL)
   SGE_STRING(JB_directive_prefix, CULL_SPOOL)
   SGE_STRING(JB_exec_file, CULL_SPOOL)
   SGE_STRING(JB_script_file, CULL_SPOOL)
   SGE_ULONG(JB_script_size, CULL_SPOOL)
   SGE_STRING(JB_script_ptr, CULL_DEFAULT)
   SGE_ULONG64(JB_submission_time, CULL_SPOOL)
   SGE_ULONG64(JB_execution_time, CULL_SPOOL)
   SGE_ULONG64(JB_deadline, CULL_SPOOL)
   SGE_STRING(JB_owner, CULL_SPOOL)
   SGE_ULONG(JB_uid, CULL_SPOOL)
   SGE_STRING(JB_group, CULL_SPOOL)
   SGE_ULONG(JB_gid, CULL_SPOOL)
   SGE_STRING(JB_account, CULL_SPOOL)
   SGE_STRING(JB_cwd, CULL_SPOOL)
   SGE_BOOL(JB_notify, CULL_SPOOL)
   SGE_ULONG(JB_type, CULL_SPOOL)
   SGE_BOOL(JB_reserve, CULL_SPOOL)
   SGE_ULONG(JB_priority, CULL_SPOOL)
   SGE_ULONG(JB_jobshare, CULL_SPOOL)
   SGE_LIST(JB_shell_list, PN_Type, CULL_SPOOL)
   SGE_ULONG(JB_verify, CULL_SPOOL)
   SGE_LIST(JB_env_list, VA_Type, CULL_SPOOL)
   SGE_LIST(JB_context, VA_Type, CULL_SPOOL)
   SGE_LIST(JB_job_args, ST_Type, CULL_SPOOL)
   SGE_ULONG(JB_checkpoint_attr, CULL_SPOOL)
   SGE_STRING(JB_checkpoint_name, CULL_SPOOL)
   SGE_OBJECT(JB_checkpoint_object, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_ULONG(JB_checkpoint_interval, CULL_SPOOL)
   SGE_ULONG(JB_restart, CULL_SPOOL)
   SGE_LIST(JB_stdout_path_list, PN_Type, CULL_SPOOL)
   SGE_LIST(JB_stderr_path_list, PN_Type, CULL_SPOOL)
   SGE_LIST(JB_stdin_path_list, PN_Type, CULL_SPOOL)
   SGE_BOOL(JB_merge_stderr, CULL_SPOOL)
   SGE_LIST(JB_request_set_list, JRS_Type, CULL_SPOOL)
   SGE_ULONG(JB_mail_options, CULL_SPOOL)
   SGE_LIST(JB_mail_list, MR_Type, CULL_SPOOL)
   SGE_STRING(JB_pe, CULL_SPOOL)
   SGE_LIST(JB_pe_range, RN_Type, CULL_SPOOL)
   SGE_STRING(JB_tgt, CULL_DEFAULT)
   SGE_STRING(JB_cred, CULL_DEFAULT)
   SGE_LIST(JB_ja_structure, RN_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_n_h_ids, RN_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_u_h_ids, RN_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_s_h_ids, RN_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_o_h_ids, RN_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_a_h_ids, RN_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_z_ids, RN_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_template, JAT_Type, CULL_SPOOL)
   SGE_LIST(JB_ja_tasks, JAT_Type, CULL_SPOOL)
   SGE_HOST(JB_host, CULL_DEFAULT)
   SGE_REF(JB_category, CULL_ANY_SUBTYPE, CULL_DEFAULT)
   SGE_LIST(JB_user_list, ST_Type, CULL_DEFAULT)
   SGE_LIST(JB_job_identifier_list, ID_Type, CULL_DEFAULT)
   SGE_ULONG(JB_verify_suitable_queues, CULL_DEFAULT)
   SGE_ULONG64(JB_soft_wallclock_gmt, CULL_SPOOL)
   SGE_ULONG64(JB_hard_wallclock_gmt, CULL_SPOOL)
   SGE_ULONG(JB_override_tickets, CULL_SPOOL)
   SGE_LIST(JB_qs_args, ST_Type, CULL_DEFAULT)
   SGE_LIST(JB_path_aliases, PA_Type, CULL_SPOOL)
   SGE_DOUBLE(JB_urg, CULL_DEFAULT)
   SGE_DOUBLE(JB_nurg, CULL_DEFAULT)
   SGE_DOUBLE(JB_nppri, CULL_DEFAULT)
   SGE_DOUBLE(JB_rrcontr, CULL_DEFAULT)
   SGE_DOUBLE(JB_dlcontr, CULL_DEFAULT)
   SGE_DOUBLE(JB_wtcontr, CULL_DEFAULT)
   SGE_ULONG(JB_ar, CULL_SPOOL)
   SGE_ULONG(JB_pty, CULL_SPOOL)
   SGE_ULONG(JB_ja_task_concurrency, CULL_SPOOL)
   SGE_LIST(JB_binding, BN_Type, CULL_SPOOL)
   SGE_STRING(JB_submission_command_line, CULL_SPOOL)
   SGE_LIST(JB_grp_list, ST_Type, CULL_SPOOL)
   SGE_LIST(JB_joker, VA_Type, CULL_SPOOL)
   SGE_ULONG(JB_sync_options, CULL_SPOOL)
LISTEND

NAMEDEF(JBN)
   NAME("JB_job_number")
   NAME("JB_job_name")
   NAME("JB_version")
   NAME("JB_jid_request_list")
   NAME("JB_jid_predecessor_list")
   NAME("JB_jid_successor_list")
   NAME("JB_ja_ad_request_list")
   NAME("JB_ja_ad_predecessor_list")
   NAME("JB_ja_ad_successor_list")
   NAME("JB_session")
   NAME("JB_project")
   NAME("JB_department")
   NAME("JB_directive_prefix")
   NAME("JB_exec_file")
   NAME("JB_script_file")
   NAME("JB_script_size")
   NAME("JB_script_ptr")
   NAME("JB_submission_time")
   NAME("JB_execution_time")
   NAME("JB_deadline")
   NAME("JB_owner")
   NAME("JB_uid")
   NAME("JB_group")
   NAME("JB_gid")
   NAME("JB_account")
   NAME("JB_cwd")
   NAME("JB_notify")
   NAME("JB_type")
   NAME("JB_reserve")
   NAME("JB_priority")
   NAME("JB_jobshare")
   NAME("JB_shell_list")
   NAME("JB_verify")
   NAME("JB_env_list")
   NAME("JB_context")
   NAME("JB_job_args")
   NAME("JB_checkpoint_attr")
   NAME("JB_checkpoint_name")
   NAME("JB_checkpoint_object")
   NAME("JB_checkpoint_interval")
   NAME("JB_restart")
   NAME("JB_stdout_path_list")
   NAME("JB_stderr_path_list")
   NAME("JB_stdin_path_list")
   NAME("JB_merge_stderr")
   NAME("JB_request_set_list")
   NAME("JB_mail_options")
   NAME("JB_mail_list")
   NAME("JB_pe")
   NAME("JB_pe_range")
   NAME("JB_tgt")
   NAME("JB_cred")
   NAME("JB_ja_structure")
   NAME("JB_ja_n_h_ids")
   NAME("JB_ja_u_h_ids")
   NAME("JB_ja_s_h_ids")
   NAME("JB_ja_o_h_ids")
   NAME("JB_ja_a_h_ids")
   NAME("JB_ja_z_ids")
   NAME("JB_ja_template")
   NAME("JB_ja_tasks")
   NAME("JB_host")
   NAME("JB_category")
   NAME("JB_user_list")
   NAME("JB_job_identifier_list")
   NAME("JB_verify_suitable_queues")
   NAME("JB_soft_wallclock_gmt")
   NAME("JB_hard_wallclock_gmt")
   NAME("JB_override_tickets")
   NAME("JB_qs_args")
   NAME("JB_path_aliases")
   NAME("JB_urg")
   NAME("JB_nurg")
   NAME("JB_nppri")
   NAME("JB_rrcontr")
   NAME("JB_dlcontr")
   NAME("JB_wtcontr")
   NAME("JB_ar")
   NAME("JB_pty")
   NAME("JB_ja_task_concurrency")
   NAME("JB_binding")
   NAME("JB_submission_command_line")
   NAME("JB_grp_list")
   NAME("JB_joker")
   NAME("JB_sync_options")
NAMEEND

#define JB_SIZE sizeof(JBN)/sizeof(char *)


