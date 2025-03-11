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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/*
 *   Parts of the code have been contributed by and are copyright of
 *   Tommy Karlsson <tommy.karlsson@bolero.se>
 */

#include "uti/sge_uidgid.h"

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
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

#include "basis_types.h"

#include "msg_common.h"

#define UIDGID_LAYER CULL_LAYER
#define MAX_LINE_LENGTH 10000

enum {
   SGE_MAX_USERGROUP_BUF = 255
};

typedef struct {
   pthread_mutex_t mutex;
   const char *user_name;
   uid_t uid;
   gid_t gid;
   bool initialized;
} admin_user_t;

static admin_user_t admin_user = {PTHREAD_MUTEX_INITIALIZER, nullptr, (uid_t) -1, (gid_t) -1, false};

static void set_admin_user(const char *user_name, uid_t, gid_t);

static int get_admin_user(uid_t *, gid_t *);

/**
 * \brief Return true/false if current real user is superuser (root/Administrator).
 *
 * \details
 * Check the real user id to determine if it is the superuser. If so, return
 * true, else return false. This function relies on getuid == 0 for UNIX.
 * Other members of the Administrators group do not have the permission
 * to "su" without password!
 *
 * \return
 * true - root was start user
 * false - otherwise
 */
bool
sge_is_start_user_superuser() {
   DENTER(UIDGID_LAYER);
   bool is_root = (getuid() == SGE_SUPERUSER_UID);
   DRETURN(is_root);
}

/****** uti/uidgid/sge_set_admin_username() ***********************************
*  NAME
*     sge_set_admin_username() -- Set SGE/EE admin user
*
*  SYNOPSIS
*     int sge_set_admin_username(const char *user, char *err_str)
*
*  FUNCTION
*     Set SGE/EE admin user. If 'user' is "none" then use the current
*     uid/gid. Ignore if current user is not root.
*
*  INPUTS
*     const char *user - admin user name
*     char *err_str    - error message
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Username does not exist
*        -2 - Admin user was already set
******************************************************************************/
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

/****** uti/uidgid/sge_switch2admin_user() ************************************
*  NAME
*     sge_switch2admin_user() -- Set euid/egid to admin uid/gid
*
*  SYNOPSIS
*     int sge_switch2admin_user()
*
*  FUNCTION
*     Set euid/egid to admin uid/gid. Silently ignore if our uid
*     is not root. Do nothing if out euid/egid is already the admin
*     uid/gid. If the admin user was not set with
*     sge_set_admin_username() the function will not return.
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - setegid()/seteuid() fails
*
*  NOTES
*     MT-NOTE: sge_switch2admin_user() is MT safe.
*
*  SEE ALSO
*     uti/uidgid/sge_switch2admin_user()
*     uti/uidgid/sge_set_admin_username()
*     uti/uidgid/sge_switch2start_user()
*     uti/uidgid/sge_run_as_user()
******************************************************************************/
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
      abort();
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

/****** uti/uidgid/sge_switch2start_user() ************************************
*  NAME
*     sge_switch2start_user() -- set euid/egid to start uid/gid
*
*  SYNOPSIS
*     int sge_switch2start_user()
*
*  FUNCTION
*     Set euid/egid to the uid/gid of that user which started the
*     application which calles this function. If our euid/egid is
*     already the start uid/gid don't do anything. If the admin user
*     was not set with sge_set_admin_username() the function will
*     not return.
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - setegid()/seteuid() fails
*
*  NOTES
*     MT-NOTE: sge_switch2start_user() is MT safe.
*
*  SEE ALSO
*     uti/uidgid/sge_switch2admin_user()
*     uti/uidgid/sge_set_admin_username()
*     uti/uidgid/sge_switch2start_user()
*     uti/uidgid/sge_run_as_user()
******************************************************************************/
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
      abort();
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

