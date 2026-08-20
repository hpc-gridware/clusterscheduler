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
 *  Portions of this software are Copyright (c) 2011-2012 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The cluster configuration: defaults, merging, and thread safe access
 *
 * The global and each host's local configuration arrive as CULL lists.
 * #merge_configuration layers them - compiled in defaults, then the global
 * configuration, then the local one - and writes the result into the file
 * static `Master_Config`, which the rest of the component reads through the
 * `mconf_get_*` accessors rather than by parsing lists again.
 *
 * Every accessor takes `LOCK_MASTER_CONF` as a read lock, so a caller sees a
 * consistent value even while a new configuration is being applied. The two
 * free form attributes `qmaster_params` and `execd_params` are parsed into
 * separate file static variables, which is why many accessors do not name a
 * #confel member.
 *
 * @see sge_conf.h
 */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#ifdef LINUX
#include <mcheck.h>
#endif

#include "cull/cull.h"

#include "uti/sge.h"
#include "uti/config_file.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "comm/commlib.h"

#include "sgeobj/ocs_TopologyString.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/cull/sge_usage_UA_L.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_utility.h"

#include <cinttypes>

/// Name of the binary directory below `$SGE_ROOT`
#define SGE_BIN "bin"
/// Default `stree_spool_time`: seconds between two share tree spool runs
#define STREESPOOLTIMEDEF 240

/**
 * @name STREE_TICK_INTERVAL bounds
 *
 * Seconds between two runs of the periodic share tree decay and republish
 * tick, which the Timed Event Thread drives (CS-1239). A configured value
 * outside the range is clamped when it is read.
 * @{
 */
#define STREE_TICK_INTERVAL_DEF 5   ///< default when `qmaster_params` does not set it
#define STREE_TICK_INTERVAL_MIN 1   ///< smallest accepted value
#define STREE_TICK_INTERVAL_MAX 300 ///< largest accepted value
/** @} */

/**
 * @name Finished job sweep bounds
 *
 * Bounds for the sweep behaviour pair `finished_jobs_sweep_interval` and
 * `finished_jobs_sweep_batch` (CS-1908). These are tuning knobs, so they live
 * in `qmaster_params`. The retention semantics pair - `finished_jobs_keep_time`
 * and `finished_jobs_max` - are top level global configuration attributes
 * instead; see #confel.
 * @{
 */
#define FINISHED_JOBS_SWEEP_INTERVAL_DEF   10       ///< default seconds between sweep ticks
#define FINISHED_JOBS_SWEEP_INTERVAL_MIN   1        ///< smallest accepted interval
#define FINISHED_JOBS_SWEEP_INTERVAL_MAX   3600     ///< largest accepted interval, one hour
#define FINISHED_JOBS_SWEEP_BATCH_DEF      100      ///< default maximum prunes per tick
#define FINISHED_JOBS_SWEEP_BATCH_MIN      1        ///< smallest accepted batch size
#define FINISHED_JOBS_SWEEP_BATCH_MAX      100000   ///< largest accepted batch size
/** @} */

/* This list is *ONLY* used by the execd and should be moved eventually */
lList *Execd_Config_List = nullptr;

/// The cluster configuration, as one C struct rather than a CULL element
struct confel {
   char        *execd_spool_dir;     ///< sge_spool directory base path
   char        *mailer;              ///< path to e-mail delivery agent
   char        *xterm;               ///< xterm path for interactive jobs
   char        *load_sensor;         ///< path to a load sensor executable
   char        *prolog;              ///< start before jobscript may be none
   char        *epilog;              ///< start after jobscript may be none
   char        *shell_start_mode;    ///< script_from_stdin/posix_compliant/unix_behavior
   char        *login_shells;        ///< list of shells to call as login shell
   uint32_t    min_uid;              ///< lower bound on UIDs that can qsub
   uint32_t    min_gid;              ///< lower bound on GIDs that can qsub
   uint32_t    load_report_time;     ///< how often to send in load
   uint32_t    max_unheard;          ///< how long before sge_execd considered dead
   uint32_t    loglevel;             ///< qmaster event logging level
   char        *enforce_project;     ///< SGEEE attribute: "true" or "false"
   char        *enforce_user;        ///< SGEEE attribute: "true" or "false"
   char        *administrator_mail;  ///< list of mail addresses
   char        *mail_tag;            ///< mail tag
   lList       *user_lists;          ///< allowed user lists
   lList       *xuser_lists;         ///< forbidden users lists
   lList       *projects;            ///< allowed project list
   lList       *xprojects;           ///< forbiddent project list
   char        *set_token_cmd;       ///< command that acquires an AFS token, from `set_token_cmd`
   char        *pag_cmd;             ///< command that puts the job into its own PAG, from `pag_cmd`
   uint32_t    token_extend_time;    ///< how long an AFS token is extended for, in seconds
   char        *shepherd_cmd;        ///< replacement for the built in shepherd, from `shepherd_cmd`
   char        *qmaster_params;      ///< free form `KEY=VALUE` list of qmaster tuning parameters
   char        *execd_params;        ///< free form `KEY=VALUE` list of execd tuning parameters
   char        *reporting_params;    ///< free form `KEY=VALUE` list of reporting parameters
   char        *gid_range;           ///< Range of additional group ids
   char        *port_range;          ///< Range of ports for qrsh client to bind to
   char        *qlogin_daemon;       ///< eg /usr/sbin/in.telnetd
   char        *qlogin_command;      ///< eg telnet $HOST $PORT
   char        *rsh_daemon;          ///< eg /usr/sbin/in.rshd
   char        *rsh_command;         ///< eg rsh -p $PORT $HOST command
   char        *jsv_url;             ///< jsv url
   char        *jsv_allowed_mod;     ///< allowed modifications for end users if JSV is enabled
   char        *gdi_request_limits;  ///< request limits for GDI commands
   char        *rlogin_daemon;       ///< eg /usr/sbin/in.rlogind
   char        *rlogin_command;      ///< eg rlogin -p $PORT $HOST
   uint32_t    reschedule_unknown;   ///< timout value used for auto. resch.
   uint32_t    max_aj_instances;     ///< max. number of ja instances of a job
   uint32_t    max_aj_tasks;         ///< max. size of an array job
   uint32_t    max_u_jobs;           ///< max. number of jobs per user
   uint32_t    max_jobs;             ///< max. number of jobs in the system
   uint32_t    max_advance_reservations; ///< max. number of advance reservations in the system
   uint32_t    auto_user_fshare;     ///< SGEEE automatic user fshare
   uint32_t    auto_user_oticket;    ///< SGEEE automatic user oticket
   char        *auto_user_default_project; ///< SGEEE automatic user default project
   uint32_t    auto_user_delete_time; ///< SGEEE automatic user delete time
   char        *delegated_file_staging; ///< drmaa attribute: "true" or "false"
   char        *libjvm_path;         ///< libjvm_path for jvm_thread
   char        *additional_jvm_args; ///< additional_jvm_args for jvm_thread
   char        *binding_params;      ///< string containing al binding specific parameters
   char        *jsv_params;          ///< string containing jsv specific parameters
   char        *topology_file;       ///< None or path to a hwloc topology file
   uint32_t    finished_jobs_keep_time; ///< CS-1908: seconds a finished ja_task is retained; 0 = time dimension off
   uint32_t    finished_jobs_max;    ///< CS-1908: global count ceiling on retained finished ja_tasks; 0 = count dimension off
};

/// The cluster configuration; see #confel
typedef struct confel sge_conf_type;

static sge_conf_type Master_Config = {
   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0,
   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr,
   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
   nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0,
   0, 0, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
   0, 0   /* CS-1908: finished_jobs_keep_time, finished_jobs_max */
};
static bool is_new_config = false;

/// Default for `DISABLE_SECONDARY_DS_READER`: reader threads use the snapshot store
#define DEFAULT_DISABLE_SECONDARY_DS_READER (false)

/// Default for `DISABLE_SECONDARY_DS_EXECD`: execd requests use the snapshot store
#define DEFAULT_DISABLE_SECONDARY_DS_EXECD (false)

/// Default for `DISABLE_AUTOMATIC_SESSIONS`: sessions are created automatically
#define DEFAULT_DISABLE_AUTOMATIC_SESSIONS (false)


#ifdef LINUX
#endif




/// Everything #merge_configuration parses out of `reporting_params`
///
/// The default of each setting is written here and nowhere else -- adding a
/// member is all it takes, there is no second list to keep in step. See
/// #binding_params_t for why that matters.
struct reporting_params_t {
   /// `accounting` -- write the accounting file that qacct(1) reads
   bool do_accounting = true;
   /// `reporting` -- write the reporting file
   bool do_reporting = false;
   /// `monitoring` -- write monitoring records to the reporting file
   bool do_monitoring = false;
   /// `joblog` -- write job log records to the reporting file
   bool do_joblog = false;
   /// `flush_time` -- how long reporting records may be buffered, in seconds
   int reporting_flush_time = 15;
   /// `accounting_flush_time` -- same for accounting, negative means "with the reporting flush"
   int accounting_flush_time = -1;
   /// `sync` -- flush every record to disk as it is written
   bool reporting_sync_write = false;
   /// `old_accounting` -- write the accounting file in the pre-9.0 format
   bool old_accounting = false;
   /// `old_reporting` -- write the reporting file in the pre-9.0 format
   bool old_reporting = false;
   /// `sharelog` -- interval of share tree log records, 0 switches them off
   int sharelog_time = 0;
   /// `log_consumables` -- include consumable values in the reporting records
   bool log_consumables = false;
   /// `usage_patterns` -- patterns that map job usage to accounting fields
   std::string usage_patterns;
   /// `online_usage` -- usage values reported while a job is still running
   std::vector<std::string> online_usage_vars;
};
static reporting_params_t reporting_conf;

/// Everything #merge_configuration parses out of `binding_params`
///
/// The default of each setting is written here and nowhere else. Before parsing
/// the string, #merge_configuration restores all of them at once with
/// `binding_conf = {}`, so that removing a token from the configuration reverts
/// that setting instead of leaving the previously parsed value in place. A
/// hand-written list of assignments would have to be kept in step with the
/// members, and drifting apart is what CS-2495 and CS-2564 were.
struct binding_params_t {
   /// `enabled` -- whether binding requests are honoured at all
   bool is_binding_enabled = true;
   /// `implicit` -- bind jobs that did not ask for binding themselves
   bool do_implicit_binding = false;
   /// `on_any_host` -- schedule binding jobs on hosts that report no topology
   bool schedule_on_any_host = true;
   /// `mode` -- how the topology of an execution host is reported
   binding_mode_t binding_mode = BINDING_MODE_DEFAULT;
   /// `default_unit` -- unit an implicit binding request is counted in
   ocs::BindingUnit::Unit default_binding_unit = ocs::BindingUnit::CCORE;
   /// `filter` -- cores that are kept free of job binding
   std::string binding_filter = NONE_STR;
};
static binding_params_t binding_conf;

/* generally simulate all execd's */

/* allow the simulation of jobs (job spooling and execution on execd side is disabled) */

/*
 * This value overrides the default scheduler timeout (10 minutes)
 * to allow graceful degradation on extremely busy systems with
 * tens of thousands or hundreds of thousands of pending jobs.
 */


/**
 * This value specifies the minimum time for spooling the sharetree usage.
 * It is used and evaluated in the sge_follow module. The users and
 * projects are spooled, when the qmaster goes down.
 */

/* CS-1239: STREE_TICK_INTERVAL qmaster_param - seconds between periodic
 * decay ticks driven from the Timed Event Thread. See
 * mconf_get_sharetree_tick_interval(). */

/* CS-1908: finished-job retention sweep-behaviour qmaster_params.
 * (Retention semantics -- finished_jobs_keep_time, finished_jobs_max -- are
 * top-level Master_Config attributes, not qmaster_params.)
 * See mconf_get_finished_jobs_sweep_{interval,batch} accessors. */

/// Default `MAX_DS_DEVIATION`: milliseconds before an update of the secondary data store is enforced
#define DEFAULT_DS_DEVIATION (1000)

/*
 * Reserved usage flags
 *
 * In SGE, hosts which support DR (dynamic repriorization) default to using
 * actual usage so we initialize the reserved usage flags to false. In
 * SGE, hosts which do not support DR default to using reserved usage for
 * sharetree purposes and actual usage for accounting purposes. For Sge,
 * hosts will default to using actual usage for accounting purposes. Sge
 * does not support the sharetree flag so it doesn't matter.
 */


/*
 * Use primary group of qsub-host also for the job execution
 */

/*
 * Job environment inheritance
 */
/* This should match the default set in
 * shepherd/builtin_starter.c:inherit_env(). */

/// Default `GPERF_NAME`: base name of the gperftools profile files
#define GPERF_NAME_DEFAULT "gperf"
/// Default `GPERF_THREADS`: which threads are profiled
#define GPERF_THREADS_DEFAULT "none"

