#pragma once
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

#include <sys/types.h>
#include <cstdint>

#include "gdi/ocs_gdi_ClientServerBase.h"

/**
 * @brief Dispatcher handler for TAG_RECONNECT_PREPARE messages from qmaster.
 *
 * Wire format unpacked from the message buffer (in this exact order):
 *   job_id        (uint32, packint)
 *   ja_task_id    (uint32, packint)
 *   host          (string, packstr)
 *   port          (uint32, packint)
 *   token         (string, packstr)
 *   owner_uid     (uint32, packint)
 *   owner_gid     (uint32, packint)
 *
 * Calls execd_write_reconnect_info() with the unpacked fields.  No reply is
 * sent — the qmaster broker treats this as fire-and-forget; the shepherd's
 * polling loop is the side that observes success or failure.
 *
 * @param aMsg The struct_msg_t produced by the dispatcher; aMsg->buf is the
 *   packed body of the message.
 * @return 0 on success, -1 on unpacking failure or writer error.
 */
int do_reconnect_prepare(ocs::gdi::ClientServerBase::struct_msg_t *aMsg);

/**
 * @brief Write the reconnect.info file into a job's active_jobs spool directory.
 *
 * Used by the IJS session-reconnect flow (CS-2118 family) to relay a one-time reconnect
 * endpoint and token from qmaster to the waiting shepherd.  The shepherd polls for this
 * file during its grace period; when it appears, the shepherd attempts a new commlib
 * connection back to {host, port} and authenticates with the token.
 *
 * The file is written atomically (write-temp + rename), with mode 0600 and ownership
 * chowned to the supplied job owner so the shepherd (running as the job owner) can
 * read it but other local users cannot.  The token is sensitive — never relax the mode.
 *
 * File format (one key=value per line, trailing newline):
 *   host=<hostname>
 *   port=<integer>
 *   token=<one-time token, opaque to execd>
 *
 * @param job_id      Job ID (as used by sge_get_active_job_file_path).
 * @param ja_task_id  Array task ID (0 for non-array jobs).
 * @param pe_task_id  PE task ID (nullptr for non-PE-task contexts).
 * @param host        Client hostname the shepherd should connect to.
 * @param port        TCP port the client is listening on.
 * @param token       One-time token; the shepherd will present this back to the client.
 * @param owner_uid   UID of the job owner.
 * @param owner_gid   GID of the job owner.
 * @return 0 on success, -1 on any I/O error (logged via ERROR).
 */
int execd_write_reconnect_info(uint32_t job_id, uint32_t ja_task_id, const char *pe_task_id,
                               const char *host, int port, const char *token,
                               uid_t owner_uid, gid_t owner_gid);
