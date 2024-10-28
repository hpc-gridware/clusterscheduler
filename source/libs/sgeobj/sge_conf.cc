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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#ifdef LINUX
#include <mcheck.h>
#endif

#include "cull/cull.h"

#include "uti/config_file.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_lock.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_profiling.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_string.h"

#include "comm/commlib.h"

#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userprj.h"
#include "sgeobj/sge_userset.h"

#include "basis_types.h"
#include "uti/sge.h"

#define SGE_BIN "bin"
#define STREESPOOLTIMEDEF 240

/* This list is *ONLY* used by the execd and should be moved eventually */
lList *Execd_Config_List = nullptr;

struct confel {                       /* cluster configuration parameters */
    char        *execd_spool_dir;     /* sge_spool directory base path */
    char        *mailer;              /* path to e-mail delivery agent */
    char        *xterm;               /* xterm path for interactive jobs */
    char        *load_sensor;         /* path to a load sensor executable */
    char        *prolog;              /* start before jobscript may be none */
    char        *epilog;              /* start after jobscript may be none */
    char        *shell_start_mode;    /* script_from_stdin/posix_compliant/unix_behavior */
    char        *login_shells;        /* list of shells to call as login shell */
    u_long32    min_uid;              /* lower bound on UIDs that can qsub */
    u_long32    min_gid;              /* lower bound on GIDs that can qsub */
    u_long32    load_report_time;     /* how often to send in load */
    u_long32    max_unheard;          /* how long before sge_execd considered dead */
    u_long32    loglevel;             /* qmaster event logging level */
    char        *enforce_project;     /* SGEEE attribute: "true" or "false" */
    char        *enforce_user;        /* SGEEE attribute: "true" or "false" */
    char        *administrator_mail;  /* list of mail addresses */
    lList       *user_lists;          /* allowed user lists */
    lList       *xuser_lists;         /* forbidden users lists */
    lList       *projects;            /* allowed project list */
    lList       *xprojects;           /* forbiddent project list */
    char        *set_token_cmd;
    char        *pag_cmd;
    u_long32    token_extend_time;
    char        *shepherd_cmd;
    char        *qmaster_params;
    char        *execd_params;
    char        *reporting_params;
    char        *gid_range;           /* Range of additional group ids */
    u_long32    zombie_jobs;          /* jobs to save after execution */
    char        *qlogin_daemon;       /* eg /usr/sbin/in.telnetd */
    char        *qlogin_command;      /* eg telnet $HOST $PORT */
    char        *rsh_daemon;          /* eg /usr/sbin/in.rshd */
    char        *rsh_command;         /* eg rsh -p $PORT $HOST command */
    char        *jsv_url;             /* jsv url */
    char        *jsv_allowed_mod;     /* allowed modifications for end users if JSV is enabled */
    char        *rlogin_daemon;       /* eg /usr/sbin/in.rlogind */
    char        *rlogin_command;      /* eg rlogin -p $PORT $HOST */
    u_long32    reschedule_unknown;   /* timout value used for auto. resch. */ 
    u_long32    max_aj_instances;     /* max. number of ja instances of a job */
    u_long32    max_aj_tasks;         /* max. size of an array job */
    u_long32    max_u_jobs;           /* max. number of jobs per user */
    u_long32    max_jobs;             /* max. number of jobs in the system */
    u_long32    max_advance_reservations; /* max. number of advance reservations in the system */
    u_long32    reprioritize;         /* reprioritize jobs based on the tickets or not */
    u_long32    auto_user_fshare;     /* SGEEE automatic user fshare */
    u_long32    auto_user_oticket;    /* SGEEE automatic user oticket */
    char        *auto_user_default_project; /* SGEEE automatic user default project */
    u_long32    auto_user_delete_time; /* SGEEE automatic user delete time */
    char        *delegated_file_staging; /*drmaa attribute: "true" or "false" */
    char        *libjvm_path;         /* libjvm_path for jvm_thread */
    char        *additional_jvm_args; /* additional_jvm_args for jvm_thread */
};

typedef struct confel sge_conf_type;

static sge_conf_type Master_Config = {
   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0,
   nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0, nullptr,
   nullptr, nullptr, nullptr, nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr,
   nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0, 0,
   0, 0, nullptr, 0, nullptr, nullptr, nullptr
};
static bool is_new_config = false;
static bool forbid_reschedule = false;
static bool forbid_apperror = false;
static bool enable_forced_qdel = false;
static bool enable_sup_grp_eval = false;
static bool enable_enforce_master_limit = false;
static bool enable_test_sleep_after_request = false;
static bool enable_forced_qdel_if_unknown = false;
static bool ignore_ngroups_max_limit = false;
static bool do_credentials = true;
static bool do_authentication = true;
static bool is_monitor_message = true;
static bool use_qidle = false;
static bool disable_reschedule = false;
static bool disable_secondary_ds = false;
#define DEFAULT_DISABLE_SECONDARY_DS_READER (false)
static bool disable_secondary_ds_reader = DEFAULT_DISABLE_SECONDARY_DS_READER;
static bool disable_secondary_ds_execd = false;
static bool prof_listener_thrd = false;
static bool prof_worker_thrd = false;
static bool prof_signal_thrd = false;
static bool prof_scheduler_thrd = false;
static bool prof_deliver_thrd = false;
static bool prof_tevent_thrd = false;
static bool prof_execd_thrd = false;
static u_long32 monitor_time = 0;
static bool enable_reschedule_kill = false;
static bool enable_reschedule_slave = false;
static bool old_reschedule_behavior = false;
static bool old_reschedule_behavior_array_job = false;

#ifdef LINUX
static bool enable_mtrace = false;
#endif

static long ptf_max_priority = -999;
static long ptf_min_priority = -999;
static int max_dynamic_event_clients = 1000;

static keep_active_t keep_active = KEEP_ACTIVE_FALSE;
static u_long32 script_timeout = 120;
#ifdef LINUX
static bool enable_binding = true;
#else
static bool enable_binding = false;
#endif
static bool enable_addgrp_kill = false;
static u_long32 pdc_interval = 1;
static char s_descriptors[100];
static char h_descriptors[100];
static char s_maxproc[100];
static char h_maxproc[100];
static char s_memorylocked[100];
static char h_memorylocked[100];
static char s_locks[100];
static char h_locks[100];

/* 
 * reporting params 
 * when you add new params, make sure to set them to default values in 
 * parsing code (merge_configuration)
 */
static bool do_accounting         = true;
static bool do_reporting          = false;
static bool do_joblog             = false;
static int reporting_flush_time   = 15;
static int accounting_flush_time  = -1;
static bool old_accounting = false;
static bool old_reporting = false;
static int sharelog_time          = 0;
static bool log_consumables       = false;
static std::string usage_patterns;

/* generally simulate all execd's */
static bool simulate_execds = false;

/* allow the simulation of jobs (job spooling and execution on execd side is disabled) */
static bool simulate_jobs = false;

/*
 * This value overrides the default scheduler timeout (10 minutes)
 * to allow graceful degradation on extremely busy systems with
 * tens of thousands or hundreds of thousands of pending jobs.
 */

static int scheduler_timeout = 0;

/**
 * This value specifies the minimum time for spooling the sharetree usage.
 * It is used and evaluated in the sge_follow module. The users and
 * projects are spooled, when the qmaster goes down.
 */
static int spool_time = STREESPOOLTIMEDEF;

// Maximum time in milliseconds to wait before update of secondary DS is enforced
#define DEFAULT_DS_DEVIATION (1000)
static int max_ds_deviation = DEFAULT_DS_DEVIATION;

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

static bool acct_reserved_usage = false;
static bool sharetree_reserved_usage = false;

/* 
 * Use primary group of qsub-host also for the job execution
 */
static bool use_qsub_gid = false;

/*
 * Job environment inheritance
 */
static bool set_lib_path = false;
/* This should match the default set in
 * shepherd/builtin_starter.c:inherit_env(). */
static bool inherit_env = true;
static bool enable_submit_lib_path = false;
static bool enable_submit_ld_preload = false;

std::string gperf_name = "gperf";
std::string gperf_threads = "*";

/*
 * notify_kill_default and notify_susp_default
 *       0  -> use the signal type stored in notify_kill and notify_susp
 *       1  -> user default signale (USR1 for susp and usr2 for kill)
 *       2  -> do not send a signal
 *
 * notify_kill and notify_susp:
 *       !nullptr -> Name of the signale (later used in sys_string2signal)
 */