/// Everything #merge_configuration parses out of `qmaster_params`
///
/// The default of each setting is written here and nowhere else. Before parsing
/// the string, #merge_configuration restores all of them at once with
/// `qmaster_conf = {}`, so that removing a token reverts that setting instead of
/// leaving the previously parsed value in place (CS-2564).
struct qmaster_params_t {
   /// `FORBID_RESCHEDULE` -- refuse to reschedule a job that asks for it
   bool forbid_reschedule = false;
   /// `FORBID_APPERROR` -- treat an application error as a job error
   bool forbid_apperror = false;
   /// `ENABLE_FORCED_QDEL` -- let a plain user force the deletion of a job
   bool enable_forced_qdel = false;
   /// `ENABLE_SUP_GRP_EVAL` -- evaluate supplementary groups in access lists
   bool enable_sup_grp_eval = false;
   /// `ENABLE_ENFORCE_MASTER_LIMIT` -- apply the limit to the master task alone
   bool enable_enforce_master_limit = false;
   /// `__TEST_SLEEP_AFTER_REQUEST` -- test hook that delays every request
   bool enable_test_sleep_after_request = false;
   /// `ENABLE_FORCED_QDEL_IF_UNKNOWN` -- force deletion while the host is unknown
   bool enable_forced_qdel_if_unknown = false;
   /// `ALLOW_ANY_SUBMITHOSTS` -- allow any host to be submit host
   bool allow_any_submithosts = false;
   /// `LOG_MONITOR_MESSAGE` -- write monitoring output to the messages file
   bool is_monitor_message = true;
   /// `DISABLE_AUTO_RESCHEDULING` -- never reschedule a job automatically
   bool disable_reschedule = false;
   /// `DISABLE_SECONDARY_DS` -- do not serve requests from the secondary data store
   bool disable_secondary_ds = false;
   /// `DISABLE_SECONDARY_DS_READER` -- same, for the reader threads only
   bool disable_secondary_ds_reader = DEFAULT_DISABLE_SECONDARY_DS_READER;
   /// `DISABLE_SECONDARY_DS_EXECD` -- same, for execution daemon requests only
   bool disable_secondary_ds_execd = DEFAULT_DISABLE_SECONDARY_DS_EXECD;
   /// `DISABLE_AUTOMATIC_SESSIONS` -- do not open a session per client automatically
   bool disable_automatic_sessions = DEFAULT_DISABLE_AUTOMATIC_SESSIONS;
   /// `ijs_escape_char` -- IJS disconnect escape char, `\0` disables it
   char s_ijs_escape_char = '~';
   /// `ijs_keepalive_interval` -- seconds between IJS keepalive probes, 0 disables them
   int s_ijs_keepalive_interval = 60;
   /// `ijs_keepalive_count` -- unanswered keepalives tolerated before disconnect
   int s_ijs_keepalive_count = 3;
   /// `ijs_reconnect_timeout` -- seconds the shepherd waits for a reconnect, 0 disables it
   int s_ijs_reconnect_timeout = 0;
   /// `PROF_LISTENER` -- profile the listener threads
   bool prof_listener_thrd = false;
   /// `PROF_WORKER` -- profile the worker threads
   bool prof_worker_thrd = false;
   /// `PROF_SIGNAL` -- profile the signal thread
   bool prof_signal_thrd = false;
   /// `PROF_SCHEDULER` -- profile the scheduler thread
   bool prof_scheduler_thrd = false;
   /// `PROF_DELIVER` -- profile the delivery thread
   bool prof_deliver_thrd = false;
   /// `PROF_TEVENT` -- profile the timed event thread
   bool prof_tevent_thrd = false;
   /// `MONITOR_TIME` -- interval of the monitoring output, 0 switches it off
   uint32_t monitor_time = 0;
   /// `ENABLE_RESCHEDULE_KILL` -- kill a job that is being rescheduled
   bool enable_reschedule_kill = false;
   /// `ENABLE_RESCHEDULE_SLAVE` -- reschedule slave tasks along with the master
   bool enable_reschedule_slave = false;
   /// `OLD_RESCHEDULE_BEHAVIOR` -- reschedule as releases before 9.0 did
   bool old_reschedule_behavior = false;
   /// `OLD_RESCHEDULE_BEHAVIOR_ARRAY_JOB` -- same, for array jobs
   bool old_reschedule_behavior_array_job = false;
   /// `ENABLE_MTRACE` -- switch on the glibc allocation trace
   bool enable_mtrace = false;
   /// `MAX_DYN_EC` -- how many dynamic event clients may register
   int max_dynamic_event_clients = 1000;
   /// `SIMULATE_EXECDS` -- pretend that execution daemons are there
   bool simulate_execds = false;
   /// `SCHEDULER_TIMEOUT` -- seconds the qmaster waits for the scheduler
   int scheduler_timeout = 0;
   /// `STREE_SPOOL_INTERVAL` -- how often the share tree usage is spooled
   int spool_time = STREESPOOLTIMEDEF;
   /// `STREE_TICK_INTERVAL` -- how often share tree usage is decayed
   int sharetree_tick_interval = STREE_TICK_INTERVAL_DEF;
   /// `FINISHED_JOBS_SWEEP_INTERVAL` -- how often finished jobs are swept
   int finished_jobs_sweep_interval = FINISHED_JOBS_SWEEP_INTERVAL_DEF;
   /// `FINISHED_JOBS_SWEEP_BATCH` -- how many finished jobs one sweep removes
   int finished_jobs_sweep_batch = FINISHED_JOBS_SWEEP_BATCH_DEF;
   /// `MAX_DS_DEVIATION` -- how far the secondary data store may lag behind
   int max_ds_deviation = DEFAULT_DS_DEVIATION;
   /// `ENABLE_SUBMIT_LIB_PATH` -- a client may pass its library path along
   bool enable_submit_lib_path = false;
   /// `ENABLE_SUBMIT_LD_PRELOAD` -- a client may pass LD_PRELOAD along
   bool enable_submit_ld_preload = false;
   /// `max_job_deletion_time` -- seconds one job deletion request may take
   int max_job_deletion_time = 3;
   /// `jsv_timeout` -- seconds a JSV script may take to answer
   int jsv_timeout = 10;
   /// `jsv_threshold` -- runtime above which a JSV script is logged, in ms
   int jsv_threshold = 5000;
   /// `GPERF_NAME` -- name of the gperftools profile
   std::string gperf_name = GPERF_NAME_DEFAULT;
   /// `GPERF_THREADS` -- which threads gperftools profiles
   std::string gperf_threads = GPERF_THREADS_DEFAULT;
};
static qmaster_params_t qmaster_conf;

/// Settings #merge_configuration accepts in `qmaster_params` and in
/// `execd_params` alike
///
/// They are restored once, before the first of the two loops. Giving them to
/// either #qmaster_params_t or #execd_params_t would break them: a reset per
/// string would let the second loop discard what the first one had parsed.
/// Otherwise the same rule as everywhere else -- the default lives at the
/// member and nowhere else.
struct security_params_t {
   /// `NO_SECURITY` (stored inverted) -- exchange credentials with the clients
   bool do_credentials = true;
   /// `NO_AUTHENTICATION` (stored inverted) -- authenticate the client of every request
   bool do_authentication = true;
};
static security_params_t security_conf;

/*
 * notify_kill_default and notify_susp_default
 *       0  -> use the signal type stored in notify_kill and notify_susp
 *       1  -> user default signale (USR1 for susp and usr2 for kill)
 *       2  -> do not send a signal
 *
 * notify_kill and notify_susp:
 *       !nullptr -> Name of the signale (later used in sys_string2signal)
 *
 * These two own their memory and are deliberately not members of
 * execd_params_t: a value-initialised struct would overwrite the pointer
 * instead of freeing it. They stay meaningful only while the matching
 * notify_*_type is 0, and that type does revert with the struct.
 */
static char* notify_susp = nullptr;
static char* notify_kill = nullptr;

/// Everything #merge_configuration parses out of `execd_params`
///
/// The default of each setting is written here and nowhere else. Before parsing
/// the string, #merge_configuration restores all of them at once with
/// `execd_conf = {}`. `PDC_INTERVAL` surviving a configuration that no longer
/// mentioned it is what CS-2495 was: the execution daemon then stopped
/// registering jobs and silently enforced no wallclock limits.
struct execd_params_t {
   /// `ACCT_RESERVED_USAGE` -- account the reserved rather than the measured usage
   bool acct_reserved_usage = false;
#ifdef COMPILE_DC
   /// `SHARETREE_RESERVED_USAGE` -- same for the share tree
   bool sharetree_reserved_usage = false;
#else
   /// `SHARETREE_RESERVED_USAGE` -- same for the share tree
   bool sharetree_reserved_usage = true;
#endif
   /// `ENABLE_ADDGRP_KILL` -- kill a job by its supplementary group id
   bool enable_addgrp_kill = false;
   /// `ENABLE_HWLOC` -- read the execution host topology through hwloc
   bool enable_hwloc = true;
   /// `ENABLE_MEM_DETAILS` -- report the detailed memory usage of a job
   bool enable_mem_details = false;
   /// `ENABLE_SYSTEMD` -- start jobs inside a systemd scope where available
   bool enable_systemd = true;
   /// `IGNORE_NGROUPS_MAX_LIMIT` -- keep going when NGROUPS_MAX looks too small
   bool ignore_ngroups_max_limit = false;
   /// `INHERIT_ENV` -- a job inherits the environment of the execution daemon
   bool inherit_env = true;
   /// `KEEP_ACTIVE` -- keep the active_jobs directory of a finished job
   keep_active_t keep_active = KEEP_ACTIVE_FALSE;
   /// `NOTIFY_KILL` -- 0 uses the signal name kept outside this struct,
   /// 1 the default signal, 2 sends none
   int notify_kill_type = 1;
   /// `NOTIFY_SUSP` -- as #execd_params_t::notify_kill_type, for suspension
   int notify_susp_type = 1;
   /// `PDC_INTERVAL` -- how often the usage collector runs, #PDC_DISABLED switches it off
   uint64_t pdc_interval = sge_gmt32_to_gmt64(1);
   /// `PROF_EXECD_THRD` -- profile the execution daemon main thread
   bool prof_execd_thrd = false;
   /// `PTF_MAX_PRIORITY` -- upper bound the PTF may give a job
   long ptf_max_priority = -999;
   /// `PTF_MIN_PRIORITY` -- lower bound the PTF may give a job
   long ptf_min_priority = -999;
   /// `SCRIPT_TIMEOUT` -- runtime allowed to prolog, epilog and PE scripts, in seconds
   uint32_t script_timeout = 120;
   /// `SET_LIB_PATH` -- put the product library path into the job environment
   bool set_lib_path = false;
   /// `SIMULATE_JOBS` -- do not start jobs, only pretend that they ran
   bool simulate_jobs = false;
   /// `USAGE_COLLECTION` -- where job usage is read from
   usage_collection_t usage_collection = USAGE_COLLECTION_DEFAULT;
   /// `USE_QIDLE` -- take idle time into account
   bool use_qidle = false;
   /// `USE_QSUB_GID` -- run the job under the group id of the submitting client
   bool use_qsub_gid = false;
   /// `S_DESCRIPTORS` -- soft limit on open file descriptors
   char s_descriptors[100] = "UNDEFINED";
   /// `H_DESCRIPTORS` -- hard limit on open file descriptors
   char h_descriptors[100] = "UNDEFINED";
   /// `S_MAXPROC` -- soft limit on processes
   char s_maxproc[100] = "UNDEFINED";
   /// `H_MAXPROC` -- hard limit on processes
   char h_maxproc[100] = "UNDEFINED";
   /// `S_MEMORYLOCKED` -- soft limit on locked memory
   char s_memorylocked[100] = "UNDEFINED";
   /// `H_MEMORYLOCKED` -- hard limit on locked memory
   char h_memorylocked[100] = "UNDEFINED";
   /// `S_LOCKS` -- soft limit on file locks
   char s_locks[100] = "UNDEFINED";
   /// `H_LOCKS` -- hard limit on file locks
   char h_locks[100] = "UNDEFINED";
};
static execd_params_t execd_conf;

/// One entry of the built in default configuration
typedef struct {
  const char *name;              ///< name of parameter
  int local;               ///< 0 | 1 -> local -> may be overidden by local conf
  const char *value;             ///< value of parameter
  int isSet;               ///< 0 | 1 -> is already set
  char *envp;              ///< pointer to environment variable
} tConfEntry;

static void sge_set_defined_defaults(const char *cell_root, lList **lpCfg);
static void setConfFromCull(lList *lpCfg);
static tConfEntry *getConfEntry(const char *name, tConfEntry conf_entries[]);
static void clean_conf();

/*
 * This value is used to override the default value for time
 * in which the qmaster tries deleting jobs, after which it
 * stops deleting and deletes remaining jobs at a later time.
 */


/**
 * @name Compiled in defaults of the global configuration
 *
 * These are what `sge_set_defined_defaults` writes into a configuration that
 * an administrator has never touched. Each corresponds to one attribute of
 * `qconf -sconf`.
 * @{
 */
/// `mailer`: the mail delivery agent
#define MAILER                    "/bin/mail"
/// `prolog`: script run before the job script
#define PROLOG                    "none"
/// `epilog`: script run after the job script
#define EPILOG                    "none"
/// `shell_start_mode`: how the job script is handed to the shell
#define SHELL_START_MODE          "posix_compliant"
/// `login_shells`: shells that are started as a login shell
#define LOGIN_SHELLS              "none"
/// `min_uid`: lowest uid allowed to submit
#define MIN_UID                   "0"
/// `min_gid`: lowest gid allowed to submit
#define MIN_GID                   "0"
/// `max_unheard`: how long an execd may stay silent before it counts as unknown
#define MAX_UNHEARD               "0:2:30"
/// `load_report_time`: how often an execd reports load
#define LOAD_LOG_TIME             "0:0:40"
/// `stat_log_time`: how often statistics are logged
#define STAT_LOG_TIME             "0:15:0"
/// `loglevel`: how much qmaster logs
#define LOGLEVEL                  "log_info"
/// `admin_user`: the account the daemons run as
#define ADMIN_USER                "none"
/// `reschedule_unknown`: how long to wait before rescheduling jobs of an unknown host
#define RESCHEDULE_UNKNOWN        "0:0:0"
/// `ignore_fqdn`: compare host names without their domain
#define IGNORE_FQDN               "true"
/// `max_aj_instances`: how many tasks of one array job may run at a time
#define MAX_AJ_INSTANCES          "2000"
/// `max_aj_tasks`: largest array job that may be submitted
#define MAX_AJ_TASKS              "75000"
/// `max_u_jobs`: jobs one user may have in the system; 0 is unlimited
#define MAX_U_JOBS                "0"
/// `max_jobs`: jobs in the whole system; 0 is unlimited
#define MAX_JOBS                  "0"
/// `max_advance_reservations`: advance reservations in the system; 0 is unlimited
#define MAX_ADVANCE_RESERVATIONS  "0"
/// `finished_jobs_keep_time`: seconds a finished task is retained; 0 turns the time dimension off
#define FINISHED_JOBS_KEEP_TIME   "0"
/// `finished_jobs_max`: ceiling on retained finished tasks; 0 turns the count dimension off
#define FINISHED_JOBS_MAX         "0"
/// `reporting_params`: what is written to the reporting and accounting files
#define REPORTING_PARAMS          "accounting=true reporting=false flush_time=00:00:15 joblog=false sharelog=00:00:00"
/** @} */

/*
 * The order of this array defines the order in which the entries of a global or local
 * configuration are shown by qconf -sconf / qconf -mconf and written to the spool file.
 * Configurations entering the qmaster via qconf -aconf/-Aconf/-mconf/-Mconf are sorted
 * into this order by conf_sort_entries(), and the installer (PrintConf/PrintLocalConf
 * in the install modules) writes the initial configurations in the same order.
 * When a new configuration parameter is added, insert it into the group it belongs to
 * and add it to the installer at the same position.
 */
