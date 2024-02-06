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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include "basis_types.h"
#include "sge_dstring.h"

#define PATH_SEPARATOR "/"
#define COMMON_DIR "common"
#define BOOTSTRAP_FILE "bootstrap"
#define CONF_FILE "configuration"
#define SCHED_CONF_FILE "sched_configuration"
#define ACCT_FILE "accounting"
#define REPORTING_FILE "reporting"
#define LOCAL_CONF_DIR "local_conf"
#define SHADOW_MASTERS_FILE "shadow_masters"

#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'


// TODO: move the defines to a different location where other program names are defines
#define SGE_PREFIX      "sge_"
#define SGE_SHEPHERD    "sge_shepherd"
#define SGE_COSHEPHERD  "sge_coshepherd"
#define SGE_SHADOWD     "sge_shadowd"
#define PE_HOSTFILE     "pe_hostfile"

enum {
   QALTER = 1,    // 1
   QCONF,         // 2
   QDEL,          // 3
   QHOLD,         // 4
   QMASTER,       // 5
   QMOD,          // 6
   QRESUB,        // 7
   QRLS,          // 8
   QSELECT,       // 9
   QSH,           // 10
   QRSH,          // 11
   QLOGIN,        // 12
   QSTAT,         // 13
   QSUB,          // 14
   EXECD,         // 15
   QEVENT,        // 16
   QRSUB,         // 17
   QRDEL,         // 18
   QRSTAT,        // 19
   QUSERDEFINED,  // 20
   ALL_OPT,       // 21

   /* programs with numbers > ALL_OPT do not use the old parsing */

   UNUSED,        // 22
   SCHEDD,        // 23
   QACCT,         // 24
   SHADOWD,       // 25
   QHOST,         // 26
   SPOOLDEFAULTS, // 27
   JAPI,          // 28
   DRMAA,         // 29
   QPING,         // 30
   QQUOTA,        // 31
   SGE_SHARE_MON  // 32
};

enum {
   MAIN_THREAD,      // 1
   LISTENER_THREAD,  // 2
   DELIVERER_THREAD, // 3
   TIMER_THREAD,     // 4
   WORKER_THREAD,    // 5
   SIGNALER_THREAD,  // 6
   SCHEDD_THREAD,    // 7
   TESTER_THREAD     // 8
};

extern const char *prognames[];
extern const char *threadnames[];

typedef void (*sge_exit_func_t)(int);

void
bootstrap_mt_init();

const char *
bootstrap_get_admin_user();

void
bootstrap_set_admin_user(const char *admin_user);

const char *
bootstrap_get_default_domain();

void
bootstrap_set_default_domain(const char *default_domain);

bool
bootstrap_get_ignore_fqdn();

void
bootstrap_set_ignore_fqdn(bool ignore_fqdn);

const char *
bootstrap_get_spooling_method();

void
bootstrap_set_spooling_method(const char *spooling_method);

const char *
bootstrap_get_spooling_lib();

void
bootstrap_set_spooling_lib(const char *spooling_lib);

const char *
bootstrap_get_spooling_params();

void
bootstrap_set_spooling_params(const char *spooling_params);

const char *
bootstrap_get_binary_path();

void
bootstrap_set_binary_path(const char *binary_path);

const char *
bootstrap_get_qmaster_spool_dir();

void
bootstrap_set_qmaster_spool_dir(const char *qmaster_spool_dir);

const char *
bootstrap_get_security_mode();

void
bootstrap_set_security_mode(const char *security_mode);

bool
bootstrap_get_job_spooling();

void
bootstrap_set_job_spooling(bool job_spooling);

int
bootstrap_get_listener_thread_count();

void
bootstrap_set_listener_thread_count(int thread_count);

int
bootstrap_get_worker_thread_count();

void
bootstrap_set_worker_thread_count(int thread_count);

int
bootstrap_get_scheduler_thread_count();

void
bootstrap_set_scheduler_thread_count(int thread_count);

const char *
bootstrap_get_sge_root();

void
bootstrap_set_sge_root(const char *sge_root);

const char *
bootstrap_get_sge_cell();

void
bootstrap_set_sge_cell(const char *sge_cell);

u_long32
bootstrap_get_sge_qmaster_port();

void bootstrap_set_sge_qmaster_port(u_long32 sge_qmaster_port);

u_long32
bootstrap_get_sge_execd_port();

void
bootstrap_set_sge_execd_port(u_long32 sge_execd_port);

bool
bootstrap_is_from_services();

void
bootstrap_set_from_services(bool from_services);

bool
bootstrap_is_qmaster_internal();

void
bootstrap_set_qmaster_internal(bool qmaster_internal);

u_long32
bootstrap_get_component_id();

void
bootstrap_set_component_id(u_long32 component_id);

bool
bootstrap_is_daemonized();

void
bootstrap_set_daemonized(bool daemonized);

const char *
bootstrap_get_component_name();

const char *
bootstrap_get_thread_name();

void
bootstrap_set_thread_name(const char *thread_name);

void
bootstrap_log_parameter();

uid_t
bootstrap_get_uid();

void
bootstrap_set_uid(uid_t uid);

gid_t
bootstrap_get_gid();

void
bootstrap_set_gid(gid_t gid);

const char *
bootstrap_get_username();

void
bootstrap_set_username(const char *username);

const char *
bootstrap_get_groupname();

void
bootstrap_set_groupname(const char *groupname);

const char *
bootstrap_get_qualified_hostname();

void
bootstrap_set_qualified_hostname(const char *qualified_hostname);

const char *
bootstrap_get_unqualified_hostname();

void
bootstrap_set_unqualified_hostname(const char *unqualified_hostname);

sge_exit_func_t
bootstrap_get_exit_func();

char *
bootstrap_get_log_buffer();

void
bootstrap_set_exit_func(sge_exit_func_t exit_func);

const char *
path_state_get_sge_root();

void
path_state_set_sge_root(const char *path);

const char *
bootstrap_get_cell_root();

void
bootstrap_set_cell_root(const char *path);

const char *
bootstrap_get_bootstrap_file();

void
bootstrap_set_bootstrap_file(const char *path);

const char *
bootstrap_get_conf_file();

void
bootstrap_set_conf_file(const char *path);

const char *
bootstrap_get_sched_conf_file();

void
bootstrap_set_sched_conf_file(const char *path);

const char *
bootstrap_get_act_qmaster_file();

void bootstrap_set_act_qmaster_file(const char *path);

const char *bootstrap_get_acct_file();

void bootstrap_set_acct_file(const char *path);

const char *
bootstrap_get_reporting_file();

void
bootstrap_set_reporting_file(const char *path);

const char *
bootstrap_get_local_conf_dir();

void
bootstrap_set_local_conf_dir(const char *path);

const char *
bootstrap_get_shadow_masters_file();

void
bootstrap_set_shadow_masters_file(const char *path);

const char *
bootstrap_get_alias_file();

void
bootstrap_set_alias_file(const char *path);
