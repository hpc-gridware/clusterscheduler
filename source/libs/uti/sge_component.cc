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

#include "ocs_Munge.h"
#include "sge_hostname.h"
#include "sge_log.h"
#include "sge_mtutil.h"
#include "sge_security.h"
#include "sge_string.h"
#include "sge_uidgid.h"
#include "sge_rmon_macros.h"

#include "msg_common.h"
#include "msg_utilib.h"

#include "sge_component.h"

#define MAX_LOG_BUFFER (8*1024)
#define MAX_COMP_NAME 32
#define MAX_USER_GROUP 512
#define MAX_HOSTNAME (2*1024)

typedef struct {
   bool user_initialized; ///< Flag indicating if the user structure has already been initialized.
   uid_t uid; ///< User ID.
   gid_t gid; ///< Primary group ID.
   char username[MAX_USER_GROUP]; ///< User name.
   char groupname[MAX_USER_GROUP]; ///< Primary group name.

   bool supplementary_grp_initialized; ///< Flag indicating if supplementary groups are initialized.
   int amount; ///< Number of supplementary groups.
   ocs_grp_elem_t *grp_array; ///< Array containing supplementary group IDs and names.

   dstring unencrypted_auth_info;  ///< dstring for building and caching the unencrypted auth_info string
   const char *cached_auth_info;   ///< cached encrypted auth_info (when not using Munge it does never change again - for Munge every request is uniquely encrypted
} sge_component_user_t;

#define COMPONENT_MUTEX_NAME "component_mutex"

typedef struct component_ts0_t {
   pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   sge_component_user_t users[COMPONENT_NUM_USERS];
   int current_user = COMPONENT_START_USER;
} sge_component_ts0_t;

sge_component_ts0_t component_ts0_data{};

typedef struct {
   char log_buffer[MAX_LOG_BUFFER]; ///< Log buffer for storing log messages.
   int thread_id; ///< Thread ID.
   char thread_name[MAX_COMP_NAME]; ///< Name of the thread.

   int component_id; ///< Component ID.
   char component_name[MAX_COMP_NAME]; ///< Name of the component.

   bool qmaster_internal; ///< Flag indicating if the component is qmaster internal.
   bool daemonized; ///< Flag indicating if the component is daemonized.
   char qualified_hostname[MAX_HOSTNAME]; ///< Qualified hostname.
   char unqualified_hostname[MAX_HOSTNAME]; ///< Unqualified hostname.
   sge_exit_func_t exit_func; ///< Exit function.
} sge_component_tl0_t;

static pthread_once_t component_once = PTHREAD_ONCE_INIT;
static pthread_key_t sge_component_tl0_key;

/**
 * \brief Set the component ID.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] component_id Component ID to set.
 */
static void set_component_id(sge_component_tl0_t *tl, int component_id) {
   tl->component_id = component_id;
}

/**
 * \brief Set the component name.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] component_name Component name to set.
 */
static void set_component_name(sge_component_tl0_t *tl, const char *component_name) {
   if (component_name != nullptr) {
      strncpy(tl->component_name, component_name, sizeof(tl->component_name)-1);
   }
}

/**
 * \brief Set the daemonized flag.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] daemonized Daemonized flag to set.
 */
static void set_daemonized(sge_component_tl0_t *tl, bool daemonized) {
   tl->daemonized = daemonized;
}

/**
 * \brief Set the exit function.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] exit_func Exit function to set.
 */
static void set_exit_func(sge_component_tl0_t *tl, sge_exit_func_t exit_func) {
   tl->exit_func = exit_func;
}

/**
 * \brief Set the group ID.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] gid Group ID to set.
 */
static void set_gid(sge_component_user_t *user, gid_t gid) {
   user->gid = gid;
}

/**
 * \brief Set the group name.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] group_name Group name to set.
 */
static void set_group_name(sge_component_user_t *user, const char *group_name) {
   strncpy(user->groupname, group_name, sizeof(user->groupname) - 1);
}