static tConfEntry conf_entries[] = {
 { "execd_spool_dir",            1, nullptr,                   1, nullptr},
 { "mailer",                     1, MAILER,                    1, nullptr},
 { "xterm",                      1, "/usr/bin/X11/xterm",      1, nullptr},
 { "load_sensor",                1, NONE_STR,                  1, nullptr},
 { "prolog",                     1, PROLOG,                    1, nullptr},
 { "epilog",                     1, EPILOG,                    1, nullptr},
 { "shell_start_mode",           1, SHELL_START_MODE,          1, nullptr},
 { "login_shells",               1, LOGIN_SHELLS,              1, nullptr},
 { "min_uid",                    0, MIN_UID,                   1, nullptr},
 { "min_gid",                    0, MIN_GID,                   1, nullptr},
 { "user_lists",                 0, NONE_STR,                  1, nullptr},
 { "xuser_lists",                0, NONE_STR,                  1, nullptr},
 { "projects",                   0, NONE_STR,                  1, nullptr},
 { "xprojects",                  0, NONE_STR,                  1, nullptr},
 { "enforce_project",            0, "false",                   1, nullptr},
 { "enforce_user",               0, "false",                   1, nullptr},
 { "load_report_time",           1, LOAD_LOG_TIME,             1, nullptr},
 { "max_unheard",                0, MAX_UNHEARD,               1, nullptr},
 { "reschedule_unknown",         1, RESCHEDULE_UNKNOWN,        1, nullptr},
 { "loglevel",                   0, LOGLEVEL,                  1, nullptr},
 { "administrator_mail",         0, NONE_STR,                  1, nullptr},
 { "mail_tag",                   0, NONE_STR,                  1, nullptr},
 { "set_token_cmd",              1, NONE_STR,                  1, nullptr},
 { "pag_cmd",                    1, NONE_STR,                  1, nullptr},
 { "token_extend_time",          1, "24:0:0",                  1, nullptr},
 { "shepherd_cmd",               1, NONE_STR,                  1, nullptr},
 { "qmaster_params",             0, NONE_STR,                  1, nullptr},
 { "execd_params",               1, NONE_STR,                  1, nullptr},
 { "reporting_params",           1, REPORTING_PARAMS,          1, nullptr},
 { "binding_params",             0, BINDING_PARAMS_DEFAULT,    1, nullptr},
 { "gdi_request_limits",         0, NONE_STR,                  1, nullptr},
 { "jsv_url",                    0, NONE_STR,                  1, nullptr},
 { "jsv_allowed_mod",            0, NONE_STR,                  1, nullptr},
 { "jsv_params",                 0, NONE_STR,                  1, nullptr},
 { "gid_range",                  1, NONE_STR,                  1, nullptr},
 { "qlogin_daemon",              1, NONE_STR,                  1, nullptr},
 { "qlogin_command",             1, NONE_STR,                  1, nullptr},
 { "rlogin_daemon",              1, NONE_STR,                  1, nullptr},
 { "rlogin_command",             1, NONE_STR,                  1, nullptr},
 { "rsh_daemon",                 1, NONE_STR,                  1, nullptr},
 { "rsh_command",                1, NONE_STR,                  1, nullptr},
 { "port_range",                 0, NONE_STR,                  1, nullptr},
 { "max_aj_instances",           0, MAX_AJ_INSTANCES,          1, nullptr},
 { "max_aj_tasks",               0, MAX_AJ_TASKS,              1, nullptr},
 { "max_u_jobs",                 0, MAX_U_JOBS,                1, nullptr},
 { "max_jobs",                   0, MAX_JOBS,                  1, nullptr},
 { "max_advance_reservations",   0, MAX_ADVANCE_RESERVATIONS,  1, nullptr},
 { "finished_jobs_keep_time",    0, FINISHED_JOBS_KEEP_TIME,   1, nullptr},
 { "finished_jobs_max",          0, FINISHED_JOBS_MAX,         1, nullptr},
 { "auto_user_oticket",          0, "0",                       1, nullptr},
 { "auto_user_fshare",           0, "0",                       1, nullptr},
 { "auto_user_default_project",  0, NONE_STR,                  1, nullptr},
 { "auto_user_delete_time",      0, "0",                       1, nullptr},
 { "delegated_file_staging",     0, "false",                   1, nullptr},
 { "libjvm_path",                1, "",                        1, nullptr},
 { "additional_jvm_args",        1, "",                        1, nullptr},
 { "topology_file",              1, NONE_STR,                  1, nullptr},
 { nullptr,                      0, nullptr,                   0, nullptr}
};

/**
 * @brief Classify a config parameter's list-ness for JSON rendering (CS-2313a).
 *
 * The two sets below mirror exactly how the config loader parses them above: the value
 * lists are split via lString2ListNone(..., " \t,"); the *_params are tokenized via
 * sge_strtok_r(..., PARAMS_DELIMITER) into KEY=VALUE entries. Everything else is a
 * plain scalar string.
 *
 * @param name  config parameter name
 * @return CONF_PARAM_VALUE_LIST, CONF_PARAM_NAMEVALUE_LIST, or CONF_PARAM_SCALAR
 */
conf_param_list_type_t
config_param_list_type(const char *name) {
   if (name == nullptr) {
      return CONF_PARAM_SCALAR;
   }
   static const char *const value_lists[] = {
      "user_lists", "xuser_lists", "projects", "xprojects",
      "login_shells", "jsv_allowed_mod", "load_sensor", "administrator_mail",
      "gid_range", "port_range", nullptr
   };
   static const char *const namevalue_lists[] = {
      "qmaster_params", "execd_params", "reporting_params", "binding_params", "jsv_params", nullptr
   };
   for (const char *const *p = value_lists; *p != nullptr; p++) {
      if (strcmp(name, *p) == 0) {
         return CONF_PARAM_VALUE_LIST;
      }
   }
   for (const char *const *p = namevalue_lists; *p != nullptr; p++) {
      if (strcmp(name, *p) == 0) {
         return CONF_PARAM_NAMEVALUE_LIST;
      }
   }
   return CONF_PARAM_SCALAR;
}

/**
 * @brief Numeric value type of a config scalar parameter (CS-2313a).
 *
 * Mirrors the chg_conf_val(..., &intval, INT|TIME) calls in the config loader (params
 * parsed into a numeric/time field). Everything else (string targets) is NONE ->
 * rendered as a string.
 *
 * @param name  config parameter name
 * @return the complex entry type (INT/TIME), or NONE for a plain string parameter
 */
ocs::CEntry::Type
config_param_value_type(const char *name) {
   if (name == nullptr) {
      return ocs::CEntry::Type::NONE;
   }
   static const char *const int_params[] = {
      "min_uid", "min_gid", "max_aj_instances", "max_aj_tasks", "max_u_jobs",
      "max_jobs", "max_advance_reservations", "finished_jobs_max",
      "auto_user_oticket", "auto_user_fshare", nullptr
   };
   static const char *const time_params[] = {
      "load_report_time", "max_unheard", "token_extend_time", "reschedule_unknown",
      "finished_jobs_keep_time", "auto_user_delete_time", nullptr
   };
   for (const char *const *p = int_params; *p != nullptr; p++) {
      if (strcmp(name, *p) == 0) {
         return ocs::CEntry::Type::INT;
      }
   }
   for (const char *const *p = time_params; *p != nullptr; p++) {
      if (strcmp(name, *p) == 0) {
         return ocs::CEntry::Type::TIME;
      }
   }
   return ocs::CEntry::Type::NONE;
}

/**
 * @brief Initialize config list with compiled in values.
 *
 * @details
 * This function sets the spool directories from the cell and initializes
 * the configuration list with compiled in values.
 *
 * @param cell_root The root directory of the cell.
 * @param lpCfg Pointer to the configuration list.
 */
static void sge_set_defined_defaults(const char *cell_root, lList **lpCfg) {
   DENTER(BASIS_LAYER);

   int i = 0;
   lListElem *ep = nullptr;
   tConfEntry *pConf = nullptr;

   pConf = getConfEntry("execd_spool_dir", conf_entries);
   if ( pConf->value == nullptr ) {
      size_t size = strlen(cell_root) + strlen(SPOOL_DIR) + 2;
      auto new_value = (char *)sge_malloc(size * sizeof(char));
      snprintf(new_value, size, "%s/%s", cell_root, SPOOL_DIR);
      pConf->value = new_value;
   }

   lFreeList(lpCfg);

   while (conf_entries[i].name) {

      ep = lAddElemStr(lpCfg, CF_name, conf_entries[i].name, CF_Type);
      lSetString(ep, CF_value, conf_entries[i].value);
      lSetUlong(ep, CF_local, conf_entries[i].local);

      i++;
   }

   DRETURN_VOID;
}

/**
 * Sorts the entries of a global or local configuration into the order of the conf_entries
 * array. This is the order in which a configuration is shown by qconf -sconf and written
 * to the spool file. Configurations entering the qmaster via qconf -aconf/-Aconf/-mconf/
 * -Mconf carry their entries in whatever order the client sent them, so they are sorted
 * here before they are stored.
 *
 * Entries which are not known in the conf_entries array are kept and appended at the end,
 * in the order in which they came in. They have no effect (merge_configuration() ignores
 * them), but dropping them here would hide a typo from the administrator.
 *
 * @param conf the global or local configuration whose entries are sorted in place
 */
void
conf_sort_entries(lListElem *conf) {
   DENTER(BASIS_LAYER);

   lList *entries = lGetListRW(conf, CONF_entries);
   if (entries == nullptr) {
      DRETURN_VOID;
   }

   lList *sorted = lCreateList(lGetListName(entries), CF_Type);
   for (int i = 0; conf_entries[i].name != nullptr; i++) {
      lListElem *ep;

      // a well formed configuration holds every name at most once, but a hand written
      // one might repeat a name - keep all occurrences, next to each other
      while ((ep = lGetElemCaseStrRW(entries, CF_name, conf_entries[i].name)) != nullptr) {
         lAppendElem(sorted, lDechainElem(entries, ep));
      }
   }

   lListElem *ep;
   while ((ep = lFirstRW(entries)) != nullptr) {
      lAppendElem(sorted, lDechainElem(entries, ep));
   }

   lSetList(conf, CONF_entries, sorted);

   DRETURN_VOID;
}

/**
 * @brief Seeks for a config attribute "name", frees old value (if string) from *cpp and writes new value into *cpp.
 *
 * @details
 * This function searches for a configuration attribute by its name, frees the old value if it is a string,
 * and writes the new value into the provided pointer. Logging is done to a file.
 *
 * @param lp_cfg The configuration list.
 * @param name The name of the configuration attribute.
 * @param cpp Pointer to the old value to be replaced.
 * @param val Pointer to the new value to be set.
 * @param type The type of the value.
 */
static void
chg_conf_val(lList *lp_cfg, const char *name, char **cpp, uint32_t *val, ocs::CEntry::Type type) {
   const lListElem *ep;
   const char *s;

   if ((ep = lGetElemStr(lp_cfg, CF_name, name))) {
      s = lGetString(ep, CF_value);
#if 0
      if (s) {
         int old_verbose = log_state_get_log_verbose();

         /* prevent logging function from writing to stderr
          * but log into log file
          */
         log_state_set_log_verbose(0);
         INFO(MSG_CONF_USING_SS, s, name);
         log_state_set_log_verbose(old_verbose);
      }
#endif
      if (cpp)
         *cpp = sge_strdup(*cpp, s);
      else
         parse_ulong_val(nullptr, val, type, s, nullptr, 0);
   }
}

/**
 * @brief set the master configuration from cull
 *
 * @details
 * This function sets the master configuration from cull.
 *
 * @param lpCfg The configuration list.
 *
 * @note
 * MT-NOTE: setConfFromCull() is not MT safe, caller needs LOCK_MASTER_CONF as write lock.
 */
static void
setConfFromCull(lList *lpCfg) {
   DENTER(BASIS_LAYER);

   const lListElem *ep;

   /* get following logging entries logged if log_info is selected */
   chg_conf_val(lpCfg, "loglevel", nullptr, &Master_Config.loglevel, ocs::CEntry::Type::TYPE_LOG);
   log_state_set_log_level(Master_Config.loglevel);

   chg_conf_val(lpCfg, "execd_spool_dir", &Master_Config.execd_spool_dir, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "mailer", &Master_Config.mailer, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "xterm", &Master_Config.xterm, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "load_sensor", &Master_Config.load_sensor, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "prolog", &Master_Config.prolog, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "epilog", &Master_Config.epilog, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "shell_start_mode", &Master_Config.shell_start_mode, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "login_shells", &Master_Config.login_shells, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "min_uid", nullptr, &Master_Config.min_uid, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "min_gid", nullptr, &Master_Config.min_gid, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "gid_range", &Master_Config.gid_range, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "port_range", &Master_Config.port_range, nullptr, ocs::CEntry::Type::NONE);

   if ((ep = lGetElemStr(lpCfg, CF_name, "user_lists"))) {
      lList *lp = nullptr;
      if (!lString2ListNone(lGetString(ep, CF_value), &lp, US_Type, US_name, " \t,")) {
         lFreeList(&(Master_Config.user_lists));
         Master_Config.user_lists = lp;
      }
   }

   if ((ep = lGetElemStr(lpCfg, CF_name, "xuser_lists"))) {
      lList *lp = nullptr;
      if (!lString2ListNone(lGetString(ep, CF_value), &lp, US_Type, US_name, " \t,")) {
         lFreeList(&(Master_Config.xuser_lists));
         Master_Config.xuser_lists = lp;
      }
   }

   if ((ep = lGetElemStr(lpCfg, CF_name, "projects"))) {
      lList *lp = nullptr;
      if (!lString2ListNone(lGetString(ep, CF_value), &lp, PR_Type, PR_name, " \t,")) {
         lFreeList(&(Master_Config.projects));
         Master_Config.projects = lp;
      }
   }

   if ((ep = lGetElemStr(lpCfg, CF_name, "xprojects"))) {
      lList *lp = nullptr;
      if (!lString2ListNone(lGetString(ep, CF_value), &lp, PR_Type, PR_name, " \t,")) {
         lFreeList(&(Master_Config.xprojects));
         Master_Config.xprojects = lp;
      }
   }

   chg_conf_val(lpCfg, "load_report_time", nullptr, &Master_Config.load_report_time, ocs::CEntry::Type::TIME);
   chg_conf_val(lpCfg, "enforce_project", &Master_Config.enforce_project, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "enforce_user", &Master_Config.enforce_user, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "max_unheard", nullptr, &Master_Config.max_unheard, ocs::CEntry::Type::TIME);
   chg_conf_val(lpCfg, "administrator_mail", &Master_Config.administrator_mail, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "mail_tag", &Master_Config.mail_tag, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "set_token_cmd", &Master_Config.set_token_cmd, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "pag_cmd", &Master_Config.pag_cmd, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "token_extend_time", nullptr, &Master_Config.token_extend_time, ocs::CEntry::Type::TIME);
   chg_conf_val(lpCfg, "shepherd_cmd", &Master_Config.shepherd_cmd, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "qmaster_params", &Master_Config.qmaster_params, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "execd_params",  &Master_Config.execd_params, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "reporting_params",  &Master_Config.reporting_params, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "binding_params",  &Master_Config.binding_params, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "jsv_params",  &Master_Config.jsv_params, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "qlogin_daemon", &Master_Config.qlogin_daemon, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "qlogin_command", &Master_Config.qlogin_command, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "rsh_daemon", &Master_Config.rsh_daemon, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "rsh_command", &Master_Config.rsh_command, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "jsv_url", &Master_Config.jsv_url, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "jsv_allowed_mod", &Master_Config.jsv_allowed_mod, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "gdi_request_limits", &Master_Config.gdi_request_limits, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "rlogin_daemon", &Master_Config.rlogin_daemon, nullptr, ocs::CEntry::Type::NONE);
   chg_conf_val(lpCfg, "rlogin_command", &Master_Config.rlogin_command, nullptr, ocs::CEntry::Type::NONE);

   chg_conf_val(lpCfg, "reschedule_unknown", nullptr, &Master_Config.reschedule_unknown, ocs::CEntry::Type::TIME);

   chg_conf_val(lpCfg, "max_aj_instances", nullptr, &Master_Config.max_aj_instances, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "max_aj_tasks", nullptr, &Master_Config.max_aj_tasks, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "max_u_jobs", nullptr, &Master_Config.max_u_jobs, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "max_jobs", nullptr, &Master_Config.max_jobs, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "max_advance_reservations", nullptr, &Master_Config.max_advance_reservations, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "finished_jobs_keep_time", nullptr, &Master_Config.finished_jobs_keep_time, ocs::CEntry::Type::TIME);
   chg_conf_val(lpCfg, "finished_jobs_max", nullptr, &Master_Config.finished_jobs_max, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "auto_user_oticket", nullptr, &Master_Config.auto_user_oticket, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "auto_user_fshare", nullptr, &Master_Config.auto_user_fshare, ocs::CEntry::Type::INT);
   chg_conf_val(lpCfg, "auto_user_default_project", &Master_Config.auto_user_default_project, nullptr, ocs::CEntry::Type::TIME);
   chg_conf_val(lpCfg, "auto_user_delete_time", nullptr, &Master_Config.auto_user_delete_time, ocs::CEntry::Type::TIME);
   chg_conf_val(lpCfg, "delegated_file_staging", &Master_Config.delegated_file_staging, nullptr, ocs::CEntry::Type::TIME);
   chg_conf_val(lpCfg, "libjvm_path", &Master_Config.libjvm_path, nullptr, ocs::CEntry::Type::TIME);
   chg_conf_val(lpCfg, "additional_jvm_args", &Master_Config.additional_jvm_args, nullptr, ocs::CEntry::Type::TIME);
   chg_conf_val(lpCfg, "topology_file", &Master_Config.topology_file, nullptr, ocs::CEntry::Type::TIME);
   DRETURN_VOID;
}