static int   notify_susp_type = 1;
static char* notify_susp = nullptr;
static int   notify_kill_type = 1;
static char* notify_kill = nullptr;

typedef struct {
  const char *name;              /* name of parameter */
  int local;               /* 0 | 1 -> local -> may be overidden by local conf */
  const char *value;             /* value of parameter */
  int isSet;               /* 0 | 1 -> is already set */        
  char *envp;              /* pointer to environment variable */
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

static int max_job_deletion_time = 3;
static int jsv_timeout = 10;
static int jsv_threshold = 5000;

#define MAILER                    "/bin/mail"
#define PROLOG                    "none"
#define EPILOG                    "none"
#define SHELL_START_MODE          "posix_compliant"
#define LOGIN_SHELLS              "none"
#define MIN_UID                   "0"
#define MIN_GID                   "0"
#define MAX_UNHEARD               "0:2:30"
#define LOAD_LOG_TIME             "0:0:40"
#define STAT_LOG_TIME             "0:15:0"
#define LOGLEVEL                  "log_info"
#define ADMIN_USER                "none"
#define FINISHED_JOBS             "0"
#define RESCHEDULE_UNKNOWN        "0:0:0"
#define IGNORE_FQDN               "true"
#define MAX_AJ_INSTANCES          "2000"
#define MAX_AJ_TASKS              "75000"
#define MAX_U_JOBS                "0"
#define MAX_JOBS                  "0"
#define MAX_ADVANCE_RESERVATIONS  "0"
#define REPORTING_PARAMS          "accounting=true reporting=false flush_time=00:00:15 joblog=false sharelog=00:00:00"

static tConfEntry conf_entries[] = {
 { "execd_spool_dir",            1, nullptr,                   1, nullptr},
 { "mailer",                     1, MAILER,                    1, nullptr},
 { "xterm",                      1, "/usr/bin/X11/xterm",      1, nullptr},
 { "load_sensor",                1, "none",                    1, nullptr},
 { "prolog",                     1, PROLOG,                    1, nullptr},
 { "epilog",                     1, EPILOG,                    1, nullptr},
 { "shell_start_mode",           1, SHELL_START_MODE,          1, nullptr},
 { "login_shells",               1, LOGIN_SHELLS,              1, nullptr},
 { "min_uid",                    0, MIN_UID,                   1, nullptr},
 { "min_gid",                    0, MIN_GID,                   1, nullptr},
 { "user_lists",                 0, "none",                    1, nullptr},
 { "xuser_lists",                0, "none",                    1, nullptr},
 { "projects",                   0, "none",                    1, nullptr},
 { "xprojects",                  0, "none",                    1, nullptr},
 { "load_report_time",           1, LOAD_LOG_TIME,             1, nullptr},
 { "max_unheard",                0, MAX_UNHEARD,               1, nullptr},
 { "loglevel",                   0, LOGLEVEL,                  1, nullptr},
 { "enforce_project",            0, "false",                   1, nullptr},
 { "enforce_user",               0, "false",                   1, nullptr},
 { "administrator_mail",         0, "none",                    1, nullptr},
 { "set_token_cmd",              1, "none",                    1, nullptr},
 { "pag_cmd",                    1, "none",                    1, nullptr},
 { "token_extend_time",          1, "24:0:0",                  1, nullptr},
 { "shepherd_cmd",               1, "none",                    1, nullptr},
 { "qmaster_params",             0, "none",                    1, nullptr},
 { "execd_params",               1, "none",                    1, nullptr},
 { "reporting_params",           1, REPORTING_PARAMS,          1, nullptr},
 { "gid_range",                  1, "none",                    1, nullptr},
 { "finished_jobs",              0, FINISHED_JOBS,             1, nullptr},
 { "qlogin_daemon",              1, "none",                    1, nullptr},
 { "qlogin_command",             1, "none",                    1, nullptr},
 { "rsh_daemon",                 1, "none",                    1, nullptr},
 { "rsh_command",                1, "none",                    1, nullptr},
 { "jsv_url",                    0, "none",                    1, nullptr},
 { "jsv_allowed_mod",            0, "none",                    1, nullptr},
 { "rlogin_daemon",              1, "none",                    1, nullptr},
 { "rlogin_command",             1, "none",                    1, nullptr},
 { "reschedule_unknown",         1, RESCHEDULE_UNKNOWN,        1, nullptr},
 { "max_aj_instances",           0, MAX_AJ_INSTANCES,          1, nullptr},
 { "max_aj_tasks",               0, MAX_AJ_TASKS,              1, nullptr},
 { "max_u_jobs",                 0, MAX_U_JOBS,                1, nullptr},
 { "max_jobs",                   0, MAX_JOBS,                  1, nullptr},
 { "max_advance_reservations",   0, MAX_ADVANCE_RESERVATIONS,  1, nullptr},
 { REPRIORITIZE,                 0, "1",                       1, nullptr},
 { "auto_user_oticket",          0, "0",                       1, nullptr},
 { "auto_user_fshare",           0, "0",                       1, nullptr},
 { "auto_user_default_project",  0, "none",                    1, nullptr},
 { "auto_user_delete_time",      0, "0",                       1, nullptr},
 { "delegated_file_staging",     0, "false",                   1, nullptr},
 { "libjvm_path",                1, "",                        1, nullptr},
 { "additional_jvm_args",        1, "",                        1, nullptr},
 { nullptr,                      0, nullptr,                   0, nullptr}
};

/**
 * \brief Initialize config list with compiled in values.
 *
 * \details
 * This function sets the spool directories from the cell and initializes
 * the configuration list with compiled in values.
 *
 * \param cell_root The root directory of the cell.
 * \param lpCfg Pointer to the configuration list.
 */
static void sge_set_defined_defaults(const char *cell_root, lList **lpCfg)
{
   int i = 0; 
   lListElem *ep = nullptr;
   tConfEntry *pConf = nullptr;

   DENTER(BASIS_LAYER);

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
 * \brief Seeks for a config attribute "name", frees old value (if string) from *cpp and writes new value into *cpp.
 *
 * \details
 * This function searches for a configuration attribute by its name, frees the old value if it is a string,
 * and writes the new value into the provided pointer. Logging is done to a file.
 *
 * \param lp_cfg The configuration list.
 * \param name The name of the configuration attribute.
 * \param cpp Pointer to the old value to be replaced.
 * \param val Pointer to the new value to be set.
 * \param type The type of the value.
 */
static void
chg_conf_val(lList *lp_cfg, const char *name, char **cpp, u_long32 *val, int type) {
   const lListElem *ep;
   const char *s;

   if ((ep = lGetElemStr(lp_cfg, CF_name, name))) {
      s = lGetString(ep, CF_value);
      if (s) {
         int old_verbose = log_state_get_log_verbose();
  
         /* prevent logging function from writing to stderr
          * but log into log file 
          */
         log_state_set_log_verbose(0);
         INFO(MSG_CONF_USING_SS, s, name);
         log_state_set_log_verbose(old_verbose);
      }
      if (cpp)
         *cpp = sge_strdup(*cpp, s);
      else
         parse_ulong_val(nullptr, val, type, s, nullptr, 0);
   }
}

/**
 * \brief set the master configuration from cull
 *
 * \details
 * This function sets the master configuration from cull.
 *
 * \param lpCfg The configuration list.
 *
 * \note
 * MT-NOTE: setConfFromCull() is not MT safe, caller needs LOCK_MASTER_CONF as write lock.
 */
static void
setConfFromCull(lList *lpCfg) {
   const lListElem *ep;

   DENTER(BASIS_LAYER);

   /* get following logging entries logged if log_info is selected */
   chg_conf_val(lpCfg, "loglevel", nullptr, &Master_Config.loglevel, TYPE_LOG);
   log_state_set_log_level(Master_Config.loglevel);
   
   chg_conf_val(lpCfg, "execd_spool_dir", &Master_Config.execd_spool_dir, nullptr, 0);
   chg_conf_val(lpCfg, "mailer", &Master_Config.mailer, nullptr, 0);
   chg_conf_val(lpCfg, "xterm", &Master_Config.xterm, nullptr, 0);
   chg_conf_val(lpCfg, "load_sensor", &Master_Config.load_sensor, nullptr, 0);
   chg_conf_val(lpCfg, "prolog", &Master_Config.prolog, nullptr, 0);
   chg_conf_val(lpCfg, "epilog", &Master_Config.epilog, nullptr, 0);
   chg_conf_val(lpCfg, "shell_start_mode", &Master_Config.shell_start_mode, nullptr, 0);
   chg_conf_val(lpCfg, "login_shells", &Master_Config.login_shells, nullptr, 0);
   chg_conf_val(lpCfg, "min_uid", nullptr, &Master_Config.min_uid, TYPE_INT);
   chg_conf_val(lpCfg, "min_gid", nullptr, &Master_Config.min_gid, TYPE_INT);
   chg_conf_val(lpCfg, "gid_range", &Master_Config.gid_range, nullptr, 0);

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
   
   chg_conf_val(lpCfg, "load_report_time", nullptr, &Master_Config.load_report_time, TYPE_TIM);
   chg_conf_val(lpCfg, "enforce_project", &Master_Config.enforce_project, nullptr, 0);
   chg_conf_val(lpCfg, "enforce_user", &Master_Config.enforce_user, nullptr, 0);
   chg_conf_val(lpCfg, "max_unheard", nullptr, &Master_Config.max_unheard, TYPE_TIM);
   chg_conf_val(lpCfg, "loglevel", nullptr, &Master_Config.loglevel, TYPE_LOG);
   chg_conf_val(lpCfg, "administrator_mail", &Master_Config.administrator_mail, nullptr, 0);
   chg_conf_val(lpCfg, "set_token_cmd", &Master_Config.set_token_cmd, nullptr, 0);
   chg_conf_val(lpCfg, "pag_cmd", &Master_Config.pag_cmd, nullptr, 0);
   chg_conf_val(lpCfg, "token_extend_time", nullptr, &Master_Config.token_extend_time, TYPE_TIM);
   chg_conf_val(lpCfg, "shepherd_cmd", &Master_Config.shepherd_cmd, nullptr, 0);
   chg_conf_val(lpCfg, "qmaster_params", &Master_Config.qmaster_params, nullptr, 0);
   chg_conf_val(lpCfg, "execd_params",  &Master_Config.execd_params, nullptr, 0);
   chg_conf_val(lpCfg, "reporting_params",  &Master_Config.reporting_params, nullptr, 0);
   chg_conf_val(lpCfg, "finished_jobs", nullptr, &Master_Config.zombie_jobs, TYPE_INT);
   chg_conf_val(lpCfg, "qlogin_daemon", &Master_Config.qlogin_daemon, nullptr, 0);
   chg_conf_val(lpCfg, "qlogin_command", &Master_Config.qlogin_command, nullptr, 0);
   chg_conf_val(lpCfg, "rsh_daemon", &Master_Config.rsh_daemon, nullptr, 0);
   chg_conf_val(lpCfg, "rsh_command", &Master_Config.rsh_command, nullptr, 0);
   chg_conf_val(lpCfg, "jsv_url", &Master_Config.jsv_url, nullptr, 0);
   chg_conf_val(lpCfg, "jsv_allowed_mod", &Master_Config.jsv_allowed_mod, nullptr, 0);
   chg_conf_val(lpCfg, "rlogin_daemon", &Master_Config.rlogin_daemon, nullptr, 0);
   chg_conf_val(lpCfg, "rlogin_command", &Master_Config.rlogin_command, nullptr, 0);

   chg_conf_val(lpCfg, "reschedule_unknown", nullptr, &Master_Config.reschedule_unknown, TYPE_TIM);

   chg_conf_val(lpCfg, "max_aj_instances", nullptr, &Master_Config.max_aj_instances, TYPE_INT);
   chg_conf_val(lpCfg, "max_aj_tasks", nullptr, &Master_Config.max_aj_tasks, TYPE_INT);
   chg_conf_val(lpCfg, "max_u_jobs", nullptr, &Master_Config.max_u_jobs, TYPE_INT);
   chg_conf_val(lpCfg, "max_jobs", nullptr, &Master_Config.max_jobs, TYPE_INT);
   chg_conf_val(lpCfg, "max_advance_reservations", nullptr, &Master_Config.max_advance_reservations, TYPE_INT);
   chg_conf_val(lpCfg, REPRIORITIZE, nullptr, &Master_Config.reprioritize, TYPE_BOO );
   chg_conf_val(lpCfg, "auto_user_oticket", nullptr, &Master_Config.auto_user_oticket, TYPE_INT);
   chg_conf_val(lpCfg, "auto_user_fshare", nullptr, &Master_Config.auto_user_fshare, TYPE_INT);
   chg_conf_val(lpCfg, "auto_user_default_project", &Master_Config.auto_user_default_project, nullptr, 0);
   chg_conf_val(lpCfg, "auto_user_delete_time", nullptr, &Master_Config.auto_user_delete_time, TYPE_TIM);
   chg_conf_val(lpCfg, "delegated_file_staging", &Master_Config.delegated_file_staging, nullptr, 0);
   chg_conf_val(lpCfg, "libjvm_path", &Master_Config.libjvm_path, nullptr, 0);
   chg_conf_val(lpCfg, "additional_jvm_args", &Master_Config.additional_jvm_args, nullptr, 0);
   DRETURN_VOID;
}

/**
 * \brief getConfEntry
 *
 * \details
 * Return a pointer to the config element "name".
 *
 * \param name The name of the configuration element.
 * \param conf The array of configuration entries.
 *
 * \return A pointer to the configuration entry.
 */
static tConfEntry *
getConfEntry(const char *name, tConfEntry conf[]) {
 int i;
   
 DENTER(BASIS_LAYER);

 for (i = 0; conf[i].name; i++) {
    if (!strcasecmp(conf[i].name,name)) {   
       DRETURN(&conf[i]);
    }
 }   
     
 DRETURN(nullptr);
}

/**
 * \brief merge global and local configuration
 *
 * \details
 * Merge global and local configuration and set lpp list and
 * set conf struct from lpp.
 *
 * \param global Global configuration.
 * \param local Local configuration.
 * \param lpp Target configuration.
 *
 * \return 0 on success, -2 if no global configuration.
 *
 * \note
 * MT-NOTE: merge_configuration() is MT safe.
 */
int merge_configuration(lList **answer_list, u_long32 progid, const char *cell_root, lListElem *global, lListElem *local, lList **lpp) {
   const lList *cl;
   const lListElem *elem;
   lListElem *ep2;
   lList *mlist = nullptr;
   
   DENTER(BASIS_LAYER);
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
      u_long32 load_report_time = mconf_get_load_report_time();
#ifdef LINUX
      bool mtrace_before = enable_mtrace;
#endif

      SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      forbid_reschedule = false;
      forbid_apperror = false;
      enable_forced_qdel = false;
      enable_sup_grp_eval = false;
      enable_enforce_master_limit = false;
      enable_test_sleep_after_request = false;
      enable_forced_qdel_if_unknown = false;
      ignore_ngroups_max_limit = false;
      do_credentials = true;
      do_authentication = true;
      is_monitor_message = true;
      spool_time = STREESPOOLTIMEDEF;
      max_ds_deviation = DEFAULT_DS_DEVIATION;
      use_qidle = false;
      disable_reschedule = false;   
      disable_secondary_ds = false;
      disable_secondary_ds_reader = DEFAULT_DISABLE_SECONDARY_DS_READER;
      disable_secondary_ds_execd = false;
      simulate_execds = false;
      simulate_jobs = false;
      prof_listener_thrd = false;
      prof_worker_thrd = false;
      prof_signal_thrd = false;
      prof_scheduler_thrd = false;
      prof_deliver_thrd = false;
      prof_tevent_thrd = false;
      monitor_time = 0;
      scheduler_timeout = 0;
      max_dynamic_event_clients = 1000;
      max_job_deletion_time = 3;
      enable_reschedule_kill = false;
      enable_reschedule_slave = false;
      old_reschedule_behavior = false;
      old_reschedule_behavior_array_job = false;
      jsv_threshold = 5000;
      jsv_timeout= 10;
      enable_submit_lib_path = false;
      enable_submit_ld_preload = false;

      for (s=sge_strtok_r(qmaster_params, PARAMS_DELIMITER, &conf_context); s; s=sge_strtok_r(nullptr, PARAMS_DELIMITER, &conf_context)) {
         if (parse_bool_param(s, "FORBID_RESCHEDULE", &forbid_reschedule)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_SIGNAL", &prof_signal_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_SCHEDULER", &prof_scheduler_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_LISTENER", &prof_listener_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_WORKER", &prof_worker_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_DELIVER", &prof_deliver_thrd)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_TEVENT", &prof_tevent_thrd)) {
            continue;
         }
         if (parse_int_param(s, "STREE_SPOOL_INTERVAL", &spool_time, TYPE_TIM)) {
            if (spool_time <= 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "STREE_SPOOL_INTERVAL",
                                       STREESPOOLTIMEDEF);
               spool_time = STREESPOOLTIMEDEF;
            }
            continue;
         }
         if (parse_bool_param(s, "FORBID_APPERROR", &forbid_apperror)) {
            continue;
         }   
         if (parse_bool_param(s, "ENABLE_FORCED_QDEL", &enable_forced_qdel)) {
            continue;
         } 
         if (parse_bool_param(s, "ENABLE_SUP_GRP_EVAL", &enable_sup_grp_eval)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_ENFORCE_MASTER_LIMIT", &enable_enforce_master_limit)) {
            continue;
         } 
         if (parse_bool_param(s, "__TEST_SLEEP_AFTER_REQUEST", &enable_test_sleep_after_request)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_FORCED_QDEL_IF_UNKNOWN", &enable_forced_qdel_if_unknown)) {
            continue;
         } 
#ifdef LINUX
         if (parse_bool_param(s, "ENABLE_MTRACE", &enable_mtrace)) {
            continue;
         }
#endif
         if (parse_time_param(s, "MONITOR_TIME", &monitor_time)) {
            continue;
         }
         if (!strncasecmp(s, "MAX_DYN_EC", sizeof("MAX_DYN_EC")-1)) {
            max_dynamic_event_clients = atoi(&s[sizeof("MAX_DYN_EC=")-1]);
            continue;
         }
         if (parse_bool_param(s, "NO_SECURITY", &do_credentials)) {
            /* reversed logic */
            do_credentials = do_credentials ? false : true;
            continue;
         } 
         if (parse_bool_param(s, "NO_AUTHENTICATION", &do_authentication)) {
            /* reversed logic */
            do_authentication = do_authentication ? false : true;
            continue;
         } 
         if (parse_bool_param(s, "DISABLE_AUTO_RESCHEDULING", &disable_reschedule)) {
            continue;
         }
         if (parse_bool_param(s, "DISABLE_SECONDARY_DS", &disable_secondary_ds)) {
            continue;
         }
         if (parse_bool_param(s, "DISABLE_SECONDARY_DS_READER", &disable_secondary_ds_reader)) {
            continue;
         }
         if (parse_bool_param(s, "DISABLE_SECONDARY_DS_EXECD", &disable_secondary_ds_execd)) {
            continue;
         }
         if (parse_int_param(s, "MAX_DS_DEVIATION", &max_ds_deviation, TYPE_TIM)) {
            if (max_ds_deviation < 0 || max_ds_deviation > 5000) {
               max_ds_deviation = DEFAULT_DS_DEVIATION;
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "MAX_DS_DEVIATION", DEFAULT_DS_DEVIATION);
            }
            continue;
         }
         if (parse_bool_param(s, "LOG_MONITOR_MESSAGE", &is_monitor_message)) {
            continue;
         }
         if (parse_bool_param(s, "SIMULATE_EXECDS", &simulate_execds)) {
            continue;
         }
         if (!strncasecmp(s, "SCHEDULER_TIMEOUT",
                    sizeof("SCHEDULER_TIMEOUT")-1)) {
            scheduler_timeout=atoi(&s[sizeof("SCHEDULER_TIMEOUT=")-1]);
            continue;
         }
         if (parse_int_param(s, "max_job_deletion_time", &max_job_deletion_time, TYPE_TIM)) {
            if (max_job_deletion_time <= 0 || max_job_deletion_time > 5) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "max_job_deletion_time",
                                       3);
               max_job_deletion_time = 3;
            }
            continue;
         }
         if (parse_bool_param(s, "ENABLE_RESCHEDULE_KILL", &enable_reschedule_kill)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_RESCHEDULE_SLAVE", &enable_reschedule_slave)) {
            continue;
         }

         // if enabled does not change submit time when a job is rescheduled
         if (parse_bool_param(s, "OLD_RESCHEDULE_BEHAVIOR", &old_reschedule_behavior)) {
            continue;
         }

         // if enabled does not change submit time when a job array task is rescheduled
         if (parse_bool_param(s, "OLD_RESCHEDULE_BEHAVIOR_ARRAY_JOB", &old_reschedule_behavior_array_job)) {
            continue;
         }

         if (parse_int_param(s, "jsv_threshold", &jsv_threshold, TYPE_TIM)) {
            if (jsv_threshold < 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "jsv_threshold",
                                       5000);
               jsv_threshold = 5000;
            }
            continue;
         }
         if (parse_int_param(s, "jsv_timeout", &jsv_timeout, TYPE_TIM)) {
            if (jsv_timeout <= 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "qmaster_params", "jsv_timeout",
                                       10);
               jsv_timeout = 10;
            }
            continue;
         }
         if (parse_bool_param(s, "ENABLE_SUBMIT_LIB_PATH", &enable_submit_lib_path)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_SUBMIT_LD_PRELOAD", &enable_submit_ld_preload)) {
            continue;
         }
         if (parse_string_param(s, "GPERF_NAME", gperf_name)) {
            continue;
         }
         if (parse_string_param(s, "GPERF_THREADS", gperf_threads)) {
            continue;
         }
      }
      SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      sge_free_saved_vars(conf_context);
      conf_context = nullptr;
     
