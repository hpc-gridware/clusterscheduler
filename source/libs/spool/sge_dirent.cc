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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Reading directory entries, for the file based spooling backends
 */

#include <cstring>
#include <cerrno>

#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "uti/sge_log.h"
#include "uti/sge_unistd.h"
#include "uti/msg_utilib.h"

#include "sgeobj/sge_str.h"

#include "spool/sge_dirent.h"

#include "msg_common.h"

/**
 * @brief Read the entries of a directory
 *
 * `.` and `..` are left out. Used by the classic and flatfile backends to
 * walk a spool directory, and by `spooldefaults`.
 *
 * @param path the directory to read
 *
 * @return the entries as an `ST_Type` list, to be freed by the caller, or
 *         nullptr on any error and for an empty directory - the two cases
 *         are not distinguishable from the return value
 *
 * @warning The `opendir()` error path leaves through a plain `return` rather
 *          than `DRETURN`, so it skips the `rmon_mexit()` that #DENTER's
 *          `rmon_menter()` paired with. With tracing enabled the trace
 *          indentation never comes back down after a failed open.
 */
lList *sge_get_dirents(const char *path) {
   DENTER(TOP_LAYER);

   lList *entries = nullptr;
   DIR *cwd;
   SGE_STRUCT_DIRENT *dent;
   char dirent[SGE_PATH_MAX*2];

   cwd = opendir(path);

   if (cwd == (DIR *) 0) {
      ERROR(MSG_FILE_CANTOPENDIRECTORYX_SS, path, strerror(errno));
      return (nullptr);
   }

   while (SGE_READDIR_R(cwd, (SGE_STRUCT_DIRENT *)dirent, &dent)==0 && dent!=nullptr) {
      if (!dent->d_name[0])
         continue;             
      if (strcmp(dent->d_name, "..") == 0 || strcmp(dent->d_name, ".") == 0)
         continue;
      lAddElemStr(&entries, ST_name, dent->d_name, ST_Type);
   }
   closedir(cwd);

   DRETURN(entries);
}

/**
 * @brief Count the entries of a directory
 *
 * @param directory_name the directory to count
 *
 * @return the number of entries, 0 for an empty or unreadable directory
 *
 * @warning No caller in the source tree.
 * @note The loop skips `.` and `..` again, although #sge_get_dirents has
 *       already dropped both - the condition is always true.
 */
uint32_t sge_count_dirents(char *directory_name) {
   uint32_t entries = 0;
   lList *dir_entries = sge_get_dirents(directory_name);
   for_each_ep_lv(dir_entry, dir_entries) {
      const char *entry = lGetString(dir_entry, ST_name);
      if (strcmp(entry, ".") && strcmp(entry, "..")) {
         entries++;
      }
   }
   lFreeList(&dir_entries);
   return entries;
}

/**
 * @brief Ask whether a directory holds more than a given number of entries
 *
 * Stops as soon as the answer is known, so it is cheaper than
 * #sge_count_dirents for large directories - except that it builds the full
 * entry list first anyway.
 *
 * @param directory_name  the directory to look at
 * @param number_of_entries the number to compare against
 *
 * @return 1 if the directory holds more than `number_of_entries` entries,
 *         else 0
 *
 * @warning No caller in the source tree.
 */
int has_more_dirents(char *directory_name, uint32_t number_of_entries) {
   uint32_t entries = 0;
   int ret = 0;
 
   lList *dir_entries = sge_get_dirents(directory_name);
   for_each_ep_lv(dir_entry, dir_entries) {
      const char *entry = lGetString(dir_entry, ST_name);
      if (strcmp(entry, ".") && strcmp(entry, "..")) {
         entries++;
         if (entries > number_of_entries) {
            ret = 1;
            break;
         }
      }
   }
   lFreeList(&dir_entries);
   return ret;
}