/****** uti/uidgid/sge_user2uid() *********************************************
*  NAME
*     sge_user2uid() -- resolves user name to uid and gid 
*
*  SYNOPSIS
*     int sge_user2uid(const char *user, uid_t *puid, gid_t *pgid, int retries) 
*
*  FUNCTION
*     Resolves a username ('user') to it's uid (stored in 'puid') and
*     it's primary gid (stored in 'pgid').
*     'retries' defines the number of (e.g. NIS/DNS) retries.
*     If 'puid' is nullptr the user name is resolved without saving it.
*
*  INPUTS
*     const char *user - username 
*     uid_t *puid      - uid pointer 
*     gid_t *pgid      - gid pointer
*     int retries      - number of retries 
*
*  NOTES
*     MT-NOTE: sge_user2uid() is MT safe.
*
*  RESULT
*     int - exit state 
*         0 - OK
*         1 - Error
******************************************************************************/
int
sge_user2uid(const char *user, uid_t *puid, uid_t *pgid, int retries) {
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

/****** uti/uidgid/sge_group2gid() ********************************************
*  NAME
*     sge_group2gid() -- Resolve a group name to its gid 
*
*  SYNOPSIS
*     int sge_group2gid(const char *gname, gid_t *gidp, int retries) 
*
*  FUNCTION
*     Resolves a groupname ('gname') to its gid (stored in 'gidp').
*     'retries' defines the number of (e.g. NIS/DNS) retries.
*     If 'gidp' is nullptr the group name is resolved without saving it.
*
*  INPUTS
*     const char *gname - group name 
*     gid_t *gidp       - gid pointer 
*     int retries       - number of retries  
*
*  NOTES
*     MT-NOTE: sge_group2gid() is MT safe.
*
*  RESULT
*     int - exit state 
*         0 - OK
*         1 - Error
******************************************************************************/
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
            size += 1024;
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

/****** uti/uidgid/sge_uid2user() *********************************************
*  NAME
*     sge_uid2user() -- Resolves uid to user name. 
*
*  SYNOPSIS
*     int sge_uid2user(uid_t uid, char *dst, size_t sz, int retries) 
*
*  FUNCTION
*     Resolves uid to user name. if 'dst' is nullptr the function checks
*     only if the uid is resolvable. 
*
*  INPUTS
*     uid_t uid   - user id 
*     char *dst   - buffer for the username 
*     size_t sz   - buffersize 
*     int retries - number of retries 
*
*  NOTES
*     MT-NOTE: sge_uid2user() is MT safe.
*
*  RESULT
*     int - error state
*         0 - OK
*         1 - Error
******************************************************************************/
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
         ERROR(MSG_SYSTEM_GETPWUIDFAILED_US, static_cast<u_long32>(uid), strerror(errno));
         sge_free(&buffer);
         DRETURN(1);
      }
      sleep(1);
   }
   sge_strlcpy(dst, pw->pw_name, sz);

   sge_free(&buffer);

   DRETURN(0);
} /* sge_uid2user() */

/****** uti/uidgid/sge_gid2group() ********************************************
*  NAME
*     sge_gid2group() -- Resolves gid to group name. 
*
*  SYNOPSIS
*     int sge_gid2group(gid_t gid, char *dst, size_t sz, int retries) 
*
*  FUNCTION
*     Resolves gid to group name. if 'dst' is nullptr the function checks
*     only if the gid is resolvable. 
*
*  INPUTS
*     uid_t gid   - group id 
*     char *dst   - buffer for the group name 
*     size_t sz   - buffersize 
*     int retries - number of retries 
*
*  NOTES
*     MT-NOTE: sge_gid2group() is MT safe.
*
*  RESULT
*     int - error state
*         0 - OK
*         1 - Error
******************************************************************************/
int
sge_gid2group(gid_t gid, char *dst, size_t sz, int retries) {
   struct group *gr;
   struct group gr_entry {};
   char *buf = nullptr;
   int size = 0;

   DENTER(UIDGID_LAYER);

   size = get_group_buffer_size();
   buf = sge_malloc(size);
   SGE_ASSERT(buf != nullptr);

   gr = sge_getgrgid_r(gid, &gr_entry, buf, size, retries);
   // TODO: We need to handle the case when the OS is unable to resolve the GID to a name.
   if (gr == nullptr) {
      sge_free(&buf);
      DRETURN(1);
   }

   /* cache group name */
   sge_strlcpy(dst, gr->gr_name, sz);
   sge_free(&buf);

   DRETURN(0);
}

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