#ifdef LINUX
      /* enable/disable GNU malloc library facility for recording of all 
         memory allocation/deallocation 
         requires MALLOC_TRACE in environment (see mtrace(3) under Linux) */
      if (enable_mtrace != mtrace_before) {
         if (enable_mtrace) {
            DPRINTF("ENABLE_MTRACE=true ---> mtrace()\n");
            mtrace();
         } else {
            DPRINTF("ENABLE_MTRACE=false ---> muntrace()\n");
            muntrace();
         }
      }
#endif

      conf_update_thread_profiling(nullptr);

      /* always initialize to defaults before we check execd_params */
      SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
#ifdef COMPILE_DC
      acct_reserved_usage = false;
      sharetree_reserved_usage = false;
#else
      acct_reserved_usage = false;
      sharetree_reserved_usage = true;
#endif
      notify_kill_type = 1;
      notify_susp_type = 1;
      ptf_max_priority = -999;
      ptf_min_priority = -999;
      keep_active = KEEP_ACTIVE_FALSE;
      script_timeout = 120;
#ifdef LINUX
      enable_binding = true;
#else
      enable_binding = false;
#endif
      enable_addgrp_kill = false;
      use_qsub_gid = false;
      prof_execd_thrd = false;
      inherit_env = true;
      set_lib_path = false;
      do_accounting = true;
      do_reporting = false;
      do_joblog = false;
      reporting_flush_time = 15;
      accounting_flush_time = -1;
      old_accounting = false;
      old_reporting = false;
      sharelog_time = 0;
      log_consumables = false;
      usage_patterns.clear();
      enable_addgrp_kill = false;
      strcpy(s_descriptors, "UNDEFINED");
      strcpy(h_descriptors, "UNDEFINED");
      strcpy(s_maxproc, "UNDEFINED");
      strcpy(h_maxproc, "UNDEFINED");
      strcpy(s_memorylocked, "UNDEFINED");
      strcpy(h_memorylocked, "UNDEFINED");
      strcpy(s_locks, "UNDEFINED");
      strcpy(h_locks, "UNDEFINED");

      for (s=sge_strtok_r(execd_params, PARAMS_DELIMITER, &conf_context); s; s=sge_strtok_r(nullptr, PARAMS_DELIMITER, &conf_context)) {
         if (parse_bool_param(s, "USE_QIDLE", &use_qidle)) {
            continue;
         }
         if (progid == EXECD) {
            if (parse_bool_param(s, "NO_SECURITY", &do_credentials)) { 
               /* reversed logic */
               do_credentials = do_credentials ? false : true;
               continue;
            }
            if (parse_bool_param(s, "NO_AUTHENTICATION", &do_authentication)) {
               /* reversed logic */
               do_authentication = do_authentication ? false : true;
               continue;
            }
            if (parse_bool_param(s, "DO_AUTHENTICATION", &do_authentication)) {
               continue;
            }
         }
         {
            if (strncasecmp(s, "KEEP_ACTIVE", sizeof("KEEP_ACTIVE")-1) == 0) {
               const char *keep_active_value = &s[sizeof("KEEP_ACTIVE=")-1];

               if (strncasecmp(keep_active_value, "ERROR", sizeof("ERROR")-1) == 0) {
                  keep_active = KEEP_ACTIVE_ERROR;
               } else if (strncasecmp(keep_active_value, TRUE_STR, sizeof(TRUE_STR)-1) == 0) {
                  keep_active = KEEP_ACTIVE_TRUE;
               } else {
                  keep_active = KEEP_ACTIVE_FALSE;
               }
               continue;
            }
         }
         if (parse_time_param(s, "SCRIPT_TIMEOUT", &script_timeout)) {
            continue;
         }
         if (parse_bool_param(s, "SIMULATE_JOBS", &simulate_jobs)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_BINDING", &enable_binding)) {
            continue;
         }
         if (parse_bool_param(s, "ENABLE_ADDGRP_KILL", &enable_addgrp_kill)) {
            continue;
         }
         if (parse_bool_param(s, "ACCT_RESERVED_USAGE", &acct_reserved_usage)) {
            continue;
         } 
         if (parse_bool_param(s, "SHARETREE_RESERVED_USAGE", &sharetree_reserved_usage)) {
            continue;
         }
         if (parse_bool_param(s, "PROF_EXECD", &prof_execd_thrd)) {
            continue;
         } 
         if (!strncasecmp(s, "NOTIFY_KILL", sizeof("NOTIFY_KILL")-1)) {
            if (!strcasecmp(s, "NOTIFY_KILL=default")) {
               notify_kill_type = 1;
            } else if (!strcasecmp(s, "NOTIFY_KILL=none")) {
               notify_kill_type = 2;
            } else if (!strncasecmp(s, "NOTIFY_KILL=", sizeof("NOTIFY_KILL=")-1)){
               notify_kill_type = 0;
               if (notify_kill) {
                  sge_free(&notify_kill);
               }
               notify_kill = sge_strdup(nullptr, &(s[sizeof("NOTIFY_KILL")]));
            }
            continue;
         } 
         if (!strncasecmp(s, "NOTIFY_SUSP", sizeof("NOTIFY_SUSP")-1)) {
            if (!strcasecmp(s, "NOTIFY_SUSP=default")) {
               notify_susp_type = 1;
            } else if (!strcasecmp(s, "NOTIFY_SUSP=none")) {
               notify_susp_type = 2;
            } else if (!strncasecmp(s, "NOTIFY_SUSP=", sizeof("NOTIFY_SUSP=")-1)){
               notify_susp_type = 0;
               if (notify_susp) {
                  sge_free(&notify_susp);
               }
               notify_susp = sge_strdup(nullptr, &(s[sizeof("NOTIFY_SUSP")]));
            }
            continue;
         } 
         if (parse_bool_param(s, "USE_QSUB_GID", &use_qsub_gid)) {
            continue;
         }
         if (!strncasecmp(s, "PTF_MAX_PRIORITY", sizeof("PTF_MAX_PRIORITY")-1)) {
            ptf_max_priority=atoi(&s[sizeof("PTF_MAX_PRIORITY=")-1]);
            continue;
         }
         if (!strncasecmp(s, "PTF_MIN_PRIORITY", sizeof("PTF_MIN_PRIORITY")-1)) {
            ptf_min_priority=atoi(&s[sizeof("PTF_MIN_PRIORITY=")-1]);
            continue;
         }
         if (parse_bool_param(s, "SET_LIB_PATH", &set_lib_path)) {
            continue;
         }
         if (parse_bool_param(s, "INHERIT_ENV", &inherit_env)) {
            continue;
         }
         if (!strncasecmp(s, "PDC_INTERVAL", sizeof("PDC_INTERVAL")-1)) {
            if (!strcasecmp(s, "PDC_INTERVAL=NEVER")) {
               pdc_interval = U_LONG32_MAX;
            } else if (!strcasecmp(s, "PDC_INTERVAL=PER_LOAD_REPORT")) {
               pdc_interval = load_report_time;
            } else if (parse_time_param(s, "PDC_INTERVAL", &pdc_interval)) {
            } else {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "execd_params", "PDC_INTERVAL",
                                       1);
               pdc_interval = 1;
            }
            continue;
         }
         if (!strncasecmp(s, "S_DESCRIPTORS", sizeof("S_DESCRIPTORS")-1)) {
            sge_strlcpy(s_descriptors, s+sizeof("S_DESCRIPTORS"), 100);
            continue;
         }
         if (!strncasecmp(s, "H_DESCRIPTORS", sizeof("H_DESCRIPTORS")-1)) {
            sge_strlcpy(h_descriptors, s+sizeof("H_DESCRIPTORS"), 100);
            continue;
         }
         if (!strncasecmp(s, "S_MAXPROC", sizeof("S_MAXPROC")-1)) {
            sge_strlcpy(s_maxproc, s+sizeof("S_MAXPROC"), 100);
            continue;
         }
         if (!strncasecmp(s, "H_MAXPROC", sizeof("H_MAXPROC")-1)) {
            sge_strlcpy(h_maxproc, s+sizeof("H_MAXPROC"), 100);
            continue;
         }
         if (!strncasecmp(s, "S_MEMORYLOCKED", sizeof("S_MEMORYLOCKED")-1)) {
            sge_strlcpy(s_memorylocked, s+sizeof("S_MEMORYLOCKED"), 100);
            continue;
         }
         if (!strncasecmp(s, "H_MEMORYLOCKED", sizeof("H_MEMORYLOCKED")-1)) {
            sge_strlcpy(h_memorylocked, s+sizeof("H_MEMORYLOCKED"), 100);
            continue;
         }
         if (!strncasecmp(s, "S_LOCKS", sizeof("S_LOCKS")-1)) {
            sge_strlcpy(s_locks, s+sizeof("S_LOCKS"), 100);
            continue;
         }
         if (!strncasecmp(s, "H_LOCKS", sizeof("H_LOCKS")-1)) {
            sge_strlcpy(h_locks, s+sizeof("H_LOCKS"), 100);
            continue;
         }
         if (parse_bool_param(s, "IGNORE_NGROUPS_MAX_LIMIT", &ignore_ngroups_max_limit)) {
            continue;
         } 
      }
      SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      sge_free_saved_vars(conf_context);
      conf_context = nullptr;

      /* If profiling configuration has changed, 
         set_thread_prof_status_by_name has to be called for each thread
      */
      set_thread_prof_status_by_name("Execd Thread", prof_execd_thrd);

      SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      /* parse reporting parameters */
      for (s=sge_strtok_r(reporting_params, PARAMS_DELIMITER, &conf_context); s; s=sge_strtok_r(nullptr, PARAMS_DELIMITER, &conf_context)) {
         if (parse_bool_param(s, "accounting", &do_accounting)) {
            continue;
         }
         if (parse_bool_param(s, "reporting", &do_reporting)) {
            continue;
         }
         if (parse_bool_param(s, "joblog", &do_joblog)) {
            continue;
         }
         if (parse_int_param(s, "flush_time", &reporting_flush_time, TYPE_TIM)) {
            if (reporting_flush_time <= 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "reporting_params", "flush_time",
                                       15);
               reporting_flush_time = 15;
            }
            continue;
         }
         if (parse_int_param(s, "accounting_flush_time", &accounting_flush_time, TYPE_TIM)) {
            if (accounting_flush_time < 0) {
               answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_WARNING,
                                       MSG_CONF_INVALIDPARAM_SSI, "reporting_params", "accounting_flush_time",
                                       -1);
               accounting_flush_time = -1;
            }
            
            continue;
         }
         if (parse_bool_param(s, "old_accounting", &old_accounting)) {
            continue;
         }
         if (parse_bool_param(s, "old_reporting", &old_reporting)) {
            continue;
         }
         if (parse_int_param(s, "sharelog", &sharelog_time, TYPE_TIM)) {
            continue;
         }
         if (parse_bool_param(s, "log_consumables", &log_consumables)) {
            continue;
         }
         if (parse_string_param(s, "usage_patterns", usage_patterns)) {
            continue;
         }
      }
      SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
      sge_free_saved_vars(conf_context);
      conf_context=nullptr;

      sge_free(&qmaster_params);
      sge_free(&execd_params);
      sge_free(&reporting_params);
   }

   lFreeList(&mlist);

   if (!global) {
      WARNING(SFNMAX, MSG_CONF_NOCONFIGFROMMASTER);
      DRETURN(-2);
   }

   DRETURN(0);
}

