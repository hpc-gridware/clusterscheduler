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
 *  Portions of this code are Copyright 2011 Univa Inc.
 * 
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Implementation of user and group identity handling
 */

/*
 *   Parts of the code have been contributed by and are copyright of
 *   Tommy Karlsson <tommy.karlsson@bolero.se>
 */

#include "uti/sge_uidgid.h"

#include <cassert>
#include <cstdio>
#include <cerrno>
#include <limits>
#include <pwd.h>
#include <grp.h>
#include <pthread.h>

#include "uti/msg_utilib.h"
#include "uti/sge_component.h"
#include "uti/sge_log.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include <cinttypes>

#include "msg_common.h"

/// debug layer used by every DENTER/DPRINTF in this file
#define UIDGID_LAYER CULL_LAYER
/// longest line accepted when reading a passwd or group file
#define MAX_LINE_LENGTH 10000

enum {
   SGE_MAX_USERGROUP_BUF = 255   ///< initial buffer size for getpwnam_r/getgrgid_r results
};

/** @brief The admin user this process runs work as, resolved once and cached */
typedef struct {
   pthread_mutex_t mutex;    ///< protects every other field
   const char *user_name;    ///< name of the admin user
   uid_t uid;                ///< resolved user id
   gid_t gid;                ///< resolved group id
   bool initialized;         ///< true once the name has been resolved
} admin_user_t;

static admin_user_t admin_user = {PTHREAD_MUTEX_INITIALIZER, nullptr, (uid_t) -1, (gid_t) -1, false};

static void set_admin_user(const char *user_name, uid_t, gid_t);

static int get_admin_user(uid_t *, gid_t *);

/**
 * @brief Return true/false if current real user is superuser (root/Administrator).
 *
 * @details
 * Check the real user id to determine if it is the superuser. If so, return
 * true, else return false. This function relies on getuid == 0 for UNIX.
 * Other members of the Administrators group do not have the permission
 * to "su" without password!
 *
 * @return
 * true - root was start user
 * false - otherwise
 */
bool
sge_is_start_user_superuser() {
   DENTER(UIDGID_LAYER);
   bool is_root = (getuid() == SGE_SUPERUSER_UID);
   DRETURN(is_root);
}

/**
 * @brief Set SGE/EE admin user
 *
 * Set SGE/EE admin user. If 'user' is "none" then use the current
 * uid/gid. Ignore if current user is not root.
 *
 * @param user admin user name, or "none" to use the current uid/gid
 * @param err_str buffer receiving an error message
 * @param err_str_size size of @p err_str in bytes
 *
 * @return error state 0 - OK -1 - Username does not exist -2 - Admin user was already set
 */
int
sge_set_admin_username(const char *user, char *err_str, size_t err_str_size) {
   DENTER(UIDGID_LAYER);

   // Do only if admin user is not already set!
   uid_t uid;
   gid_t gid;
   if (get_admin_user(&uid, &gid) != ESRCH) {
      DRETURN(-2);
   }
   if (!user || user[0] == '\0') {
      if (err_str) {
         snprintf(err_str, err_str_size, SFNMAX, MSG_POINTER_SETADMINUSERNAMEFAILED);
      }
      DRETURN(-1);
   }

   int ret = 0;
   if (!strcasecmp(user, "none")) {
      set_admin_user("root", getuid(), getgid());
   } else {
      int size = get_pw_buffer_size();
      char *buffer = sge_malloc(size);
      SGE_ASSERT(buffer != nullptr);

      struct passwd pw_struct{};
      struct passwd *admin = sge_getpwnam_r(user, &pw_struct, buffer, size);
      if (admin) {
         set_admin_user(user, admin->pw_uid, admin->pw_gid);
      } else {
         if (err_str)
            snprintf(err_str, err_str_size, MSG_SYSTEM_ADMINUSERNOTEXIST_S, user);
         ret = -1;
      }
      sge_free(&buffer);
   }
   DRETURN(ret);
} /* sge_set_admin_username() */

/**
 * @brief Set euid/egid to admin uid/gid
 *
 * Set euid/egid to admin uid/gid. Silently ignore if our uid
 * is not root. Do nothing if out euid/egid is already the admin
 * uid/gid. If the admin user was not set with
 * sge_set_admin_username() the function will not return.
 *
 * @return error state 0 - OK -1 - setegid()/seteuid() fails
 *
 * @note MT-NOTE: sge_switch2admin_user() is MT safe.
 *
 * @see #sge_switch2admin_user, #sge_set_admin_username, #sge_switch2start_user, `sge_run_as_user()`
 */
