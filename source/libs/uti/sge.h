#pragma once
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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Cluster-wide names: objects, attributes, files and directories
 */

#define DEFAULT_EDITOR     "vi"                      ///< editor used when `$EDITOR` is not set

/* template/global/default/queue names */
#define SGE_TEMPLATE_NAME        "template"          ///< pseudo object name that selects the configuration template
#define SGE_UNKNOWN_NAME         "unknown"           ///< placeholder shown when a name could not be resolved
#define SGE_GLOBAL_NAME          "global"            ///< pseudo host name for settings that apply cluster-wide
#define SGE_RQS_NAME             "resource_quota"    ///< object name of a resource quota set

/* sge object names */
#define SGE_OBJ_CQUEUE                 "queue"       ///< object name of a cluster queue, as used on the qconf command line
#define SGE_OBJ_HGROUP                 "hostgroup"   ///< object name of a host group
#define SGE_OBJ_EXECHOST               "exechost"    ///< object name of an execution host
#define SGE_OBJ_PE                     "pe"          ///< object name of a parallel environment
#define SGE_OBJ_CKPT                   "ckpt"        ///< object name of a checkpointing environment
#define SGE_OBJ_RQS                    "resource_quota" ///< object name of a resource quota set
#define SGE_OBJ_JOB                    "job"         ///< object name of a job
#define SGE_OBJ_AR                     "advance_reservation" ///< object name of an advance reservation
#define SGE_OBJ_ROLE                   "role"        ///< object name of an RBAC role

