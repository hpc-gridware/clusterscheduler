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

#include <cstring>
#include <pthread.h>

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_rmon_macros.h"

#include "sge_component.h"

#define MAX_LOG_BUFFER (8*1024)
#define MAX_COMP_NAME 32
#define MAX_USER_GROUP 128
#define MAX_HOSTNAME (2*1024)

typedef struct {
   char log_buffer[MAX_LOG_BUFFER];
   int thread_id;
   char thread_name[MAX_COMP_NAME];

   int component_id;
   char component_name[MAX_COMP_NAME];
   uid_t uid;
   gid_t gid;
   char username[MAX_USER_GROUP];
   char groupname[MAX_USER_GROUP];
   bool qmaster_internal;
   bool daemonized;
   char qualified_hostname[MAX_HOSTNAME];
   char unqualified_hostname[MAX_HOSTNAME];
   sge_exit_func_t exit_func;
} sge_component_tl0_t;

static pthread_once_t component_once = PTHREAD_ONCE_INIT;
static pthread_key_t sge_component_tl0_key;


static void
set_component_id(sge_component_tl0_t *tl, int component_id) {
   tl->component_id = component_id;
}

static void
set_component_name(sge_component_tl0_t *tl, const char *component_name) {
   if (component_name != nullptr) {
      strncpy(tl->component_name, component_name, sizeof(tl->component_name)-1);
   }
}

static void
set_daemonized(sge_component_tl0_t *tl, bool daemonized) {
   tl->daemonized = daemonized;
}

static void
set_exit_func(sge_component_tl0_t *tl, sge_exit_func_t exit_func) {
   tl->exit_func = exit_func;
}

static void
set_gid(sge_component_tl0_t *tl, gid_t gid) {
   tl->gid = gid;
}

static void
set_group_name(sge_component_tl0_t *tl, const char *group_name) {
   strncpy(tl->groupname, group_name, sizeof(tl->groupname) - 1);
}

static void
set_qualified_hostname(sge_component_tl0_t *tl, const char *qualified_hostname) {
   if (qualified_hostname != nullptr) {
      strncpy(tl->qualified_hostname, qualified_hostname, sizeof(tl->qualified_hostname)-1);
   }
}

static void
set_qmaster_internal(sge_component_tl0_t *tl, bool qmaster_internal) {
   tl->qmaster_internal = qmaster_internal;
}

static void
set_thread_name(sge_component_tl0_t *tl, const char *thread_name) {
   if (thread_name != nullptr) {
      strncpy(tl->thread_name, thread_name, sizeof(tl->thread_name)-1);
   }
}

static void
set_uid(sge_component_tl0_t *tl, uid_t uid) {
   tl->uid = uid;
}

static void
set_unqualified_hostname(sge_component_tl0_t *tl, const char *unqualified_hostname) {
   if (unqualified_hostname != nullptr) {
      strncpy(tl->unqualified_hostname, unqualified_hostname, sizeof(tl->unqualified_hostname)-1);
   }
}

static void
set_username(sge_component_tl0_t *tl, const char *username) {
   strncpy(tl->username, username, sizeof(tl->username)-1);
}

static void
component_tl0_destroy(void *tl) {
   auto _tl = (sge_component_tl0_t *) tl;

   // wrapping structure
   sge_free(&_tl);
}

static void
component_tl0_init(sge_component_tl0_t *tl) {
   static bool already_shown = false;
   memset(tl, 0, sizeof(sge_component_tl0_t));

   // some default values
   set_component_id(tl, -1);
   set_component_name(tl, "unknown");
   set_thread_name(tl, "unknown");
   set_daemonized(tl, false);
   set_qmaster_internal(tl, false);

   // setup uid/gid and corresponding names
   char user[256];
   char group[256];
   uid_t uid = geteuid();
   gid_t gid = getegid();
   set_uid(tl, uid);
   set_gid(tl, gid);
   SGE_ASSERT(sge_uid2user(uid, user, sizeof(user), MAX_NIS_RETRIES) == 0);
   SGE_ASSERT(sge_gid2group(gid, group, sizeof(group), MAX_NIS_RETRIES) == 0);
   set_username(tl, user);
   set_group_name(tl, group);

   // setup short and long hostnames
   char *s = nullptr;
   stringT tmp_str;
   struct hostent *hent = nullptr;
   /* Fetch hostnames */
   SGE_ASSERT((gethostname(tmp_str, sizeof(tmp_str)) == 0));
   SGE_ASSERT(((hent = sge_gethostbyname(tmp_str, nullptr)) != nullptr));
   set_qualified_hostname(tl, hent->h_name);
   s = sge_dirname(hent->h_name, '.');
   set_unqualified_hostname(tl, s);
   sge_free(&s);
   /* Bad resolving in some networks leads to short qualified host names */
   if (!strcmp(tl->qualified_hostname, tl->unqualified_hostname)) {
      char tmp_addr[8];
      struct hostent *hent2 = nullptr;
      memcpy(tmp_addr, hent->h_addr, hent->h_length);
      SGE_ASSERT(((hent2 = sge_gethostbyaddr((const struct in_addr *) tmp_addr, nullptr)) != nullptr));

      set_qualified_hostname(tl, hent2->h_name);
      s = sge_dirname(hent2->h_name, '.');
      set_unqualified_hostname(tl, s);
      sge_free(&s);
      sge_free_hostent(&hent2);
   }
   sge_free_hostent(&hent);

   if (!already_shown) {
      //component_component_log(tl);
      already_shown = true;
   }
}