/**
 * \brief sge\_show\_conf
 *
 * \details
 * In debug mode, prints out the master configuration.
 *
 * \note
 * MT-NOTE: sge\_show\_conf() is MT safe.
 */
void sge_show_conf()
{
   const lListElem *ep;

   DENTER(BASIS_LAYER);
 
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   DPRINTF("conf.execd_spool_dir        >%s<\n", Master_Config.execd_spool_dir);
   DPRINTF("conf.mailer                 >%s<\n", Master_Config.mailer);
   DPRINTF("conf.prolog                 >%s<\n", Master_Config.prolog);
   DPRINTF("conf.epilog                 >%s<\n", Master_Config.epilog);
   DPRINTF("conf.shell_start_mode       >%s<\n", Master_Config.shell_start_mode);
   DPRINTF("conf.login_shells           >%s<\n", Master_Config.login_shells);
   DPRINTF("conf.administrator_mail     >%s<\n", Master_Config.administrator_mail?Master_Config.administrator_mail:"none");
   DPRINTF("conf.min_gid                >%u<\n", (unsigned) Master_Config.min_gid);
   DPRINTF("conf.min_uid                >%u<\n", (unsigned) Master_Config.min_uid);
   DPRINTF("conf.load_report_time       >%u<\n", (unsigned) Master_Config.load_report_time);
   DPRINTF("conf.max_unheard            >%u<\n", (unsigned) Master_Config.max_unheard);
   DPRINTF("conf.loglevel               >%u<\n", (unsigned) Master_Config.loglevel);
   DPRINTF("conf.xterm                  >%s<\n", Master_Config.xterm?Master_Config.xterm:"none");
   DPRINTF("conf.load_sensor            >%s<\n", Master_Config.load_sensor?Master_Config.load_sensor:"none");
   DPRINTF("conf.enforce_project        >%s<\n", Master_Config.enforce_project?Master_Config.enforce_project:"none");
   DPRINTF("conf.enforce_user           >%s<\n", Master_Config.enforce_user?Master_Config.enforce_user:"none");
   DPRINTF("conf.set_token_cmd          >%s<\n", Master_Config.set_token_cmd?Master_Config.set_token_cmd:"none");
   DPRINTF("conf.pag_cmd                >%s<\n", Master_Config.pag_cmd?Master_Config.pag_cmd:"none");
   DPRINTF("conf.token_extend_time      >%u<\n", (unsigned) Master_Config.token_extend_time);
   DPRINTF("conf.shepherd_cmd           >%s<\n", Master_Config.shepherd_cmd?Master_Config.pag_cmd:"none");
   DPRINTF("conf.qmaster_params         >%s<\n", Master_Config.qmaster_params?Master_Config.qmaster_params:"none");
   DPRINTF("conf.execd_params           >%s<\n", Master_Config.execd_params?Master_Config.execd_params:"none");
   DPRINTF("conf.gid_range              >%s<\n", Master_Config.gid_range?Master_Config.gid_range:"none");
   DPRINTF("conf.zombie_jobs            >%u<\n", (unsigned) Master_Config.zombie_jobs);
   DPRINTF("conf.qlogin_daemon          >%s<\n", Master_Config.qlogin_daemon?Master_Config.qlogin_daemon:"none");
   DPRINTF("conf.qlogin_command         >%s<\n", Master_Config.qlogin_command?Master_Config.qlogin_command:"none");
   DPRINTF("conf.rsh_daemon             >%s<\n", Master_Config.rsh_daemon?Master_Config.rsh_daemon:"none");
   DPRINTF("conf.rsh_command            >%s<\n", Master_Config.rsh_command?Master_Config.rsh_command:"none");
   DPRINTF("conf.jsv_url                >%s<\n", Master_Config.jsv_url?Master_Config.jsv_url:"none");
   DPRINTF("conf.jsv_allowed_mod        >%s<\n", Master_Config.jsv_allowed_mod?Master_Config.jsv_allowed_mod:"none");
   DPRINTF("conf.rlogin_daemon          >%s<\n", Master_Config.rlogin_daemon?Master_Config.rlogin_daemon:"none");
   DPRINTF("conf.rlogin_command         >%s<\n", Master_Config.rlogin_command?Master_Config.rlogin_command:"none");
   DPRINTF("conf.reschedule_unknown     >%u<\n", (unsigned) Master_Config.reschedule_unknown);
   DPRINTF("conf.max_aj_instances       >%u<\n", (unsigned) Master_Config.max_aj_instances);
   DPRINTF("conf.max_aj_tasks           >%u<\n", (unsigned) Master_Config.max_aj_tasks);
   DPRINTF("conf.max_u_jobs             >%u<\n", (unsigned) Master_Config.max_u_jobs);
   DPRINTF("conf.max_jobs               >%u<\n", (unsigned) Master_Config.max_jobs);
   DPRINTF("conf.max_advance_reservations >%u<\n", (unsigned) Master_Config.max_advance_reservations);
   DPRINTF("conf.reprioritize           >%u<\n", Master_Config.reprioritize);
   DPRINTF("conf.auto_user_oticket      >%u<\n", Master_Config.auto_user_oticket);
   DPRINTF("conf.auto_user_fshare       >%u<\n", Master_Config.auto_user_fshare);
   DPRINTF("conf.auto_user_default_project >%s<\n", Master_Config.auto_user_default_project);
   DPRINTF("conf.auto_user_delete_time  >" sge_u64 "<\n", Master_Config.auto_user_delete_time);
   DPRINTF("conf.delegated_file_staging >%s<\n", Master_Config.delegated_file_staging);
   DPRINTF("conf.libjvm_path >%s<\n", Master_Config.libjvm_path);
   DPRINTF("conf.additional_jvm_args >%s<\n", Master_Config.additional_jvm_args);

   for_each_ep(ep, Master_Config.user_lists) {
      DPRINTF("%s             >%s<\n",
              lPrev(ep)?"             ":"conf.user_lists", 
              lGetString(ep, US_name));
   }
   for_each_ep(ep, Master_Config.xuser_lists) {
      DPRINTF("%s            >%s<\n",
              lPrev(ep)?"              ":"conf.xuser_lists", 
              lGetString(ep, US_name));
   }

   for_each_ep(ep, Master_Config.projects) {
      DPRINTF("%s             >%s<\n",
              lPrev(ep)?"             ":"conf.projects", 
              lGetString(ep, PR_name));
   }
   for_each_ep(ep, Master_Config.xprojects) {
      DPRINTF("%s            >%s<\n",
              lPrev(ep)?"              ":"conf.xprojects", 
              lGetString(ep, PR_name));
   }
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);

   DRETURN_VOID;
}

