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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <fcntl.h>
#include <sys/time.h>

#include <string>
#include <vector>

#if defined(LINUX)

#  include <limits.h>

#endif

#include "uti/msg_utilib.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "basis_types.h"

typedef enum {
   FILE_TYPE_NOT_EXISTING,
   FILE_TYPE_FILE,
   FILE_TYPE_DIRECTORY
} file_type_t;

/* MT-NOTE: This module is MT safe */

static int sge_domkdir(const char *, int, bool, bool);

static file_type_t sge_get_file_type(const char *name);

static file_type_t sge_get_file_type(const char *name) {
   SGE_STRUCT_STAT stat_buffer{};
   file_type_t ret = FILE_TYPE_NOT_EXISTING;

   if (SGE_STAT(name, &stat_buffer)) {
      ret = FILE_TYPE_NOT_EXISTING;
   } else {
      if (S_ISDIR(stat_buffer.st_mode)) {
         ret = FILE_TYPE_DIRECTORY;
      } else if (S_ISREG(stat_buffer.st_mode)) {
         ret = FILE_TYPE_FILE;
      } else {
         ret = FILE_TYPE_NOT_EXISTING;
      }
   }
   return ret;
}

static int sge_domkdir(const char *path_, int fmode, bool exit_on_error, bool may_not_exist) {
   SGE_STRUCT_STAT stat_buffer{};

   DENTER(TOP_LAYER);

   if (mkdir(path_, (mode_t) fmode)) {
      if (errno == EEXIST) {
         if (may_not_exist) {
            DRETURN(-1);
         } else {
            DRETURN(0);
         }
      }

      if (!SGE_STAT(path_, &stat_buffer) && S_ISDIR(stat_buffer.st_mode)) {
         /*
          * may be we do not have permission, 
          * but directory already exists 
          */
         DRETURN(0);
      }

      if (exit_on_error) {
         CRITICAL(MSG_FILE_CREATEDIRFAILED_SS, path_, strerror(errno));
         sge_exit(1);
      } else {
         ERROR(MSG_FILE_CREATEDIRFAILED_SS, path_, strerror(errno));
         DRETURN(-1);
      }
   }

   DRETURN(0);
}

/****** uti/unistd/sge_unlink() ***********************************************
*  NAME
*     sge_unlink() -- delete a name and possibly the file it refers to
*
*  SYNOPSIS
*     bool sge_unlink(const char *prefix, const char *suffix) 
*
*  FUNCTION
*     Replacement for unlink(). 'prefix' and 'suffix' will be combined
*     to a filename. This file will be deleted. 'prefix' may be nullptr.
*
*  INPUTS
*     const char *prefix - pathname or nullptr
*     const char *suffix - filename 
*
*  RESULT
*     int - error state
*         true  - OK
*         false - Error
******************************************************************************/
bool sge_unlink(const char *prefix, const char *suffix) {
   int status;
   stringT str;

   DENTER(TOP_LAYER);

   if (!suffix) {
      ERROR(SFNMAX, MSG_POINTER_SUFFIXISNULLINSGEUNLINK);
      DRETURN(false);
   }

   if (prefix) {
      snprintf(str, sizeof(str), "%s/%s", prefix, suffix);
   } else {
      snprintf(str, sizeof(str), "%s", suffix);
   }

   DPRINTF("file to unlink: \"%s\"\n", str);
   status = unlink(str);

   if (status) {
      ERROR(MSG_FILE_UNLINKFAILED_SS, str, strerror(errno));
      DRETURN(false);
   } else {
      DRETURN(true);
   }
}

/****** uti/unistd/sge_sleep() ************************************************
*  NAME
*     sge_sleep() -- sleep for x microseconds 
*
*  SYNOPSIS
*     void sge_sleep(int sec, int usec) 
*
*  FUNCTION
*     Delays the calling application for 'sec' seconds and 'usec'
*     microseconds 
*
*  INPUTS
*     int sec  - seconds 
*     int usec - microseconds 
******************************************************************************/
void sge_sleep(int sec, int usec) {
   struct timeval timeout{};

   timeout.tv_sec = sec;
   timeout.tv_usec = usec;

   select(0, (fd_set *) nullptr, (fd_set *) nullptr, (fd_set *) nullptr, &timeout);
}

