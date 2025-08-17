/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *  
 *  Copyright 2024-2025 HPC-Gridware GmbH
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
#define MAX_USER_GROUP 512
#define MAX_HOSTNAME (2*1024)

typedef struct {
   char log_buffer[MAX_LOG_BUFFER]; ///< Log buffer for storing log messages.
   int thread_id; ///< Thread ID.
   char thread_name[MAX_COMP_NAME]; ///< Name of the thread.

   int component_id; ///< Component ID.
   char component_name[MAX_COMP_NAME]; ///< Name of the component.

   uid_t uid; ///< User ID.
   gid_t gid; ///< Primary group ID.
   char username[MAX_USER_GROUP]; ///< User name.
   char groupname[MAX_USER_GROUP]; ///< Primary group name.

   bool supplementary_grp_initialized; ///< Flag indicating if supplementary groups are initialized.
   int amount; ///< Number of supplementary groups.
   ocs_grp_elem_t *grp_array; ///< Array containing supplementary group IDs and names.

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
static void set_gid(sge_component_tl0_t *tl, gid_t gid) {
   tl->gid = gid;
}

/**
 * \brief Set the group name.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 * \param[in] group_name Group name to set.
 */
static void set_group_name(sge_component_tl0_t *tl, const char *group_name) {
   strncpy(tl->groupname, group_name, sizeof(tl->groupname) - 1);
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
static void set_supplementray_groups(sge_component_tl0_t *tl, int amount, ocs_grp_elem_t *grp_array) {
   tl->amount = amount;
   tl->grp_array = grp_array;
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
static void set_uid(sge_component_tl0_t *tl, uid_t uid) {
   tl->uid = uid;
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
static void set_username(sge_component_tl0_t *tl, const char *username) {
   strncpy(tl->username, username, sizeof(tl->username)-1);
}

/**
 * \brief Destroy the thread-local component structure.
 *
 * \param[in] tl Pointer to the thread-local component structure.
 */
static void component_tl0_destroy(void *tl) {
   auto _tl = (sge_component_tl0_t *) tl;

   sge_free(&_tl->grp_array);

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

   // supplementary groups are lazy initialized in component_get
   tl->supplementary_grp_initialized = false;
   set_supplementray_groups(tl, 0, nullptr);

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
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->gid;
}

/**
 * \brief Get the group name.
 *
 * \return The group name.
 */
const char *component_get_groupname() {
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->groupname;
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
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);

   // Lazy initialize of supplementary groups.
   if (!tl->supplementary_grp_initialized) {
      char err_str[MAX_STRING_SIZE];
      int amount_l = 0;
      ocs_grp_elem_t *grp_array_l = nullptr;
      bool lret = ocs_get_groups(&amount_l, &grp_array_l, err_str, sizeof(err_str));
      set_supplementray_groups(tl, amount_l, grp_array_l);
      if (!lret) {
         ERROR("%s", err_str);
      }
      tl->supplementary_grp_initialized = true;
   }

   // Pass values to caller
   *amount = tl->amount;
   *grp_array = tl->grp_array;
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
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->uid;
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
   GET_SPECIFIC(sge_component_tl0_t, tl, component_tl0_init, sge_component_tl0_key);
   return tl->username;
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
   DPRINTF("USER ===\n");
   DPRINTF("   uid                              >%d<\n", tl->uid);
   DPRINTF("   gid                              >%d<\n", tl->gid);
   DPRINTF("   username                         >%s<\n", tl->username);
   DPRINTF("   groupname                        >%s<\n", tl->groupname);
   DPRINTF("   supplementary_grp_initialized    >%0<\n", tl->supplementary_grp_initialized);
   if (tl->supplementary_grp_initialized) {
      for (int i = 0; i <= tl->amount; i++) {
         DPRINTF("      grp_array               >%s< >%d<\n", tl->grp_array[i].name, tl->grp_array[i].id);
      }
   }
   DPRINTF("HOST ===\n");
   DPRINTF("   qualified_hostname               >%s<\n", tl->qualified_hostname);
   DPRINTF("   unqualified_hostname             >%s<\n", tl->unqualified_hostname);
   DRETURN_VOID;
}