/**
 * \brief clean\_conf
 *
 * \details
 * Frees the whole master configuration.
 *
 * \note
 * MT-NOTE: clean\_conf() is not MT safe, caller needs LOCK\_MASTER\_CONF as write lock.
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
   sge_free(&Master_Config.gid_range);
   sge_free(&Master_Config.qlogin_daemon);
   sge_free(&Master_Config.qlogin_command);
   sge_free(&Master_Config.rsh_daemon);
   sge_free(&Master_Config.rsh_command);
   sge_free(&Master_Config.jsv_url);
   sge_free(&Master_Config.jsv_allowed_mod);
   sge_free(&Master_Config.rlogin_daemon);
   sge_free(&Master_Config.rlogin_command);
   sge_free(&Master_Config.auto_user_default_project);
   sge_free(&Master_Config.delegated_file_staging);
   sge_free(&Master_Config.libjvm_path);
   sge_free(&Master_Config.additional_jvm_args);
   
   memset(&Master_Config, 0, sizeof(sge_conf_type));

   DRETURN_VOID;
}

/**
 * \brief
 * Enables or disables profiling for thread(s) according to the actual
 * global config, qmaster_params.
 *
 * If no thread name (nullptr pointer) is given, profiling information of all
 * threads is updated.
 * If a name is given, all threads with that name are updated.
 *
 * \param thread_name Thread name, nullptr for all threads.
 */
