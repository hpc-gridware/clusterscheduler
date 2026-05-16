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
#include <cstddef>
#include <cstdint>

#include "cull/cull_list.h"

/**
 * @brief Handle a client "prepare reconnect" request inside qmaster.
 *
 * Called from the qmaster GDI handler for the client's RECONNECT_PREPARE request
 * (added by CS-2144 Commit 3).  Validates ownership and job state, generates a
 * one-time hex token, looks up the exec host and owner UID/GID for the job,
 * sends a TAG_RECONNECT_PREPARE message to that execd (which writes
 * reconnect.info into the active_jobs spool dir for the shepherd to pick up),
 * and returns the token + exec host back to the caller so the GDI response can
 * carry them to the client.
 *
 * Stage-2 hardening note: this Commit accepts any running job that the requester
 * owns.  A stricter "shepherd is actually waiting for a reconnect" check is
 * deferred to a hardening pass — the ownership check is the security boundary.
 *
 * @param job_id              Job ID the client wants to reconnect to.
 * @param requester_user      Username of the GDI request originator (`packet->user`).
 * @param client_host         Hostname the shepherd should connect back to.
 * @param client_port         TCP port the new client is listening on.
 * @param out_token           Caller-allocated buffer for the generated token (hex, NUL-terminated).
 * @param out_token_size      Size of out_token; must be >= 65 (64 hex chars + NUL).
 * @param out_exec_host       Caller-allocated buffer for the exec hostname.
 * @param out_exec_host_size  Size of out_exec_host buffer.
 * @param answer_list         Standard error list; populated on validation/lookup failure.
 * @return 0 on success; -1 on any validation, lookup, or send failure (answer_list filled).
 */
int qmaster_handle_reconnect_request(uint32_t job_id,
                                     const char *requester_user,
                                     const char *client_host,
                                     int client_port,
                                     char *out_token, size_t out_token_size,
                                     char *out_exec_host, size_t out_exec_host_size,
                                     lList **answer_list);

/**
 * @brief Send a TAG_RECONNECT_PREPARE message from qmaster to the execd that runs a job.
 *
 * Part of the IJS session-reconnect broker (CS-2143).  The qmaster GDI handler for the
 * client's RECONNECT_PREPARE request (added by Stage 3 / CS-2144) calls this helper to
 * relay the reconnect info to the right execd; the execd's dispatcher routes the message
 * to do_reconnect_prepare(), which writes reconnect.info into the job's spool dir for the
 * waiting shepherd's polling loop to find.
 *
 * Wire format packed into the message buffer (matches do_reconnect_prepare's unpacker):
 *   job_id        (uint32, packint)
 *   ja_task_id    (uint32, packint)
 *   client_host   (string, packstr)
 *   client_port   (uint32, packint)
 *   token         (string, packstr)
 *   owner_uid     (uint32, packint)
 *   owner_gid     (uint32, packint)
 *
 * The send is asynchronous (synchron=0); no reply is expected.  Errors at the commlib
 * layer are logged and returned to the caller.
 *
 * @param job_id        Job ID.
 * @param ja_task_id    Array task ID (0 for non-array jobs).
 * @param exec_host     Hostname of the execd that runs the job.
 * @param client_host   Hostname the shepherd should connect to.
 * @param client_port   TCP port the new client is listening on.
 * @param token         One-time reconnect token.
 * @param owner_uid     UID of the job owner.
 * @param owner_gid     GID of the job owner.
 * @return 0 on success; a commlib error code (>0) on send failure; -1 on packing failure.
 */
int sge_qmaster_send_reconnect_prepare(uint32_t job_id, uint32_t ja_task_id,
                                       const char *exec_host,
                                       const char *client_host, int client_port,
                                       const char *token,
                                       uid_t owner_uid, gid_t owner_gid);