/****** uti/uidgid/get_pw_buffer_size() ****************************************
*  NAME
*     get_pw_buffer_size() -- get the buffer size required for getpw*_r
*
*  SYNOPSIS
*     int get_pw_buffer_size() 
*
*  FUNCTION
*     Returns the buffer size required for functions like getpwnam_r.
*     It can either be retrieved via sysconf, or a bit (20k) buffer
*     size is taken.
*
*  RESULT
*     int - buffer size in bytes
*
*  NOTES
*     MT-NOTE: get_pw_buffer_size() is MT safe 
*
*  SEE ALSO
*     uti/uidgid/get_group_buffer_size()
*******************************************************************************/
int
get_pw_buffer_size() {
   static const int buf_size = 20480;

   int sz = buf_size;

#ifdef _SC_GETPW_R_SIZE_MAX
   if ((sz = (int) sysconf(_SC_GETPW_R_SIZE_MAX)) == -1) {
      sz = buf_size;
   } else {
      sz = MAX(sz, buf_size);
   }
#endif

   return sz;
}

/****** uti/uidgid/get_group_buffer_size() ****************************************
*  NAME
*     get_group_buffer_size() -- get the buffer size required for getgr*_r
*
*  SYNOPSIS
*     int get_group_buffer_size() 
*
*  FUNCTION
*     Returns the buffer size required for functions like getgrnam_r.
*     It can either be retrieved via sysconf, or a bit (20k) buffer
*     size is taken.
*
*  RESULT
*     int - buffer size in bytes
*
*  NOTES
*     MT-NOTE: get_group_buffer_size() is MT safe 
*
*  SEE ALSO
*     uti/uidgid/get_pw_buffer_size()
*******************************************************************************/
int
get_group_buffer_size() {
   enum {
      buf_size = 20480
   };  /* default is 20 KB */

   int sz = buf_size;

#ifdef _SC_GETGR_R_SIZE_MAX
   if ((sz = (int) sysconf(_SC_GETGR_R_SIZE_MAX)) == -1) {
      sz = buf_size;
   } else {
      sz = MAX(sz, buf_size);
   }
#endif

   return sz;
}

