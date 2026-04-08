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

#include "uti/sge_uidgid.h"
#include "uti/ocs_ProgName.h"
#include "uti/ocs_ThreadName.h"

#include "sge.h"

// TODO: move the defines to a different location where other program names are defines
#define SGE_PREFIX      "sge_"
#define SGE_SHEPHERD    "sge_shepherd"
#define SGE_COSHEPHERD  "sge_coshepherd"
#define SGE_SHADOWD     "sge_shadowd"
#define PE_HOSTFILE     "pe_hostfile"

constexpr std::array<const char*, 7> sec_mode_names = {
   "tls",
   "munge",
   "afs",
   "csp",
   "dce",
   "kerberos",
   NONE_STR
};

enum component_user_type_t {
   COMPONENT_FIRST_USER = 0,
   COMPONENT_START_USER = COMPONENT_FIRST_USER,
   COMPONENT_ADMIN_USER,

   COMPONENT_NUM_USERS
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


