#ifndef SGE_JB_L_H
#define SGE_JB_L_H
/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "cull/cull.h"
#include "sgeobj/cull/sge_boundaries.h"

#ifdef __cplusplus
extern "C" {
#endif

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
*    SGE_STRING(JB_exec_file) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_script_file) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_script_size) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_script_ptr) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_submission_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_execution_time) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_deadline) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_owner) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_uid) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_group) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_gid) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_account) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_cwd) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JB_notify) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_type) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JB_reserve) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_priority) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_jobshare) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_shell_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_verify) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_env_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_context) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_job_args) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_checkpoint_attr) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_checkpoint_name) - @todo add summary
*    @todo add description
*
*    SGE_OBJECT(JB_checkpoint_object) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_checkpoint_interval) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_restart) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_stdout_path_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_stderr_path_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_stdin_path_list) - @todo add summary
*    @todo add description
*
*    SGE_BOOL(JB_merge_stderr) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_hard_resource_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_soft_resource_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_hard_queue_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_soft_queue_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_mail_options) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_mail_list) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_pe) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_pe_range) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_master_hard_queue_list) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_tgt) - @todo add summary
*    @todo add description
*
*    SGE_STRING(JB_cred) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_ja_structure) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_ja_n_h_ids) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_ja_u_h_ids) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_ja_s_h_ids) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_ja_o_h_ids) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_ja_a_h_ids) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_ja_z_ids) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_ja_template) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_ja_tasks) - @todo add summary
*    @todo add description
*
*    SGE_HOST(JB_host) - @todo add summary
*    @todo add description
*
*    SGE_REF(JB_category) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_user_list) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_job_identifier_list) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_verify_suitable_queues) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_soft_wallclock_gmt) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_hard_wallclock_gmt) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_override_tickets) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_qs_args) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_path_aliases) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JB_urg) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JB_nurg) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JB_nppri) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JB_rrcontr) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JB_dlcontr) - @todo add summary
*    @todo add description
*
*    SGE_DOUBLE(JB_wtcontr) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_ar) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_pty) - @todo add summary
*    @todo add description
*
*    SGE_ULONG(JB_ja_task_concurrency) - @todo add summary
*    @todo add description
*
*    SGE_LIST(JB_binding) - @todo add summary
*    @todo add description
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
   JB_hard_resource_list,
   JB_soft_resource_list,
   JB_hard_queue_list,
   JB_soft_queue_list,
   JB_mail_options,
   JB_mail_list,
   JB_pe,
   JB_pe_range,
   JB_master_hard_queue_list,
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
   JB_binding
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
   SGE_ULONG(JB_submission_time, CULL_SPOOL)
   SGE_ULONG(JB_execution_time, CULL_SPOOL)
   SGE_ULONG(JB_deadline, CULL_SPOOL)
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
   SGE_LIST(JB_hard_resource_list, CE_Type, CULL_SPOOL)
   SGE_LIST(JB_soft_resource_list, CE_Type, CULL_SPOOL)
   SGE_LIST(JB_hard_queue_list, QR_Type, CULL_SPOOL)
   SGE_LIST(JB_soft_queue_list, QR_Type, CULL_SPOOL)
   SGE_ULONG(JB_mail_options, CULL_SPOOL)
   SGE_LIST(JB_mail_list, MR_Type, CULL_SPOOL)
   SGE_STRING(JB_pe, CULL_SPOOL)
   SGE_LIST(JB_pe_range, RN_Type, CULL_SPOOL)
   SGE_LIST(JB_master_hard_queue_list, QR_Type, CULL_SPOOL)
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
   SGE_ULONG(JB_soft_wallclock_gmt, CULL_SPOOL)
   SGE_ULONG(JB_hard_wallclock_gmt, CULL_SPOOL)
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
   NAME("JB_hard_resource_list")
   NAME("JB_soft_resource_list")
   NAME("JB_hard_queue_list")
   NAME("JB_soft_queue_list")
   NAME("JB_mail_options")
   NAME("JB_mail_list")
   NAME("JB_pe")
   NAME("JB_pe_range")
   NAME("JB_master_hard_queue_list")
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
NAMEEND

#define JB_SIZE sizeof(JBN)/sizeof(char *)

#ifdef __cplusplus
}
#endif

#endif
