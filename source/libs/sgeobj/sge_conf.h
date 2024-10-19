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
 *  Portions of this software are Copyright (c) 2011-2012 Univa Corporation
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <string>

#include "sgeobj/cull/sge_conf_CONF_L.h"
#include "sgeobj/cull/sge_conf_CF_L.h"

/* The scheduler configuration changes this configuration element only. It is
   not spooled and is not shown in qconf -mconf */
#define REPRIORITIZE "reprioritize"

#define GID_RANGE_NOT_ALLOWED_ID 100
#define RLIMIT_UNDEFINED -9999

#define PDC_DISABLED U_LONG64_MAX

typedef enum {
   KEEP_ACTIVE_TRUE = 0,
   KEEP_ACTIVE_FALSE,
   KEEP_ACTIVE_ERROR
} keep_active_t;

typedef int (*tDaemonizeFunc)(void *ctx);

/* This list is *ONLY* used by the execd and should be moved eventually */
extern lList *Execd_Config_List;

int merge_configuration(lList **answer_list, u_long32 progid, const char *cell_root, lListElem *global, lListElem *local, lList **lpp);
void sge_show_conf();
void conf_update_thread_profiling(const char *thread_name);

char* mconf_get_execd_spool_dir();
char* mconf_get_mailer();
char* mconf_get_xterm();
char* mconf_get_load_sensor();
char* mconf_get_prolog();
char* mconf_get_epilog();
char* mconf_get_shell_start_mode();
char* mconf_get_login_shells();
u_long32 mconf_get_min_uid();
u_long32 mconf_get_min_gid();
u_long32 mconf_get_load_report_time();
u_long32 mconf_get_max_unheard();
u_long32 mconf_get_loglevel();
char* mconf_get_enforce_project();
char* mconf_get_enforce_user();
char* mconf_get_administrator_mail();
lList* mconf_get_user_lists();
lList* mconf_get_xuser_lists();
lList* mconf_get_projects();
lList* mconf_get_xprojects();
char* mconf_get_set_token_cmd();
char* mconf_get_pag_cmd();
u_long32 mconf_get_token_extend_time();
char* mconf_get_shepherd_cmd();
char* mconf_get_qmaster_params();
char* mconf_get_execd_params();
char* mconf_get_reporting_params();
char* mconf_get_gid_range();
u_long32 mconf_get_zombie_jobs();
char* mconf_get_qlogin_daemon();
char* mconf_get_qlogin_command();
char* mconf_get_rsh_daemon();
char* mconf_get_rsh_command();
char* mconf_get_jsv_url();
char* mconf_get_jsv_allowed_mod();
char* mconf_get_rlogin_daemon();
char* mconf_get_rlogin_command();
u_long32 mconf_get_reschedule_unknown();
u_long32 mconf_get_max_aj_instances();
u_long32 mconf_get_max_aj_tasks();
u_long32 mconf_get_max_u_jobs();
u_long32 mconf_get_max_jobs();
u_long32 mconf_get_max_advance_reservations();
u_long32 mconf_get_reprioritize();
u_long32 mconf_get_auto_user_fshare();
u_long32 mconf_get_auto_user_oticket();
char* mconf_get_auto_user_default_project();
u_long32 mconf_get_auto_user_delete_time();
char* mconf_get_delegated_file_staging();
void mconf_set_new_config(bool new_config);
bool mconf_is_new_config();
bool mconf_get_old_reschedule_behavior();
bool mconf_get_old_reschedule_behavior_array_job();
std::string mconf_get_gperf_name();
std::string mconf_get_gperf_threads();

/* params */
bool mconf_is_monitor_message();
bool mconf_get_use_qidle();
bool mconf_get_forbid_reschedule();
bool mconf_get_forbid_apperror();
bool mconf_get_do_credentials();
bool mconf_get_do_authentication();
bool mconf_get_acct_reserved_usage();
bool mconf_get_sharetree_reserved_usage();
keep_active_t mconf_get_keep_active();
bool mconf_get_enable_binding();
bool mconf_get_simulate_execds();
bool mconf_get_simulate_jobs();
long mconf_get_ptf_max_priority();
long mconf_get_ptf_min_priority();
bool mconf_get_use_qsub_gid();
int mconf_get_notify_susp_type();
char* mconf_get_notify_susp();
int mconf_get_notify_kill_type();
char* mconf_get_notify_kill();
bool mconf_get_disable_reschedule();
bool mconf_get_disable_secondary_ds();
bool mconf_get_disable_secondary_ds_reader();
bool mconf_get_disable_secondary_ds_execd();
int mconf_get_scheduler_timeout();
int mconf_get_max_dynamic_event_clients();
void mconf_set_max_dynamic_event_clients(int value);
bool mconf_get_set_lib_path();
bool mconf_get_inherit_env();
int mconf_get_spool_time();
int mconf_get_max_ds_deviation();
u_long32 mconf_get_monitor_time();
bool mconf_get_do_accounting();
bool mconf_get_do_reporting();
bool mconf_get_do_joblog();
int mconf_get_reporting_flush_time();
int mconf_get_accounting_flush_time();
bool mconf_get_old_accounting();
bool mconf_get_old_reporting();
int mconf_get_sharelog_time();
int mconf_get_log_consumables();
std::string mconf_get_usage_patterns();
bool mconf_get_enable_forced_qdel();
bool mconf_get_enable_sup_grp_eval();
bool mconf_get_enable_forced_qdel_if_unknown();
bool mconf_get_enable_enforce_master_limit();
bool mconf_get_enable_test_sleep_after_request();
int mconf_get_max_job_deletion_time();
bool mconf_get_enable_addgrp_kill();
u_long64 mconf_get_pdc_interval();
bool mconf_get_enable_reschedule_kill();
bool mconf_get_enable_reschedule_slave();
void mconf_get_h_descriptors(char **pret);
void mconf_get_s_descriptors(char **pret);
void mconf_get_h_maxproc(char **pret);
void mconf_get_s_maxproc(char **pret);
void mconf_get_h_memorylocked(char **pret);
void mconf_get_s_memorylocked(char **pret);
void mconf_get_h_locks(char **pret);
void mconf_get_s_locks(char **pret);
int mconf_get_jsv_timeout();
int mconf_get_jsv_threshold();
bool mconf_get_ignore_ngroups_max_limit();
bool mconf_get_enable_submit_lib_path();
bool mconf_get_enable_submit_ld_preload();
u_long32 mconf_get_script_timeout();