int
sge_switch2admin_user() {
   uid_t uid;
   gid_t gid;
   int ret = 0;

   DENTER(UIDGID_LAYER);
   /*
    * On Windows Vista (and probably later versions) we can't set the effective
    * user ID to somebody else during boot time, because the local Administrator
    * doesn't have his primary group set before booting finished.
    * This problem occurs solely when the execd is started by a RC script
    * during boot time.
    * But we don't need to switch to the UGE admin user anyway, as spooling
    * always has to be done locally, so we can just skip it always.
    */
   if (get_admin_user(&uid, &gid) == ESRCH) {
      CRITICAL(SFNMAX, MSG_SWITCH_USER_NOT_INITIALIZED);
      ocs::TerminationManager::trigger_abort();
   }

   if (!sge_is_start_user_superuser()) {
      DPRINTF(MSG_SWITCH_USER_NOT_ROOT);
      ret = 0;
      goto exit;
   } else {
      if (getegid() != gid) {
         if (setegid(gid) == -1) {
            DTRACE;
            ret = -1;
            goto exit;
         }
      }

      if (geteuid() != uid) {
         if (seteuid(uid) == -1) {
            DTRACE;
            ret = -1;
            goto exit;
         }
      }
   }

   // update component
   component_set_current_user_type(COMPONENT_ADMIN_USER);

   exit:
DPRINTF("uid=%ld; gid=%ld; euid=%ld; egid=%ld auid=%ld; agid=%ld\n", (long) getuid(), (long) getgid(),
        (long) geteuid(), (long) getegid(), (long) uid, (long) gid);
   DRETURN(ret);
} /* sge_switch_2admin_user() */

/**
 * @brief Set euid/egid to start uid/gid
 *
 * Set euid/egid to the uid/gid of that user which started the
 * application which calles this function. If our euid/egid is
 * already the start uid/gid don't do anything. If the admin user
 * was not set with sge_set_admin_username() the function will
 * not return.
 *
 * @return error state 0 - OK -1 - setegid()/seteuid() fails
 *
 * @note MT-NOTE: sge_switch2start_user() is MT safe.
 *
 * @see #sge_switch2admin_user, #sge_set_admin_username, #sge_switch2start_user, `sge_run_as_user()`
 */
int
sge_switch2start_user() {
   uid_t uid, start_uid;
   gid_t gid, start_gid;
   int ret = 0;

   DENTER(UIDGID_LAYER);
   /*
    * On Windows Vista (and probably later versions) we can't set the effective
    * user ID to somebody else during boot time, because the local Administrator
    * doesn't have his primary group set before booting finished.
    * This problem occurs solely when the execd is started by a RC script
    * during boot time.
    * But we don't need to switch to the UGE admin user anyway, as spooling
    * always has to be done locally, so we can just skip it always.
    */

   if (get_admin_user(&uid, &gid) == ESRCH) {
      CRITICAL(SFNMAX, MSG_SWITCH_USER_NOT_INITIALIZED);
      ocs::TerminationManager::trigger_abort();
   }

   start_uid = getuid();
   start_gid = getgid();

   if (!sge_is_start_user_superuser()) {
      DPRINTF(MSG_SWITCH_USER_NOT_ROOT);
      ret = 0;
      goto exit;
   } else {
      if (start_gid != getegid()) {
         if (setegid(start_gid) == -1) {
            DTRACE;
            ret = -1;
            goto exit;
         }
      }
      if (start_uid != geteuid()) {
         if (seteuid(start_uid) == -1) {
            DTRACE;
            ret = -1;
            goto exit;
         }
      }
   }

   // update component
   component_set_current_user_type(COMPONENT_START_USER);

exit:
   DPRINTF("uid=%ld; gid=%ld; euid=%ld; egid=%ld auid=%ld; agid=%ld\n", (long) getuid(), (long) getgid(),
      (long) geteuid(), (long) getegid(), (long) uid, (long) gid);
   DRETURN(ret);
} /* sge_switch2start_user() */

/**
 * @brief Resolves user name to uid and gid
 *
 * Resolves a username ('user') to it's uid (stored in 'puid') and
 * it's primary gid (stored in 'pgid').
 * 'retries' defines the number of (e.g. NIS/DNS) retries.
 * If 'puid' is nullptr the user name is resolved without saving it.
 *
 * @param user username
 * @param puid uid pointer
 * @param pgid gid pointer
 * @param retries number of retries
 *
 * @return exit state 0 - OK 1 - Error
 *
 * @note MT-NOTE: sge_user2uid() is MT safe.
 */
int
sge_user2uid(const char *user, uid_t *puid, gid_t *pgid, int retries) {
   struct passwd *pw;
   struct passwd pwentry{};
   char *buffer;
   int size;

   DENTER(UIDGID_LAYER);

   size = get_pw_buffer_size();
   buffer = sge_malloc(size);
   SGE_ASSERT(buffer != nullptr);

   do {
      DPRINTF("name: %s retries: %d\n", user, retries);

      if (!retries--) {
         sge_free(&buffer);
         DRETURN(1);
      }
      if (getpwnam_r(user, &pwentry, buffer, size, &pw) != 0) {
         pw = nullptr;
      }
   } while (pw == nullptr);

   if (puid) {
      *puid = pw->pw_uid;
   }
   if (pgid) {
      *pgid = pw->pw_gid;
   }

   sge_free(&buffer);
   DRETURN(0);
} /* sge_user2uid() */

/**
 * @brief Resolve a group name to its gid
 *
 * Resolves a groupname ('gname') to its gid (stored in 'gidp').
 * 'retries' defines the number of (e.g. NIS/DNS) retries.
 * If 'gidp' is nullptr the group name is resolved without saving it.
 *
 * @param gname group name
 * @param gidp gid pointer
 * @param retries number of retries
 *
 * @return exit state 0 - OK 1 - Error
 *
 * @note MT-NOTE: sge_group2gid() is MT safe.
 */