/****** uti/unistd/sge_chdir_exit() *******************************************
*  NAME
*     sge_chdir_exit() -- Replacement for chdir() 
*
*  SYNOPSIS
*     int sge_chdir_exit(const char *path, int exit_on_error) 
*
*  FUNCTION
*     Change working directory 
*
*  INPUTS
*     const char *path  - pathname 
*     int exit_on_error - exit in case of errors 
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - ERROR ('exit_on_error'==1 the function may not return)
*
*  SEE ALSO
*     uti/unistd/sge_chdir()
******************************************************************************/
int sge_chdir_exit(const char *path, int exit_on_error) {
   DENTER(BASIS_LAYER);

   if (chdir(path)) {
      if (exit_on_error) {
         CRITICAL(MSG_FILE_NOCDTODIRECTORY_S, path);
         sge_exit(1);
      } else {
         ERROR(MSG_FILE_NOCDTODIRECTORY_S, path);
         return -1;
      }
   }

   DRETURN(0);
}

/****** uti/unistd/sge_chdir() ************************************************
*  NAME
*     sge_chdir() --  Replacement for chdir()
*
*  SYNOPSIS
*     int sge_chdir(const char *dir) 
*
*  FUNCTION
*     Change working directory 
*
*  INPUTS
*     const char *dir - pathname 
*
*  RESULT
*     int - error state
*        0 - success
*        != 0 - error 
*
*  NOTE
*     Might be used in shepherd because it does not use CRITICAL/ERROR.
*     TODO: pass a dstring for the return of error messages.
*
*  SEE ALSO
*     uti/unistd/sge_chdir_exit()
******************************************************************************/
int sge_chdir(const char *dir) {
   if (dir != nullptr) {
      SGE_STRUCT_STAT stat_buffer{};

      /*
       * force automount
       */
      SGE_STAT(dir, &stat_buffer);
      return chdir(dir);
   }

   /* on error return -1 */
   return -1;
}

/****** uti/unistd/sge_exit() *************************************************
*  NAME
*     sge_exit() -- Wrapped exit Function 
*
*  SYNOPSIS
*     void sge_exit(int i) 
*
*  FUNCTION
*     Calls 'exit_func' if installed. Stops monitoring with DCLOSE 
*
*  INPUTS
*     ocs::gdi::Client::sge_gdi_ctx_class_t **ref_ctx - address of the context, the context is freed in exit_func
*     int i          - exit state 
*
*  SEE ALSO
*     uti/unistd/sge_install_exit_func()
******************************************************************************/
void sge_exit(int i) {
   sge_exit_func_t exit_func = component_get_exit_func();
   if (exit_func) {
      exit_func(i);
   }
   exit(i);
}


/****** uti/unistd/sge_mkdir() ************************************************
*  NAME
*     sge_mkdir() -- Create a directory (and subdirectorys)  
*
*  SYNOPSIS
*     int sge_mkdir(const char *path, int fmode, bool exit_on_error, bool may_not_exist) 
*
*  FUNCTION
*     Create a directory 
*
*  INPUTS
*     const char *path  - path 
*     int fmode         - file mode 
*     bool exit_on_error - as it says 
*     bool may_not_exist - if true an error is returned if the last component
*                         of the path exists
*
*  RESULT
*     int - error state
*         0 - OK
*        -1 - Error (The function may never return)
******************************************************************************/
int sge_mkdir(const char *path, int fmode, bool exit_on_error, bool may_not_exist) {
   int i = 0;
   stringT path_;

   DENTER(TOP_LAYER);
   if (path == nullptr) {
      if (exit_on_error) {
         CRITICAL(SFNMAX, MSG_VAR_PATHISNULLINSGEMKDIR);
         sge_exit(1);
      } else {
         ERROR(SFNMAX, MSG_VAR_PATHISNULLINSGEMKDIR);
         DRETURN(-1);
      }
   }

   DPRINTF("Making dir \"%s\"\n", path);

   memset(path_, 0, sizeof(path_));
   while ((unsigned char) path[i]) {
      path_[i] = path[i];
      if ((path[i] == '/') && (i != 0)) {
         path_[i] = (unsigned char) 0;
         int res = sge_domkdir(path_, fmode, exit_on_error, false);
         if (res) {
            DPRINTF("retval = %d\n", res);
            DRETURN(res);
         }
      }
      path_[i] = path[i];
      i++;
   }

   i = sge_domkdir(path_, fmode, exit_on_error, may_not_exist);

   DPRINTF("retval = %d\n", i);
   DRETURN(i);
}