/**
 * \brief Set the qualified hostname.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] qualified_hostname Qualified hostname to set.
 */
static void set_qualified_hostname(sge_component_tl0_t *tl, const char *qualified_hostname) {
   if (qualified_hostname != nullptr) {
      strncpy(tl->qualified_hostname, qualified_hostname, sizeof(tl->qualified_hostname)-1);
   }
}

/**
 * \brief Set the qmaster internal flag.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] qmaster_internal Qmaster internal flag to set.
 */
static void set_qmaster_internal(sge_component_tl0_t *tl, bool qmaster_internal) {
   tl->qmaster_internal = qmaster_internal;
}

/**
 * \brief Set the supplementary groups.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] amount Number of supplementary groups.
 * \param[in] grp_array Array of supplementary group elements.
 */
static void set_supplementray_groups(sge_component_user_t *user, int amount, ocs_grp_elem_t *grp_array) {
   user->amount = amount;
   user->grp_array = grp_array;
}

/**
 * \brief Set the thread name.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] thread_name Thread name to set.
 */
static void set_thread_name(sge_component_tl0_t *tl, const char *thread_name) {
   if (thread_name != nullptr) {
      strncpy(tl->thread_name, thread_name, sizeof(tl->thread_name)-1);
   }
}

/**
 * \brief Set the user ID.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] uid User ID to set.
 */
static void set_uid(sge_component_user_t *user, uid_t uid) {
   user->uid = uid;
}

/**
 * \brief Set the unqualified hostname.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] unqualified_hostname Unqualified hostname to set.
 */
static void set_unqualified_hostname(sge_component_tl0_t *tl, const char *unqualified_hostname) {
   if (unqualified_hostname != nullptr) {
      strncpy(tl->unqualified_hostname, unqualified_hostname, sizeof(tl->unqualified_hostname)-1);
   }
}

/**
 * \brief Set the username.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] username Username to set.
 */
static void set_username(sge_component_user_t *user, const char *username) {
   strncpy(user->username, username, sizeof(user->username)-1);
}

static void component_ts0_init_user() {
   // assuming that we hold the component mutex
   // setup uid/gid and corresponding names
   char user_name[256];
   char group_name[256];
   uid_t uid = geteuid();
   gid_t gid = getegid();
   sge_component_user_t *user = &component_ts0_data.users[component_ts0_data.current_user];
   set_uid(user, uid);
   set_gid(user, gid);
   SGE_ASSERT(sge_uid2user(uid, user_name, sizeof(user_name), MAX_NIS_RETRIES) == 0);
   SGE_ASSERT(sge_gid2group(gid, group_name, sizeof(group_name), MAX_NIS_RETRIES) == 0);
   set_username(user, user_name);
   set_group_name(user, group_name);

   // supplementary groups are lazy initialized in component_get
   user->supplementary_grp_initialized = false;
   set_supplementray_groups(user, 0, nullptr);

   // we'll initialize the unencrypted auth info when first needed
   sge_dstring_init_dynamic(&user->unencrypted_auth_info, 0);
   user->cached_auth_info = nullptr;
}

static void component_ts0_init_supplementary_groups() {
   // assuming that we hold the component mutex
   sge_component_user_t *user = &component_ts0_data.users[component_ts0_data.current_user];
   char err_str[MAX_STRING_SIZE];
   int amount_l = 0;
   ocs_grp_elem_t *grp_array_l = nullptr;
   bool lret = ocs_get_groups(&amount_l, &grp_array_l, err_str, sizeof(err_str));
   set_supplementray_groups(user, amount_l, grp_array_l);
   if (!lret) {
      ERROR(SFNMAX, err_str);
   }
   user->supplementary_grp_initialized = true;
}

static void component_ts0_destroy_user(component_user_type_t user_type) {
   sge_dstring_free(&component_ts0_data.users[user_type].unencrypted_auth_info);
   if (component_ts0_data.users[user_type].supplementary_grp_initialized) {
      sge_free(&component_ts0_data.users[user_type].grp_array);
   }
}