int
sge_group2gid(const char *gname, gid_t *gidp, int retries) {
   struct group *gr;
   struct group gr_entry {};
   char *buffer;
   int size;

   DENTER(UIDGID_LAYER);

   size = get_group_buffer_size();
   buffer = sge_malloc(size);
   SGE_ASSERT(buffer != nullptr);

   do {
      if (!retries--) {
         sge_free(&buffer);
         DRETURN(1);
      }
      if (getgrnam_r(gname, &gr_entry, buffer, size, &gr) != 0) {
         if (errno == ERANGE) {
            retries++;
            size += 1024 * 32;
            buffer = (char *) sge_realloc(buffer, size, 1);
         }
         gr = nullptr;
      }
   } while (gr == nullptr);

   if (gidp) {
      *gidp = gr->gr_gid;
   }

   sge_free(&buffer);
   DRETURN(0);
} /* sge_group2gid() */

/**
 * @brief Resolves uid to user name
 *
 * Resolves uid to user name. if 'dst' is nullptr the function checks
 * only if the uid is resolvable.
 *
 * @param uid user id
 * @param dst buffer for the username
 * @param sz buffersize
 * @param retries number of retries
 *
 * @return error state 0 - OK 1 - Error
 *
 * @note MT-NOTE: sge_uid2user() is MT safe.
 */
int
sge_uid2user(uid_t uid, char *dst, size_t sz, int retries) {
   struct passwd *pw;
   struct passwd pw_entry {};
   int size;
   char *buffer;

   DENTER(UIDGID_LAYER);

   size = get_pw_buffer_size();
   buffer = sge_malloc(size);
   SGE_ASSERT(buffer != nullptr);

   /* max retries that are made resolving user name */
   while (getpwuid_r(uid, &pw_entry, buffer, size, &pw) != 0 || !pw) {
      if (!retries--) {
         ERROR(MSG_SYSTEM_GETPWUIDFAILED_uS, uid, strerror(errno));
         sge_free(&buffer);
         DRETURN(1);
      }
      sleep(1);
   }
   sge_strlcpy(dst, pw->pw_name, sz);

   sge_free(&buffer);

   DRETURN(0);
} /* sge_uid2user() */

/** @brief Resolve gid to group name.
 *
 * Resolves gid to group name. If 'dst' is nullptr the function checks only if the gid is resolvable.
 *
 * @param gid [in] Group ID to resolve.
 * @param dst [out] Destination buffer to store the group name.
 * @param sz [in] Size of the destination buffer.
 * @param retries [in] Number of retries for resolving the group name.
 * @return 0 on success, 1 on failure.
 * @note This function is MT safe.
 */
int
sge_gid2group(gid_t gid, char *dst, const size_t sz, const int retries) {
   DENTER(UIDGID_LAYER);

   // allocate buffer for group entry
   auto size = static_cast<size_t>(get_group_buffer_size());
   char *buf = sge_malloc(size);
   SGE_ASSERT(buf != nullptr);

   // try to find group entry. if not found after retries return error.
   struct group gr_entry {};
   struct group *gr = sge_getgrgid_r(gid, &gr_entry, &buf, &size, retries);
   if (gr == nullptr) {
      sge_free(&buf);
      DRETURN(1);
   }

   // copy group name to destination buffer
   sge_strlcpy(dst, gr->gr_name, sz);
   sge_free(&buf);
   DRETURN(0);
}

/** @brief Resolve a group id to its name, with a one entry cache
 *
 * @param gid the group id to resolve
 * @param[in,out] last_gid gid resolved by the previous call; used as a cache so
 *                repeated lookups of the same group cost nothing
 * @param[in,out] group_name_p receives the group name; the previous value is
 *                reused when @p gid equals @p last_gid, otherwise it is freed
 *                and replaced. The caller owns the result
 * @param retries number of retries when NIS or LDAP is slow to answer
 * @return 0 on success, 1 when the group could not be resolved
 *
 * @note MT-NOTE: sge_gid2group() is MT safe
 */
int
sge_gid2group(gid_t gid, gid_t *last_gid, char **group_name_p, int retries) {
   struct group *gr;
   struct group gr_entry {};

   DENTER(TOP_LAYER);

   if (!group_name_p || !last_gid) {
      DRETURN(1);
   }

   if (!(*group_name_p) || *last_gid != gid) {
      char *buf = nullptr;
      int size = 0;

      size = get_group_buffer_size();
      buf = sge_malloc(size);
      SGE_ASSERT(buf != nullptr);

      /* max retries that are made resolving group name */
      while (getgrgid_r(gid, &gr_entry, buf, size, &gr) != 0) {
         if (!retries--) {
            sge_free(&buf);

            DRETURN(1);
         }

         sleep(1);
      }

      /* Bugfix: Issuezilla 1256
       * We need to handle the case when the OS is unable to resolve the GID to
       * a name. [DT] */
      if (gr == nullptr) {
         sge_free(&buf);
         DRETURN(1);
      }

      /* cache group name */
      *group_name_p = sge_strdup(*group_name_p, gr->gr_name);
      *last_gid = gid;

      sge_free(&buf);
   }
   DRETURN(0);
} /* _sge_gid2group() */