int sge_mkdir2(const char *base_dir, const char *name, int fmode, bool exit_on_error) {
   dstring path = DSTRING_INIT;
   int ret;

   DENTER(TOP_LAYER);

   if (base_dir == nullptr || name == nullptr) {
      if (exit_on_error) {
         CRITICAL(SFNMAX, MSG_VAR_PATHISNULLINSGEMKDIR);
         sge_exit(1);
      } else {
         ERROR(SFNMAX, MSG_VAR_PATHISNULLINSGEMKDIR);
         DRETURN(-1);
      }
   }

   sge_dstring_sprintf(&path, "%s/%s", base_dir, name);

   ret = sge_mkdir(sge_dstring_get_string(&path), fmode, exit_on_error, false);
   sge_dstring_free(&path);

   DRETURN(ret);
}

/**
 * @brief Create a directory and hand it over to a user without following a symbolic link
 *
 * Meant for a directory at a predictable path in a parent that users can write to, created by a
 * privileged caller, like the job's TMPDIR made by execd as root (CS-3203). The directory is
 * created with mkdir(), opened with O_DIRECTORY | O_NOFOLLOW and handed over with fchown() and
 * fchmod() on the descriptor, so whatever a user planted at the path cannot redirect the change:
 * - a symbolic link, a regular file, a fifo or any other non-directory is removed with unlink(),
 *   which does not follow a link, with a warning, and the directory is created once more;
 * - an existing directory is used when it belongs to the caller (the effective user id) or
 *   already to @p uid, for example the directory left behind by an earlier attempt of the same job;
 * - a directory of any other user is renamed aside within the same parent
 *   (`<path>.stale.<pid>.<time>`, rename() does not follow a link either), with a warning that names
 *   both paths and the uid and gid of the directory, and the directory is created once more. Nothing
 *   here removes it: the caller does that with ocs_moved_aside_paths() and sge_rmdir() once the
 *   directory at @p path is no longer needed (CS-3221).
 *
 * A failing mkdir() (e.g. EACCES or ENOSPC) and a failing fchown() or fchmod() are problems of the
 * host and give #OCS_MKDIR_FOR_OWNER_ERROR: the latter two act on the descriptor of a directory
 * that was verified to belong to the caller or to @p uid, so a user cannot make them fail, but the
 * environment can (an NFS export with root_squash, a filesystem without ownership). Everything
 * else that goes wrong after mkdir() succeeded or found the path occupied is caused by what is at
 * the path and gives #OCS_MKDIR_FOR_OWNER_PLANTED, also when the path is occupied again after
 * every attempt.
 *
 * @param path the directory to create
 * @param fmode the mode of the directory, set with fchmod(), so the umask does not apply
 * @param uid the new owner
 * @param gid the new group
 * @param[out] error destination for an error message, may be nullptr
 * @param before_attempt called before each attempt with its number, starting at 0, may be nullptr;
 *                       a seam for module tests that occupy the path again between attempts
 * @return #OCS_MKDIR_FOR_OWNER_OK when @p path is a directory owned by @p uid and @p gid,
 *         #OCS_MKDIR_FOR_OWNER_PLANTED when what is at @p path keeps it from being one,
 *         #OCS_MKDIR_FOR_OWNER_ERROR when mkdir() fails for another reason or the directory cannot
 *         be handed over with fchown() or fchmod()
 */