void component_ts0_init() {
   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
   component_ts0_init_user();
   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
}

// call this function in the gdi_default_exit_func()
void component_ts0_destroy() {
   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);

   for (int i = COMPONENT_FIRST_USER; i < COMPONENT_NUM_USERS; i++) {
      component_ts0_destroy_user(component_user_type_t(i));
   }

   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
}

/**
 * \brief Destroy the thread-local component structure.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 */
static void component_tl0_destroy(void *tl) {
   auto _tl = (sge_component_tl0_t *) tl;

   // wrapping structure
   sge_free(&_tl);
}

/**
 * \brief Initialize the thread-local component structure.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 */
static void component_tl0_init(sge_component_tl0_t *tl) {
   bool rmon_enabled = rmon_is_enabled();
   if (rmon_enabled) {
      rmon_disable();
   }
   static bool already_shown = false;
   memset(tl, 0, sizeof(sge_component_tl0_t));

   // some default values
   set_component_id(tl, -1);
   set_component_name(tl, "unknown");
   set_thread_name(tl, "unknown");
   set_daemonized(tl, false);
   set_qmaster_internal(tl, false);

   // setup short and long hostnames
   char *s = nullptr;
   char tmp_str[CL_MAXHOSTNAMELEN + 1];
   struct hostent *hent = nullptr;
   /* Fetch hostnames */
   SGE_ASSERT((gethostname(tmp_str, CL_MAXHOSTNAMELEN) == 0));
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

   if (rmon_enabled) {
      rmon_enable();
   }
}

/**
 * \brief Initialize the thread-local key for the component structure.
 */
static void component_thread_local_once_init() {
   pthread_key_create(&sge_component_tl0_key, component_tl0_destroy);
}

/**
 * \brief Initialize the component for multi-threading.
 */
static void component_mt_init() {
   pthread_once(&component_once, component_thread_local_once_init);
}

/**
 * \brief Class to initialize the component for multi-threading.
 */
class ComponentThreadInit {
public:
   /**
    * \brief Constructor that initializes the component for multi-threading.
    */
   ComponentThreadInit() {
      component_mt_init();
   }
};

/**
 * \brief Static instance to ensure the constructor is called, initializing the pthread key.
 * \note Although not used, the constructor call has the side effect to initialize the pthread key, so do not delete.
 */
static ComponentThreadInit component_component_obj{};

/**
 * \brief Check if the component is daemonized.
 *
 * \return True if the component is daemonized, false otherwise.
 */
bool component_is_daemonized() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->daemonized;
}

/**
 * \brief Get the component ID.
 *
 * \return The component ID.
 */
int component_get_component_id() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->component_id;
}

/**
 * \brief Get the component name.
 *
 * \return The component name.
 */
const char *component_get_component_name() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->component_name;
}

/**
 * \brief Get the exit function.
 *
 * \return The exit function.
 */
sge_exit_func_t component_get_exit_func() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->exit_func;
}

/**
 * \brief Get the group ID.
 *
 * \return The group ID.
 */
gid_t component_get_gid() {
   gid_t ret;

   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
   sge_component_user_t *user = &component_ts0_data.users[component_ts0_data.current_user];
   if (!user->user_initialized) {
      component_ts0_init_user();
   }
   ret = user->gid;
   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);

   return ret;
}

/**
 * \brief Get the group name.
 *
 * \return The group name.
 */
const char *component_get_groupname() {
   const char *ret;

   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
   sge_component_user_t *user = &component_ts0_data.users[component_ts0_data.current_user];
   if (!user->user_initialized) {
      component_ts0_init_user();
   }
   ret = user->groupname;
   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);

   return ret;
}

/**
 * \brief Get the log buffer.
 *
 * \return Pointer to the log buffer.
 */
char *component_get_log_buffer() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->log_buffer;
}