/**
 * @brief Get the buffer size required for getpw*_r
 *
 * Returns the buffer size required for functions like getpwnam_r.
 * It can either be retrieved via sysconf, or a bit (20k) buffer
 * size is taken.
 *
 * @return buffer size in bytes
 *
 * @note MT-NOTE: get_pw_buffer_size() is MT safe
 *
 * @see #get_group_buffer_size
 */
int
get_pw_buffer_size() {
   static const int buf_size = 20480;

   int sz = buf_size;

#ifdef _SC_GETPW_R_SIZE_MAX
   if ((sz = (int) sysconf(_SC_GETPW_R_SIZE_MAX)) == -1) {
      sz = buf_size;
   } else {
      sz = std::max(sz, buf_size);
   }
#endif

   return sz;
}

/**
 * @brief Get the buffer size required for getgr*_r
 *
 * Returns the buffer size required for functions like getgrnam_r.
 * It can either be retrieved via sysconf, or a bit (20k) buffer
 * size is taken.
 *
 * @return buffer size in bytes
 *
 * @note MT-NOTE: get_group_buffer_size() is MT safe
 *
 * @see #get_pw_buffer_size
 */
int
get_group_buffer_size() {
   constexpr int buf_size = 20480;
   int sz = buf_size;

#ifdef _SC_GETGR_R_SIZE_MAX
   if ((sz = (int) sysconf(_SC_GETGR_R_SIZE_MAX)) == -1) {
      sz = buf_size;
   } else {
      sz = std::max(sz, buf_size);
   }
#endif

   return sz;
}

/**
 * @brief Set uid and gid of calling process
 *
 * Set uid and gid of calling process. This can be done only by root.
 *
 * @param user ???
 * @param intermediate_user ???
 * @param min_gid ???
 * @param min_uid ???
 * @param add_grp ???
 * @param err_str ???
 * @param use_qsub_gid ???
 * @param qsub_gid ???
 * @param skip_silently skip silently when add_grp could not be added due to NGROUP_MAX limit
 *
 * @return error state 0 - OK -1 - we can't switch to user since we are not root 1 - we can't switch to user or we can't set add_grp 4 - switch to user failed, likely wrong password for this user
 *
 * @note MT-NOTE: sge_set_uid_gid_addgrp() is MT safe
 *
 *       TODO: This function needs to be rewritten from scratch! It calls
 *       'initgroups()' which is not part of POSIX. The call to 'initgroups()'
 *       shall be replaced by a combination of 'getgroups()/getegid()/setgid()'.
 *
 *       This function is used by 'shepherd' only anyway. Hence it shall be
 *       considered to move it from 'libuti' to 'shepherd'.
 */
static int
_sge_set_uid_gid_addgrp(const char *user, const char *intermediate_user, gid_t min_gid, uid_t min_uid, gid_t add_grp,
                        char *err_str, size_t err_str_size, int use_qsub_gid, gid_t qsub_gid, char *buffer, int size, bool skip_silently) {
   int status;
   struct passwd *pw;
   struct passwd pw_struct;
   gid_t old_grp_id;

   sge_switch2start_user();

   if (!sge_is_start_user_superuser()) {
      snprintf(err_str, err_str_size, SFNMAX, MSG_SYSTEM_CHANGEUIDORGIDFAILED);
      return -1;
   }

   if (intermediate_user) {
      user = intermediate_user;
   }

   if (!(pw = sge_getpwnam_r(user, &pw_struct, buffer, size))) {
      snprintf(err_str, err_str_size, MSG_SYSTEM_GETPWNAMFAILED_S, user);
      return 1;
   }

   /*
    * preserve the old primary gid for initgroups()
    * see cr 6590010
    */
   old_grp_id = pw->pw_gid;

   /*
    *  Should we use the primary group of qsub host? (qsub_gid)
    */
   if (use_qsub_gid) {
      pw->pw_gid = qsub_gid;
   }

   if (!intermediate_user) {
      /*
       *  It should not be necessary to set min_gid/min_uid to 0
       *  for being able to run prolog/epilog/pe_start/pe_stop
       *  as root
       */
      if (pw->pw_gid < min_gid) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_GIDLESSTHANMINIMUM_Sgg, user, pw->pw_gid, min_gid);
         return 1;
      }
      if (setgid(pw->pw_gid)) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_SETGIDFAILED_g, pw->pw_gid);
         return 1;
      }
   } else {
      if (setegid(pw->pw_gid)) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_SETEGIDFAILED_g, pw->pw_gid);
         return 1;
      }
   }

   status = initgroups(pw->pw_name, old_grp_id);

   /* Why am I doing it this way?  Good question,
      an even better question would be why vendors
      can't get their act together on what is returned,
      at least get it right in the man pages!
      on error heres what I get:
      (subject to change with OS releases)
      OS      return       errno
      SUNOS  -1            1
      SOLARIS-1
      UGH!!!
    */

   if (status) {
      snprintf(err_str, err_str_size, MSG_SYSTEM_INITGROUPSFAILED_I, status);
      return 1;
   }

#if defined(SOLARIS) || defined(LINUX) || defined(FREEBSD) || defined(DARWIN)
   /* add Additional group id to current list of groups */
   if (add_grp != 0) {
      if (sge_add_group(add_grp, err_str, err_str_size, skip_silently) == -1) {
         return 5;
      }
   }