int ocs_mkdir_for_owner(const char *path, const int fmode, const uid_t uid, const gid_t gid, dstring *error,
                        const std::function<void(int)> &before_attempt) {
   DENTER(TOP_LAYER);

   if (path == nullptr) {
      sge_dstring_sprintf(error, MSG_POINTER_NULLPARAMETER);
      DRETURN(OCS_MKDIR_FOR_OWNER_ERROR);
   }

   // one attempt, a second one after a planted entry was removed or moved aside, a third one when
   // the entry vanished or was swapped in between
   constexpr int max_attempts = 3;
   for (int attempt = 0; attempt < max_attempts; attempt++) {
      if (before_attempt) {
         before_attempt(attempt);
      }

      const bool created = mkdir(path, static_cast<mode_t>(fmode)) == 0;
      if (!created && errno != EEXIST) {
         const int saved_errno = errno;
         sge_dstring_sprintf(error, MSG_FILE_CREATEDIRFAILED_SS, path, strerror(saved_errno));
         DRETURN(OCS_MKDIR_FOR_OWNER_ERROR);
      }

      // O_NOFOLLOW | O_DIRECTORY: a symbolic link or a non-directory at the path is not opened,
      // O_NONBLOCK: a fifo does not block the open
      const int fd = SGE_OPEN2(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
      if (fd < 0) {
         const int open_errno = errno;
         sge_dstring_sprintf(error, MSG_FILE_OPENDIRECTORYFAILED_SS, path, strerror(open_errno));
         if (open_errno == ENOENT) {
            // removed in between, try again
            continue;
         }
         // ELOOP (a symbolic link), EMLINK (a symbolic link on FreeBSD), ENOTDIR (any other non-directory)
         if (open_errno != ELOOP && open_errno != EMLINK && open_errno != ENOTDIR) {
            break;
         }
         WARNING(MSG_FILE_REMOVINGPLANTED_S, path);
         if (unlink(path) != 0 && errno != ENOENT) {
            const int unlink_errno = errno;
            sge_dstring_sprintf(error, MSG_FILE_UNLINKFAILED_SS, path, strerror(unlink_errno));
            break;
         }
         continue;
      }

      SGE_STRUCT_STAT stat_buffer{};
      if (SGE_FSTAT(fd, &stat_buffer) != 0) {
         const int saved_errno = errno;
         sge_dstring_sprintf(error, MSG_FILE_STATFAILED_SS, path, strerror(saved_errno));
         close(fd);
         break;
      }
      if (stat_buffer.st_uid != geteuid() && stat_buffer.st_uid != uid) {
         // a directory of another user: keep it, but out of the way
         close(fd);
         DSTRING_STATIC(aside, SGE_PATH_MAX);
         sge_dstring_sprintf(&aside, "%s" OCS_MOVED_ASIDE_INFIX "%d.%ld", path, static_cast<int>(getpid()),
                             static_cast<long>(time(nullptr)));
         WARNING(MSG_FILE_MOVINGASIDE_SSUU, path, sge_dstring_get_string(&aside), stat_buffer.st_uid,
                 stat_buffer.st_gid);
         if (rename(path, sge_dstring_get_string(&aside)) != 0 && errno != ENOENT) {
            const int rename_errno = errno;
            sge_dstring_sprintf(error, MSG_FILE_RENAMEFAILED_SSS, path, sge_dstring_get_string(&aside),
                                strerror(rename_errno));
            break;
         }
         sge_dstring_sprintf(error, MSG_FILE_DIRNOTOURS_S, path);
         continue;
      }

      // the directory is verified, a failure from here on is one of the host, not of what is at the path
      int ret = OCS_MKDIR_FOR_OWNER_OK;
      if (fchown(fd, uid, gid) != 0) {
         const int saved_errno = errno;
         sge_dstring_sprintf(error, MSG_FILE_FCHOWNFAILED_SS, path, strerror(saved_errno));
         ret = OCS_MKDIR_FOR_OWNER_ERROR;
      } else if (fchmod(fd, static_cast<mode_t>(fmode)) != 0) {
         const int saved_errno = errno;
         sge_dstring_sprintf(error, MSG_FILE_FCHMODFAILED_SS, path, strerror(saved_errno));
         ret = OCS_MKDIR_FOR_OWNER_ERROR;
      }
      close(fd);
      if (ret != OCS_MKDIR_FOR_OWNER_OK && created) {
         // rmdir() removes neither a symbolic link nor a directory that is not empty
         rmdir(path);
      }
      DRETURN(ret);
   }

   DRETURN(OCS_MKDIR_FOR_OWNER_PLANTED);
}

/**
 * @brief Does a name follow the pattern ocs_mkdir_for_owner() gives a directory it moved aside?
 *
 * @param name the name of a directory entry
 * @param prefix the name of the directory that was to be created, followed by #OCS_MOVED_ASIDE_INFIX
 * @return true when @p name is @p prefix followed by `<digits>.<digits>` and nothing else
 */
static bool ocs_is_moved_aside_name(const char *name, const std::string &prefix) {
   if (strncmp(name, prefix.c_str(), prefix.size()) != 0) {
      return false;
   }
   const char *p = name + prefix.size();
   // <pid>
   const char *start = p;
   while (*p >= '0' && *p <= '9') {
      p++;
   }
   if (p == start || *p != '.') {
      return false;
   }
   // <time>
   start = ++p;
   while (*p >= '0' && *p <= '9') {
      p++;
   }
   return p != start && *p == '\0';
}

/**
 * @brief Find the directories ocs_mkdir_for_owner() moved aside from a path
 *
 * The name of a directory moved aside is derived from @p path (`<path>.stale.<pid>.<time>`), so the
 * caller does not have to remember it: the parent of @p path is searched for entries of exactly that
 * pattern, which also finds those moved aside by an earlier process, e.g. before a restart of execd.
 * Only directories are listed; every entry is examined with fstatat(AT_SYMLINK_NOFOLLOW), so a
 * symbolic link or another non-directory that bears such a name is never listed. The caller removes
 * the directories with sge_rmdir(), which does not follow a symbolic link either, also not one that
 * replaces a listed directory in between (CS-3221).
 *
 * The name alone does not prove that a directory was moved aside: another directory may carry the
 * same name, e.g. the live TMPDIR `<job>.<task>.<queue>` of a queue named like `all.q.stale.5.6` next
 * to that of `all.q`. ocs_mkdir_for_owner() moves a directory aside only when it belongs to a user
 * other than @p owner, so a directory that belongs to @p owner is never listed.
 *
 * @param path the directory that was to be created
 * @param owner the user the directory at @p path was created for; a directory of this user is not listed
 * @param[out] paths receives the full paths of the directories moved aside, in no particular order
 * @param[out] error destination for an error message, may be nullptr
 * @return 0 on success, also when the parent does not exist, -1 when the parent cannot be read
 */
int ocs_moved_aside_paths(const char *path, const uid_t owner, std::vector<std::string> &paths, dstring *error) {
   DENTER(TOP_LAYER);

   paths.clear();
   if (path == nullptr) {
      sge_dstring_sprintf(error, MSG_POINTER_NULLPARAMETER);
      DRETURN(-1);
   }

   const std::string full{path};
   const size_t slash = full.rfind('/');
   std::string parent = ".";
   std::string prefix = full;
   if (slash != std::string::npos) {
      parent = slash == 0 ? "/" : full.substr(0, slash);
      prefix = full.substr(slash + 1);
   }
   prefix += OCS_MOVED_ASIDE_INFIX;

   const int parent_fd = SGE_OPEN2(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
   if (parent_fd < 0) {
      const int open_errno = errno;
      if (open_errno == ENOENT) {
         DRETURN(0);
      }
      sge_dstring_sprintf(error, MSG_FILE_OPENDIRFAILED_SS, parent.c_str(), strerror(open_errno));
      DRETURN(-1);
   }
   DIR *dir = fdopendir(parent_fd);
   if (dir == nullptr) {
      const int saved_errno = errno;
      sge_dstring_sprintf(error, MSG_FILE_OPENDIRFAILED_SS, parent.c_str(), strerror(saved_errno));
      close(parent_fd);
      DRETURN(-1);
   }

   const std::string separator = parent == "/" ? "" : "/";
   const dirent *entry;
   while ((entry = readdir(dir)) != nullptr) {
      if (!ocs_is_moved_aside_name(entry->d_name, prefix)) {
         continue;
      }
      SGE_STRUCT_STAT stat_buffer{};
      if (SGE_FSTATAT(dirfd(dir), entry->d_name, &stat_buffer, AT_SYMLINK_NOFOLLOW) == 0 &&
          S_ISDIR(stat_buffer.st_mode) && stat_buffer.st_uid != owner) {
         paths.push_back(parent + separator + entry->d_name);
      }
   }
   closedir(dir);

   DRETURN(0);
}

/** @brief The deepest directory level below its top that sge_rmdir() descends into */
static constexpr size_t OCS_RMDIR_MAX_DEPTH = 4096;

/**
 * @brief Remove everything inside an open directory without following symbolic links
 *
 * Every entry is examined with fstatat(AT_SYMLINK_NOFOLLOW) and removed relative to its open
 * parent with unlinkat(), a subdirectory is opened with openat(O_DIRECTORY | O_NOFOLLOW) and
 * emptied the same way. Nothing is ever looked up by its full path, so a directory that is
 * replaced by a symbolic link while the tree is removed is not followed: the open fails and the
 * removal stops with an error, the link's target is not touched.
 *
 * The walk keeps its state on the heap, one open directory per level, so the depth of the tree
 * does not depend on the stack of the caller. A tree deeper than #OCS_RMDIR_MAX_DEPTH levels, or
 * one that needs more descriptors than the process may open, is an error; whatever was not
 * removed by then stays in place.
 *
 * @param top_fd descriptor of the directory, opened with O_DIRECTORY and O_NOFOLLOW;
 *               it is always closed when the function returns
 * @param path path of the directory, used in error messages only
 * @param error destination for an error message, may be nullptr
 * @return 0 when the directory is empty afterwards, -1 on error
 */
static int ocs_rmdir_contents(const int top_fd, const char *path, dstring *error) {
   DENTER(TOP_LAYER);

   /** @brief One open directory of the walk */
   struct Level {
      DIR *dir;            ///< the open directory
      std::string name;    ///< its name in the parent directory, the path for the top
   };

   DIR *top = fdopendir(top_fd);
   if (top == nullptr) {
      sge_dstring_sprintf(error, MSG_FILE_OPENDIRFAILED_SS, path, strerror(errno));
      close(top_fd);
      DRETURN(-1);
   }

   std::vector<Level> levels;
   levels.push_back({top, path});

   // the path of an entry of the deepest open directory, built for error messages only
   auto path_of = [&levels](const char *name) {
      std::string result;
      for (const auto &level : levels) {
         result += level.name;
         result += '/';
      }
      return result + name;
   };

   int ret = 0;
   while (ret == 0 && !levels.empty()) {
      const int dir_fd = dirfd(levels.back().dir);
      const SGE_STRUCT_DIRENT *dent = SGE_READDIR(levels.back().dir);
      if (dent == nullptr) {
         // the directory is empty now: close it and remove it from its parent
         const std::string name = levels.back().name;
         closedir(levels.back().dir);
         levels.pop_back();
         if (!levels.empty() && unlinkat(dirfd(levels.back().dir), name.c_str(), AT_REMOVEDIR) != 0) {
            const int saved_errno = errno;
            sge_dstring_sprintf(error, MSG_FILE_RMDIRFAILED_SS, path_of(name.c_str()).c_str(), strerror(saved_errno));
            ret = -1;
         }
         continue;
      }

      const char *name = dent->d_name;
      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
         continue;
      }

      SGE_STRUCT_STAT stat_buffer{};
      if (SGE_FSTATAT(dir_fd, name, &stat_buffer, AT_SYMLINK_NOFOLLOW) != 0) {
         const int saved_errno = errno;
         sge_dstring_sprintf(error, MSG_FILE_STATFAILED_SS, path_of(name).c_str(), strerror(saved_errno));
         ret = -1;
      } else if (S_ISDIR(stat_buffer.st_mode)) {
         if (levels.size() > OCS_RMDIR_MAX_DEPTH) {
            sge_dstring_sprintf(error, MSG_FILE_RMDIRTOODEEP_IS, static_cast<int>(OCS_RMDIR_MAX_DEPTH), path);
            ret = -1;
            continue;
         }
         // O_NOFOLLOW: a directory swapped for a symbolic link since the fstatat() is not entered
         const int sub_fd = openat(dir_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
         DIR *sub = sub_fd < 0 ? nullptr : fdopendir(sub_fd);
         if (sub == nullptr) {
            const int saved_errno = errno;
            sge_dstring_sprintf(error, MSG_FILE_OPENDIRFAILED_SS, path_of(name).c_str(), strerror(saved_errno));
            if (sub_fd >= 0) {
               close(sub_fd);
            }
            ret = -1;
         } else {
            levels.push_back({sub, name});
         }
      } else {
         // a symbolic link is removed itself, never its target
         DPRINTF("sge_rmdir: unlink %s\n", name);
         if (unlinkat(dir_fd, name, 0) != 0) {
            const int saved_errno = errno;
            sge_dstring_sprintf(error, MSG_FILE_UNLINKFAILED_SS, path_of(name).c_str(), strerror(saved_errno));
            ret = -1;
         }
      }
   }

   for (const auto &level : levels) {
      closedir(level.dir);
   }
   DRETURN(ret);
}

/**
 * @brief Remove a directory, optionally with everything it contains
 *
 * Unless @p recursive is true, only an empty directory can be removed. With @p recursive the
 * directory and all files and directories it contains are removed.
 *
 * The removal never follows a symbolic link, neither at @p cp itself nor anywhere inside the
 * tree: a symbolic link inside the tree is removed, its target stays untouched, and a
 * symbolic link at @p cp is an error. The tree is walked without recursion, with descriptors (openat(),
 * fstatat(), unlinkat() relative to the open parent), so a directory that is replaced by a
 * symbolic link while the tree is removed cannot redirect the removal to the link's target,
 * even when the caller runs as root on a tree that belongs to another user (CS-3202).
 *
 * @param[in] cp path of the directory to remove
 * @param[out] error destination for an error message, may be nullptr
 * @param[in] recursive if true (default), remove the directory with its contents
 * @param[in] ignore_notempty if true, a directory that is not empty at the final rmdir() is
 *            no error
 * @return 0 on success, -1 on error
 */
int sge_rmdir(const char *cp, dstring *error, const bool recursive, const bool ignore_notempty) {
   DENTER(TOP_LAYER);

   if (cp == nullptr) {
      sge_dstring_sprintf(error, MSG_POINTER_NULLPARAMETER);
      DRETURN(-1);
   }

   if (recursive) {
      const int dir_fd = SGE_OPEN2(cp, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      if (dir_fd < 0) {
         sge_dstring_sprintf(error, MSG_FILE_OPENDIRFAILED_SS, cp, strerror(errno));
         DRETURN(-1);
      }
      if (ocs_rmdir_contents(dir_fd, cp, error) != 0) {
         DRETURN(-1);
      }
   }

   if (rmdir(cp) != 0) {
      if (errno == ENOTEMPTY && ignore_notempty) {
         DPRINTF("rmdir %s: not empty, ignoring\n", cp);
      } else {
         sge_dstring_sprintf(error, MSG_FILE_RMDIRFAILED_SS, cp, strerror(errno));
         DRETURN(-1);
      }
   }

   DRETURN(0);
}

int sge_is_executable(const char *name) {
   SGE_STRUCT_STAT stat_buffer{};
   int ret = SGE_STAT(name, &stat_buffer);
   if (!ret) {
      return (int) (stat_buffer.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
   } else {
      return 0;
   }
}


/****** uti/unistd/sge_is_directory() *****************************************
*  NAME
*     sge_is_directory() -- Does 'name' exist and is it a directory? 
*
*  SYNOPSIS
*     int sge_is_directory(const char *name) 
*
*  FUNCTION
*     Does 'name' exist and is it a directory?  
*
*  INPUTS
*     const char *name  - directory name 
*
*  RESULT
*     int - error state
*         0 - No
*         1 - Yes 
******************************************************************************/
int sge_is_directory(const char *name) {
   return (sge_get_file_type(name) == FILE_TYPE_DIRECTORY);
}

/****** uti/unistd/sge_is_file() **********************************************
*  NAME
*     sge_is_file() -- Does 'name' exist and is it a file? 
*
*  SYNOPSIS
*     int sge_is_file(const char *name) 
*
*  FUNCTION
*     Does 'name' exist and is it a file?  
*
*  INPUTS
*     const char *name  - filename 
*
*  RESULT
*     int - error state
*         0 - No
*         1 - Yes 
******************************************************************************/
int sge_is_file(const char *name) {
   return (sge_get_file_type(name) == FILE_TYPE_FILE);
}

/****** uti/unistd/sge_sysconf() **********************************************
*  NAME
*     sge_sysconf() -- Replacement for sysconf 
*
*  SYNOPSIS
*     u_long32 sge_sysconf(sge_sysconf_t id)
*
*  FUNCTION
*     Replacement for sysconf  
*
*  INPUTS
*     sge_sysconf_t id - value 
*
*  RESULT
*     u_long32 - meaning depends on 'id' 
*
*  SEE ALSO
*     uti/unistd/sge_sysconf_t
******************************************************************************/
u_long32 sge_sysconf(sge_sysconf_t id) {
   u_long32 ret = 0;

   DENTER(BASIS_LAYER);
   switch (id) {
      case SGE_SYSCONF_NGROUPS_MAX:
         ret = sysconf(_SC_NGROUPS_MAX);
         break;
      default:
         CRITICAL(MSG_SYSCONF_UNABLETORETRIEVE_I, (int) id);
         break;
   }
   DRETURN(ret);
}