/****** uti/uidgid/sge_set_uid_gid_addgrp() ***********************************
*  NAME
*     sge_set_uid_gid_addgrp() -- Set uid and gid of calling process
*
*  SYNOPSIS
*     int sge_set_uid_gid_addgrp(const char *user, 
*                                const char *intermediate_user,
*                                int min_gid, int min_uid, int add_grp,
*                                char *err_str, int use_qsub_gid, 
*                                gid_t qsub_gid)
*
*  FUNCTION
*     Set uid and gid of calling process. This can be done only by root.
*
*  INPUTS
*     const char *user              - ???
*     const char *intermediate_user - ???
*     int min_gid                   - ???
*     int min_uid                   - ???
*     int add_grp                   - ???
*     char *err_str                 - ???
*     int use_qsub_gid              - ???
*     gid_t qsub_gid                - ???
*     bool skip_silently            - skip silently when add_grp could not 
*                                     be added due to NGROUP_MAX limit
*
*  NOTES
*     MT-NOTE: sge_set_uid_gid_addgrp() is MT safe
*
*     TODO: This function needs to be rewritten from scratch! It calls
*     'initgroups()' which is not part of POSIX. The call to 'initgroups()'
*     shall be replaced by a combination of 'getgroups()/getegid()/setgid()'.
*      
*     This function is used by 'shepherd' only anyway. Hence it shall be
*     considered to move it from 'libuti' to 'shepherd'.
* 
*  RESULT
*     int - error state
*         0 - OK
*        -1 - we can't switch to user since we are not root
*         1 - we can't switch to user or we can't set add_grp
*         4 - switch to user failed, likely wrong password for this user
******************************************************************************/
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
         snprintf(err_str, err_str_size, MSG_SYSTEM_GIDLESSTHANMINIMUM_SUI, user, static_cast<u_long32>(pw->pw_gid), min_gid);
         return 1;
      }
      if (setgid(pw->pw_gid)) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_SETGIDFAILED_U, static_cast<u_long32>(pw->pw_gid));
         return 1;
      }
   } else {
      if (setegid(pw->pw_gid)) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_SETEGIDFAILED_U, static_cast<u_long32>(pw->pw_gid));
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
   if (add_grp) {
      if (sge_add_group(add_grp, err_str, err_str_size, skip_silently) == -1) {
         return 5;
      }
   }
#endif

   if (!intermediate_user) {
      if (pw->pw_uid < min_uid) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_UIDLESSTHANMINIMUM_SUI, user, static_cast<u_long32>(pw->pw_uid), min_uid);
         return 1;
      }

      if (use_qsub_gid) {
         if (setgid(pw->pw_gid)) {
            snprintf(err_str, err_str_size, MSG_SYSTEM_SETGIDFAILED_U, static_cast<u_long32>(pw->pw_gid));
            return 1;
         }
      }
      if (setuid(pw->pw_uid)) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_SETUIDFAILED_U, static_cast<u_long32>(pw->pw_uid));
         return 1;
      }
   } else {
      if (use_qsub_gid) {
         if (setgid(pw->pw_gid)) {
            snprintf(err_str, err_str_size, MSG_SYSTEM_SETGIDFAILED_U, static_cast<u_long32>(pw->pw_gid));
            return 1;
         }
      }

      if (seteuid(pw->pw_uid)) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_SETEUIDFAILED_U, static_cast<u_long32>(pw->pw_uid));
         return 1;
      }
   }

   return 0;
}

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