/**
 * @brief Find a configuration entry by name
 *
 * @param name The name of the configuration element.
 * @param conf The array of configuration entries.
 *
 * @return A pointer to the configuration entry.
 */
static tConfEntry *
getConfEntry(const char *name, tConfEntry conf[]) {
   DENTER(BASIS_LAYER);
   for (int i = 0; conf[i].name; i++) {
      if (!strcasecmp(conf[i].name, name)) {
         DRETURN(&conf[i]);
      }
   }
   DRETURN(nullptr);
}

/**
 * @brief merge global and local configuration
 *
 * @details
 * Merge global and local configuration and set lpp list and
 * set conf struct from lpp.
 *
 * @param[out] answer_list receives error messages
 * @param progid the component asking, so component specific parameters are applied
 * @param cell_root the cell directory, used to build the compiled in defaults
 * @param global Global configuration.
 * @param local Local configuration.
 * @param[in,out] lpp Target configuration; when nullptr a temporary list is used and freed
 *
 * @return 0 on success, -2 if no global configuration.
 *
 * @note
 * MT-NOTE: merge_configuration() is MT safe.
 */
int merge_configuration(lList **answer_list, uint32_t progid, const char *cell_root, const lListElem *global, const lListElem *local, lList **lpp) {
   DENTER(BASIS_LAYER);

   const lList *cl;
   const lListElem *elem;
   lListElem *ep2;
   lList *mlist = nullptr;

   if (lpp == nullptr) {
      lpp = &mlist;
   }
   sge_set_defined_defaults(cell_root, lpp);

   /* Merge global configuration */
   /*
   ** the error global == nullptr is not ignored
   ** handled later
   */
   if (global) {
      cl = lGetList(global, CONF_entries);
      for_each_ep(elem, cl) {
         ep2 = lGetElemCaseStrRW(*lpp, CF_name, lGetString(elem, CF_name));
         if (ep2) {
            lSetString(ep2, CF_value, lGetString(elem, CF_value));
         }
      }
   }


   /* Merge in local configuration */
   if (local) {
      cl = lGetList(local, CONF_entries);
      for_each_ep(elem, cl) {
         ep2 = lGetElemCaseStrRW(*lpp, CF_name, lGetString(elem, CF_name));
         if (ep2 && lGetUlong(ep2, CF_local)) {
            lSetString(ep2, CF_value, lGetString(elem, CF_value));
         }
      }
   }

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
   clean_conf();
   setConfFromCull(*lpp);
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   /* put contents of qmaster_params and execd_params
      into some convenient global variables */
   {
      struct saved_vars_s *conf_context = nullptr;
      const char *s;
      char* qmaster_params = mconf_get_qmaster_params();
      char* execd_params = mconf_get_execd_params();
      char* reporting_params = mconf_get_reporting_params();
      char* binding_params = mconf_get_binding_params();
      char* jsv_params = mconf_get_jsv_params();
      uint32_t load_report_time = mconf_get_load_report_time();
#ifdef LINUX
      bool mtrace_before = qmaster_conf.enable_mtrace;
#endif

      SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      // every default comes from qmaster_params_t, so a removed token reverts.
      // It has to happen after mtrace_before was taken above, otherwise both are
      // always equal and removing ENABLE_MTRACE would never reach muntrace().
      qmaster_conf = {};

      // accepted in qmaster_params and execd_params alike, so it is reset here,
      // before the first of the two loops -- see security_params_t
      security_conf = {};

      for (s=sge_strtok_r(qmaster_params, PARAMS_DELIMITER, &conf_context); s; s=sge_strtok_r(nullptr, PARAMS_DELIMITER, &conf_context)) {
         if (parse_bool_param(s, "FORBID_RESCHEDULE", &qmaster_conf.forbid_reschedule)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_SIGNAL", &qmaster_conf.prof_signal_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_SCHEDULER", &qmaster_conf.prof_scheduler_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_LISTENER", &qmaster_conf.prof_listener_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_WORKER", &qmaster_conf.prof_worker_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_DELIVER", &qmaster_conf.prof_deliver_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_TEVENT", &qmaster_conf.prof_tevent_thrd)) {
            continue;
         }
         if (parse_int_param(s, "STREE_SPOOL_INTERVAL", &qmaster_conf.spool_time, ocs::CEntry::Type::TIME)) {
            if (qmaster_conf.spool_time <= 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "STREE_SPOOL_INTERVAL",
                                       STREESPOOLTIMEDEF);
               qmaster_conf.spool_time = STREESPOOLTIMEDEF;
            }
            continue;
         }
         /* CS-1239: STREE_TICK_INTERVAL = seconds between share-tree decay
          * ticks. Out-of-range values (<= 0) reject + warn + fall back to
          * the default. Values above the upper bound are clamped silently
          * at read time in mconf_get_sharetree_tick_interval(). */
         if (parse_int_param(s, "STREE_TICK_INTERVAL", &qmaster_conf.sharetree_tick_interval, ocs::CEntry::Type::TIME)) {
            if (qmaster_conf.sharetree_tick_interval <= 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "STREE_TICK_INTERVAL",
                                       STREE_TICK_INTERVAL_DEF);
               qmaster_conf.sharetree_tick_interval = STREE_TICK_INTERVAL_DEF;
            }
            continue;
         }
         /* CS-1908: finished-job retention sweep-behaviour qmaster_params.
          * (Retention semantics keep_time / max are top-level global-config
          * attributes; see chg_conf_val above.) SWEEP_INTERVAL and SWEEP_BATCH
          * reject <= 0 and fall back to DEF. Values above the upper bound are
          * clamped silently at read time in their mconf_get accessors. */
         if (parse_int_param(s, "FINISHED_JOBS_SWEEP_INTERVAL", &qmaster_conf.finished_jobs_sweep_interval, ocs::CEntry::Type::TIME)) {
            if (qmaster_conf.finished_jobs_sweep_interval <= 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "FINISHED_JOBS_SWEEP_INTERVAL",
                                       FINISHED_JOBS_SWEEP_INTERVAL_DEF);
               qmaster_conf.finished_jobs_sweep_interval = FINISHED_JOBS_SWEEP_INTERVAL_DEF;
            }
            continue;
         }
         if (parse_int_param(s, "FINISHED_JOBS_SWEEP_BATCH", &qmaster_conf.finished_jobs_sweep_batch, ocs::CEntry::Type::INT)) {
            if (qmaster_conf.finished_jobs_sweep_batch <= 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "FINISHED_JOBS_SWEEP_BATCH",
                                       FINISHED_JOBS_SWEEP_BATCH_DEF);
               qmaster_conf.finished_jobs_sweep_batch = FINISHED_JOBS_SWEEP_BATCH_DEF;
            }
            continue;
         }
         if (parse_bool_param(s, "FORBID_APPERROR", &qmaster_conf.forbid_apperror)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_FORCED_QDEL", &qmaster_conf.enable_forced_qdel)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_SUP_GRP_EVAL", &qmaster_conf.enable_sup_grp_eval)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_ENFORCE_MASTER_LIMIT", &qmaster_conf.enable_enforce_master_limit)) {
            continue;
         }
         if (parse_bool_param(s, "__TEST_SLEEP_AFTER_REQUEST", &qmaster_conf.enable_test_sleep_after_request)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_FORCED_QDEL_IF_UNKNOWN", &qmaster_conf.enable_forced_qdel_if_unknown)) {
            continue;
         }
         if (parse_bool_param(s, "ALLOW_ANY_SUBMITHOSTS", &qmaster_conf.allow_any_submithosts)) {
            continue;
         }
#ifdef LINUX
         if (parse_bool_param(s, "ENABLE_MTRACE", &qmaster_conf.enable_mtrace)) {
            continue;
         }
#endif
         if (parse_time_param(s, "MONITOR_TIME", &qmaster_conf.monitor_time)) {
            continue;
         }
         if (!strncasecmp(s, "MAX_DYN_EC", sizeof("MAX_DYN_EC")-1)) {
            qmaster_conf.max_dynamic_event_clients = atoi(&s[sizeof("MAX_DYN_EC=")-1]);
            continue;
         }
         if (parse_bool_param(s, "NO_SECURITY", &security_conf.do_credentials)) {
            /* reversed logic */
            security_conf.do_credentials = security_conf.do_credentials ? false : true;
            continue;
         }
         if (parse_bool_param(s, "NO_AUTHENTICATION", &security_conf.do_authentication)) {
            /* reversed logic */
            security_conf.do_authentication = security_conf.do_authentication ? false : true;
            continue;
         }
         if (parse_bool_param(s, "DISABLE_AUTO_RESCHEDULING", &qmaster_conf.disable_reschedule)) {
            continue;
         }
         if (parse_bool_param(s, "DISABLE_SECONDARY_DS", &qmaster_conf.disable_secondary_ds)) {
            continue;
         }
         if (parse_bool_param(s, "DISABLE_SECONDARY_DS_READER", &qmaster_conf.disable_secondary_ds_reader)) {
            continue;
         }
         if (parse_bool_param(s, "DISABLE_SECONDARY_DS_EXECD", &qmaster_conf.disable_secondary_ds_execd)) {
            continue;
         }
         if (parse_bool_param(s, "DISABLE_AUTOMATIC_SESSIONS", &qmaster_conf.disable_automatic_sessions)) {
            continue;
         }
         if (parse_int_param(s, "MAX_DS_DEVIATION", &qmaster_conf.max_ds_deviation, ocs::CEntry::Type::TIME)) {
            if (qmaster_conf.max_ds_deviation < 0 || qmaster_conf.max_ds_deviation > 5000) {
               qmaster_conf.max_ds_deviation = DEFAULT_DS_DEVIATION;
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "MAX_DS_DEVIATION", DEFAULT_DS_DEVIATION);
            }
            continue;
         }
         if (parse_bool_param(s, "LOG_MONITOR_MESSAGE", &qmaster_conf.is_monitor_message)) {
            continue;
         }
         if (parse_bool_param(s, "SIMULATE_EXECDS", &qmaster_conf.simulate_execds)) {
            continue;
         }
         if (!strncasecmp(s, "SCHEDULER_TIMEOUT",
                    sizeof("SCHEDULER_TIMEOUT")-1)) {
            qmaster_conf.scheduler_timeout=atoi(&s[sizeof("SCHEDULER_TIMEOUT=")-1]);
            continue;
         }
         if (parse_int_param(s, "max_job_deletion_time", &qmaster_conf.max_job_deletion_time, ocs::CEntry::Type::TIME)) {
            if (qmaster_conf.max_job_deletion_time <= 0 || qmaster_conf.max_job_deletion_time > 5) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "max_job_deletion_time",
                                       3);
               qmaster_conf.max_job_deletion_time = 3;
            }
            continue;
         }
         if (parse_bool_param(s, "ENABLE_RESCHEDULE_KILL", &qmaster_conf.enable_reschedule_kill)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_RESCHEDULE_SLAVE", &qmaster_conf.enable_reschedule_slave)) {
            continue;
         }

         // if enabled does not change submit time when a job is rescheduled
         if (parse_bool_param(s, "OLD_RESCHEDULE_BEHAVIOR", &qmaster_conf.old_reschedule_behavior)) {
            continue;
         }

         // if enabled does not change submit time when a job array task is rescheduled
         if (parse_bool_param(s, "OLD_RESCHEDULE_BEHAVIOR_ARRAY_JOB", &qmaster_conf.old_reschedule_behavior_array_job)) {
            continue;
         }

         if (parse_int_param(s, "jsv_threshold", &qmaster_conf.jsv_threshold, ocs::CEntry::Type::TIME)) {
            if (qmaster_conf.jsv_threshold < 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "jsv_threshold",
                                       5000);
               qmaster_conf.jsv_threshold = 5000;
            }
            continue;
         }
         if (parse_int_param(s, "jsv_timeout", &qmaster_conf.jsv_timeout, ocs::CEntry::Type::TIME)) {
            if (qmaster_conf.jsv_timeout <= 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "jsv_timeout",
                                       10);
               qmaster_conf.jsv_timeout = 10;
            }
            continue;
         }
         if (parse_bool_param(s, "ENABLE_SUBMIT_LIB_PATH", &qmaster_conf.enable_submit_lib_path)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_SUBMIT_LD_PRELOAD", &qmaster_conf.enable_submit_ld_preload)) {
            continue;
         }
         if (parse_string_param(s, "GPERF_NAME", qmaster_conf.gperf_name)) {
            continue;
         }
         if (parse_string_param(s, "GPERF_THREADS", qmaster_conf.gperf_threads)) {
            continue;
         }
         {
            std::string ijs_escape_char_val;
            if (parse_string_param(s, "ijs_escape_char", ijs_escape_char_val)) {
               if (ijs_escape_char_val == "none" || ijs_escape_char_val.empty()) {
                  qmaster_conf.s_ijs_escape_char = '\0';
               } else {
                  qmaster_conf.s_ijs_escape_char = ijs_escape_char_val[0];
               }
               continue;
            }
         }
         {
            std::string kv;
            if (parse_string_param(s, "ijs_keepalive_interval", kv)) {
               int v = atoi(kv.c_str());
               qmaster_conf.s_ijs_keepalive_interval = (v >= 0) ? v : 60;
               continue;
            }
            if (parse_string_param(s, "ijs_keepalive_count", kv)) {
               int v = atoi(kv.c_str());
               qmaster_conf.s_ijs_keepalive_count = (v > 0) ? v : 3;
               continue;
            }
            if (parse_string_param(s, "ijs_reconnect_timeout", kv)) {
               int v = atoi(kv.c_str());
               qmaster_conf.s_ijs_reconnect_timeout = (v >= 0) ? v : 0;
               continue;
            }
         }
      }
      SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      sge_free_saved_vars(conf_context);
      conf_context = nullptr;