#endif

   if (!intermediate_user) {
      if (pw->pw_uid < min_uid) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_UIDLESSTHANMINIMUM_Suu, user, pw->pw_uid, min_uid);
         return 1;
      }

      if (use_qsub_gid) {
         if (setgid(pw->pw_gid)) {
            snprintf(err_str, err_str_size, MSG_SYSTEM_SETGIDFAILED_g, pw->pw_gid);
            return 1;
         }
      }
      if (setuid(pw->pw_uid)) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_SETUIDFAILED_u, pw->pw_uid);
         return 1;
      }
   } else {
      if (use_qsub_gid) {
         if (setgid(pw->pw_gid)) {
            snprintf(err_str, err_str_size, MSG_SYSTEM_SETGIDFAILED_g, pw->pw_gid);
            return 1;
         }
      }

      if (seteuid(pw->pw_uid)) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_SETEUIDFAILED_u, pw->pw_uid);
         return 1;
      }
   }

   return 0;
}

/** @brief Switch the process to a user, its group and an additional group
 *
 * Resolves @p user, sets the group id, the supplementary group @p add_grp_id
 * and finally the user id, so the process gives up its privileges in the order
 * that cannot be undone.
 *
 * @param user the user to become
 * @param intermediate_user if not nullptr, the user to switch to first
 * @param min_gid lowest acceptable group id; smaller ones are rejected
 * @param min_uid lowest acceptable user id; smaller ones are rejected
 * @param add_grp supplementary group to add, 0 for none
 * @param err_str buffer receiving an error message
 * @param err_str_size size of @p err_str in bytes
 * @param use_qsub_gid use the group id configured for qsub instead of the
 *                     user's own
 * @param qsub_gid group id to use when @p use_qsub_gid is set
 * @param skip_silently do not log when the supplementary group is already set
 * @return 0 on success, a negative value on error, with @p err_str describing it
 *
 * @note MT-NOTE: sge_set_uid_gid_addgrp() is not MT safe
 */
int sge_set_uid_gid_addgrp(const char *user, const char *intermediate_user,
                           int min_gid, int min_uid, int add_grp, char *err_str, size_t err_str_size,
                           int use_qsub_gid, gid_t qsub_gid, bool skip_silently) {
   int size = get_pw_buffer_size();
   char *buffer = sge_malloc(size);
   SGE_ASSERT(buffer != nullptr);
   int ret = _sge_set_uid_gid_addgrp(user, intermediate_user, min_gid, min_uid, add_grp, err_str, err_str_size, use_qsub_gid,
                                     qsub_gid, buffer, size, skip_silently);
   sge_free(&buffer);
   return ret;
}


/**
 * @brief Add a gid to the list of additional group ids
 *
 * Add a gid to the list of additional group ids. If 'add_grp_id'
 * is 0 don't add value to group id list (but return successfully).
 * If an error occurs, a descriptive string will be written to
 * err_str.
 *
 * @param add_grp_id new gid; 0 means do nothing and succeed
 * @param err_str if not nullptr, an error description is written here
 * @param err_str_size size of @p err_str in bytes
 * @param skip_silently skip silently if setting the group is skipped because this would exceed the NGROUPS_MAX limit.
 *
 * @return error state 0 - Success -1 - Error
 *
 * @note MT-NOTE: sge_add_group() is MT safe
 */
int
sge_add_group(gid_t add_grp_id, char *err_str, size_t err_str_size, bool skip_silently) {
   uint32_t max_groups;
   gid_t *list;
   int groups;

   if (err_str != nullptr) {
      err_str[0] = 0;
   }

   if (add_grp_id == 0) {
      return 0;
   }

   max_groups = sge_sysconf(SGE_SYSCONF_NGROUPS_MAX);
   if (max_groups <= 0) {
      if (err_str != nullptr) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_uuS, getuid(), geteuid(), MSG_SYSTEM_INVALID_NGROUPS_MAX);
      }
      return -1;
   }

/*
 * INSURE detects a WRITE_OVERFLOW when getgroups was invoked (LINUX).
 * Is this a bug in the kernel or in INSURE?
 */
#if defined(LINUX)
   list = (gid_t *) sge_malloc(2 * max_groups * sizeof(gid_t));
#else
   list = (gid_t *) sge_malloc(max_groups * sizeof(gid_t));
#endif
   if (list == nullptr) {
      if (err_str != nullptr) {
         int error = errno;
         snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_uuS, getuid(), geteuid(), strerror(error));
      }
      return -1;
   }

   groups = getgroups(max_groups, list);
   if (groups == -1) {
      if (err_str != nullptr) {
         int error = errno;
         snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_uuS, getuid(), geteuid(), strerror(error));
      }
      sge_free(&list);
      return -1;
   }

   if (groups < (int) max_groups) {
      list[groups] = add_grp_id;
      groups++;
      groups = setgroups(groups, list);
      if (groups == -1) {
         if (err_str != nullptr) {
            int error = errno;
            snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_uuS, getuid(), geteuid(), strerror(error));
         }
         sge_free(&list);
         return -1;
      }
   } else if (!skip_silently) {
      if (err_str != nullptr) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_uuS, getuid(), geteuid(), MSG_SYSTEM_USER_HAS_TOO_MANY_GIDS);
      }
      sge_free(&list);
      return -1;
   } else {
      sge_free(&list);
      return 0;
   }
   sge_free(&list);
   return 0;
}