/****** uti/uidgid/sge_add_group() ********************************************
*  NAME
*     sge_add_group() -- Add a gid to the list of additional group ids
*
*  SYNOPSIS
*     int sge_add_group(gid_t add_grp_id, char *err_str)
*
*  FUNCTION
*     Add a gid to the list of additional group ids. If 'add_grp_id' 
*     is 0 don't add value to group id list (but return successfully).
*     If an error occurs, a descriptive string will be written to 
*     err_str.
*
*  INPUTS
*     gid_t add_grp_id   - new gid
*     char *err_str      - if points to a valid string buffer
*                          error descriptions 
*                          will be written here
*     bool skip_silently - skip silently if setting the group is skipped
*                          because this would exceed the NGROUPS_MAX limit.
*
*  NOTE
*     MT-NOTE: sge_add_group() is MT safe
*
*  RESULT
*     int - error state
*         0 - Success
*        -1 - Error
******************************************************************************/
int
sge_add_group(gid_t add_grp_id, char *err_str, size_t err_str_size, bool skip_silently) {
   u_long32 max_groups;
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
         snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_UUS, static_cast<u_long32>(getuid()),
                  static_cast<u_long32>(geteuid()), MSG_SYSTEM_INVALID_NGROUPS_MAX);
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
         snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_UUS, static_cast<u_long32>(getuid()), static_cast<u_long32>(geteuid()), strerror(error));
      }
      return -1;
   }

   groups = getgroups(max_groups, list);
   if (groups == -1) {
      if (err_str != nullptr) {
         int error = errno;
         snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_UUS, static_cast<u_long32>(getuid()), static_cast<u_long32>(geteuid()), strerror(error));
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
            snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_UUS, static_cast<u_long32>(getuid()), static_cast<u_long32>(geteuid()), strerror(error));
         }
         sge_free(&list);
         return -1;
      }
   } else if (!skip_silently) {
      if (err_str != nullptr) {
         snprintf(err_str, err_str_size, MSG_SYSTEM_ADDGROUPIDFORSGEFAILED_UUS, static_cast<u_long32>(getuid()), static_cast<u_long32>(geteuid()), MSG_SYSTEM_USER_HAS_TOO_MANY_GIDS);
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

/****** uti/uidgid/sge_getpwnam_r() ********************************************
*  NAME
*     sge_getpwnam_r() -- Return password file entry for a given user name. 
*
*  SYNOPSIS
*     struct passwd* sge_getpwnam_r(const char*, struct passwd*, char*, int) 
*
*  FUNCTION
*     Search user database for a name. This function is just a wrapper for
*     'getpwnam_r()', taking into account some additional possible errors.
*     For a detailed description see 'getpwnam_r()' man page.
*
*  INPUTS
*     const char *name  - points to user name 
*     struct passwd *pw - points to structure which will be updated upon success 
*     char *buffer      - points to memory referenced by 'pw'
*     size_t buflen     - size of 'buffer' in bytes 
*
*  RESULT
*     struct passwd* - Pointer to entry matching user name upon success,
*                      nullptr otherwise.
*
*  NOTES
*     MT-NOTE: sge_getpwnam_r() is MT safe. 
*
*******************************************************************************/
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


/****** uti/uidgid/sge_getgrgid_r() ********************************************
*  NAME
*     sge_getgrgid_r() -- Return group informations for a given group ID.
*
*  SYNOPSIS
*     struct group* sge_getgrgid_r(gid_t gid, struct group *pg,
*                                  char *buffer, size_t bufsize, int retires)
*
*  FUNCTION
*     Search account database for a group. This function is just a wrapper for
*     'getgrgid_r()', taking into account some additional possible errors.
*     For a detailed description see 'getgrgid_r()' man page.
*
*  INPUTS
*     gid_t gid         - group ID
*     struct group *pg  - points to structure which will be updated upon success
*     char *buffer      - points to memory referenced by 'pg'
*     size_t buflen     - size of 'buffer' in bytes 
*     int retries       - number of retries to connect to NIS
*
*  RESULT
*     struct group*  - Pointer to entry matching group informations upon success,
*                      nullptr otherwise.
*
*  NOTES
*     MT-NOTE: sge_getpwnam_r() is MT safe. 
*
*******************************************************************************/
struct group *
sge_getgrgid_r(gid_t gid, struct group *pg, char *buffer, size_t buffer_size, int retries) {
   struct group *res = nullptr;

   DENTER(UIDGID_LAYER);

   while (retries-- && !res) {
      if (getgrgid_r(gid, pg, buffer, buffer_size, &res) != 0) {
         if (errno == ERANGE) {
            retries++;
            buffer_size += 1024;
            buffer = (char *) sge_realloc(buffer, buffer_size, 1);
         }
         res = nullptr;
      }
   }

   /* could be that struct is not nullptr but group nam is empty */
   if (res && !res->gr_name) {
      res = nullptr;
   }

   DRETURN(res);
} /* sge_getgrgid_r() */

/****** uti/uidgid/sge_is_user_superuser() *************************************
*  NAME
*     sge_is_user_superuser() -- check if provided user is the superuser
*
*  SYNOPSIS
*     bool sge_is_user_superuser(const char *name); 
*
*  FUNCTION
*     Checks platform indepently if the provided user is the superuser.  
*
*  INPUTS
*     const char *name - name of the user to check
*
*  RESULT
*     bool - true if it is the superuser,
*            false if not.
*
*  NOTES
*     MT-NOTE: sge_is_user_superuser() is MT safe. 
*
*******************************************************************************/
bool
sge_is_user_superuser(const char *name) {
   return (strcmp(name, "root") == 0);
}

/****** uti/uidgid/set_admin_user() ********************************************
*  NAME
*     set_admin_user() -- Set user and group id of admin user. 
*
*  SYNOPSIS
*     static void set_admin_user(uid_t theUID, gid_t theGID) 
*
*  FUNCTION
*     Set user and group id of admin user. 
*
*  INPUTS
*     uid_t theUID - user id of admin user 
*     gid_t theGID - group id of admin user 
*
*  RESULT
*     static void - none
*
*  NOTES
*     MT-NOTE: set_admin_user() is MT safe. 
*
*******************************************************************************/
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

/****** uti/uidgid/get_admin_user() ********************************************
*  NAME
*     get_admin_user() -- Get user and group id of admin user.
*
*  SYNOPSIS
*     static int get_admin_user(uid_t* theUID, gid_t* theGID) 
*
*  FUNCTION
*     Get user and group id of admin user. 'theUID' and 'theGID' will contain
*     the user and group id respectively, upon successful completion.
*
*     If the admin user has not been set by a call to 'set_admin_user()'
*     previously, an error is returned. In case of an error, the locations
*     pointed to by 'theUID' and 'theGID' remain unchanged.
*
*  OUTPUTS
*     uid_t* theUID - pointer to user id storage.
*     gid_t* theGID - pointer to group id storage.
*
*  RESULT
*     int - Returns ESRCH, if no admin user has been initialized. 
*
*  EXAMPLE
*
*     uid_t uid;
*     gid_t gid;
*
*     if (get_admin_user(&uid, &gid) == ESRCH) {
*        printf("error: no admin user\n");
*     } else {
*        printf("uid = %d, gid =%d\n", (int)uid, (int)gid);
*     }
*       
*  NOTES
*     MT-NOTE: get_admin_user() is MT safe.
*
*******************************************************************************/
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

/****** uti/uidgid/get_admin_user_name() ***************************************
*  NAME
*     get_admin_user_name() -- Returns the admin user name
*
*  SYNOPSIS
*     const char* get_admin_user_name() 
*
*  FUNCTION
*     Returns the admin user name. 
*
*  INPUTS
*     void - None 
*
*  RESULT
*     const char* - Admin user name
*
*  NOTES
*     MT-NOTE: get_admin_user_name() is MT safe 
*******************************************************************************/
const char *
get_admin_user_name() {
   return admin_user.user_name;
}

/****** uti/uidgid/sge_has_admin_user() ****************************************
*  NAME
*     sge_has_admin_user() -- is there a admin user configured and set
*
*  SYNOPSIS
*     bool sge_has_admin_user() 
*
*  FUNCTION
*     Returns if there is a admin user setting configured and set. 
*
*  INPUTS
*     void - None 
*
*  RESULT
*     bool - result
*        true  - there is a setting
*
*  NOTES
*     MT-NOTE: sge_has_admin_user() is MT safe 
*******************************************************************************/
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

   // allocate buffer for group IDs
   auto *grp_id_list = reinterpret_cast<gid_t *>(sge_malloc(max_groups * sizeof(gid_t)));
   if (grp_id_list == nullptr) {
      sge_dstring_sprintf(error_dstr, "Unable to allocate buffer that should hold group IDs");
      DRETURN(false);
   }

   // fetch group IDs
   int num_group_ids = getgrouplist(user, gid, grp_id_list, &max_groups);
   if (num_group_ids == -1) {
      sge_dstring_sprintf(error_dstr, "getgrouplist() failed.");
      sge_free(&grp_id_list);
      DRETURN(false);
   }
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
      // skip the primary group
      if (grp_id_list[i] == gid) {
         continue;
      }
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