#ifdef LINUX
      /* enable/disable GNU malloc library facility for recording of all
         memory allocation/deallocation
         requires MALLOC_TRACE in environment (see mtrace(3) under Linux) */
      if (qmaster_conf.enable_mtrace != mtrace_before) {
         if (qmaster_conf.enable_mtrace) {
            DPRINTF("ENABLE_MTRACE=true ---> mtrace()\n");
            mtrace();
         } else {
            DPRINTF("ENABLE_MTRACE=false ---> muntrace()\n");
            muntrace();
         }
      }
#endif

      conf_update_thread_profiling(nullptr);

      SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      // every default comes from execd_params_t, so a removed token reverts
      execd_conf = {};

      for (s=sge_strtok_r(execd_params, PARAMS_DELIMITER, &conf_context); s; s=sge_strtok_r(nullptr, PARAMS_DELIMITER, &conf_context)) {
         if (parse_bool_param(s, "USE_QIDLE", &execd_conf.use_qidle)) {
            continue;
         }
         if (progid == EXECD) {
            if (parse_bool_param(s, "NO_SECURITY", &security_conf.do_credentials)) {
               /* reversed logic */
               security_conf.do_credentials = security_conf.do_credentials ? false : true;
               continue;
            }
            if (parse_bool_param(s, "NO_AUTHENTICATION", &security_conf.do_authentication)) {
               /* reversed logic */
               security_conf.do_authentication = security_conf.do_authentication ? false : true;
               continue;
            }
            if (parse_bool_param(s, "DO_AUTHENTICATION", &security_conf.do_authentication)) {
               continue;
            }
         }
         {
            if (strncasecmp(s, "KEEP_ACTIVE", sizeof("KEEP_ACTIVE")-1) == 0) {
               const char *keep_active_value = &s[sizeof("KEEP_ACTIVE=")-1];

               if (strncasecmp(keep_active_value, "ERROR", sizeof("ERROR")-1) == 0) {
                  execd_conf.keep_active = KEEP_ACTIVE_ERROR;
               } else if (strncasecmp(keep_active_value, TRUE_STR, sizeof(TRUE_STR)-1) == 0) {
                  execd_conf.keep_active = KEEP_ACTIVE_TRUE;
               } else {
                  execd_conf.keep_active = KEEP_ACTIVE_FALSE;
               }
               continue;
            }
         }
         {
            if (strncasecmp(s, "USAGE_COLLECTION", sizeof("USAGE_COLLECTION")-1) == 0) {
               const char *usage_collection_str = &s[sizeof("USAGE_COLLECTION=")-1];

               if (strncasecmp(usage_collection_str, TRUE_STR, sizeof(TRUE_STR)-1) == 0) {
                  execd_conf.usage_collection = USAGE_COLLECTION_DEFAULT;
               } else if (strncasecmp(usage_collection_str, "PDC", sizeof("PDC")-1) == 0) {
                  execd_conf.usage_collection = USAGE_COLLECTION_PDC;
               } else if (strncasecmp(usage_collection_str, "HYBRID", sizeof("HYBRID")-1) == 0) {
                  execd_conf.usage_collection = USAGE_COLLECTION_HYBRID;
               } else {
                  execd_conf.usage_collection = USAGE_COLLECTION_NONE;
               }
               continue;
            }
         }
#if defined(WITH_EXTENSIONS)
         if (parse_bool_param(s, "ENABLE_MEM_DETAILS", &execd_conf.enable_mem_details)) {
            continue;
         }
#endif
         if (parse_time_param(s, "SCRIPT_TIMEOUT", &execd_conf.script_timeout)) {
            continue;
         }
         if (parse_bool_param(s, "SIMULATE_JOBS", &execd_conf.simulate_jobs)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_ADDGRP_KILL", &execd_conf.enable_addgrp_kill)) {
            continue;
         }
         if (parse_bool_param(s, "ACCT_RESERVED_USAGE", &execd_conf.acct_reserved_usage)) {
            continue;
         }
         if (parse_bool_param(s, "SHARETREE_RESERVED_USAGE", &execd_conf.sharetree_reserved_usage)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_EXECD", &execd_conf.prof_execd_thrd)) {
            continue;
         }
         if (!strncasecmp(s, "NOTIFY_KILL", sizeof("NOTIFY_KILL")-1)) {
            if (!strcasecmp(s, "NOTIFY_KILL=default")) {
               execd_conf.notify_kill_type = 1;
            } else if (!strcasecmp(s, "NOTIFY_KILL=none")) {
               execd_conf.notify_kill_type = 2;
            } else if (!strncasecmp(s, "NOTIFY_KILL=", sizeof("NOTIFY_KILL=")-1)){
               execd_conf.notify_kill_type = 0;
               if (notify_kill) {
                  sge_free(&notify_kill);
               }
               notify_kill = sge_strdup(nullptr, &(s[sizeof("NOTIFY_KILL")]));
            }
            continue;
         }
         if (!strncasecmp(s, "NOTIFY_SUSP", sizeof("NOTIFY_SUSP")-1)) {
            if (!strcasecmp(s, "NOTIFY_SUSP=default")) {
               execd_conf.notify_susp_type = 1;
            } else if (!strcasecmp(s, "NOTIFY_SUSP=none")) {
               execd_conf.notify_susp_type = 2;
            } else if (!strncasecmp(s, "NOTIFY_SUSP=", sizeof("NOTIFY_SUSP=")-1)){
               execd_conf.notify_susp_type = 0;
               if (notify_susp) {
                  sge_free(&notify_susp);
               }
               notify_susp = sge_strdup(nullptr, &(s[sizeof("NOTIFY_SUSP")]));
            }
            continue;
         }
         if (parse_bool_param(s, "USE_QSUB_GID", &execd_conf.use_qsub_gid)) {
            continue;
         }
         if (!strncasecmp(s, "PTF_MAX_PRIORITY", sizeof("PTF_MAX_PRIORITY")-1)) {
            execd_conf.ptf_max_priority=atoi(&s[sizeof("PTF_MAX_PRIORITY=")-1]);
            continue;
         }
         if (!strncasecmp(s, "PTF_MIN_PRIORITY", sizeof("PTF_MIN_PRIORITY")-1)) {
            execd_conf.ptf_min_priority=atoi(&s[sizeof("PTF_MIN_PRIORITY=")-1]);
            continue;
         }
         if (parse_bool_param(s, "SET_LIB_PATH", &execd_conf.set_lib_path)) {
            continue;
         }
         if (parse_bool_param(s, "INHERIT_ENV", &execd_conf.inherit_env)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_HWLOC", &execd_conf.enable_hwloc)) {
            continue;
         }
         if (!strncasecmp(s, "PDC_INTERVAL", sizeof("PDC_INTERVAL")-1)) {
            uint32_t tmp_pdc_interval;

            if (!strcasecmp(s, "PDC_INTERVAL=NEVER")) {
               execd_conf.pdc_interval = PDC_DISABLED;
            } else if (!strcasecmp(s, "PDC_INTERVAL=PER_LOAD_REPORT")) {
               execd_conf.pdc_interval = sge_gmt32_to_gmt64(load_report_time);
            } else if (parse_time_param(s, "PDC_INTERVAL", &tmp_pdc_interval)) {
               execd_conf.pdc_interval = sge_gmt32_to_gmt64(tmp_pdc_interval);
            } else {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "execd_params", "PDC_INTERVAL", 1);
               execd_conf.pdc_interval = sge_gmt32_to_gmt64(1);
            }
            continue;
         }
         if (!strncasecmp(s, "S_DESCRIPTORS", sizeof("S_DESCRIPTORS")-1)) {
            sge_strlcpy(execd_conf.s_descriptors, s+sizeof("S_DESCRIPTORS"), 100);
            continue;
         }
         if (!strncasecmp(s, "H_DESCRIPTORS", sizeof("H_DESCRIPTORS")-1)) {
            sge_strlcpy(execd_conf.h_descriptors, s+sizeof("H_DESCRIPTORS"), 100);
            continue;
         }
         if (!strncasecmp(s, "S_MAXPROC", sizeof("S_MAXPROC")-1)) {
            sge_strlcpy(execd_conf.s_maxproc, s+sizeof("S_MAXPROC"), 100);
            continue;
         }
         if (!strncasecmp(s, "H_MAXPROC", sizeof("H_MAXPROC")-1)) {
            sge_strlcpy(execd_conf.h_maxproc, s+sizeof("H_MAXPROC"), 100);
            continue;
         }
         if (!strncasecmp(s, "S_MEMORYLOCKED", sizeof("S_MEMORYLOCKED")-1)) {
            sge_strlcpy(execd_conf.s_memorylocked, s+sizeof("S_MEMORYLOCKED"), 100);
            continue;
         }
         if (!strncasecmp(s, "H_MEMORYLOCKED", sizeof("H_MEMORYLOCKED")-1)) {
            sge_strlcpy(execd_conf.h_memorylocked, s+sizeof("H_MEMORYLOCKED"), 100);
            continue;
         }
         if (!strncasecmp(s, "S_LOCKS", sizeof("S_LOCKS")-1)) {
            sge_strlcpy(execd_conf.s_locks, s+sizeof("S_LOCKS"), 100);
            continue;
         }
         if (!strncasecmp(s, "H_LOCKS", sizeof("H_LOCKS")-1)) {
            sge_strlcpy(execd_conf.h_locks, s+sizeof("H_LOCKS"), 100);
            continue;
         }
         if (parse_bool_param(s, "IGNORE_NGROUPS_MAX_LIMIT", &execd_conf.ignore_ngroups_max_limit)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_SYSTEMD", &execd_conf.enable_systemd)) {
            continue;
         }
      }
      SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      sge_free_saved_vars(conf_context);
      conf_context = nullptr;

      /* If profiling configuration has changed,
         set_thread_prof_status_by_name has to be called for each thread
      */
      set_thread_prof_status_by_name("Execd Thread", execd_conf.prof_execd_thrd);

      SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      /* parse reporting parameters */
      // every default comes from reporting_params_t, so a removed token reverts
      reporting_conf = {};

      for (s=sge_strtok_r(reporting_params, PARAMS_DELIMITER, &conf_context); s; s=sge_strtok_r(nullptr, PARAMS_DELIMITER, &conf_context)) {
         if (parse_bool_param(s, "accounting", &reporting_conf.do_accounting)) {
            continue;
         }
         if (parse_bool_param(s, "reporting", &reporting_conf.do_reporting)) {
            continue;
         }
         if (parse_bool_param(s, "monitoring", &reporting_conf.do_monitoring)) {
            continue;
         }
         if (parse_bool_param(s, "joblog", &reporting_conf.do_joblog)) {
            continue;
         }
         if (parse_int_param(s, "flush_time", &reporting_conf.reporting_flush_time, ocs::CEntry::Type::TIME)) {
            if (reporting_conf.reporting_flush_time <= 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "reporting_params", "flush_time",
                                       15);
               reporting_conf.reporting_flush_time = 15;
            }
            continue;
         }
         if (parse_int_param(s, "accounting_flush_time", &reporting_conf.accounting_flush_time, ocs::CEntry::Type::TIME)) {
            if (reporting_conf.accounting_flush_time < 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "reporting_params", "accounting_flush_time",
                                       -1);
               reporting_conf.accounting_flush_time = -1;
            }

            continue;
         }
         if (parse_bool_param(s, "sync_write", &reporting_conf.reporting_sync_write)) {
            continue;
         }
         if (parse_bool_param(s, "old_accounting", &reporting_conf.old_accounting)) {
            continue;
         }
         if (parse_bool_param(s, "old_reporting", &reporting_conf.old_reporting)) {
            continue;
         }
         if (parse_int_param(s, "sharelog", &reporting_conf.sharelog_time, ocs::CEntry::Type::TIME)) {
            continue;
         }
         if (parse_bool_param(s, "log_consumables", &reporting_conf.log_consumables)) {
            continue;
         }
         if (parse_string_param(s, "usage_patterns", reporting_conf.usage_patterns)) {
            continue;
         }
         std::string online_usage_str;
         if (parse_string_param(s, "online_usage", online_usage_str)) {
            // The pre-commit validator in qmaster's check_config rejects bad
            // values before they reach us. Re-validate here as defence in
            // depth (e.g. for spool data written before validation existed).
            // On rejection, leave online_usage_vars unchanged.
            std::vector<std::string> new_online_usage_vars;
            if (parse_online_usage_value(answer_list, online_usage_str.c_str(),
                                         new_online_usage_vars)) {
               reporting_conf.online_usage_vars = std::move(new_online_usage_vars);
            }
            continue;
         }
      }
      SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      sge_free_saved_vars(conf_context);
      conf_context=nullptr;

      /* parse binding parameters */
      SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

      // every default comes from binding_params_t, so a removed token reverts
      binding_conf = {};

      for (s=sge_strtok_r(binding_params, PARAMS_DELIMITER, &conf_context); s; s=sge_strtok_r(nullptr, PARAMS_DELIMITER, &conf_context)) {
         if (parse_bool_param(s, "enabled", &binding_conf.is_binding_enabled)) {
            continue;
         }
         if (parse_bool_param(s, "implicit", &binding_conf.do_implicit_binding)) {
            continue;
         }
         if (parse_bool_param(s, "on_any_host", &binding_conf.schedule_on_any_host)) {
            continue;
         }
         std::string binding_mode_str;
         if (parse_string_param(s, "mode", binding_mode_str)) {
            if (binding_mode_str == "default" || binding_mode_str == "DEFAULT") {
               binding_conf.binding_mode = BINDING_MODE_DEFAULT;
            } else if (binding_mode_str == "ocs" || binding_mode_str == "OCS") {
               binding_conf.binding_mode = BINDING_MODE_OCS;
            } else if (binding_mode_str == "gcs" || binding_mode_str == "GCS") {
               binding_conf.binding_mode = BINDING_MODE_GCS;
            } else {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "binding_params", "mode", -1);
               binding_conf.binding_mode = BINDING_MODE_DEFAULT;
            }
            continue;
         }
         std::string default_binding_unit_str;
         if (parse_string_param(s, "default_unit", default_binding_unit_str)) {
            binding_conf.default_binding_unit = ocs::BindingUnit::from_string(default_binding_unit_str);

            if (binding_conf.default_binding_unit == ocs::BindingUnit::UNINITIALIZED) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "binding_params", "default_unit", -1);
               binding_conf.default_binding_unit = ocs::BindingUnit::CCORE;
               continue;
            }

            continue;
         }
         if (parse_string_param(s, "filter", binding_conf.binding_filter)) {
            if (const char *binding_filter_cstr = binding_conf.binding_filter.c_str();
                strcasecmp(binding_filter_cstr, NONE_STR) != 0 && strcasecmp(binding_filter_cstr, FIRST_CORE) != 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "binding_params", "filter", -1);
               binding_conf.binding_filter = NONE_STR;
               continue;
            }

            continue;
         }
      }
      SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      sge_free_saved_vars(conf_context);
      conf_context=nullptr;