/**
 * \brief Get the size of the log buffer.
 *
 * \return The size of the log buffer.
 */
size_t component_get_log_buffer_size() {
   return MAX_LOG_BUFFER;
}

/**
 * \brief Get the qualified hostname.
 *
 * \return The qualified hostname.
 */
const char *component_get_qualified_hostname() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->qualified_hostname;
}

/**
 * \brief Get the supplementary groups.
 *
 * \param[out] amount Number of supplementary groups.
 * \param[out] grp_array Array of supplementary group elements.
 */
void component_get_supplementray_groups(int *amount, ocs_grp_elem_t **grp_array) {
   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
   // Lazy initialize of supplementary groups.
   sge_component_user_t *user = &component_ts0_data.users[component_ts0_data.current_user];
   if (!user->supplementary_grp_initialized) {
      component_ts0_init_supplementary_groups();
   }

   // Pass values to caller
   *amount = user->amount;
   *grp_array = user->grp_array;
   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
}

/**
 * \brief Get the thread ID.
 *
 * \return The thread ID.
 */
int component_get_thread_id() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->thread_id;
}

/**
 * \brief Get the thread name.
 *
 * \return The thread name.
 */
const char *component_get_thread_name() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->thread_name;
}

/**
 * \brief Get the user ID.
 *
 * \return The user ID.
 */
uid_t component_get_uid() {
   uid_t ret;

   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
   sge_component_user_t *user = &component_ts0_data.users[component_ts0_data.current_user];
   if (!user->user_initialized) {
      component_ts0_init_user();
   }
   ret = user->uid;
   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);

   return ret;
}

/**
 * \brief Get the unqualified hostname.
 *
 * \return The unqualified hostname.
 */
const char *component_get_unqualified_hostname() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->unqualified_hostname;
}

/**
 * \brief Get the username.
 *
 * \return The username.
 */
const char *component_get_username() {
   const char * ret;

   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
   sge_component_user_t *user = &component_ts0_data.users[component_ts0_data.current_user];
   if (!user->user_initialized) {
      component_ts0_init_user();
   }
   ret = user->username;
   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);

   return ret;
}

/**
 * \brief Check if the component is qmaster internal.
 *
 * \return True if the component is qmaster internal, false otherwise.
 */
bool component_is_qmaster_internal() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->qmaster_internal;
}

/**
 * \brief Set the component ID and name.
 *
 * \param[in] component_id Component ID to set.
 */
void component_set_component_id(int component_id) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_component_id(tl, component_id);
   set_component_name(tl, prognames[component_id]);
}

/**
 * \brief Set the daemonized flag.
 *
 * \param[in] daemonized Daemonized flag to set.
 */
void component_set_daemonized(bool daemonized) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_daemonized(tl, daemonized);
}

/**
 * \brief Set the exit function.
 *
 * \param[in] exit_func Exit function to set.
 */
void component_set_exit_func(sge_exit_func_t exit_func) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_exit_func(tl, exit_func);
}

/**
 * \brief Set the qmaster internal flag.
 *
 * \param[in] qmaster_internal Qmaster internal flag to set.
 */
void component_set_qmaster_internal(bool qmaster_internal) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_qmaster_internal(tl, qmaster_internal);
}

/**
 * \brief Set the qualified hostname.
 *
 * \param[in] qualified_hostname Qualified hostname to set.
 */
void component_set_qualified_hostname(const char *qualified_hostname) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_qualified_hostname(tl, qualified_hostname);
}

/**
 * \brief Set the thread ID.
 *
 * \param[in] thread_id Thread ID to set.
 */
void component_set_thread_id(int thread_id) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   tl->thread_id = thread_id;
}

/**
 * \brief Set the thread name.
 *
 * \param[in] thread_name Thread name to set.
 */
void component_set_thread_name(const char *thread_name) {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   set_thread_name(tl, thread_name);
}

/**
 * \brief Log the component details.
 */
