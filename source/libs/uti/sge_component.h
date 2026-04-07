#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024-2026 HPC-Gridware GmbH
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

#include <array>
#include <string_view>
#include <optional>
#include <cstddef>
#include <iostream>

#include "sge.h"
#include "uti/sge_uidgid.h"

// TODO: move the defines to a different location where other program names are defines
#define SGE_PREFIX      "sge_"
#define SGE_SHEPHERD    "sge_shepherd"
#define SGE_COSHEPHERD  "sge_coshepherd"
#define SGE_SHADOWD     "sge_shadowd"
#define PE_HOSTFILE     "pe_hostfile"

// For those applications that should be handled with sge_options the entry has to be before ALL_OPT
#define PROGNAME_LIST(X) \
   X(UNKNOWN_APP,   "unknown")       \
   X(QALTER,        "qalter")        \
   X(QCONF,         "qconf")         \
   X(QDEL,          "qdel")          \
   X(QHOLD,         "qhold")         \
   X(QMASTER,       "qmaster")       \
   X(QMOD,          "qmod")          \
   X(QRESUB,        "qresub")        \
   X(QRLS,          "qrls")          \
   X(QSELECT,       "qselect")       \
   X(QSH,           "qsh")           \
   X(QRSH,          "qrsh")          \
   X(QLOGIN,        "qlogin")        \
   X(QSTAT,         "qstat")         \
   X(QSUB,          "qsub")          \
   X(EXECD,         "execd")         \
   X(QEVENT,        "qevent")        \
   X(QRSUB,         "qrsub")         \
   X(QRDEL,         "qrdel")         \
   X(QRSTAT,        "qrstat")        \
   X(__UNUSED__,    "unknown")       \
   X(ALL_OPT,       "unknown")       \
   X(SCHEDD,        "schedd")        \
   X(QACCT,         "qacct")         \
   X(SHADOWD,       "shadowd")       \
   X(QHOST,         "qhost")         \
   X(SPOOLDEFAULTS, "spoolinit")     \
   X(JAPI,          "japi")          \
   X(DRMAA,         "drmaa")         \
   X(QPING,         "qping")         \
   X(QQUOTA,        "qquota")        \
   X(SGE_SHARE_MON, "sge_share_mon") \
   X(PYTHON_CLIENT, "python_client")

enum ProgName {
#define X(name, str) name,
   PROGNAME_LIST(X)
#undef X
   PROGNAME_COUNT
};

constexpr std::array<std::string_view, PROGNAME_COUNT> prognames = {
#define X(name, str) str,
   PROGNAME_LIST(X)
#undef X
};

constexpr const char* to_cstr(ProgName p) {
   return prognames[static_cast<std::size_t>(p)].data();
}

constexpr std::string_view to_string_view(ProgName p) {
   const auto idx = static_cast<std::size_t>(p);
   return idx < prognames.size() ? prognames[idx] : std::string_view{};
}

constexpr std::string to_string(ProgName p) {
   return std::string(to_string_view(p));
}

constexpr std::optional<ProgName> from_string(const std::string_view s) {
   for (std::size_t i = 0; i < prognames.size(); ++i) {
      if (prognames[i] == s) {
         return static_cast<ProgName>(i);
      }
   }
   return std::nullopt;
}



enum thread_type_t {
   MAIN_THREAD, // 1
   LISTENER_THREAD, // 2
   EVENT_MASTER_THREAD, // 3
   TIMER_THREAD, // 4
   WORKER_THREAD, // 5
   SIGNAL_THREAD, // 6
   SCHEDD_THREAD, // 7
   EVENT_MIRROR_THREAD, // 8
   READER_THREAD, // 9
};

enum component_user_type_t {
   COMPONENT_FIRST_USER = 0,
   COMPONENT_START_USER = COMPONENT_FIRST_USER,
   COMPONENT_ADMIN_USER,

   COMPONENT_NUM_USERS
};


constexpr std::array<const char*, 10> threadnames = {{
   "main",          /* 1 */
   "listener",      /* 2 */
   "event-master",  /* 3 */
   "timer",         /* 4 */
   "worker",        /* 5 */
   "signal",        /* 6 */
   "scheduler",     /* 7 */
   "mirror",        /* 8 */
   "reader",        /* 9 */
   nullptr
}};

constexpr std::array<const char*, 7> sec_mode_names = {
   "tls",
   "munge",
   "afs",
   "csp",
   "dce",
   "kerberos",
   NONE_STR
};

typedef void (*sge_exit_func_t)(int);

void component_ts0_init();
void component_ts0_destroy();

void
component_do_log();

bool
component_is_qmaster_internal();

void
component_set_qmaster_internal(bool qmaster_internal);

ProgName
component_get_component_id();

void
component_set_component_id(ProgName component_id);

bool
component_is_daemonized();

void
component_set_daemonized(bool daemonized);

const char *
component_get_component_name();

char *
component_get_log_buffer();

size_t
component_get_log_buffer_size();

void
component_set_thread_id(int thread_id);

int
component_get_thread_id();

const char *
component_get_thread_name();

void
component_set_thread_name(const char *thread_name);

uid_t
component_get_uid();

gid_t
component_get_gid();

const char *
component_get_username();

const char *
component_get_groupname();

const char *
component_get_qualified_hostname();

void
component_set_qualified_hostname(const char *qualified_hostname);

void
component_get_supplementray_groups(int *amount, ocs_grp_elem_t **grp_array);

const char *
component_get_unqualified_hostname();

sge_exit_func_t
component_get_exit_func();

void
component_set_exit_func(sge_exit_func_t exit_func);

void
component_set_current_user_type(component_user_type_t type);

char *
component_get_auth_info();

bool
component_parse_auth_info(dstring *error_dstr, char *auth_info, uid_t *uid, char *user, size_t user_len, gid_t *gid, char *group, size_t group_len, int *amount, ocs_grp_elem_t **grp_array);