/* attribute names of sge objects */
#define SGE_ATTR_LOAD_FORMULA          "load_formula" ///< scheduler attribute: expression ranking hosts by load
#define SGE_ATTR_DYNAMICAL_LIMIT       "dynamical_limit" ///< complex attribute: limit computed from a load value
#define SGE_ATTR_LOAD_SCALING          "load_scaling" ///< exec host attribute: per-host scaling of reported load values
#define SGE_ATTR_PE_LIST               "pe_list"     ///< queue attribute: parallel environments the queue offers
#define SGE_ATTR_HOST_LIST             "hostlist"    ///< host group attribute: member hosts
#define SGE_ATTR_CKPT_LIST             "ckpt_list"   ///< queue attribute: checkpointing environments the queue offers
#define SGE_ATTR_COMPLEX_VALUES        "complex_values" ///< exec host attribute: values of the complex resources it provides
#define SGE_ATTR_LOAD_VALUES           "load_values" ///< exec host attribute: load values last reported
#define SGE_ATTR_PROCESSORS            "processors"  ///< exec host attribute: processor count
#define SGE_ATTR_USER_LISTS            "user_lists"  ///< access attribute: user lists allowed access
#define SGE_ATTR_XUSER_LISTS           "xuser_lists" ///< access attribute: user lists denied access
#define SGE_ATTR_PROJECTS              "projects"    ///< access attribute: projects allowed access
#define SGE_ATTR_RQSRULES              "resource_quota_rules" ///< resource quota set attribute: the rules it contains
#define SGE_ATTR_XPROJECTS             "xprojects"   ///< access attribute: projects denied access
#define SGE_ATTR_USAGE_SCALING         "usage_scaling" ///< exec host attribute: per-host scaling of reported usage
#define SGE_ATTR_SEQ_NO                "seq_no"      ///< queue attribute: sequence number deciding scheduling order
#define SGE_ATTR_LOAD_THRESHOLD        "load_thresholds" ///< queue attribute: load values above which the queue is overloaded
#define SGE_ATTR_SUSPEND_THRESHOLD     "suspend_thresholds" ///< queue attribute: load values above which jobs are suspended
#define SGE_ATTR_NSUSPEND              "nuspend"     ///< queue attribute: how many jobs to suspend per interval
#define SGE_ATTR_SUSPEND_INTERVAL      "suspend_interval" ///< queue attribute: time between suspend rounds
#define SGE_ATTR_PRIORITY              "priority"    ///< queue attribute: nice value jobs are started with
#define SGE_ATTR_MIN_CPU_INTERVAL      "min_cpu_interval" ///< queue attribute: shortest interval between checkpoints
#define SGE_ATTR_PROCESSORS            "processors"  ///< exec host attribute: processor count (duplicate definition, identical value)
#define SGE_ATTR_QTYPE                 "qtype"       ///< queue attribute: which job types the queue accepts
#define SGE_ATTR_RERUN                 "rerun"       ///< queue attribute: whether jobs are rerunnable by default
#define SGE_ATTR_SLOTS                 "slots"       ///< queue attribute: number of job slots
#define SGE_ATTR_TMPDIR                "tmpdir"      ///< queue attribute: directory for a job temporary directory
#define SGE_ATTR_SHELL                 "shell"       ///< queue attribute: shell jobs are started with
#define SGE_ATTR_SHELL_START_MODE      "shell_start_mode" ///< queue attribute: how the shell is invoked
#define SGE_ATTR_PROLOG                "prolog"      ///< queue attribute: command run before a job
#define SGE_ATTR_EPILOG                "epilog"      ///< queue attribute: command run after a job
#define SGE_ATTR_STARTER_METHOD        "starter_method" ///< queue attribute: command that starts a job
#define SGE_ATTR_SUSPEND_METHOD        "suspend_method" ///< queue attribute: command that suspends a job
#define SGE_ATTR_RESUME_METHOD         "resume_method" ///< queue attribute: command that resumes a job
#define SGE_ATTR_TERMINATE_METHOD      "terminate_method" ///< queue attribute: command that terminates a job
#define SGE_ATTR_NOTIFY                "notify"      ///< queue attribute: delay between the warning signal and the real one
#define SGE_ATTR_OWNER_LIST            "owner_list"  ///< queue attribute: users allowed to suspend the queue
#define SGE_ATTR_CALENDAR              "calendar"    ///< queue attribute: calendar controlling availability
#define SGE_ATTR_INITIAL_STATE         "initial_state" ///< queue attribute: state the queue starts in
#define SGE_ATTR_QNAME                 "qname"       ///< queue attribute: the queue name
#define SGE_ATTR_QTYPE                 "qtype"       ///< queue attribute: which job types the queue accepts (duplicate definition, identical value)
#define SGE_ATTR_SUBORDINATE_LIST      "subordinate_list" ///< queue attribute: queues suspended when this one is busy
#define SGE_ATTR_MAIL_LIST             "mail_list"   ///< queue attribute: recipients of queue mail
#define SGE_ATTR_QUEUE_LIST            "queue_list"  ///< attribute naming a list of queues
#define SGE_ATTR_HOSTNAME              "hostname"    ///< exec host attribute: the host name
#define SGE_ATTR_HOSTLIST              "hostlist"    ///< attribute naming a list of hosts
#define SGE_ATTR_PE_NAME               "pe_name"     ///< parallel environment attribute: its name
#define SGE_ATTR_CKPT_NAME             "ckpt_name"   ///< checkpointing environment attribute: its name
#define SGE_ATTR_HGRP_NAME             "group_name"  ///< host group attribute: its name
#define SGE_ATTR_RQS_NAME              "name"        ///< resource quota set attribute: its name
#define SGE_ATTR_CKPT_NAME             "ckpt_name"   ///< checkpointing environment attribute: its name (duplicate definition, identical value)
#define SGE_ATTR_INTERFACE             "interface"   ///< checkpointing environment attribute: the checkpointing interface
#define SGE_ATTR_CKPT_COMMAND          "ckpt_command" ///< checkpointing environment attribute: command that checkpoints a job
#define SGE_ATTR_MIGR_COMMAND          "migr_command" ///< checkpointing environment attribute: command that migrates a job
#define SGE_ATTR_RESTART_COMMAND       "restart_command" ///< checkpointing environment attribute: command that restarts a job
#define SGE_ATTR_CLEAN_COMMAND         "clean_command" ///< checkpointing environment attribute: command that cleans up after a job
#define SGE_ATTR_CKPT_DIR              "ckpt_dir"    ///< checkpointing environment attribute: directory holding checkpoints
#define SGE_ATTR_SIGNAL                "signal"      ///< checkpointing environment attribute: signal that triggers a checkpoint
#define SGE_ATTR_H_FSIZE               "h_fsize"     ///< resource limit: hard limit on file size
#define SGE_ATTR_S_FSIZE               "s_fsize"     ///< resource limit: soft limit on file size
#define SGE_ATTR_H_RT                  "h_rt"        ///< resource limit: hard limit on wall clock run time
#define SGE_ATTR_S_RT                  "s_rt"        ///< resource limit: soft limit on wall clock run time
#define SGE_ATTR_H_CPU                 "h_cpu"       ///< resource limit: hard limit on CPU time
#define SGE_ATTR_S_CPU                 "s_cpu"       ///< resource limit: soft limit on CPU time
#define SGE_ATTR_H_DATA                "h_data"      ///< resource limit: hard limit on the data segment
#define SGE_ATTR_S_DATA                "s_data"      ///< resource limit: soft limit on the data segment
#define SGE_ATTR_H_STACK               "h_stack"     ///< resource limit: hard limit on the stack
#define SGE_ATTR_S_STACK               "s_stack"     ///< resource limit: soft limit on the stack
#define SGE_ATTR_H_CORE                "h_core"      ///< resource limit: hard limit on core file size
#define SGE_ATTR_S_CORE                "s_core"      ///< resource limit: soft limit on core file size
#define SGE_ATTR_H_RSS                 "h_rss"       ///< resource limit: hard limit on resident set size
#define SGE_ATTR_S_RSS                 "s_rss"       ///< resource limit: soft limit on resident set size
#define SGE_ATTR_H_VMEM                "h_vmem"      ///< resource limit: hard limit on virtual memory
#define SGE_ATTR_S_VMEM                "s_vmem"      ///< resource limit: soft limit on virtual memory
#define SGE_ATTR_M_THREAD              "m_thread"    ///< binding attribute: number of hardware threads
#define SGE_ATTR_M_CORE                "m_core"      ///< binding attribute: number of cores

