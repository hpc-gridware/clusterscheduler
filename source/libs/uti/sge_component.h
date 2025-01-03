#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024 HPC-Gridware GmbH
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

#include "uti/sge_uidgid.h"

// TODO: move the defines to a different location where other program names are defines
#define SGE_PREFIX      "sge_"
#define SGE_SHEPHERD    "sge_shepherd"
#define SGE_COSHEPHERD  "sge_coshepherd"
#define SGE_SHADOWD     "sge_shadowd"
#define PE_HOSTFILE     "pe_hostfile"

enum {
   UNKNOWN_APP = 0, // 0
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
   SGE_SHARE_MON, // 32
   PYTHON_CLIENT  // 33
};

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

extern const char *prognames[];
extern const char *threadnames[];

typedef void (*sge_exit_func_t)(int);

void
component_do_log();

bool
component_is_qmaster_internal();

void
component_set_qmaster_internal(bool qmaster_internal);

int
component_get_component_id();

void
component_set_component_id(int component_id);

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