void conf_update_thread_profiling(const char *thread_name) 
{
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   if (thread_name == nullptr) {
      set_thread_prof_status_by_name("Signal Thread", prof_signal_thrd);
      set_thread_prof_status_by_name("Scheduler Thread", prof_scheduler_thrd);
      set_thread_prof_status_by_name("Listener Thread", prof_listener_thrd);
      set_thread_prof_status_by_name("Worker Thread", prof_worker_thrd);
      set_thread_prof_status_by_name("Event Master Thread", prof_deliver_thrd);
      set_thread_prof_status_by_name("TEvent Thread", prof_tevent_thrd);
   } else {
      if (strcmp(thread_name, "Signal Thread") == 0) {
         set_thread_prof_status_by_name("Signal Thread", prof_signal_thrd);
      } else if (strcmp(thread_name, "Scheduler Thread") == 0) {
         set_thread_prof_status_by_name("Scheduler Thread", prof_scheduler_thrd);
      } else if (strcmp(thread_name, "Listener Thread") == 0) {
         set_thread_prof_status_by_name("Listener Thread", prof_listener_thrd);
      } else if (strcmp(thread_name, "Worker Thread") == 0) {
         set_thread_prof_status_by_name("Worker Thread", prof_worker_thrd);
      } else if (strcmp(thread_name, "Event Master Thread") == 0) {
         set_thread_prof_status_by_name("Event Master Thread", prof_deliver_thrd);
      } else if (strcmp(thread_name, "TEvent Thread") == 0) {
         set_thread_prof_status_by_name("TEvent Thread", prof_tevent_thrd);
      }
   }
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

/* returned pointer needs to be freed */
char* mconf_get_execd_spool_dir() {
   char* execd_spool_dir = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   execd_spool_dir = sge_strdup(execd_spool_dir, Master_Config.execd_spool_dir);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(execd_spool_dir);
}

/* returned pointer needs to be freed */
char* mconf_get_mailer() {
   char* mailer = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   mailer = sge_strdup(mailer, Master_Config.mailer);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(mailer);
}

/* returned pointer needs to be freed */
char* mconf_get_xterm() {
   char* xterm = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   xterm = sge_strdup(xterm, Master_Config.xterm);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(xterm);

}

/* returned pointer needs to be freed */
char* mconf_get_load_sensor() {
   char* load_sensor = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   load_sensor = sge_strdup(load_sensor, Master_Config.load_sensor);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(load_sensor);
}

/* returned pointer needs to be freed */
char* mconf_get_prolog() {
   char* prolog = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   prolog = sge_strdup(prolog, Master_Config.prolog);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(prolog);
}

/* returned pointer needs to be freed */
char* mconf_get_epilog() {
   char* epilog = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   epilog = sge_strdup(epilog, Master_Config.epilog);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(epilog);
}

/* returned pointer needs to be freed */
char* mconf_get_shell_start_mode() {
   char* shell_start_mode = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   shell_start_mode = sge_strdup(shell_start_mode, Master_Config.shell_start_mode);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(shell_start_mode);
}

/* returned pointer needs to be freed */
char* mconf_get_login_shells() {
   char* login_shells = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   login_shells = sge_strdup(login_shells, Master_Config.login_shells);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(login_shells);
}

u_long32 mconf_get_min_uid() {
   u_long32 min_uid;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   min_uid = Master_Config.min_uid;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(min_uid);
}

u_long32 mconf_get_min_gid() {
   u_long32 min_gid;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   min_gid = Master_Config.min_gid;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(min_gid);
}

u_long32 mconf_get_load_report_time() {
   u_long32 load_report_time;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   load_report_time = Master_Config.load_report_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(load_report_time);
}

u_long32 mconf_get_max_unheard() {
   u_long32 max_unheard;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_unheard = Master_Config.max_unheard;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_unheard);
}