void component_do_log() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);

   DENTER(TOP_LAYER);
   DPRINTF("THREAD ===\n");
   DPRINTF("   thread_name                      >%s<\n", tl->thread_name);
   DPRINTF("   thread_id                        >%d<\n", tl->thread_id);
   DPRINTF("   qmaster_internal                 >%s<\n", tl->qmaster_internal);
   DPRINTF("COMPONENT ===\n");
   DPRINTF("   component_id                     >%d<\n", tl->component_id);
   DPRINTF("   component_name                   >%s<\n", tl->component_name);
   DPRINTF("   daemonized                       >%s<\n", tl->daemonized);
   DPRINTF("   exit_func                        >%p<\n", tl->exit_func);
   DPRINTF("HOST ===\n");
   DPRINTF("   qualified_hostname               >%s<\n", tl->qualified_hostname);
   DPRINTF("   unqualified_hostname             >%s<\n", tl->unqualified_hostname);

   // thread shared info
   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
   DPRINTF("USER ===\n");
   sge_component_user_t *user = &component_ts0_data.users[component_ts0_data.current_user];
   DPRINTF("   uid                              >%d<\n", user->uid);
   DPRINTF("   gid                              >%d<\n", user->gid);
   DPRINTF("   username                         >%s<\n", user->username);
   DPRINTF("   groupname                        >%s<\n", user->groupname);
   DPRINTF("   supplementary_grp_initialized    >%0<\n", user->supplementary_grp_initialized);
   if (user->supplementary_grp_initialized) {
      for (int i = 0; i <= user->amount; i++) {
         DPRINTF("      grp_array               >%s< >%d<\n", user->grp_array[i].name, user->grp_array[i].id);
      }
   }
   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);

   DRETURN_VOID;
}

void
component_set_current_user_type(component_user_type_t type) {
   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
   if (type >= COMPONENT_FIRST_USER && type < COMPONENT_NUM_USERS) {
      component_ts0_data.current_user = type;
      if (!component_ts0_data.users[type].user_initialized) {
         component_ts0_init_user();
      }
   }
   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
}

char *
component_get_auth_info() {
   DENTER(TOP_LAYER);
   char *ret = nullptr;

   sge_mutex_lock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);
   sge_component_user_t *user = &component_ts0_data.users[component_ts0_data.current_user];

   // initialize the unencrypted auth info if not already done
   if (sge_dstring_strlen(&user->unencrypted_auth_info) == 0) {
      if (!user->user_initialized) {
         component_ts0_init_user();
      }
      if (!user->supplementary_grp_initialized) {
         component_ts0_init_supplementary_groups();
      }

      // create one compact string containing primary user and group information as
      // well as supplementary groups (id and names)
      constexpr char sep = static_cast<char>(0xff);
      sge_dstring_sprintf(&user->unencrypted_auth_info, uid_t_fmt "%c" gid_t_fmt "%c%s%c%s%c%d",
                          user->uid, sep, user->gid, sep, user->username, sep, user->groupname, sep, user->amount);

      for (int i = 0; i < user->amount; i++) {
         sge_dstring_sprintf_append(&user->unencrypted_auth_info, "%c" gid_t_fmt "%c%s",
                                    sep, user->grp_array[i].id, sep, user->grp_array[i].name);
      }
   }

   if (bootstrap_get_use_munge()) {
#if defined(OCS_WITH_MUNGE)
      // we need to encode the auth info in every call to this function
      // even if the user information and the payload will never change
      // munge certificates are valid for a limited time only and for a single use
      char *munge_auth_info = nullptr;
      munge_err_t err = ocs::uti::Munge::munge_encode_func(&munge_auth_info, nullptr,
         sge_dstring_get_string(&user->unencrypted_auth_info), sge_dstring_strlen(&user->unencrypted_auth_info) + 1);
      if (err != EMUNGE_SUCCESS) {
         ERROR("failed to Munge encode for user " sge_uu32 ": " SFN ": " SFN, user->uid, user->username, ocs::uti::Munge::munge_strerror_func(err));
      } else {
         ret = munge_auth_info;
      }
#else
      ERROR(SFNMAX, MSG_GDI_BUILT_WITHOUT_MUNGE);
#endif
   } else {
      if (user->cached_auth_info == nullptr) {
         // encrypt and store the information once - it will never change
         size_t size = sge_dstring_strlen(&user->unencrypted_auth_info) * 3;
         char *obuffer = sge_malloc(size);
         SGE_ASSERT(obuffer != nullptr);
         if (sge_encrypt(sge_dstring_get_string(&user->unencrypted_auth_info), obuffer, size)) {
            user->cached_auth_info = obuffer;
         }
      }
      // if available we return the cached auth info
      if (user->cached_auth_info != nullptr) {
         ret = strdup(user->cached_auth_info);
      }
   }

   sge_mutex_unlock(COMPONENT_MUTEX_NAME, __func__, __LINE__, &component_ts0_data.mutex);

   DRETURN(ret);
}

