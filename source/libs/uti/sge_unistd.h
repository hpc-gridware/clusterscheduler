#pragma once
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

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <cinttypes>

#include "sge_dstring.h"

/** @name Portability wrappers
 *
 * Each of these resolves to the spelling the current platform needs, so the
 * rest of the code does not have to know which one that is.
 * @{
 */
/** @def SGE_OPEN2
 * @brief open a file, using the 64 bit variant where the platform needs it
 */
/** @def SGE_OPEN3
 * @brief open or create a file, using the 64 bit variant where the platform needs it
 */
/** @def SGE_STAT
 * @brief stat a path, 64 bit safe
 */
/** @def SGE_LSTAT
 * @brief stat a path without following symlinks, 64 bit safe
 */
/** @def SGE_FSTAT
 * @brief stat an open file descriptor, 64 bit safe
 */
/** @def SGE_READDIR
 * @brief read the next directory entry
 */
/** @def SGE_READDIR_R
 * @brief read the next directory entry, reentrant variant
 */
/** @def SGE_TELLDIR
 * @brief current position within a directory stream
 */
/** @def SGE_SEEKDIR
 * @brief seek within a directory stream
 */
/** @def SETPGRP
 * @brief put the process into its own process group
 */
/** @def GETPGRP
 * @brief process group of the calling process
 */
/** @def SGE_STRUCT_STAT
 * @brief stat structure, 64 bit safe on every platform
 */
/** @def SGE_INO_T
 * @brief inode number type, 64 bit safe on every platform
 */
/** @def SGE_OFF_T
 * @brief file offset type, 64 bit safe on every platform
 */
/** @def SGE_STRUCT_DIRENT
 * @brief directory entry structure, 64 bit safe on every platform
 */
/** @} */

#if defined(SOLARIS) || defined(LINUX)
#  define SGE_OPEN2(filename, oflag)       open64(filename, oflag)
#  define SGE_OPEN3(filename, oflag, mode) open64(filename, oflag, mode)
#else
#  define SGE_OPEN2(filename, oflag)       open(filename, oflag)
#  define SGE_OPEN3(filename, oflag, mode) open(filename, oflag, mode)
#endif

/// close a file descriptor
#define SGE_CLOSE(fd) close(fd);

#if defined(SOLARIS)
#  define SGE_STAT(filename, buffer) stat64(filename, buffer)
#  define SGE_LSTAT(filename, buffer) lstat64(filename, buffer)
#  define SGE_FSTAT(filename, buffer) fstat64(filename, buffer)
#  define SGE_STRUCT_STAT struct stat64
#  define SGE_INO_T ino64_t
#  define SGE_OFF_T off64_t
#else
#  define SGE_STAT(filename, buffer) stat(filename, buffer)
#  define SGE_LSTAT(filename, buffer) lstat(filename, buffer)
#  define SGE_FSTAT(filename, buffer) fstat(filename, buffer)
#  define SGE_STRUCT_STAT struct stat
#  define SGE_INO_T ino_t
#  define SGE_OFF_T off_t
#endif

#if defined(SOLARIS) || defined(LINUX)
#  define SGE_READDIR(directory) readdir64(directory)
#  define SGE_READDIR_R(directory, entry, result) readdir64_r(directory, entry, result)
#  define SGE_TELLDIR(directory) telldir64(directory)
#  define SGE_SEEKDIR(directory, offset) seekdir64(directory, offset)
#  define SGE_STRUCT_DIRENT struct dirent64
#else
#  define SGE_READDIR(directory) readdir(directory)
#  define SGE_READDIR_R(directory, entry, result) readdir_r(directory, entry, result)
#  define SGE_TELLDIR(directory) telldir(directory)
#  define SGE_SEEKDIR(directory, offset) seekdir(directory, offset)
#  define SGE_STRUCT_DIRENT struct dirent
#endif

#if defined(SOLARIS) || defined(LINUX) || defined(DARWIN10)
#   define SETPGRP setpgrp()
#else
#   define SETPGRP setpgrp(getpid(),getpid())
#endif

#define GETPGRP getpgrp()

void sge_exit(int i);

int sge_chdir_exit(const char *path, int exit_on_error);

int sge_chdir(const char *dir);

int sge_mkdir(const char *path, int fmode, bool exit_on_error, bool may_not_exist);

int sge_mkdir2(const char *base_dir, const char *name, int fmode, bool exit_on_error);

int sge_rmdir(const char *cp, dstring *err_str, bool recursive = true, bool ignore_notempty = false);

bool sge_unlink(const char *prefix, const char *suffix);

int sge_is_directory(const char *name);

int sge_is_file(const char *name);

int sge_is_executable(const char *name);


void sge_sleep(int sec, int usec);

/**
 * @brief Constants for sge_sysconf()
 *
 * SGE_SYSCONF_NGROUPS_MAX - Maximum number of additional group ids
 *                           which are allowed per user
 *
 * @see #sge_sysconf
 */
typedef enum {
   SGE_SYSCONF_NGROUPS_MAX   ///< largest number of supplementary groups a process may have
} sge_sysconf_t;

uint32_t sge_sysconf(sge_sysconf_t id);

double sge_normalize_value(double value, double range_min, double range_max);
