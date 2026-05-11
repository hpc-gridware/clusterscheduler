/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2026 HPC-Gridware GmbH
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

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"

#include "execd_reconnect.h"

int execd_write_reconnect_info(uint32_t job_id, uint32_t ja_task_id, const char *pe_task_id,
                               const char *host, int port, const char *token,
                               uid_t owner_uid, gid_t owner_gid) {
   DENTER(TOP_LAYER);

   if (host == nullptr || token == nullptr || port <= 0) {
      ERROR("execd_write_reconnect_info: invalid arguments (host=%s port=%d token=%s)",
            host != nullptr ? host : "<nullptr>", port,
            token != nullptr ? "<set>" : "<nullptr>");
      DRETURN(-1);
   }

   // Resolve $EXECD_SPOOL/<host>/active_jobs/<job_id>.<task_id>/reconnect.info{,.tmp}.
   DSTRING_STATIC(final_path, SGE_PATH_MAX);
   DSTRING_STATIC(tmp_path, SGE_PATH_MAX);
   sge_get_active_job_file_path(&final_path, job_id, ja_task_id, pe_task_id, "reconnect.info");
   sge_get_active_job_file_path(&tmp_path,   job_id, ja_task_id, pe_task_id, "reconnect.info.tmp");

   const char *final_str = sge_dstring_get_string(&final_path);
   const char *tmp_str   = sge_dstring_get_string(&tmp_path);

   // Atomic write: create the .tmp file mode 0600, write contents, fsync, then rename.
   // Pre-existing .tmp leftovers from a crashed previous write must be removed first
   // so O_EXCL would otherwise fail; explicit unlink is simpler than O_TRUNC because
   // we never want to overwrite a file with the wrong ownership.
   unlink(tmp_str);

   int fd = open(tmp_str, O_WRONLY | O_CREAT | O_EXCL, 0600);
   if (fd < 0) {
      ERROR("execd_write_reconnect_info: open(%s) failed: %s", tmp_str, strerror(errno));
      DRETURN(-1);
   }

   // The token is opaque to execd; just byte-pass it.  Reject embedded newlines
   // so the on-disk format stays one-key-per-line and trivially parseable.
   if (strchr(host, '\n') != nullptr || strchr(token, '\n') != nullptr) {
      ERROR("execd_write_reconnect_info: host or token contains a newline; refusing to write");
      close(fd);
      unlink(tmp_str);
      DRETURN(-1);
   }

   char buf[1024];
   int n = snprintf(buf, sizeof(buf), "host=%s\nport=%d\ntoken=%s\n", host, port, token);
   if (n <= 0 || (size_t)n >= sizeof(buf)) {
      ERROR("execd_write_reconnect_info: formatted contents exceed buffer (n=%d)", n);
      close(fd);
      unlink(tmp_str);
      DRETURN(-1);
   }

   ssize_t total = 0;
   while (total < n) {
      ssize_t w = write(fd, buf + total, (size_t)(n - total));
      if (w < 0) {
         if (errno == EINTR) {
            continue;
         }
         ERROR("execd_write_reconnect_info: write(%s) failed: %s", tmp_str, strerror(errno));
         close(fd);
         unlink(tmp_str);
         DRETURN(-1);
      }
      total += w;
   }

   if (fsync(fd) != 0) {
      ERROR("execd_write_reconnect_info: fsync(%s) failed: %s", tmp_str, strerror(errno));
      close(fd);
      unlink(tmp_str);
      DRETURN(-1);
   }
   close(fd);

   // The shepherd reads this file as the job owner.  execd writes as root, so we must
   // chown to the job owner; without this the shepherd's fopen() returns EACCES.
   if (chown(tmp_str, owner_uid, owner_gid) != 0) {
      ERROR("execd_write_reconnect_info: chown(%s, %d, %d) failed: %s",
            tmp_str, (int)owner_uid, (int)owner_gid, strerror(errno));
      unlink(tmp_str);
      DRETURN(-1);
   }

   // rename() is atomic on POSIX filesystems: the shepherd either sees no file
   // or a fully-written, correctly-permissioned one.
   if (rename(tmp_str, final_str) != 0) {
      ERROR("execd_write_reconnect_info: rename(%s -> %s) failed: %s",
            tmp_str, final_str, strerror(errno));
      unlink(tmp_str);
      DRETURN(-1);
   }

   INFO("execd_write_reconnect_info: wrote %s (host=%s port=%d, token redacted)",
        final_str, host, port);
   DRETURN(0);
}