/* attribute values for certain object attributes */
#define SGE_ATTRVAL_MIN                "min"         ///< attribute value selecting the minimum of a range
#define SGE_ATTRVAL_MAX                "max"         ///< attribute value selecting the maximum of a range
#define SGE_ATTRVAL_AVG                "avg"         ///< attribute value selecting the average of a range

/* tmp filenames */
#define TMP_ERR_FILE_SNBU         "/tmp/sge_messages" ///< fallback log file used before the spool directory is known
#define TMP_ERR_FILE_EXECD        "/tmp/execd_messages" ///< fallback log file for the execd
#define TMP_ERR_FILE_SHADOWD      "/tmp/shadowd_messages" ///< fallback log file for the shadow daemon

#define COMMON_DIR               "common"            ///< cell subdirectory holding the shared configuration files
#define SPOOL_DIR                "spool"             ///< cell subdirectory holding the spool areas

#define QMASTER_PID_FILE          "qmaster.pid"      ///< file holding the running qmaster process id
#define EXECD_PID_FILE            "execd.pid"        ///< file holding the running execd process id
#define SHADOWD_PID_FILE          "shadowd_%s.pid"   ///< file holding a shadow daemon process id; `%s` is the host name

#define DEFAULT_ACCOUNT           "sge"              ///< account name recorded when a job names none
#define DEFAULT_CELL              "default"          ///< cell name used when `$SGE_CELL` is not set
#define SHARETREE_FILE            "sharetree"        ///< file holding the share tree
#define ACTIVE_DIR                "active_jobs"      ///< execd spool subdirectory for jobs currently running
#define OSJOBID                   "osjobid"          ///< name of the job id assigned by the operating system
#define ADDGRPID                  "addgrpid"         ///< name of the supplementary group id used to track a job

/* These files exist in the qmaster spool directory. These files may be
 * accessed directly, since they are used after chdir() of qmaster/execd
 * to their spool directory 
 */
#define EXECHOST_DIR              "exec_hosts"       ///< qmaster spool subdirectory holding execution hosts
#define ADMINHOST_DIR             "admin_hosts"      ///< qmaster spool subdirectory holding administration hosts
#define SUBMITHOST_DIR            "submit_hosts"     ///< qmaster spool subdirectory holding submit hosts
#define CQUEUE_DIR                "cqueues"          ///< qmaster spool subdirectory holding cluster queues
#define QINSTANCES_DIR            "qinstances"       ///< qmaster spool subdirectory holding queue instances
#define PE_DIR                    "pe"               ///< qmaster spool subdirectory holding parallel environments
#define ROLE_DIR                  "roles"            ///< qmaster spool subdirectory holding RBAC roles
#define HGROUP_DIR                "hostgroups"       ///< qmaster spool subdirectory holding host groups
#define CENTRY_DIR                "centry"           ///< qmaster spool subdirectory holding complex entries
#define CKPTOBJ_DIR               "ckpt"             ///< qmaster spool subdirectory holding checkpointing environments
#define CAL_DIR                   "calendars"        ///< qmaster spool subdirectory holding calendars
#define RESOURCEQUOTAS_DIR        "resource_quotas"  ///< qmaster spool subdirectory holding resource quota sets
#define AR_DIR                    "advance_reservations" ///< qmaster spool subdirectory holding advance reservations

#define SEQ_NUM_FILE              "jobseqnum"        ///< file holding the next job id to hand out
#define ARSEQ_NUM_FILE            "arseqnum"         ///< file holding the next advance reservation id to hand out
#define ALIAS_FILE                "host_aliases"     ///< file mapping host names to their cluster-wide alias
#define ACT_QMASTER_FILE          "act_qmaster"      ///< file naming the host the qmaster currently runs on

/* These files exist in the qmaster and execd spool area */
#define EXEC_DIR                  "job_scripts"      ///< spool subdirectory holding job scripts
#define JOB_DIR                   "jobs"             ///< spool subdirectory holding jobs
#define ERR_FILE                  "messages"         ///< name of a daemon message log inside its spool directory
#define USER_DIR                  "users"            ///< spool subdirectory holding users
#define USERSET_DIR               "usersets"         ///< spool subdirectory holding user sets
#define PROJECT_DIR               "projects"         ///< spool subdirectory holding projects
#define CONFIG_DIR                "configs"          ///< spool subdirectory holding host configurations

/* delimiters for parsing params */
#define PARAMS_DELIMITER ", "                        ///< separator between entries of a parameter list

#define FALSE_STR                 "FALSE"            ///< canonical spelling of a false boolean in configuration files
#define TRUE_STR                  "TRUE"             ///< canonical spelling of a true boolean in configuration files

#define NONE_STR                  "NONE"             ///< canonical spelling of an unset value in configuration files

/* canonical token for an unlimited resource value (TIME/MEM/INT/DOUBLE). Used for
 * display and as the stored sentinel; parsing remains case-insensitive. */
#define INFINITY_STR              "INFINITY"         ///< canonical token for an unlimited resource value

#define FIRST_CORE                "first_core"       ///< binding keyword selecting the first core of a socket