/**
 * @brief Return password file entry for a given user name
 *
 * Search user database for a name. This function is just a wrapper for
 * 'getpwnam_r()', taking into account some additional possible errors.
 * For a detailed description see 'getpwnam_r()' man page.
 *
 * @param name points to user name
 * @param pw points to structure which will be updated upon success
 * @param buffer points to memory referenced by 'pw'
 * @param bufsize size of @p buffer in bytes
 *
 * @return Pointer to entry matching user name upon success, nullptr otherwise.
 *
 * @note MT-NOTE: sge_getpwnam_r() is MT safe.
 */
struct passwd *
sge_getpwnam_r(const char *name, struct passwd *pw, char *buffer, size_t bufsize) {
   struct passwd *res = nullptr;
   int i = MAX_NIS_RETRIES;

   DENTER(UIDGID_LAYER);

   while (i-- && !res) {
      if (getpwnam_r(name, pw, buffer, bufsize, &res) != 0) {
         res = nullptr;
      }
   }

   /* sometime on failure struct is non nullptr but name is empty */
   if (res && !res->pw_name) {
      res = nullptr;
   }

   DRETURN(res);
} /* sge_getpwnam_r() */

/** @brief Return group information for a given group ID.
 *
 * Search account database for a group. This function is just a wrapper for
 * getgrgid_r(), taking into account some additional possible errors. For a
 * detailed description see getgrgid_r() man page.
 *
 *  @param gid group ID
 *  @param pg points to structure which will be updated upon success
 *  @param buffer points to memory referenced by 'pg'
 *  @param buffer_size size of @p buffer in bytes, grown when too small
 *  @param retries number of retries to connect to NIS/LDAP
 *  @return Pointer to entry matching group information upon success, nullptr otherwise.
 */
struct group *
sge_getgrgid_r(gid_t gid, struct group *pg, char **buffer, size_t *buffer_size, int retries) {
   DENTER(UIDGID_LAYER);

   // validate pointer parameter
   if (buffer == nullptr || buffer_size == nullptr) {
      DRETURN(nullptr);
   }

   // ensure that buffers are allocated
   if (*buffer == nullptr || *buffer_size == 0) {
      *buffer_size = get_group_buffer_size();
      *buffer = static_cast<char *>(sge_malloc(*buffer_size));
      SGE_ASSERT(*buffer != nullptr);
   }

   struct group *res = nullptr;
   while (retries-- && !res) {
      if (getgrgid_r(gid, pg, *buffer, *buffer_size, &res) != 0) {

         // check if buffer was too small
         if (errno == ERANGE) {
            retries++;
            *buffer_size += 1024;
            *buffer = static_cast<char *>(sge_realloc(*buffer, *buffer_size, 1));
            if (*buffer == nullptr) {
               res = nullptr;
               break;
            }
         } else {
            res = nullptr;
         }
      }
   }

   // Check the result (pointer and empty string)
   if (res && (res->gr_name == nullptr || res->gr_name[0] == '\0')) {
      res = nullptr;
   }

   DRETURN(res);
}

/**
 * @brief Check if provided user is the superuser
 *
 * Checks platform indepently if the provided user is the superuser.
 *
 * @param name name of the user to check
 *
 * @return true if it is the superuser, false if not.
 *
 * @note MT-NOTE: sge_is_user_superuser() is MT safe.
 */
bool
sge_is_user_superuser(const char *name) {
   return (strcmp(name, "root") == 0);
}

/**
 * @brief Set user and group id of admin user
 *
 * Set user and group id of admin user.
 *
 * @param theUID user id of admin user
 * @param theGID group id of admin user
 *
 * @return none
 *
 * @note MT-NOTE: set_admin_user() is MT safe.
 */
static void
set_admin_user(const char *user_name, uid_t theUID, gid_t theGID) {
   DENTER(UIDGID_LAYER);

   sge_mutex_lock("admin_user_mutex", __func__, __LINE__, &admin_user.mutex);
   admin_user.user_name = user_name;
   admin_user.uid = theUID;
   admin_user.gid = theGID;
   admin_user.initialized = true;
   sge_mutex_unlock("admin_user_mutex", __func__, __LINE__, &admin_user.mutex);

   DPRINTF("auid=%ld; agid=%ld\n", (long) theUID, (long) theGID);

   DRETURN_VOID;
} /* set_admin_user() */

/**
 * @brief Get user and group id of admin user
 *
 * Get user and group id of admin user. 'theUID' and 'theGID' will contain
 * the user and group id respectively, upon successful completion.
 *
 * If the admin user has not been set by a call to 'set_admin_user()'
 * previously, an error is returned. In case of an error, the locations
 * pointed to by 'theUID' and 'theGID' remain unchanged.
 *
 * @code
 * uid_t uid;
 * gid_t gid;
 *
 * if (get_admin_user(&uid, &gid) == ESRCH) {
 *    printf("error: no admin user\n");
 * } else {
 *    printf("uid = %d, gid =%d\n", (int)uid, (int)gid);
 * }
 * @endcode
 *
 * @param theUID pointer to user id storage.
 * @param theGID pointer to group id storage.
 *
 * @return Returns ESRCH, if no admin user has been initialized.
 *
 * @note MT-NOTE: get_admin_user() is MT safe.
 */