u_long32 mconf_get_loglevel() {
   u_long32 loglevel;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   loglevel = Master_Config.loglevel;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(loglevel);
}

/* returned pointer needs to be freed */
char* mconf_get_enforce_project() {
   char* enforce_project = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   enforce_project = sge_strdup(enforce_project, Master_Config.enforce_project);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(enforce_project);
}

/* returned pointer needs to be freed */
char* mconf_get_enforce_user() {
   char* enforce_user = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   enforce_user = sge_strdup(enforce_user, Master_Config.enforce_user);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(enforce_user);
}


/* returned pointer needs to be freed */
char* mconf_get_administrator_mail() {
   char* administrator_mail = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   administrator_mail = sge_strdup(administrator_mail, Master_Config.administrator_mail);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(administrator_mail);
}

/* returned pointer needs to be freed */
lList* mconf_get_user_lists() {
   lList* user_lists = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   user_lists = lCopyList("user_lists", Master_Config.user_lists);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(user_lists);
}

/* returned pointer needs to be freed */
lList* mconf_get_xuser_lists() {
   lList* xuser_lists = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   xuser_lists = lCopyList("xuser_lists", Master_Config.xuser_lists);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(xuser_lists);
}

/* returned pointer needs to be freed */
lList* mconf_get_projects() {
   lList* projects = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   projects = lCopyList("projects", Master_Config.projects);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(projects);
}

/* returned pointer needs to be freed */
lList* mconf_get_xprojects() {
   lList* xprojects = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   xprojects = lCopyList("xprojects", Master_Config.xprojects);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(xprojects);
}

/* returned pointer needs to be freed */
char* mconf_get_set_token_cmd() {
   char* set_token_cmd = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   set_token_cmd = sge_strdup(set_token_cmd, Master_Config.set_token_cmd);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(set_token_cmd);
}

/* returned pointer needs to be freed */
char* mconf_get_pag_cmd() {
   char* pag_cmd = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   pag_cmd = sge_strdup(pag_cmd, Master_Config.pag_cmd);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(pag_cmd);
}

u_long32 mconf_get_token_extend_time() {
   u_long32 token_extend_time;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   token_extend_time = Master_Config.token_extend_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(token_extend_time);
}

/* returned pointer needs to be freed */
char* mconf_get_shepherd_cmd() {
   char* shepherd_cmd = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   shepherd_cmd = sge_strdup(shepherd_cmd, Master_Config.shepherd_cmd);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(shepherd_cmd);
}

/* returned pointer needs to be freed */
char* mconf_get_qmaster_params() {
   char* qmaster_params = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   qmaster_params = sge_strdup(qmaster_params, Master_Config.qmaster_params);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(qmaster_params);
}

/* returned pointer needs to be freed */
char* mconf_get_execd_params() {
   char* execd_params = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   execd_params = sge_strdup(execd_params, Master_Config.execd_params);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(execd_params);
}

/* returned pointer needs to be freed */
char* mconf_get_reporting_params() {
   char* reporting_params = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   reporting_params = sge_strdup(reporting_params, Master_Config.reporting_params);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(reporting_params);
}

/* returned pointer needs to be freed */
char* mconf_get_gid_range() {
   char* gid_range = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   gid_range = sge_strdup(gid_range, Master_Config.gid_range);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(gid_range);
}

u_long32 mconf_get_zombie_jobs() {
   u_long32 zombie_jobs;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   zombie_jobs = Master_Config.zombie_jobs;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(zombie_jobs);
}

/* returned pointer needs to be freed */
char* mconf_get_qlogin_daemon() {
   char* qlogin_daemon = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   qlogin_daemon = sge_strdup(qlogin_daemon, Master_Config.qlogin_daemon);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(qlogin_daemon);
}

/* returned pointer needs to be freed */
char* mconf_get_qlogin_command() {
   char* qlogin_command = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   qlogin_command = sge_strdup(qlogin_command, Master_Config.qlogin_command);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(qlogin_command);
}

/* returned pointer needs to be freed */
char* mconf_get_rsh_daemon() {
   char* rsh_daemon = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   rsh_daemon = sge_strdup(rsh_daemon, Master_Config.rsh_daemon);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(rsh_daemon);
}

void mconf_set_new_config(bool new_config)
{
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);
   
   is_new_config = new_config;
   
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
   DRETURN_VOID;
}

/* make chached values from configuration invalid. */
bool mconf_is_new_config() {
   bool is;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   is = is_new_config;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(is);
}

/* returned pointer needs to be freed */
char* mconf_get_rsh_command() {
   char* rsh_command = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   rsh_command = sge_strdup(rsh_command, Master_Config.rsh_command);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(rsh_command);
}

/* returned pointer needs to be freed */
char* mconf_get_jsv_url() {
   char* jsv_url = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   jsv_url = sge_strdup(jsv_url, Master_Config.jsv_url);
   sge_strip_white_space_at_eol(jsv_url);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(jsv_url);
}

/* returned pointer needs to be freed */
char* mconf_get_jsv_allowed_mod() {
   char* jsv_allowed_mod = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   jsv_allowed_mod = sge_strdup(jsv_allowed_mod, Master_Config.jsv_allowed_mod);
   sge_strip_white_space_at_eol(jsv_allowed_mod);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(jsv_allowed_mod);
}