static void
component_thread_local_once_init() {
   pthread_key_create(&sge_component_tl0_key, component_tl0_destroy);
}

static void
component_mt_init() {
   pthread_once(&component_once, component_thread_local_once_init);
}

class ComponentThreadInit {
public:
   ComponentThreadInit() {
      component_mt_init();
   }
};

// although not used the constructor call has the side effect to initialize the pthread_key => do not delete
static ComponentThreadInit component_component_obj{};

bool
component_is_daemonized() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->daemonized;
}

int
component_get_component_id() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->component_id;
}

const char *
component_get_component_name() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->component_name;
}

sge_exit_func_t
component_get_exit_func() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->exit_func;
}

gid_t
component_get_gid() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->gid;
}

const char *
component_get_groupname() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->groupname;
}

char *
component_get_log_buffer() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->log_buffer;
}

size_t
component_get_log_buffer_size() {
   return MAX_LOG_BUFFER;
}

const char *
component_get_qualified_hostname() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->qualified_hostname;
}

int
component_get_thread_id() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->thread_id;
}

const char *
component_get_thread_name() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->thread_name;
}

uid_t
component_get_uid() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->uid;
}

const char *
component_get_unqualified_hostname() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->unqualified_hostname;
}

const char *
component_get_username() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->username;
}

bool
component_is_qmaster_internal() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->qmaster_internal;
}

void
component_set_component_id(int component_id) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_component_id(tl, component_id);
   set_component_name(tl, prognames[component_id]);
}

void
component_set_daemonized(bool daemonized) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_daemonized(tl, daemonized);
}

void
component_set_exit_func(sge_exit_func_t exit_func) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_exit_func(tl, exit_func);
}

void
component_set_qmaster_internal(bool qmaster_internal) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_qmaster_internal(tl, qmaster_internal);
}

void
component_set_qualified_hostname(const char *qualified_hostname) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_qualified_hostname(tl, qualified_hostname);
}

void
component_set_thread_id(int thread_id) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   tl->thread_id = thread_id;
}

void
component_set_thread_name(const char *thread_name) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_thread_name(tl, thread_name);
}

void
component_do_log() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);

   DENTER(TOP_LAYER);
   DPRINTF("THREAD ===\n");
   DPRINTF("   thread_name          >%s<\n", tl->thread_name);
   DPRINTF("   thread_id            >%d<\n", tl->thread_id);
   DPRINTF("   qmaster_internal     >%s<\n", tl->qmaster_internal);
   DPRINTF("COMPONENT ===\n");
   DPRINTF("   component_id         >%d<\n", tl->component_id);
   DPRINTF("   component_name       >%s<\n", tl->component_name);
   DPRINTF("   daemonized           >%s<\n", tl->daemonized);
   DPRINTF("   exit_func            >%p<\n", tl->exit_func);
   DPRINTF("USER ===\n");
   DPRINTF("   uid                  >%d<\n", tl->uid);
   DPRINTF("   gid                  >%d<\n", tl->gid);
   DPRINTF("   username             >%s<\n", tl->username);
   DPRINTF("   groupname            >%s<\n", tl->groupname);
   DPRINTF("HOST ===\n");
   DPRINTF("   qualified_hostname   >%s<\n", tl->qualified_hostname);
   DPRINTF("   unqualified_hostname >%s<\n", tl->unqualified_hostname);
   DRETURN_VOID;
}