#if 0
      // @todo: implement parsing of jsv parameters
      // @todo: CS-1700: Add possibility to define a timeout for JSVs
      // @todo: CS-1701: Add possibility to define a threshold for JSV's
      /* parse JSV parameters */
      SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      for (s=sge_strtok_r(jsv_params, PARAMS_DELIMITER, &conf_context); s; s=sge_strtok_r(nullptr, PARAMS_DELIMITER, &conf_context)) {
         ;
      }
      SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      sge_free_saved_vars(conf_context);
      conf_context=nullptr;
#endif

      sge_free(&qmaster_params);
      sge_free(&execd_params);
      sge_free(&reporting_params);
      sge_free(&binding_params);
      sge_free(&jsv_params);
   }

   lFreeList(&mlist);

   if (!global) {
      WARNING(SFNMAX, MSG_CONF_NOCONFIGFROMMASTER);
      DRETURN(-2);
   }

   DRETURN(0);
}

/**
 * @brief Print the master configuration, in debug mode only
 *
 * @note
 * MT-NOTE: sge_show_conf() is MT safe.
 */
void sge_show_conf() {
   DENTER(BASIS_LAYER);
   dstring dstr = DSTRING_INIT;

   // prevent logging function from writing to stderr but log into log file
   int old_verbose = log_state_get_log_verbose();
   log_state_set_log_verbose(0);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   INFO(MSG_CONF_USING_US, Master_Config.loglevel, "loglevel");
   INFO(MSG_CONF_USING_SS, Master_Config.execd_spool_dir != nullptr ? Master_Config.execd_spool_dir : NONE_STR, "execd_spool_dir");
   INFO(MSG_CONF_USING_SS, Master_Config.mailer != nullptr ? Master_Config.mailer : NONE_STR, "mailer");
   INFO(MSG_CONF_USING_SS, Master_Config.xterm != nullptr ? Master_Config.xterm : NONE_STR, "xterm");
   INFO(MSG_CONF_USING_SS, Master_Config.load_sensor != nullptr ? Master_Config.load_sensor : NONE_STR, "load_sensor");
   INFO(MSG_CONF_USING_SS, Master_Config.prolog != nullptr ? Master_Config.prolog : NONE_STR, "prolog");
   INFO(MSG_CONF_USING_SS, Master_Config.epilog != nullptr ? Master_Config.epilog : NONE_STR, "epilog");
   INFO(MSG_CONF_USING_SS, Master_Config.shell_start_mode != nullptr ? Master_Config.shell_start_mode : NONE_STR, "shell_start_mode");
   INFO(MSG_CONF_USING_SS, Master_Config.login_shells != nullptr ? Master_Config.login_shells : NONE_STR, "login_shells");
   INFO(MSG_CONF_USING_US, Master_Config.min_gid, "min_gid");
   INFO(MSG_CONF_USING_US, Master_Config.min_uid, "min_uid");
   INFO(MSG_CONF_USING_SS, Master_Config.gid_range != nullptr ? Master_Config.gid_range : NONE_STR, "gid_range");
   INFO(MSG_CONF_USING_SS, Master_Config.port_range != nullptr ? Master_Config.port_range : NONE_STR, "port_range");
   INFO(MSG_CONF_USING_US, Master_Config.load_report_time, "load_report_time");
   INFO(MSG_CONF_USING_SS, Master_Config.enforce_project != nullptr ? Master_Config.enforce_project : NONE_STR, "enforce_project");
   INFO(MSG_CONF_USING_SS, Master_Config.enforce_user != nullptr ? Master_Config.enforce_user : NONE_STR, "enforce_user");
   INFO(MSG_CONF_USING_US, Master_Config.max_unheard, "max_unheard");
   INFO(MSG_CONF_USING_SS, Master_Config.administrator_mail != nullptr ? Master_Config.administrator_mail : NONE_STR, "administrator_mail");
   INFO(MSG_CONF_USING_SS, Master_Config.mail_tag != nullptr ? Master_Config.mail_tag : NONE_STR, "mail_tag");
   INFO(MSG_CONF_USING_SS, Master_Config.set_token_cmd != nullptr ? Master_Config.set_token_cmd : NONE_STR, "set_token_cmd");
   INFO(MSG_CONF_USING_SS, Master_Config.pag_cmd != nullptr ? Master_Config.pag_cmd : NONE_STR, "pag_cmd");
   INFO(MSG_CONF_USING_US, Master_Config.token_extend_time, "token_extend_time");
   INFO(MSG_CONF_USING_SS, Master_Config.shepherd_cmd != nullptr ? Master_Config.shepherd_cmd : NONE_STR, "shepherd_cmd");
   INFO(MSG_CONF_USING_SS, Master_Config.reporting_params != nullptr ? Master_Config.reporting_params : NONE_STR, "reporting_params");
   INFO(MSG_CONF_USING_SS, Master_Config.binding_params != nullptr ? Master_Config.binding_params : NONE_STR, "binding_params");
   INFO(MSG_CONF_USING_SS, Master_Config.jsv_params != nullptr ? Master_Config.jsv_params : NONE_STR, "jsv_params");
   INFO(MSG_CONF_USING_SS, Master_Config.qlogin_daemon != nullptr ? Master_Config.qlogin_daemon : NONE_STR, "qlogin_daemon");
   INFO(MSG_CONF_USING_SS, Master_Config.qlogin_command != nullptr ? Master_Config.qlogin_command : NONE_STR, "qlogin_command");
   INFO(MSG_CONF_USING_SS, Master_Config.rsh_daemon != nullptr ? Master_Config.rsh_daemon : NONE_STR, "rsh_daemon");
   INFO(MSG_CONF_USING_SS, Master_Config.rsh_command != nullptr ? Master_Config.rsh_command : NONE_STR, "rsh_command");
   INFO(MSG_CONF_USING_SS, Master_Config.jsv_url != nullptr ? Master_Config.jsv_url : NONE_STR, "jsv_url");
   INFO(MSG_CONF_USING_SS, Master_Config.jsv_allowed_mod != nullptr ? Master_Config.jsv_allowed_mod : NONE_STR, "jsv_allowed_mod");
   INFO(MSG_CONF_USING_SS, Master_Config.gdi_request_limits != nullptr ? Master_Config.gdi_request_limits : NONE_STR, "gdi_request_limits");
   INFO(MSG_CONF_USING_SS, Master_Config.rlogin_daemon != nullptr ? Master_Config.rlogin_daemon : NONE_STR, "rlogin_daemon");
   INFO(MSG_CONF_USING_SS, Master_Config.rlogin_command != nullptr ? Master_Config.rlogin_command : NONE_STR, "rlogin_command");
   INFO(MSG_CONF_USING_US, Master_Config.reschedule_unknown, "reschedule_unknown");
   INFO(MSG_CONF_USING_US, Master_Config.max_aj_instances, "max_aj_instances");
   INFO(MSG_CONF_USING_US, Master_Config.max_aj_tasks, "max_aj_tasks");
   INFO(MSG_CONF_USING_US, Master_Config.max_u_jobs, "max_u_jobs");
   INFO(MSG_CONF_USING_US, Master_Config.max_jobs, "max_jobs");
   INFO(MSG_CONF_USING_US, Master_Config.max_advance_reservations, "max_advance_reservations");
   INFO(MSG_CONF_USING_US, Master_Config.auto_user_oticket, "auto_user_oticket");
   INFO(MSG_CONF_USING_US, Master_Config.auto_user_fshare, "auto_user_fshare");
   INFO(MSG_CONF_USING_SS, Master_Config.auto_user_default_project != nullptr ? Master_Config.auto_user_default_project : NONE_STR, "auto_user_default_project");
   INFO(MSG_CONF_USING_US, Master_Config.auto_user_delete_time, "auto_user_delete_time");
   INFO(MSG_CONF_USING_SS, Master_Config.delegated_file_staging != nullptr ? Master_Config.delegated_file_staging : NONE_STR, "delegated_file_staging");
   INFO(MSG_CONF_USING_SS, Master_Config.libjvm_path != nullptr ? Master_Config.libjvm_path : NONE_STR, "libjvm_path");
   INFO(MSG_CONF_USING_SS, Master_Config.additional_jvm_args != nullptr ? Master_Config.additional_jvm_args : NONE_STR, "additional_jvm_args");

   INFO(MSG_CONF_USING_SS, Master_Config.qmaster_params != nullptr ? Master_Config.qmaster_params : NONE_STR, "qmaster_params");
   INFO(MSG_CONF_USING_SS, Master_Config.execd_params != nullptr ? Master_Config.execd_params : NONE_STR, "execd_params");

   userset_list_append_to_dstring(Master_Config.user_lists, &dstr);
   INFO(MSG_CONF_USING_SS, sge_dstring_get_string(&dstr), "user_lists");
   sge_dstring_clear(&dstr);

   userset_list_append_to_dstring(Master_Config.xuser_lists, &dstr);
   INFO(MSG_CONF_USING_SS, sge_dstring_get_string(&dstr), "xuser_lists");
   sge_dstring_clear(&dstr);

   prj_list_append_to_dstring(Master_Config.projects, &dstr);
   INFO(MSG_CONF_USING_SS, sge_dstring_get_string(&dstr), "projects");
   sge_dstring_clear(&dstr);

   prj_list_append_to_dstring(Master_Config.xprojects, &dstr);
   INFO(MSG_CONF_USING_SS, sge_dstring_get_string(&dstr), "xprojects");
   sge_dstring_clear(&dstr);

   INFO(MSG_CONF_USING_SS, Master_Config.topology_file != nullptr ? Master_Config.topology_file : NONE_STR, "topology_file");

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   // reset to original setting
   log_state_set_log_verbose(old_verbose);

   sge_dstring_free(&dstr);

   DRETURN_VOID;
}

/**
 * @brief Free the whole master configuration
 *
 * @note
 * MT-NOTE: clean_conf() is not MT safe, caller needs LOCK_MASTER_CONF as write lock.
 */
static void clean_conf() {
   DENTER(BASIS_LAYER);

   sge_free(&Master_Config.execd_spool_dir);
   sge_free(&Master_Config.mailer);
   sge_free(&Master_Config.xterm);
   sge_free(&Master_Config.load_sensor);
   sge_free(&Master_Config.prolog);
   sge_free(&Master_Config.epilog);
   sge_free(&Master_Config.shell_start_mode);
   sge_free(&Master_Config.login_shells);
   sge_free(&Master_Config.enforce_project);
   sge_free(&Master_Config.enforce_user);
   sge_free(&Master_Config.administrator_mail);
   sge_free(&Master_Config.mail_tag);
   lFreeList(&Master_Config.user_lists);
   lFreeList(&Master_Config.xuser_lists);
   lFreeList(&Master_Config.projects);
   lFreeList(&Master_Config.xprojects);
   sge_free(&Master_Config.set_token_cmd);
   sge_free(&Master_Config.pag_cmd);
   sge_free(&Master_Config.shepherd_cmd);
   sge_free(&Master_Config.qmaster_params);
   sge_free(&Master_Config.execd_params);
   sge_free(&Master_Config.reporting_params);
   sge_free(&Master_Config.binding_params);
   sge_free(&Master_Config.jsv_params);
   sge_free(&Master_Config.gid_range);
   sge_free(&Master_Config.port_range);
   sge_free(&Master_Config.qlogin_daemon);
   sge_free(&Master_Config.qlogin_command);
   sge_free(&Master_Config.rsh_daemon);
   sge_free(&Master_Config.rsh_command);
   sge_free(&Master_Config.jsv_url);
   sge_free(&Master_Config.jsv_allowed_mod);
   sge_free(&Master_Config.gdi_request_limits);
   sge_free(&Master_Config.rlogin_daemon);
   sge_free(&Master_Config.rlogin_command);
   sge_free(&Master_Config.auto_user_default_project);
   sge_free(&Master_Config.delegated_file_staging);
   sge_free(&Master_Config.libjvm_path);
   sge_free(&Master_Config.additional_jvm_args);
   sge_free(&Master_Config.topology_file);

   memset(&Master_Config, 0, sizeof(sge_conf_type));

   DRETURN_VOID;
}

/**
 * @brief Enable or disable profiling for threads
 *
 * Follows the actual
 * global config, qmaster_params.
 *
 * If no thread name (nullptr pointer) is given, profiling information of all
 * threads is updated.
 * If a name is given, all threads with that name are updated.
 *
 * @param thread_name Thread name, nullptr for all threads.
 */