bool
component_parse_auth_info(dstring *error_dstr, char *auth_info, uid_t *uid, char *user, size_t user_len, gid_t *gid, char *group, size_t group_len, int *amount, ocs_grp_elem_t **grp_array) {
   DENTER(TOP_LAYER);
   char auth_buffer[2 * SGE_SEC_BUFSIZE];

   if (auth_info == nullptr) {
      // @todo different error message, authinfo is null
      sge_dstring_sprintf(error_dstr, SFNMAX, MSG_AUTHINFO_IS_NULL);
      DRETURN(false);
   }

   //INFO("<=== received auth info: %s", auth_info);
   //printf("<=== received auth info: %s\n", auth_info);
   // decrypt received auth_info
   uid_t munge_uid{0};
   gid_t munge_gid{0};
   bool use_munge = bootstrap_get_use_munge();
   if (use_munge) {
#if defined(OCS_WITH_MUNGE)
      munge_err_t err;
      char *local_auth_buffer{nullptr};
      int local_len{0};
      err = ocs::uti::Munge::munge_decode_func(auth_info, nullptr, (void **)(&local_auth_buffer), &local_len,
         &munge_uid, &munge_gid);
      if (err != EMUNGE_SUCCESS) {
         sge_dstring_sprintf(error_dstr, MSG_UTI_MUNGE_DECODE_FAILED_S, ocs::uti::Munge::munge_strerror_func(err));
         DRETURN(false);
      }
      sge_strlcpy(auth_buffer, local_auth_buffer, sizeof(auth_buffer));
      sge_free(&local_auth_buffer);
#else
      sge_dstring_sprintf(error_dstr, SFNMAX, MSG_GDI_BUILT_WITHOUT_MUNGE);
      DRETURN(false);
#endif
   } else {
      int dlen = 0;
      if (!sge_decrypt(auth_info, strlen(auth_info), auth_buffer, &dlen)) {
         sge_dstring_sprintf(error_dstr, SFNMAX, MSG_GDI_FAILEDTOEXTRACTAUTHINFO);
         DRETURN(false);
      }
   }

   bool ret = true;
   saved_vars_s *context = nullptr;
   constexpr char separator[] = "\xff";
   const char *token;
   const char *next_token = sge_strtok_r(auth_buffer, separator, &context);
   int pos = 0;
   while ((token = next_token) != nullptr) {
      switch (pos) {
         case 0:
            uid_t auth_uid;
            if (sscanf(token, uid_t_fmt, &auth_uid) == 1) {
               if (uid != nullptr) {
                  *uid = auth_uid;
               }
               // verify that auth_info uid matches Munge uid
               if (use_munge && munge_uid != auth_uid) {
                  sge_dstring_sprintf(error_dstr, MSG_UTI_MUNGE_AUTH_UID_MISMATCH_II, munge_uid, auth_uid);
                  ret = false;
               }
            } else {
               sge_dstring_sprintf(error_dstr, SFNMAX, MSG_UTI_UNABLE_TO_EXTRACT_UID);
               ret = false;
            }
            break;
         case 1:
            gid_t auth_gid;
            if (sscanf(token, gid_t_fmt, &auth_gid) == 1) {
               if (gid != nullptr) {
                  *gid = auth_gid;
               }
               // verify that auth_info gid matches Munge gid
               if (use_munge && munge_gid != auth_gid) {
                  sge_dstring_sprintf(error_dstr, MSG_UTI_MUNGE_AUTH_UID_MISMATCH_II, munge_gid, auth_gid);
                  ret = false;
               }
            } else {
               sge_dstring_sprintf(error_dstr, SFNMAX, MSG_UTI_UNABLE_TO_EXTRACT_GID);
               ret = false;
            }
            break;
         case 2:
            if (user != nullptr) {
               sge_strlcpy(user, token, user_len);
            }
            break;
         case 3:
            if (group != nullptr) {
               sge_strlcpy(group, token, group_len);
            }
            break;
         case 4:
            if (amount != nullptr && grp_array != nullptr) {
               if (sscanf(token, "%d", amount) == 1) {
                  if (*amount > 0) {
                     const size_t size = *amount * sizeof(ocs_grp_elem_t);
                     *grp_array = reinterpret_cast<ocs_grp_elem_t *>(sge_malloc(size));
                     if (*grp_array == nullptr) {
                        sge_dstring_sprintf(error_dstr, SFNMAX, MSG_UTI_UNABLE_TO_EXTRACT_NSUP);
                        ret = false;
                     }
                  } else {
                     // no error but there are no supplementary groups
                     break;
                  }
               }
            }
            break;
         default:
            if (amount != nullptr && grp_array != nullptr) {
               // beginning from token 5 we will find the supplementary gids and group names
               int idx = (pos - 5) / 2;
               if (idx < *amount) {
                  if (pos % 2 == 1) {
                     gid_t supplementary_gid;

                     if (sscanf(token, gid_t_fmt, &supplementary_gid) == 1) {
                        (*grp_array)[idx].id= supplementary_gid;
                     } else {
                        sge_dstring_sprintf(error_dstr, MSG_UTI_UNABLE_TO_EXTRACT_SUP_S, "failed parsing gid");
                        ret = false;
                     }
                  } else {
                     sge_strlcpy((*grp_array)[idx].name, token, sizeof((*grp_array)[idx].name));
                  }
               } else {
                  sge_dstring_sprintf(error_dstr, MSG_UTI_UNABLE_TO_EXTRACT_SUP_S, "too many IDs");
                  ret = false;
               }
            }
            break;
      }

      // early return if an error happens during parsing authinfo
      if (!ret) {
         break;
      }
      pos++;
      next_token = sge_strtok_r(nullptr, separator, &context);
   }

   // beginning with v9.0.0 authinfo has to contain at least 5 token (uid, uname, git, gname, #grps)
   if (pos < 4) {
      sge_dstring_sprintf(error_dstr, MSG_UTI_UNABLE_TO_EXTRACT_SUP_S, "old client tried to connect");
      ret = false;
   } else if (amount != nullptr && (pos < (4 + (*amount) * 2))) {
      sge_dstring_sprintf(error_dstr, MSG_UTI_UNABLE_TO_EXTRACT_SUP_S, "unexpected amount of supplementary groups");
      ret = false;
   }

   sge_free_saved_vars(context);
   if (!ret) {
      // cleanup in case of errors
      sge_free(grp_array);
   } else {
      // show information in debug output
      if (DPRINTF_IS_ACTIVE) {
         dstring dbg_msg = DSTRING_INIT;
         ocs_id2dstring(&dbg_msg, *uid, user, *gid, group, *amount, *grp_array);
         sge_dstring_free(&dbg_msg);
      }
   }

   DRETURN(ret);
}
