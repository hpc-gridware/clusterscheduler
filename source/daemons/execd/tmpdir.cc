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
 * @brief The temporary directory a job runs with as $TMPDIR
 */

#include <cerrno>
#include <string>
#include <vector>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_qinstance.h"
#include "sgeobj/tom/ocs_tom_QU.h"
#include "tom/ocs_Element.h"

#include "tmpdir.h"
#include "msg_common.h"
#include "msg_execd.h"

/**
 * @brief Create the temporary directory a job runs with as `$TMPDIR`
 *
 * Created as root (the start user of the daemon) and then handed to the job owner, because the
 * parent directory is usually not writable by users. The parent (QU_tmpdir) is often world
 * writable and the name is predictable, so a user may have planted something at the path:
 * ocs_mkdir_for_owner() never follows a symbolic link and changes owner and mode through the open
 * descriptor (CS-3203). A link, a file or another non-directory at the path is removed, a
 * directory of another user is renamed aside, which logs a warning naming its uid and gid, and the
 * directory is created; sge_remove_tmpdir() removes what was renamed aside when the job ends
 * (CS-3221). A directory that already belongs to root or to the job owner, for example one left
 * behind by an earlier start of the same task, is used as before. A failing mkdir(), fchown() or
 * fchmod() is a problem of the host; when the path cannot be made the directory for any other
 * reason, @p planted tells the caller that only this job failed, not the queue.
 *
 * @param qep the queue instance, which configures where temporary directories live
 * @param jobid the job
 * @param jataskid the array task
 * @param uid the job owner
 * @param gid the job owner's group
 * @param[out] tmpdir receives the directory
 * @param tmpdir_size size of that buffer
 * @param[out] planted set to true when the directory could not be made because a user occupied
 *             its path, the failure then belongs to the job, not to the queue
 * @return the directory, or nullptr when none was configured or it could not be handed to the job owner
 */
char *
sge_make_tmpdir(const ocs::tom::ConstElement<ocs::tom::QU> qep, const uint32_t jobid, const uint32_t jataskid,
                const uid_t uid, const gid_t gid, char *tmpdir, const size_t tmpdir_size, bool &planted) {
   DENTER(TOP_LAYER);

   planted = false;

   const char *t = qep.get<QU_tmpdir>();
   if (t == nullptr) {
      DRETURN(nullptr);
   }

   // the same job may run in several queue instances of the same host
   snprintf(tmpdir, tmpdir_size, "%s/" sge_u32 "." sge_u32 ".%s", t, jobid, jataskid, qep.get<QU_qname>());

   DPRINTF("making TMPDIR=%s\n", tmpdir);

   sge_switch2start_user();

   // the configured parent is created when missing, as before; it is the administrator's path
   sge_mkdir(t, 0755, false, false);

   DSTRING_STATIC(err, MAX_STRING_SIZE);
   const int ret = ocs_mkdir_for_owner(tmpdir, 0755, uid, gid, &err);

   sge_switch2admin_user();

   if (ret != OCS_MKDIR_FOR_OWNER_OK) {
      planted = ret == OCS_MKDIR_FOR_OWNER_PLANTED;
      ERROR(SFNMAX, sge_dstring_get_string(&err));
      DRETURN(nullptr);
   }

   DRETURN(tmpdir);
}

/**
 * @brief Delete a job's temporary directory when it ends
 *
 * The directory belongs to the job owner, but it is removed as root (the start user of the
 * daemon): files that a prolog or epilog running as root left in it have to go as well, and
 * root can remove a subdirectory the owner made read-only. That is safe because sge_rmdir()
 * never follows a symbolic link, also not one the owner swaps in for a subdirectory while the
 * tree is being removed (CS-3202).
 *
 * The directories of other users that sge_make_tmpdir() renamed aside from the path
 * (`<tmpdir>.stale.<pid>.<time>`, CS-3203) are removed the same way (CS-3221). Their names are
 * derived from the path of the temporary directory, so nothing has to be remembered between the
 * start and the end of the job, and directories renamed aside before a restart of execd or by a
 * further task of a parallel job are found as well. A directory that cannot be removed is logged,
 * the job is not affected.
 *
 * @param dir the configured parent of the temporary directories (QU_tmpdir), may be nullptr
 * @param job_owner the job owner (unused)
 * @param jobid the job
 * @param jataskid the array task
 * @param queue_name the queue instance, part of the directory's name
 * @return 0 on success or when no parent is configured, -1 when the directory or one renamed aside
 *         from its path could not be removed
 */
int sge_remove_tmpdir(const char *dir, [[maybe_unused]] const char *job_owner, const uint32_t jobid,
                      const uint32_t jataskid, const char *queue_name) {
   DENTER(TOP_LAYER);

   if (!dir) {
      DRETURN(0);
   }

   stringT tmpstr;
   snprintf(tmpstr, sizeof(tmpstr), "%s/" sge_u32 "." sge_u32 ".%s", dir, jobid, jataskid, queue_name);

   int ret = 0;
   DSTRING_STATIC(err_str, MAX_STRING_SIZE);

   sge_switch2start_user();

   DPRINTF("recursively unlinking \"%s\"\n", tmpstr);
   if (sge_rmdir(tmpstr, &err_str)) {
      ERROR(MSG_FILE_RECURSIVERMDIR_SS, tmpstr, sge_dstring_get_string(&err_str));
      ret = -1;
   }

   std::vector<std::string> aside;
   sge_dstring_clear(&err_str);
   if (ocs_moved_aside_paths(tmpstr, aside, &err_str) != 0) {
      ERROR(SFNMAX, sge_dstring_get_string(&err_str));
      ret = -1;
   }
   for (const auto &path : aside) {
      DPRINTF("recursively unlinking \"%s\", moved aside from \"%s\"\n", path.c_str(), tmpstr);
      sge_dstring_clear(&err_str);
      if (sge_rmdir(path.c_str(), &err_str)) {
         ERROR(MSG_FILE_RECURSIVERMDIR_SS, path.c_str(), sge_dstring_get_string(&err_str));
         ret = -1;
      }
   }

   sge_switch2admin_user();

   DRETURN(ret);
}

/**
 * @brief The temporary directory a job was given
 * @param qep the queue instance, which configures where temporary directories live
 * @param jobid the job
 * @param jataskid the array task
 * @param[out] tmpdir receives the directory
 * @param tmpdir_size size of that buffer
 * @return the directory, or nullptr when none was configured
 */
char *sge_get_tmpdir(const ocs::tom::ConstElement<ocs::tom::QU> qep, const uint32_t jobid, const uint32_t jataskid,
                     char *tmpdir, const size_t tmpdir_size) {
   DENTER(TOP_LAYER);
   const char *t;

   if (!(t=qep.get<QU_tmpdir>())) {
      DRETURN(nullptr);
   }

   snprintf(tmpdir, tmpdir_size, "%s/" sge_u32"." sge_u32".%s", t, jobid, jataskid, qep.get<QU_qname>());
   DPRINTF("TMPDIR=%s\n", tmpdir);
   DRETURN(tmpdir);
}
