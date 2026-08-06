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
 * @brief Runs the user's editor on a temporary file
 */

#include <cstring>
#include <cstdio>
#include <cerrno>
#include <sys/wait.h>

#include "uti/msg_utilib.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_signal.h"
#include "uti/sge_string.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "sge.h"

#include "msg_common.h"

/**
 * @brief Run the user's editor on a file and report whether it changed
 *
 * Forks, drops back to the real user id, and execs `$EDITOR` — or #DEFAULT_EDITOR
 * when that is unset — on @p fname. Change is detected by comparing the file's
 * modification time and size before and after, so an edit that rewrites the
 * file identically counts as unchanged.
 *
 * @param fname the file to edit
 * @param myuid user id to run the editor as
 * @param mygid group id to run the editor as
 * @return 0 when the file was changed, 1 when it was left unchanged, and -1 on
 *         error — the editor could not be started, exited non-zero, was killed
 *         by a signal, or the file no longer exists
 */
int sge_edit(const char *fname, uid_t myuid, gid_t mygid) {
   SGE_STRUCT_STAT before, after;
   pid_t pid;
   int status;
   int ws = 0;

   DENTER(TOP_LAYER);;

   if (fname == nullptr) {
      ERROR(SFNMAX, MSG_NULLPOINTER);
      DRETURN(-1);
   }

   if (SGE_STAT(fname, &before)) {
      ERROR(MSG_FILE_EDITFILEXDOESNOTEXIST_S, fname);
      DRETURN(-1);
   }

   if (chown(fname, myuid, mygid) != 0) {
      dstring ds = DSTRING_INIT;
      ERROR(MSG_FILE_NOCHOWN_SS, fname, sge_strerror(errno, &ds));
      sge_dstring_free(&ds);
      DRETURN(-1);
   }

   pid = fork();
   if (pid) {
      while (ws != pid) {
         ws = waitpid(pid, &status, 0);
         if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
               ERROR(MSG_QCONF_EDITOREXITEDWITHERROR_I, (int) WEXITSTATUS(status));
               DRETURN(-1);
            } else {
               if (SGE_STAT(fname, &after)) {
                  ERROR(MSG_QCONF_EDITFILEXNOLONGEREXISTS_S, fname);
                  DRETURN(-1);
               }
               if ((before.st_mtime != after.st_mtime) ||
                   (before.st_size != after.st_size)) {
                  DRETURN(0);
               } else {
                  /* file is unchanged; inform caller */
                  DRETURN(1);
               }
            }
         }
         if (WIFSIGNALED(status)) {
            ERROR(MSG_QCONF_EDITORWASTERMINATEDBYSIGX_I, (int) WTERMSIG(status));
            DRETURN(-1);
         }
      }
   } else {
      const char *cp = nullptr;

      sge_set_def_sig_mask(nullptr, nullptr);
      sge_unblock_all_signals();
      if (setuid(getuid())) {
         ERROR(MSG_SYSTEM_SETUIDFAILED_u, getuid());
         sge_exit(1);
      }
      if (setgid(getgid())) {
         ERROR(MSG_SYSTEM_SETGIDFAILED_g, getgid());
         sge_exit(1);
      }

      cp = sge_getenv("EDITOR");
      if (cp == nullptr || strlen(cp) == 0) {
         cp = DEFAULT_EDITOR;
      }

      execlp(cp, cp, fname, (char *) 0);
      ERROR(MSG_QCONF_CANTSTARTEDITORX_S, cp);
      sge_exit(1);
   }

   DRETURN(-1);
}