static int
get_admin_user(uid_t *theUID, gid_t *theGID) {
   DENTER(UIDGID_LAYER);

   sge_mutex_lock("admin_user_mutex", __func__, __LINE__, &admin_user.mutex);
   uid_t uid = admin_user.uid;
   gid_t gid = admin_user.gid;
   bool init = admin_user.initialized;
   sge_mutex_unlock("admin_user_mutex", __func__, __LINE__, &admin_user.mutex);

   int res = ESRCH;
   if (init) {
      *theUID = uid;
      *theGID = gid;
      res = 0;
   }

   DRETURN(res);
} /* get_admin_user() */

/**
 * @brief Returns the admin user name
 *
 * Returns the admin user name.
 *
 *
 * @return Admin user name
 *
 * @note MT-NOTE: get_admin_user_name() is MT safe
 */
const char *
get_admin_user_name() {
   return admin_user.user_name;
}

/**
 * @brief Is there a admin user configured and set
 *
 * Returns if there is a admin user setting configured and set.
 *
 *
 * @return result true  - there is a setting
 *
 * @note MT-NOTE: sge_has_admin_user() is MT safe
 */
bool
sge_has_admin_user() {
   DENTER(TOP_LAYER);
   uid_t uid;
   gid_t gid;
   DRETURN(!(get_admin_user(&uid, &gid) == ESRCH));
}

/**
 * @brief Returns supplementary groups of the executing user.
 *
 * Calling function is responsible to free grp_array.
 *
 * @param amount        of supplementary groups the user is part of
 * @param grp_array     containing elements with the grp id and name
 * @param err_str       variable where the function can store an error message
 * @param err_str_len   length of the error string buffer
 * @return              false in case on error or true in case of success
 *                      if true is returned the also amount and grp_array will be set.
 */
bool
ocs_get_groups(int *amount, ocs_grp_elem_t **grp_array, char *err_str, int err_str_len) {
   DENTER(TOP_LAYER);

   // check input parameter
   if (err_str == nullptr || err_str_len <= 0) {
      // nothing we can do here. caller should have specified the string.
      DRETURN(false);
   }
   if (amount == nullptr) {
      snprintf(err_str, err_str_len, "invalid input parameter (amount).");
      DRETURN(false);
   }
   if (grp_array == nullptr) {
      snprintf(err_str, err_str_len, "invalid input parameter (grp_array).");
      DRETURN(false);
   }

   // get maximum amount of supplementary group IDs
   int max_groups = static_cast<int>(sge_sysconf(SGE_SYSCONF_NGROUPS_MAX));
   if (max_groups == -1) {
      snprintf(err_str, err_str_len, "sge_sysconf(SGE_SYSCONF_NGROUPS_MAX) failed.");
      DRETURN(false);
   }

   // allocate buffer for group IDs
   auto *grp_id_list = reinterpret_cast<gid_t *>(sge_malloc(max_groups * sizeof(gid_t)));
   if (grp_id_list == nullptr) {
      snprintf(err_str, err_str_len, "Unable to allocate buffer that should hold group IDs");
      DRETURN(false);
   }

   // fetch group IDs
   int grp_ids = getgroups(max_groups, grp_id_list);
   if (grp_ids == -1) {
      snprintf(err_str, err_str_len, "getgroups() failed.");
      sge_free(&grp_id_list);
      DRETURN(false);
   }
   if (grp_ids == 0) {
      // success case: user has no supplementary groups
      *amount = 0;
      *grp_array = nullptr;
      sge_free(&grp_id_list);
      DRETURN(true);
   }

   // fetch group names and store them with corresponding IDs in the array to be returned
   auto array = reinterpret_cast<ocs_grp_elem_t *>(sge_malloc(grp_ids * sizeof(ocs_grp_elem_t)));
   if (array == nullptr) {
       snprintf(err_str, err_str_len, "Unable to allocate buffer that should hold group information");
       sge_free(&grp_id_list);
       DRETURN(false);
   }
   for (int i = 0; i < grp_ids; i++) {
      // try to get the name
      array[i].id = grp_id_list[i];
      int lret = sge_gid2group(grp_id_list[i], array[i].name, MAX_STRING_SIZE, 1);

      // non-resolvable groups are no error. also OCS uses GIDs without name for job tracing
      if (lret != 0) {
          snprintf(array[i].name, MAX_STRING_SIZE, gid_t_fmt, grp_id_list[i]);
      }
   }
   sge_free(&grp_id_list);
   *amount = grp_ids;
   *grp_array = array;
   DRETURN(true);
}

/**
 * @brief returns supplementary group ids for a specific user
 *
 * @param user user name
 * @param gid gid of the user
 * @param amount used to return the number of group ids
 * @param grp_array used to return the array of group ids
 * @param error_dstr dstring to return error messages
 * @return true if the supplementary group information could be retrieved, else false (and error_dstr contains the reason)
 */