/* returned pointer needs to be freed */
char* mconf_get_rlogin_daemon() {
   char* rlogin_daemon = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   rlogin_daemon = sge_strdup(rlogin_daemon, Master_Config.rlogin_daemon);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(rlogin_daemon);
}

/* returned pointer needs to be freed */
char* mconf_get_rlogin_command() {
   char* rlogin_command = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   rlogin_command = sge_strdup(rlogin_command, Master_Config.rlogin_command);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(rlogin_command);
}

u_long32 mconf_get_reschedule_unknown() {
   u_long32 reschedule_unknown;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   reschedule_unknown = Master_Config.reschedule_unknown;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(reschedule_unknown);
}

u_long32 mconf_get_max_aj_instances() {
   u_long32 max_aj_instances;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_aj_instances = Master_Config.max_aj_instances;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_aj_instances);
}

u_long32 mconf_get_max_aj_tasks() {
   u_long32 max_aj_tasks;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_aj_tasks = Master_Config.max_aj_tasks;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_aj_tasks);
}

u_long32 mconf_get_max_u_jobs() {
   u_long32 max_u_jobs;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_u_jobs = Master_Config.max_u_jobs;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_u_jobs);
}

u_long32 mconf_get_max_jobs() {
   u_long32 max_jobs;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_jobs = Master_Config.max_jobs;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_jobs);
}

u_long32 mconf_get_max_advance_reservations() {
   u_long32 max_advance_reservations;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   max_advance_reservations = Master_Config.max_advance_reservations;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(max_advance_reservations);
}

u_long32 mconf_get_reprioritize() {
   u_long32 reprioritize;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   reprioritize = Master_Config.reprioritize;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(reprioritize);
}

u_long32 mconf_get_auto_user_fshare() {
   u_long32 auto_user_fshare;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   auto_user_fshare = Master_Config.auto_user_fshare;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(auto_user_fshare);
}

u_long32 mconf_get_auto_user_oticket() {
   u_long32 auto_user_oticket;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   auto_user_oticket = Master_Config.auto_user_oticket;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(auto_user_oticket);
}

/* returned pointer needs to be freed */
char* mconf_get_auto_user_default_project() {
   char* auto_user_default_project = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   auto_user_default_project = sge_strdup(auto_user_default_project, Master_Config.auto_user_default_project);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(auto_user_default_project);
}

u_long32 mconf_get_auto_user_delete_time() {
   u_long32 auto_user_delete_time;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   auto_user_delete_time = Master_Config.auto_user_delete_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(auto_user_delete_time);
}

/* returned pointer needs to be freed */
char* mconf_get_delegated_file_staging() {
   char* delegated_file_staging = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   delegated_file_staging = sge_strdup(delegated_file_staging, Master_Config.delegated_file_staging);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(delegated_file_staging);
}


/* params */
bool mconf_is_monitor_message() {
  bool is;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   is = is_monitor_message;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(is);
}

bool mconf_get_use_qidle() {
   bool idle;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   idle = use_qidle;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(idle);
}

bool mconf_get_forbid_reschedule() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = forbid_reschedule;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_forbid_apperror() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = forbid_apperror;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_do_credentials() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = do_credentials;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_do_authentication() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = do_authentication;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_acct_reserved_usage() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = acct_reserved_usage;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_sharetree_reserved_usage() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = sharetree_reserved_usage;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

keep_active_t mconf_get_keep_active() {
   keep_active_t ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = keep_active;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_enable_binding() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = enable_binding;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_enable_addgrp_kill() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = enable_addgrp_kill;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

u_long32 mconf_get_pdc_interval() {
   u_long32 ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = pdc_interval;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_enable_reschedule_kill() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = enable_reschedule_kill;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_enable_reschedule_slave() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = enable_reschedule_slave;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_old_reschedule_behavior() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = old_reschedule_behavior;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

std::string mconf_get_gperf_name() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   std::string ret = gperf_name;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

std::string mconf_get_gperf_threads() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   std::string ret = gperf_threads;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_old_reschedule_behavior_array_job() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = old_reschedule_behavior_array_job;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_simulate_execds() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = simulate_execds;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_simulate_jobs() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = simulate_jobs;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

long mconf_get_ptf_max_priority() {
   long ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = ptf_max_priority;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

long mconf_get_ptf_min_priority() {
   long ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = ptf_min_priority;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_use_qsub_gid() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = use_qsub_gid;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

int mconf_get_notify_susp_type() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = notify_susp_type;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/* returned pointer needs to be freed */
char* mconf_get_notify_susp() {
   char* ret = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = sge_strdup(ret, notify_susp);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

int mconf_get_notify_kill_type() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = notify_kill_type;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

/* returned pointer needs to be freed */
char* mconf_get_notify_kill() {
   char* ret = nullptr;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = sge_strdup(ret, notify_kill);

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_disable_reschedule() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = disable_reschedule;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_disable_secondary_ds() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = disable_secondary_ds;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_disable_secondary_ds_reader() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = disable_secondary_ds_reader;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_disable_secondary_ds_execd() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = disable_secondary_ds_execd;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

int mconf_get_scheduler_timeout() {
   int timeout;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   timeout = scheduler_timeout;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(timeout);
}

void mconf_set_max_dynamic_event_clients(int value) {

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_WRITE);

   max_dynamic_event_clients = value;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_WRITE);
   DRETURN_VOID;
}

int mconf_get_max_dynamic_event_clients() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = max_dynamic_event_clients;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_set_lib_path() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = set_lib_path;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_inherit_env() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = inherit_env;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

// spooling interval in seconds
int mconf_get_spool_time() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = spool_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

int mconf_get_max_ds_deviation() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = max_ds_deviation;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

u_long32 mconf_get_monitor_time() {
   u_long32 monitor;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   monitor = monitor_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(monitor);

}

bool mconf_get_do_accounting() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = do_accounting;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

bool mconf_get_do_reporting() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = do_reporting;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

bool mconf_get_do_joblog() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = do_joblog;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

int mconf_get_reporting_flush_time() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = reporting_flush_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

int mconf_get_accounting_flush_time() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = accounting_flush_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_old_accounting() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = old_accounting;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_old_reporting() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = old_reporting;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

int mconf_get_sharelog_time() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = sharelog_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

int mconf_get_log_consumables() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = log_consumables;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

std::string mconf_get_usage_patterns() {
   std::string ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = usage_patterns;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_enable_forced_qdel() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = enable_forced_qdel;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

bool mconf_get_enable_sup_grp_eval() {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   bool ret = false;
#if defined(OGE_WITH_EXTENSIONS)
   ret = enable_sup_grp_eval;
#endif
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

bool mconf_get_enable_enforce_master_limit() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = enable_enforce_master_limit;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

bool mconf_get_enable_test_sleep_after_request() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = enable_test_sleep_after_request;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);

}

bool mconf_get_enable_forced_qdel_if_unknown() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = enable_forced_qdel_if_unknown;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_ignore_ngroups_max_limit() {
   bool ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);
   ret = ignore_ngroups_max_limit;
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_enable_submit_lib_path() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = enable_submit_lib_path;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

bool mconf_get_enable_submit_ld_preload() {
   int ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = enable_submit_ld_preload;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

int mconf_get_max_job_deletion_time() {
   int deletion_time;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   deletion_time = max_job_deletion_time;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(deletion_time);
}

void mconf_get_h_descriptors(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(h_descriptors);
   
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

void mconf_get_s_descriptors(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(s_descriptors);
   
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

void mconf_get_h_maxproc(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(h_maxproc);
   
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

void mconf_get_s_maxproc(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(s_maxproc);
   
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

void mconf_get_h_memorylocked(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(h_memorylocked);
   
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

void mconf_get_s_memorylocked(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(s_memorylocked);
   
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

void mconf_get_h_locks(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(h_locks);
   
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

void mconf_get_s_locks(char **pret) {
   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   *pret = strdup(s_locks);
   
   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN_VOID;
}

int mconf_get_jsv_threshold() {
   int threshold;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   threshold = jsv_threshold;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(threshold);
}

int mconf_get_jsv_timeout() {
   int timeout;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   timeout = jsv_timeout;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(timeout);
}

u_long32 mconf_get_script_timeout() {
   u_long32 ret;

   DENTER(BASIS_LAYER);
   SGE_LOCK(LOCK_MASTER_CONF, LOCK_READ);

   ret = script_timeout;

   SGE_UNLOCK(LOCK_MASTER_CONF, LOCK_READ);
   DRETURN(ret);
}

