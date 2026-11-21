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

#include "tom/ocs_Types.h"

#include "uti/sge_log.h"
#include "uti/sge_unistd.h"
#include "uti/msg_utilib.h"

#include "sgeobj/sge_str.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_ST.h"

#include "spool/sge_dirent.h"

#include "msg_common.h"

/**
 * @brief Read the entries of a directory
 *
 * `.` and `..` are left out. Used by the classic and flatfile backends to
 * walk a spool directory, and by `spooldefaults`.
 *
 * The opendir() error path used to leave through a plain `return`, skipping
 * the rmon_mexit() that DENTER's rmon_menter() paired with; it returns
 * through DRETURN now.
 *
 * @param path the directory to read
 *
 * @return the entries, or an empty handle on any error and for an empty
 *         directory - the two cases are not distinguishable from the return value
 */
ocs::tom::ListPtr<ocs::tom::ST> ocs_get_dirents(const char *path) {
   DENTER(TOP_LAYER);

   ocs::tom::ListPtr<ocs::tom::ST> entries;
   SGE_STRUCT_DIRENT *dent;
   char dirent[SGE_PATH_MAX*2];

   DIR *cwd = opendir(path);

   if (cwd == (DIR *) 0) {
      ERROR(MSG_FILE_CANTOPENDIRECTORYX_SS, path, strerror(errno));
      DRETURN(entries);
   }

   while (SGE_READDIR_R(cwd, (SGE_STRUCT_DIRENT *)dirent, &dent)==0 && dent!=nullptr) {
      if (!dent->d_name[0])
         continue;             
      if (strcmp(dent->d_name, "..") == 0 || strcmp(dent->d_name, ".") == 0)
         continue;
      // the list is created with the first entry, so an empty directory gives an empty handle as before
      if (!entries) {
         entries = ocs::tom::ListPtr<ocs::tom::ST>::create("");
      }
      entries->emplace_back<ST_name>(dent->d_name);
   }
   closedir(cwd);

   DRETURN(entries);
}