void conf_update_thread_profiling(const char *thread_name) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   if (thread_name == nullptr) {
      set_thread_prof_status_by_name("Signal Thread", qmaster_conf.prof_signal_thrd);
      set_thread_prof_status_by_name("Scheduler Thread", qmaster_conf.prof_scheduler_thrd);
      set_thread_prof_status_by_name("Listener Thread", qmaster_conf.prof_listener_thrd);
      set_thread_prof_status_by_name("Worker Thread", qmaster_conf.prof_worker_thrd);
      set_thread_prof_status_by_name("Event Master Thread", qmaster_conf.prof_deliver_thrd);
      set_thread_prof_status_by_name("TEvent Thread", qmaster_conf.prof_tevent_thrd);
   } else {
      if (strcmp(thread_name, "Signal Thread") == 0) {
         set_thread_prof_status_by_name("Signal Thread", qmaster_conf.prof_signal_thrd);
      } else if (strcmp(thread_name, "Scheduler Thread") == 0) {
         set_thread_prof_status_by_name("Scheduler Thread", qmaster_conf.prof_scheduler_thrd);
      } else if (strcmp(thread_name, "Listener Thread") == 0) {
         set_thread_prof_status_by_name("Listener Thread", qmaster_conf.prof_listener_thrd);
      } else if (strcmp(thread_name, "Worker Thread") == 0) {
         set_thread_prof_status_by_name("Worker Thread", qmaster_conf.prof_worker_thrd);
      } else if (strcmp(thread_name, "Event Master Thread") == 0) {
         set_thread_prof_status_by_name("Event Master Thread", qmaster_conf.prof_deliver_thrd);
      } else if (strcmp(thread_name, "TEvent Thread") == 0) {
         set_thread_prof_status_by_name("TEvent Thread", qmaster_conf.prof_tevent_thrd);
      }
   }
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/**
 * @brief The `execd_spool_dir` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_execd_spool_dir() {
   DENTER(BASIS_LAYER);

   char* execd_spool_dir = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   execd_spool_dir = sge_strdup(execd_spool_dir, Master_Config.execd_spool_dir);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(execd_spool_dir);
}

/**
 * @brief The `mailer` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_mailer() {
   DENTER(BASIS_LAYER);

   char* mailer = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   mailer = sge_strdup(mailer, Master_Config.mailer);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(mailer);
}

/**
 * @brief The `xterm` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_xterm() {
   DENTER(BASIS_LAYER);

   char* xterm = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   xterm = sge_strdup(xterm, Master_Config.xterm);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(xterm);

}

/**
 * @brief The `load_sensor` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_load_sensor() {
   DENTER(BASIS_LAYER);

   char* load_sensor = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   load_sensor = sge_strdup(load_sensor, Master_Config.load_sensor);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(load_sensor);
}

/**
 * @brief The `prolog` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_prolog() {
   DENTER(BASIS_LAYER);

   char* prolog = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   prolog = sge_strdup(prolog, Master_Config.prolog);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(prolog);
}

/**
 * @brief The `epilog` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_epilog() {
   DENTER(BASIS_LAYER);

   char* epilog = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   epilog = sge_strdup(epilog, Master_Config.epilog);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(epilog);
}

/**
 * @brief The `shell_start_mode` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_shell_start_mode() {
   DENTER(BASIS_LAYER);

   char* shell_start_mode = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   shell_start_mode = sge_strdup(shell_start_mode, Master_Config.shell_start_mode);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(shell_start_mode);
}

/**
 * @brief The `login_shells` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_login_shells() {
   DENTER(BASIS_LAYER);

   char* login_shells = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   login_shells = sge_strdup(login_shells, Master_Config.login_shells);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(login_shells);
}

/**
 * @brief The `min_uid` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_min_uid() {
   DENTER(BASIS_LAYER);

   uint32_t min_uid;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   min_uid = Master_Config.min_uid;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(min_uid);
}

/**
 * @brief The `min_gid` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_min_gid() {
   DENTER(BASIS_LAYER);

   uint32_t min_gid;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   min_gid = Master_Config.min_gid;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(min_gid);
}

/**
 * @brief The `load_report_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_load_report_time() {
   DENTER(BASIS_LAYER);

   uint32_t load_report_time;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   load_report_time = Master_Config.load_report_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(load_report_time);
}

/**
 * @brief The `max_unheard` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_max_unheard() {
   DENTER(BASIS_LAYER);

   uint32_t max_unheard;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_unheard = Master_Config.max_unheard;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_unheard);
}

/**
 * @brief The `loglevel` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_loglevel() {
   DENTER(BASIS_LAYER);

   uint32_t loglevel;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   loglevel = Master_Config.loglevel;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(loglevel);
}

/**
 * @brief The `enforce_project` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_enforce_project() {
   DENTER(BASIS_LAYER);

   char* enforce_project = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   enforce_project = sge_strdup(enforce_project, Master_Config.enforce_project);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(enforce_project);
}

/**
 * @brief The `enforce_user` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_enforce_user() {
   DENTER(BASIS_LAYER);

   char* enforce_user = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   enforce_user = sge_strdup(enforce_user, Master_Config.enforce_user);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(enforce_user);
}


/**
 * @brief The `administrator_mail` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_administrator_mail() {
   DENTER(BASIS_LAYER);

   char* administrator_mail = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   administrator_mail = sge_strdup(administrator_mail, Master_Config.administrator_mail);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(administrator_mail);
}

/**
 * @brief The `mail_tag` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_mail_tag() {
   DENTER(BASIS_LAYER);

   char* mail_tag = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   mail_tag = sge_strdup(mail_tag, Master_Config.mail_tag);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(mail_tag);
}

/**
 * @brief The `user_lists` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
lList* mconf_get_user_lists() {
   DENTER(BASIS_LAYER);

   lList* user_lists = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   user_lists = lCopyList("user_lists", Master_Config.user_lists);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(user_lists);
}

/**
 * @brief The `xuser_lists` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
lList* mconf_get_xuser_lists() {
   DENTER(BASIS_LAYER);

   lList* xuser_lists = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   xuser_lists = lCopyList("xuser_lists", Master_Config.xuser_lists);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(xuser_lists);
}

/**
 * @brief The `projects` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
lList* mconf_get_projects() {
   DENTER(BASIS_LAYER);

   lList* projects = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   projects = lCopyList("projects", Master_Config.projects);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(projects);
}

/**
 * @brief The `xprojects` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
lList* mconf_get_xprojects() {
   DENTER(BASIS_LAYER);

   lList* xprojects = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   xprojects = lCopyList("xprojects", Master_Config.xprojects);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(xprojects);
}

/**
 * @brief The `set_token_cmd` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_set_token_cmd() {
   DENTER(BASIS_LAYER);

   char* set_token_cmd = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   set_token_cmd = sge_strdup(set_token_cmd, Master_Config.set_token_cmd);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(set_token_cmd);
}

/**
 * @brief The `pag_cmd` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_pag_cmd() {
   DENTER(BASIS_LAYER);

   char* pag_cmd = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   pag_cmd = sge_strdup(pag_cmd, Master_Config.pag_cmd);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(pag_cmd);
}

/**
 * @brief The `token_extend_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_token_extend_time() {
   DENTER(BASIS_LAYER);

   uint32_t token_extend_time;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   token_extend_time = Master_Config.token_extend_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(token_extend_time);
}

/**
 * @brief The `shepherd_cmd` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_shepherd_cmd() {
   DENTER(BASIS_LAYER);

   char* shepherd_cmd = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   shepherd_cmd = sge_strdup(shepherd_cmd, Master_Config.shepherd_cmd);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(shepherd_cmd);
}

/**
 * @brief The `qmaster_params` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_qmaster_params() {
   DENTER(BASIS_LAYER);

   char* qmaster_params = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   qmaster_params = sge_strdup(qmaster_params, Master_Config.qmaster_params);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(qmaster_params);
}

/**
 * @brief The `execd_params` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_execd_params() {
   DENTER(BASIS_LAYER);

   char* execd_params = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   execd_params = sge_strdup(execd_params, Master_Config.execd_params);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(execd_params);
}

/**
 * @brief The `reporting_params` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_reporting_params() {
   DENTER(BASIS_LAYER);

   char* reporting_params = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   reporting_params = sge_strdup(reporting_params, Master_Config.reporting_params);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(reporting_params);
}

/**
 * @brief The `binding_params` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_binding_params() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   char *binding_params = sge_strdup(nullptr, Master_Config.binding_params);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(binding_params);
}

/**
 * @brief The `jsv_params` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_jsv_params() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   char *jsv_params = sge_strdup(nullptr, Master_Config.jsv_params);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(jsv_params);
}

/**
 * @brief The `gid_range` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_gid_range() {
   DENTER(BASIS_LAYER);

   char* gid_range = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   gid_range = sge_strdup(gid_range, Master_Config.gid_range);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(gid_range);
}

/**
 * @brief The `port_range` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_port_range() {
   DENTER(BASIS_LAYER);

   char* port_range = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   port_range = sge_strdup(port_range, Master_Config.port_range);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(port_range);
}

/**
 * @brief The `qlogin_daemon` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_qlogin_daemon() {
   DENTER(BASIS_LAYER);

   char* qlogin_daemon = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   qlogin_daemon = sge_strdup(qlogin_daemon, Master_Config.qlogin_daemon);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(qlogin_daemon);
}

/**
 * @brief The `qlogin_command` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_qlogin_command() {
   DENTER(BASIS_LAYER);

   char* qlogin_command = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   qlogin_command = sge_strdup(qlogin_command, Master_Config.qlogin_command);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(qlogin_command);
}

/**
 * @brief The `rsh_daemon` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_rsh_daemon() {
   DENTER(BASIS_LAYER);

   char* rsh_daemon = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   rsh_daemon = sge_strdup(rsh_daemon, Master_Config.rsh_daemon);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(rsh_daemon);
}

/**
 * @brief Set the `is_new_config` setting of the master configuration
 *
 * Takes the master configuration write lock.
 *
 * @param new_config the new value
 */
void mconf_set_new_config(bool new_config) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   is_new_config = new_config;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
   DRETURN_VOID;
}

/* make chached values from configuration invalid. */
/**
 * @brief The `is_new_config` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_is_new_config() {
   DENTER(BASIS_LAYER);

   bool is;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   is = is_new_config;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(is);
}

/**
 * @brief The `rsh_command` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_rsh_command() {
   DENTER(BASIS_LAYER);

   char* rsh_command = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   rsh_command = sge_strdup(rsh_command, Master_Config.rsh_command);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(rsh_command);
}

/**
 * @brief The `jsv_url` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_jsv_url() {
   DENTER(BASIS_LAYER);

   char* jsv_url = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   jsv_url = sge_strdup(jsv_url, Master_Config.jsv_url);
   sge_strip_trailing_blanks(jsv_url);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(jsv_url);
}

/**
 * @brief The `jsv_allowed_mod` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_jsv_allowed_mod() {
   DENTER(BASIS_LAYER);

   char* jsv_allowed_mod = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   jsv_allowed_mod = sge_strdup(jsv_allowed_mod, Master_Config.jsv_allowed_mod);
   sge_strip_trailing_blanks(jsv_allowed_mod);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(jsv_allowed_mod);
}

/**
 * @brief The `gdi_request_limits` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_gdi_request_limits() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   char* gdi_request_limits = sge_strdup(nullptr, Master_Config.gdi_request_limits);
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   sge_strip_trailing_blanks(gdi_request_limits);
   DRETURN(gdi_request_limits);
}

/**
 * @brief The `rlogin_daemon` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_rlogin_daemon() {
   DENTER(BASIS_LAYER);

   char* rlogin_daemon = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   rlogin_daemon = sge_strdup(rlogin_daemon, Master_Config.rlogin_daemon);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(rlogin_daemon);
}

/**
 * @brief The `rlogin_command` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_rlogin_command() {
   DENTER(BASIS_LAYER);

   char* rlogin_command = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   rlogin_command = sge_strdup(rlogin_command, Master_Config.rlogin_command);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(rlogin_command);
}

/**
 * @brief The `reschedule_unknown` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_reschedule_unknown() {
   DENTER(BASIS_LAYER);

   uint32_t reschedule_unknown;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   reschedule_unknown = Master_Config.reschedule_unknown;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(reschedule_unknown);
}

/**
 * @brief The `max_aj_instances` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_max_aj_instances() {
   DENTER(BASIS_LAYER);

   uint32_t max_aj_instances;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_aj_instances = Master_Config.max_aj_instances;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_aj_instances);
}

/**
 * @brief The `max_aj_tasks` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_max_aj_tasks() {
   DENTER(BASIS_LAYER);

   uint32_t max_aj_tasks;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_aj_tasks = Master_Config.max_aj_tasks;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_aj_tasks);
}

/**
 * @brief The `max_u_jobs` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_max_u_jobs() {
   DENTER(BASIS_LAYER);

   uint32_t max_u_jobs;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_u_jobs = Master_Config.max_u_jobs;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_u_jobs);
}

/**
 * @brief The `max_jobs` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_max_jobs() {
   DENTER(BASIS_LAYER);

   uint32_t max_jobs;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_jobs = Master_Config.max_jobs;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_jobs);
}

/**
 * @brief The `max_advance_reservations` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_max_advance_reservations() {
   DENTER(BASIS_LAYER);

   uint32_t max_advance_reservations;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_advance_reservations = Master_Config.max_advance_reservations;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_advance_reservations);
}

/* CS-1908: retention "age at which a finished ja_task is prunable", in seconds.
 * 0 disables the time dimension (retention bounded only by finished_jobs_max).
 * Default 0 = feature off. Top-level global-config attribute. */
/**
 * @brief The `finished_jobs_keep_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_finished_jobs_keep_time() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   const uint32_t value = Master_Config.finished_jobs_keep_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(value);
}

/* CS-1908: retention global count ceiling across master_job_list. 0 disables
 * the count dimension (retention bounded only by finished_jobs_keep_time).
 * Default 0 = feature off. Top-level global-config attribute. */
/**
 * @brief The `finished_jobs_max` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_finished_jobs_max() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   const uint32_t value = Master_Config.finished_jobs_max;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(value);
}

/**
 * @brief The `auto_user_fshare` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_auto_user_fshare() {
   DENTER(BASIS_LAYER);

   uint32_t auto_user_fshare;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   auto_user_fshare = Master_Config.auto_user_fshare;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(auto_user_fshare);
}

/**
 * @brief The `auto_user_oticket` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_auto_user_oticket() {
   DENTER(BASIS_LAYER);

   uint32_t auto_user_oticket;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   auto_user_oticket = Master_Config.auto_user_oticket;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(auto_user_oticket);
}

/**
 * @brief The `auto_user_default_project` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_auto_user_default_project() {
   DENTER(BASIS_LAYER);

   char* auto_user_default_project = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   auto_user_default_project = sge_strdup(auto_user_default_project, Master_Config.auto_user_default_project);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(auto_user_default_project);
}

/**
 * @brief The `auto_user_delete_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_auto_user_delete_time() {
   DENTER(BASIS_LAYER);

   uint32_t auto_user_delete_time;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   auto_user_delete_time = Master_Config.auto_user_delete_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(auto_user_delete_time);
}

/**
 * @brief The `delegated_file_staging` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_delegated_file_staging() {
   DENTER(BASIS_LAYER);

   char* delegated_file_staging = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   delegated_file_staging = sge_strdup(delegated_file_staging, Master_Config.delegated_file_staging);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(delegated_file_staging);
}


/* params */
/**
 * @brief The `is_monitor_message` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_is_monitor_message() {
   DENTER(BASIS_LAYER);

  bool is;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   is = qmaster_conf.is_monitor_message;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(is);
}

/**
 * @brief The `use_qidle` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_use_qidle() {
   DENTER(BASIS_LAYER);

   bool idle;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   idle = execd_conf.use_qidle;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(idle);
}

/**
 * @brief The `forbid_reschedule` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_forbid_reschedule() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.forbid_reschedule;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `forbid_apperror` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_forbid_apperror() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.forbid_apperror;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `do_credentials` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_do_credentials() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = security_conf.do_credentials;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `do_authentication` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_do_authentication() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = security_conf.do_authentication;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `acct_reserved_usage` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_acct_reserved_usage() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.acct_reserved_usage;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `sharetree_reserved_usage` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_sharetree_reserved_usage() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.sharetree_reserved_usage;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `keep_active` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
keep_active_t mconf_get_keep_active() {
   DENTER(BASIS_LAYER);

   keep_active_t ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.keep_active;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `usage_collection` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
usage_collection_t mconf_get_usage_collection() {
   DENTER(BASIS_LAYER);

   usage_collection_t ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = execd_conf.usage_collection;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN(ret);
}

/**
 * @brief The `enable_mem_details` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_mem_details() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.enable_mem_details;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `enable_addgrp_kill` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_addgrp_kill() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.enable_addgrp_kill;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/** @brief Get the value of the PDC_INTERVAL configuration parameter.
 *
 * Returns the value of PDC_DISABLED if the PDC_INTERVAL has been set to NEVER.
 * Will correspond to the load_report_time if the PDC_INTERVAL has been set PER_LOAD_REPORT.
 * 1 if not other specified.
 *
 * @return The value of the pdc_interval configuration parameter.
 */
/**
 * @brief The `pdc_interval` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint64_t mconf_get_pdc_interval() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   uint64_t ret = execd_conf.pdc_interval;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `enable_reschedule_kill` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_reschedule_kill() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.enable_reschedule_kill;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `enable_reschedule_slave` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_reschedule_slave() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.enable_reschedule_slave;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `old_reschedule_behavior` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_old_reschedule_behavior() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.old_reschedule_behavior;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `GPERF_NAME` setting from `qmaster_params`
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
std::string mconf_get_gperf_name() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   std::string ret = qmaster_conf.gperf_name;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `GPERF_THREADS` setting from `qmaster_params`
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
std::string mconf_get_gperf_threads() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   std::string ret = qmaster_conf.gperf_threads;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `old_reschedule_behavior_array_job` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_old_reschedule_behavior_array_job() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.old_reschedule_behavior_array_job;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `simulate_execds` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_simulate_execds() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.simulate_execds;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `simulate_jobs` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_simulate_jobs() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.simulate_jobs;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `ptf_max_priority` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
long mconf_get_ptf_max_priority() {
   DENTER(BASIS_LAYER);

   long ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.ptf_max_priority;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `ptf_min_priority` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
long mconf_get_ptf_min_priority() {
   DENTER(BASIS_LAYER);

   long ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.ptf_min_priority;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `use_qsub_gid` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_use_qsub_gid() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.use_qsub_gid;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `notify_susp_type` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_notify_susp_type() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.notify_susp_type;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `notify_susp` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_notify_susp() {
   DENTER(BASIS_LAYER);

   char* ret = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = sge_strdup(ret, notify_susp);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `notify_kill_type` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_notify_kill_type() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.notify_kill_type;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `notify_kill` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @return the configured value
 */