bool
ocs_get_groups(const char *user, gid_t gid, int *amount, ocs_grp_elem_t **grp_array, dstring *error_dstr) {
   DENTER(TOP_LAYER);
   bool ret = true;

   if (amount == nullptr) {
      sge_dstring_sprintf(error_dstr, "invalid input parameter (amount).");
      DRETURN(false);
   }
   if (grp_array == nullptr) {
      sge_dstring_sprintf(error_dstr, "invalid input parameter (grp_array).");
      DRETURN(false);
   }

   // get maximum amount of supplementary group IDs
   int max_groups = static_cast<int>(sge_sysconf(SGE_SYSCONF_NGROUPS_MAX));
   if (max_groups == -1) {
      sge_dstring_sprintf(error_dstr, "sge_sysconf(SGE_SYSCONF_NGROUPS_MAX) failed.");
      DRETURN(false);
   }

   DPRINTF("max_groups=%d\n", max_groups);

   // allocate buffer for group IDs
   auto *grp_id_list = reinterpret_cast<gid_t *>(sge_malloc(max_groups * sizeof(gid_t)));
   if (grp_id_list == nullptr) {
      sge_dstring_sprintf(error_dstr, "Unable to allocate buffer that should hold group IDs");
      DRETURN(false);
   }

   // fetch group IDs
#ifdef DARWIN
   static_assert(sizeof(gid_t) == sizeof(int), "Size of gid_t does not match that of type int!");
   int num_group_ids = getgrouplist(user, static_cast<int>(gid), reinterpret_cast<int *>(grp_id_list), &max_groups);
#else
   int num_group_ids = getgrouplist(user, gid, grp_id_list, &max_groups);
#endif
   if (num_group_ids == -1) {
      sge_dstring_sprintf(error_dstr, "getgrouplist() failed.");
      sge_free(&grp_id_list);
      DRETURN(false);
   }

   DPRINTF("num_group_ids=%d\n", num_group_ids);

   if (num_group_ids == 0) {
      // success case: user has no supplementary groups (this case probably does not exist)
      *amount = 0;
      *grp_array = nullptr;
      sge_free(&grp_id_list);
      DRETURN(true);
   }

   // fetch group names and store them with corresponding IDs in the array to be returned
   auto array = reinterpret_cast<ocs_grp_elem_t *>(sge_malloc(num_group_ids * sizeof(ocs_grp_elem_t)));
   if (array == nullptr) {
       sge_dstring_sprintf(error_dstr, "Unable to allocate buffer that should hold group information");
       sge_free(&grp_id_list);
       DRETURN(false);
   }
   for (int i = 0; i < num_group_ids; i++) {
      // try to get the name
      array[i].id = grp_id_list[i];
      int lret = sge_gid2group(grp_id_list[i], array[i].name, MAX_STRING_SIZE, 1);

      // non-resolvable groups are no error. also OCS uses GIDs without name for job tracing
      if (lret != 0) {
          snprintf(array[i].name, MAX_STRING_SIZE, gid_t_fmt, grp_id_list[i]);
      }
   }
   sge_free(&grp_id_list);
   *amount = num_group_ids;
   *grp_array = array;
   DRETURN(ret);
}

/**
 * @brief Fills a dstring with the information about user, group, supplementary group's similar to the id-command.
 *
 * As sise effect the string will be printed to
 *
 * @param dstr       Dstring that will contain the information
 * @param uid        user ID
 * @param username   user name
 * @param gid        primary group ID
 * @param groupname  primary group name
 * @param amount     number of supplementary groups
 * @param grp_array  array with entries for each sup-grp (ID and name)
 */
void
ocs_id2dstring(dstring *dstr, uid_t uid, const char *username,
               gid_t gid, const char *groupname, int amount, ocs_grp_elem_t *grp_array) {
   DENTER(TOP_LAYER);
   sge_dstring_sprintf(dstr, "uid=" uid_t_fmt "(%s) gid=" gid_t_fmt "(%s) groups=", uid, username, gid, groupname);
   if (amount == 0) {
      sge_dstring_sprintf_append(dstr, "NONE\n");
   } else {
      bool is_first = true;
      for (int i = 0; i < amount; i++) {
         if (is_first) {
            is_first = false;
         } else {
            sge_dstring_append(dstr, ", ");
         }
         sge_dstring_sprintf_append(dstr, gid_t_fmt "(%s)", grp_array[i].id, grp_array[i].name);
      }
      sge_dstring_append_char(dstr, '\n');
   }
   DPRINTF("%s", sge_dstring_get_string(dstr));
   DRETURN_VOID;
}

/**
 * @brief Returns normalized value with passed value range
 *
 * The value passed is normalized and resulting value (0.0-1.0) is returned
 * The value range passed is assumed. In case there is no range because
 * min/max are (nearly) equal 0.5 is returned.
 *
 * @param value Value to be normalized.
 * @param range_min Range minimum value.
 * @param range_max Range maximum value.
 *
 * @return Normalized value (0.0-1.0)
 *
 * @note MT-NOTE: sge_normalize_value() is MT safe
 */
double sge_normalize_value(double value, double range_min, double range_max)
{
   if (range_max - range_min < std::numeric_limits<double>::epsilon())
      return 0.5;
   return (value - range_min) / (range_max - range_min);
}
