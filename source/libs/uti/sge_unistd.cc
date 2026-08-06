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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Filesystem and process helpers around the unistd calls
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sys/time.h>

#if defined(LINUX)

#  include <limits.h>

#endif

#include "uti/msg_utilib.h"
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include <cinttypes>

/** @brief What a path refers to */
typedef enum {
   FILE_TYPE_NOT_EXISTING,   ///< the path does not exist
   FILE_TYPE_FILE,           ///< the path is a regular file
   FILE_TYPE_DIRECTORY       ///< the path is a directory
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

/**
 * @brief Delete a name and possibly the file it refers to
 *
 * Replacement for unlink(). 'prefix' and 'suffix' will be combined
 * to a filename. This file will be deleted. 'prefix' may be nullptr.
 *
 * @param prefix pathname or nullptr
 * @param suffix filename
 *
 * @return error state true  - OK false - Error
 */
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

/**
 * @brief Sleep for x microseconds
 *
 * Delays the calling application for 'sec' seconds and 'usec'
 * microseconds
 *
 * @param sec seconds
 * @param usec microseconds
 */
void sge_sleep(int sec, int usec) {
   struct timeval timeout{};

   timeout.tv_sec = sec;
   timeout.tv_usec = usec;

   select(0, (fd_set *) nullptr, (fd_set *) nullptr, (fd_set *) nullptr, &timeout);
}

/**
 * @brief Replacement for chdir()
 *
 * Change working directory
 *
 * @param path pathname
 * @param exit_on_error exit in case of errors
 *
 * @return error state 0 - OK -1 - ERROR ('exit_on_error'==1 the function may not return)
 *
 * @see #sge_chdir
 */
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

/**
 * @brief Replacement for chdir()
 *
 * Change working directory
 *
 * @param dir pathname
 *
 * @return error state 0 - success != 0 - error
 *
 * @note Might be used in shepherd because it does not use CRITICAL/ERROR.
 *       TODO: pass a dstring for the return of error messages.
 *
 * @see #sge_chdir_exit
 */
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

/**
 * @brief Wrapped exit Function
 *
 * Calls 'exit_func' if installed. Stops monitoring with DCLOSE
 *
 * @param i exit state
 *
 * @see `sge_install_exit_func()`
 */
void sge_exit(int i) {
   sge_exit_func_t exit_func = component_get_exit_func();
   if (exit_func) {
      exit_func(i);
   }
   exit(i);
}


/**
 * @brief Create a directory (and subdirectorys)
 *
 * Create a directory
 *
 * @param path path
 * @param fmode file mode
 * @param exit_on_error as it says
 * @param may_not_exist if true an error is returned if the last component of the path exists
 *
 * @return error state 0 - OK -1 - Error (The function may never return)
 */
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

/** @brief Create a directory below an existing base directory
 *
 * Convenience wrapper around #sge_mkdir that joins @p base_dir and @p name.
 *
 * @param base_dir the existing parent directory
 * @param name name of the directory to create below @p base_dir
 * @param fmode permissions of the new directory
 * @param exit_on_error terminate the process when the directory cannot be created
 * @return 0 on success, -1 on error
 *
 * @note MT-NOTE: sge_mkdir2() is MT safe
 */
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
 * Remove a directory tree. In case of errors a message may be found in
 * 'error' afterwards.
 * Unless parameter 'recursive' is set to true, only empty directories can be
 * deleted. If 'recursive' is set to true, the directory and all files and directories
 * it contains will be deleted.
 *
 * @param[in] cp path to the directory to be deleted
 * @param[in] error destination for error message if non-nullptr
 * @param[in] recursive if true (default), delete the directory recursively
 * @param[in] ignore_notempty
 * @return 0 on success, -1 on error
 */
int sge_rmdir(const char *cp, dstring *error, bool recursive, bool ignore_notempty) {
   DENTER(TOP_LAYER);

   if (cp == nullptr) {
      sge_dstring_sprintf(error, MSG_POINTER_NULLPARAMETER);
      DRETURN(-1);
   }

   if (recursive) {
      DIR *dir = opendir(cp);
      if (dir == nullptr) {
         sge_dstring_sprintf(error, MSG_FILE_OPENDIRFAILED_SS, cp, strerror(errno));
         DRETURN(-1);
      }

      SGE_STRUCT_DIRENT *dent;
      char dirent[SGE_PATH_MAX * 2];
      while (SGE_READDIR_R(dir, (SGE_STRUCT_DIRENT *) dirent, &dent) == 0 && dent != nullptr) {
         if (strcmp(dent->d_name, ".") != 0 && strcmp(dent->d_name, "..") != 0) {

            char fname[SGE_PATH_MAX];
            snprintf(fname, sizeof(fname), "%s/%s", cp, dent->d_name);

            SGE_STRUCT_STAT stat_buffer{};
            if (SGE_LSTAT(fname, &stat_buffer)) {
               sge_dstring_sprintf(error, MSG_FILE_STATFAILED_SS, fname, strerror(errno));
               closedir(dir);
               DRETURN(-1);
            }

            if (S_ISDIR(stat_buffer.st_mode) && !S_ISLNK(stat_buffer.st_mode)) {
               if (sge_rmdir(fname, error)) {
                  sge_dstring_sprintf(error, MSG_FILE_RECURSIVERMDIRFAILED);
                  closedir(dir);
                  DRETURN(-1);
               }
            } else {
#ifdef TEST
               printf("unlink %s\n", fname);
#else
               DPRINTF("sge_rmdir: unlink %s\n", fname);
               if (unlink(fname)) {
                  sge_dstring_sprintf(error, MSG_FILE_UNLINKFAILED_SS,
                                      fname, strerror(errno));
                  closedir(dir);
                  DRETURN(-1);
               }
#endif
            }
         }
      }

      closedir(dir);
   }

#ifdef TEST
   printf("rmdir %s\n", cp);
#else
   if (rmdir(cp) != 0) {
      if (errno == ENOTEMPTY && ignore_notempty) {
         DPRINTF("rmdir %s: not empty, ignoring\n", cp);
      } else {
         sge_dstring_sprintf(error, MSG_FILE_RMDIRFAILED_SS, cp, strerror(errno));
         DRETURN(-1);
      }
   }
#endif

   return 0;
}

/** @brief Is the path an executable file?
 *
 * @param name the path to check
 * @return 1 when @p name exists and is executable by the calling user, else 0
 *
 * @note MT-NOTE: sge_is_executable() is MT safe
 */
int sge_is_executable(const char *name) {
   SGE_STRUCT_STAT stat_buffer{};
   int ret = SGE_STAT(name, &stat_buffer);
   if (!ret) {
      return (int) (stat_buffer.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
   } else {
      return 0;
   }
}


/**
 * @brief Does 'name' exist and is it a directory?
 *
 * Does 'name' exist and is it a directory?
 *
 * @param name directory name
 *
 * @return error state 0 - No 1 - Yes
 */
int sge_is_directory(const char *name) {
   return (sge_get_file_type(name) == FILE_TYPE_DIRECTORY);
}

/**
 * @brief Does 'name' exist and is it a file?
 *
 * Does 'name' exist and is it a file?
 *
 * @param name filename
 *
 * @return error state 0 - No 1 - Yes
 */
int sge_is_file(const char *name) {
   return (sge_get_file_type(name) == FILE_TYPE_FILE);
}

/**
 * @brief Replacement for sysconf
 *
 * Replacement for sysconf
 *
 * @param id value
 *
 * @return meaning depends on 'id'
 */
uint32_t sge_sysconf(sge_sysconf_t id) {
   uint32_t ret = 0;

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

#ifdef TEST
int main(int argc, char **argv)
{
   char err_str[1024];
 
   if (argc!=2) {
      fprintf(stderr, "usage: rmdir <dir>\n");
      exit(1);
   }
   if (sge_rmdir(argv[1], err_str)) {
      fprintf(stderr, "%s", err_str);
      return 1;
   }
   return 0;
}
#endif   