char* mconf_get_notify_kill() {
   DENTER(BASIS_LAYER);

   char* ret = nullptr;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = sge_strdup(ret, notify_kill);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `disable_reschedule` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_disable_reschedule() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.disable_reschedule;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `disable_secondary_ds` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_disable_secondary_ds() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.disable_secondary_ds;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `disable_secondary_ds_reader` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_disable_secondary_ds_reader() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.disable_secondary_ds_reader;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `disable_secondary_ds_execd` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_disable_secondary_ds_execd() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.disable_secondary_ds_execd;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `disable_automatic_sessions` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_disable_automatic_session() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.disable_automatic_sessions;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `scheduler_timeout` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_scheduler_timeout() {
   DENTER(BASIS_LAYER);

   int timeout;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   timeout = qmaster_conf.scheduler_timeout;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(timeout);
}

/**
 * @brief Set the `max_dynamic_event_clients` setting of the master configuration
 *
 * Takes the master configuration write lock.
 *
 * @param value the new value
 */
void mconf_set_max_dynamic_event_clients(int value) {
   DENTER(BASIS_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   qmaster_conf.max_dynamic_event_clients = value;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
   DRETURN_VOID;
}

/**
 * @brief The `max_dynamic_event_clients` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_max_dynamic_event_clients() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.max_dynamic_event_clients;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `set_lib_path` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_set_lib_path() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.set_lib_path;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `inherit_env` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_inherit_env() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.inherit_env;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `enable_hwloc` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_hwloc() {
   DENTER(BASIS_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   const bool ret = execd_conf.enable_hwloc;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN(ret);
}

// spooling interval in seconds
/**
 * @brief The `spool_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_spool_time() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.spool_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/* CS-1239: share-tree decay tick interval in seconds, as used by the
 * Timed Event Thread decay task. Returns the configured qmaster_params
 * STREE_TICK_INTERVAL value, clamped into
 * [STREE_TICK_INTERVAL_MIN, STREE_TICK_INTERVAL_MAX]. The configured
 * value defaults to STREE_TICK_INTERVAL_DEF if not set. */
/**
 * @brief The `sharetree_tick_interval` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_sharetree_tick_interval() {
   DENTER(BASIS_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   int value = qmaster_conf.sharetree_tick_interval;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   if (value < STREE_TICK_INTERVAL_MIN) {
      value = STREE_TICK_INTERVAL_MIN;
   } else if (value > STREE_TICK_INTERVAL_MAX) {
      value = STREE_TICK_INTERVAL_MAX;
   }

   DRETURN(value);
}

/* CS-1908: retention sweep tick interval in seconds. Dedicated to CS-1908 —
 * NOT shared with sharetree_tick_interval. Clamped into [MIN, MAX] at read time. */
/**
 * @brief The `finished_jobs_sweep_interval` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_finished_jobs_sweep_interval() {
   DENTER(BASIS_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   int value = qmaster_conf.finished_jobs_sweep_interval;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   if (value < FINISHED_JOBS_SWEEP_INTERVAL_MIN) {
      value = FINISHED_JOBS_SWEEP_INTERVAL_MIN;
   } else if (value > FINISHED_JOBS_SWEEP_INTERVAL_MAX) {
      value = FINISHED_JOBS_SWEEP_INTERVAL_MAX;
   }

   DRETURN(value);
}

/* CS-1908: retention sweep per-tick prune cap enforcing R18. When the
 * eligible-prune queue exceeds this cap, overflow defers to subsequent ticks.
 * Clamped into [MIN, MAX] at read time. */
/**
 * @brief The `finished_jobs_sweep_batch` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_finished_jobs_sweep_batch() {
   DENTER(BASIS_LAYER);

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   int value = qmaster_conf.finished_jobs_sweep_batch;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   if (value < FINISHED_JOBS_SWEEP_BATCH_MIN) {
      value = FINISHED_JOBS_SWEEP_BATCH_MIN;
   } else if (value > FINISHED_JOBS_SWEEP_BATCH_MAX) {
      value = FINISHED_JOBS_SWEEP_BATCH_MAX;
   }

   DRETURN(value);
}

/**
 * @brief The `max_ds_deviation` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_max_ds_deviation() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.max_ds_deviation;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `monitor_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_monitor_time() {
   DENTER(BASIS_LAYER);

   uint32_t monitor;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   monitor = qmaster_conf.monitor_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(monitor);

}

/**
 * @brief The `do_accounting` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_do_accounting() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.do_accounting;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

/**
 * @brief The `do_reporting` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_do_reporting() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.do_reporting;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

/**
 * @brief The `do_monitoring` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_do_monitoring() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.do_monitoring;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

/**
 * @brief The `do_joblog` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_do_joblog() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.do_joblog;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

/**
 * @brief The `reporting_flush_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_reporting_flush_time() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.reporting_flush_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

/**
 * @brief The `accounting_flush_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_accounting_flush_time() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.accounting_flush_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * Return whether accounting / reporting / monitoring files are to be flushed
 * with fsync(2) before close. Controlled by the reporting_params sub-option
 * sync_write; defaults to false. See CS-2411 for the NFSv3 "Stale file handle"
 * scenario that motivates this option.
 */
/**
 * @brief The `reporting_sync_write` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_reporting_sync_write() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   bool ret = reporting_conf.reporting_sync_write;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `old_accounting` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_old_accounting() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.old_accounting;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `old_reporting` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_old_reporting() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.old_reporting;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `sharelog_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_sharelog_time() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.sharelog_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `log_consumables` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_log_consumables() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.log_consumables;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `usage_patterns` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
std::string mconf_get_usage_patterns() {
   DENTER(BASIS_LAYER);

   std::string ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.usage_patterns;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * Validate an online_usage value (the text after `online_usage=`).
 *
 * The value is a `|`-separated list of tokens. Each token must be a
 * valid complex_name (= object_name) per sge_types(1); empty tokens are
 * rejected. An empty whole value is the disabled case and is valid; it
 * produces an empty `out_vars`.
 *
 * Used both by `check_config()` in qmaster (pre-commit rejection of bad
 * configurations from `qconf -Mconf`) and by #merge_configuration
 * (parse-time defence in depth).
 *
 * @param[out] answer_list receives the explanation when the value is rejected
 * @param value the `online_usage` value to parse; nullptr and the empty
 *              string are the disabled case and are valid
 * @param[out] out_vars receives the parsed tokens; its content is unspecified
 *                      when the function fails
 * @return true when the whole value was accepted
 */
bool
parse_online_usage_value(lList **answer_list, const char *value,
                         std::vector<std::string> &out_vars) {
   out_vars.clear();
   if (value == nullptr || *value == '\0') {
      return true;
   }
   const char *start = value;
   for (const char *p = value;; ++p) {
      if (*p == '|' || *p == '\0') {
         const size_t len = static_cast<size_t>(p - start);
         if (len == 0) {
            answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                                    MSG_CONF_INVALIDPARAM_EMPTYTOKEN_SS,
                                    "reporting_params", "online_usage");
            return false;
         }
         std::string token(start, len);
         if (verify_str_key(answer_list, token.c_str(), MAX_VERIFY_STRING,
                            "complex_name", KEY_TABLE) != STATUS_OK) {
            // verify_str_key has already added a descriptive message.
            return false;
         }
         out_vars.emplace_back(std::move(token));
         if (*p == '\0') {
            break;
         }
         start = p + 1;
      }
   }
   return true;
}

/**
 * Return the parsed online_usage variable names from the reporting_params.
 *
 * Set via `reporting_params=... online_usage=<var>[|<var>...]`.
 * An empty vector means the feature is disabled and no online_usage
 * records shall be written to the JSONL reporting file.
 *
 * @see merge_configuration()
 */
/**
 * @brief The `online_usage_vars` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
std::vector<std::string> mconf_get_online_usage_vars() {
   DENTER(BASIS_LAYER);

   std::vector<std::string> ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_conf.online_usage_vars;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `enable_forced_qdel` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_forced_qdel() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.enable_forced_qdel;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

/**
 * @brief The `enable_sup_grp_eval` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_sup_grp_eval() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   bool ret = false;
#if defined(WITH_EXTENSIONS)
   ret = qmaster_conf.enable_sup_grp_eval;
#endif
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

/**
 * @brief Set the `enable_sup_grp_eval` setting of the master configuration
 *
 * Takes the master configuration write lock.
 *
 * @param value the new value
 *
 * @note The assignment is compiled only with `WITH_EXTENSIONS`. Without it the
 *       call still takes the write lock and then does nothing.
 */
void mconf_set_enable_sup_grp_eval(bool value) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
#if defined(WITH_EXTENSIONS)
   qmaster_conf.enable_sup_grp_eval = value;
#endif
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
   DRETURN_VOID;
}

/**
 * @brief The `enable_enforce_master_limit` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_enforce_master_limit() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = qmaster_conf.enable_enforce_master_limit;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

/**
 * @brief The `enable_test_sleep_after_request` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_test_sleep_after_request() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = qmaster_conf.enable_test_sleep_after_request;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

/**
 * @brief The `enable_forced_qdel_if_unknown` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_forced_qdel_if_unknown() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = qmaster_conf.enable_forced_qdel_if_unknown;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * Whether any host which can reach the qmaster counts as a submit host.
 *
 * It grants what a submit host is granted and no more. Requests which ask for an admin
 * host are unaffected, and so is everything which is decided by the user rather than by
 * the host the request came from.
 */
bool mconf_get_allow_any_submithosts() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = qmaster_conf.allow_any_submithosts;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `ignore_ngroups_max_limit` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_ignore_ngroups_max_limit() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = execd_conf.ignore_ngroups_max_limit;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `enable_systemd` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_systemd() {
   DENTER(BASIS_LAYER);

   bool ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = execd_conf.enable_systemd;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `enable_submit_lib_path` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_submit_lib_path() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.enable_submit_lib_path;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `enable_submit_ld_preload` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_get_enable_submit_ld_preload() {
   DENTER(BASIS_LAYER);

   int ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = qmaster_conf.enable_submit_ld_preload;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `max_job_deletion_time` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_max_job_deletion_time() {
   DENTER(BASIS_LAYER);

   int deletion_time;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   deletion_time = qmaster_conf.max_job_deletion_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(deletion_time);
}

/**
 * @brief Read the `h_descriptors` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @param[out] pret receives the value
 */
void mconf_get_h_descriptors(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(execd_conf.h_descriptors);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/**
 * @brief Read the `s_descriptors` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @param[out] pret receives the value
 */
void mconf_get_s_descriptors(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(execd_conf.s_descriptors);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/**
 * @brief Read the `h_maxproc` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @param[out] pret receives the value
 */
void mconf_get_h_maxproc(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(execd_conf.h_maxproc);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/**
 * @brief Read the `s_maxproc` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @param[out] pret receives the value
 */
void mconf_get_s_maxproc(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(execd_conf.s_maxproc);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/**
 * @brief Read the `h_memorylocked` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @param[out] pret receives the value
 */
void mconf_get_h_memorylocked(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(execd_conf.h_memorylocked);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/**
 * @brief Read the `s_memorylocked` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @param[out] pret receives the value
 */
void mconf_get_s_memorylocked(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(execd_conf.s_memorylocked);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/**
 * @brief Read the `h_locks` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @param[out] pret receives the value
 */
void mconf_get_h_locks(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(execd_conf.h_locks);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/**
 * @brief Read the `s_locks` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @note The returned string is freshly allocated; the caller frees it.
 *
 * @param[out] pret receives the value
 */
void mconf_get_s_locks(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(execd_conf.s_locks);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/**
 * @brief The `jsv_threshold` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_jsv_threshold() {
   DENTER(BASIS_LAYER);

   int threshold;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   threshold = qmaster_conf.jsv_threshold;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(threshold);
}

/**
 * @brief The `jsv_timeout` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_jsv_timeout() {
   DENTER(BASIS_LAYER);

   int timeout;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   timeout = qmaster_conf.jsv_timeout;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(timeout);
}

/**
 * @brief The `script_timeout` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
uint32_t mconf_get_script_timeout() {
   DENTER(BASIS_LAYER);

   uint32_t ret;

   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = execd_conf.script_timeout;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `do_monitoring` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
std::tuple<uint32_t, bool, bool> mconf_get_monitoring_options() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   auto ret =  std::make_tuple(qmaster_conf.monitor_time, qmaster_conf.is_monitor_message, reporting_conf.do_monitoring);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/** @brief Returns true if binding is enabled
 *
 * This value is cached in the assignment structure for the scheduler to avoid calling this function.
 */
/**
 * @brief The `is_binding_enabled` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_is_binding_enabled() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   bool ret = binding_conf.is_binding_enabled;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `do_implicit_binding` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_do_implicit_binding() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   bool ret = binding_conf.do_implicit_binding;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `schedule_on_any_host` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
bool mconf_schedule_on_any_host() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   bool ret = binding_conf.schedule_on_any_host;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `binding_mode` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
binding_mode_t mconf_get_binding_mode() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   binding_mode_t ret = binding_conf.binding_mode;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `default_binding_unit` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
ocs::BindingUnit::Unit mconf_get_default_binding_unit() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ocs::BindingUnit::Unit ret = binding_conf.default_binding_unit;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `binding_filter` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
std::string mconf_get_binding_filter() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   std::string ret = binding_conf.binding_filter;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `topology_file` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
std::string mconf_get_topology_file() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   std::string ret = Master_Config.topology_file;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `s_ijs_escape_char` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
char mconf_get_ijs_escape_char() {
   DENTER(BASIS_LAYER);

   char ret;
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = qmaster_conf.s_ijs_escape_char;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `s_ijs_keepalive_interval` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_ijs_keepalive_interval() {
   DENTER(BASIS_LAYER);

   int ret;
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = qmaster_conf.s_ijs_keepalive_interval;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `s_ijs_keepalive_count` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_ijs_keepalive_count() {
   DENTER(BASIS_LAYER);

   int ret;
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = qmaster_conf.s_ijs_keepalive_count;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/**
 * @brief The `s_ijs_reconnect_timeout` setting of the master configuration
 *
 * Takes the master configuration read lock, so the value is a consistent
 * snapshot even while a new configuration is being applied.
 *
 * @return the configured value
 */
int mconf_get_ijs_reconnect_timeout() {
   DENTER(BASIS_LAYER);

   int ret;
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = qmaster_conf.s_ijs_reconnect_timeout;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

