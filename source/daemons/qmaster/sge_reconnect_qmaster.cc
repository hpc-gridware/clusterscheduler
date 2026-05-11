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
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pwd.h>
#include <unistd.h>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/ocs_ProgName.h"

#include "cull/pack.h"

#include "comm/lists/cl_errors.h"

#include "gdi/ocs_gdi_ClientServerBase.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_ja_task.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"
#include "sgeobj/sge_qinstance.h"

#include "msg_common.h"
#include "msg_qmaster.h"

#include "sge_reconnect_qmaster.h"

/**
 * @brief Generate a one-time hex token by reading 32 bytes from /dev/urandom.
 *
 * 32 random bytes → 64 hex chars + NUL.  /dev/urandom is the right entropy source
 * for cryptographic tokens on Linux and is always non-blocking after first boot.
 *
 * @param buf       Caller-allocated buffer.
 * @param buf_size  Must be >= 65.
 * @return 0 on success; -1 on I/O error or undersized buffer.
 */
static int generate_reconnect_token(char *buf, size_t buf_size) {
   if (buf == nullptr || buf_size < 65) {
      return -1;
   }

   int fd = open("/dev/urandom", O_RDONLY);
   if (fd < 0) {
      return -1;
   }

   unsigned char raw[32];
   size_t got = 0;
   while (got < sizeof(raw)) {
      ssize_t r = read(fd, raw + got, sizeof(raw) - got);
      if (r < 0) {
         if (errno == EINTR) {
            continue;
         }
         close(fd);
         return -1;
      }
      if (r == 0) {
         close(fd);
         return -1;
      }
      got += r;
   }
   close(fd);

   static const char hex[] = "0123456789abcdef";
   for (size_t i = 0; i < sizeof(raw); ++i) {
      buf[i * 2]     = hex[(raw[i] >> 4) & 0xF];
      buf[i * 2 + 1] = hex[raw[i] & 0xF];
   }
   buf[sizeof(raw) * 2] = '\0';
   return 0;
}

int qmaster_handle_reconnect_request(uint32_t job_id,
                                     const char *requester_user,
                                     const char *client_host,
                                     int client_port,
                                     char *out_token, size_t out_token_size,
                                     char *out_exec_host, size_t out_exec_host_size,
                                     lList **answer_list) {
   DENTER(TOP_LAYER);

   if (requester_user == nullptr || client_host == nullptr || client_port <= 0 ||
       out_token == nullptr || out_token_size < 65 ||
       out_exec_host == nullptr || out_exec_host_size == 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              "invalid arguments to reconnect request for job " sge_u32, job_id);
      DRETURN(-1);
   }

   // Locate the job.
   const lList *master_job_list = *ocs::DataStore::get_master_list(SGE_TYPE_JOB);
   INFO("qmaster_handle_reconnect_request: looking up job " sge_u32 " in master_job_list with %d entries",
        job_id, lGetNumberOfElem(master_job_list));
   lListElem *jep = lGetElemUlongRW(master_job_list, JB_job_number, job_id);
   if (jep == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EEXIST, ANSWER_QUALITY_ERROR,
                              "job " sge_u32 " does not exist (master_job_list has %d entries)",
                              job_id, lGetNumberOfElem(master_job_list));
      DRETURN(-1);
   }

   // Ownership check: only the job owner may reconnect.  Managers/operators are
   // intentionally NOT allowed here — reconnect attaches a new interactive client,
   // which is a privilege specific to the job owner.
   const char *owner = lGetString(jep, JB_owner);
   if (owner == nullptr || sge_strnullcmp(requester_user, owner) != 0) {
      answer_list_add_sprintf(answer_list, STATUS_ENOTOWNER, ANSWER_QUALITY_ERROR,
                              MSG_JOB_NOMODJOBPERMS_SU, requester_user, job_id);
      DRETURN(-1);
   }

   // Find the running ja_task and its exec host.  Reconnect only applies to a job
   // that has been dispatched; the master queue gives us the exec host.
   const lListElem *jatep = lFirst(lGetList(jep, JB_ja_tasks));
   if (jatep == nullptr || lGetString(jatep, JAT_master_queue) == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              "job " sge_u32 " is not currently dispatched", job_id);
      DRETURN(-1);
   }

   const lList *master_cqueue_list = *ocs::DataStore::get_master_list(SGE_TYPE_CQUEUE);
   const lListElem *qinstance = cqueue_list_locate_qinstance(master_cqueue_list,
                                                             lGetString(jatep, JAT_master_queue));
   if (qinstance == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              "could not locate master queue for job " sge_u32, job_id);
      DRETURN(-1);
   }
   const char *exec_host = lGetHost(qinstance, QU_qhostname);
   if (exec_host == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              "could not determine exec host for job " sge_u32, job_id);
      DRETURN(-1);
   }

   // Resolve owner → uid/gid via the local passwd database.  This matches the existing
   // OCS assumption that uid/gid mapping is consistent across submit/exec hosts (NIS,
   // LDAP, or shared /etc/passwd).  If the job uses MUNGE id mapping the resolution
   // happens on the exec host via the same uid the shepherd already runs under.
   struct passwd *pw = getpwnam(owner);
   if (pw == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              "could not resolve owner %s of job " sge_u32, owner, job_id);
      DRETURN(-1);
   }
   uid_t owner_uid = pw->pw_uid;
   gid_t owner_gid = pw->pw_gid;

   // Generate the one-time token AFTER all validation so we don't burn entropy on
   // requests that won't proceed.
   if (generate_reconnect_token(out_token, out_token_size) != 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              "failed to generate reconnect token (open /dev/urandom failed)");
      DRETURN(-1);
   }

   // Copy exec_host to the caller's output buffer before we kick off the async send;
   // the master_cqueue_list pointer can be invalidated by subsequent mutations.
   snprintf(out_exec_host, out_exec_host_size, "%s", exec_host);

   uint32_t ja_task_id = lGetUlong(jatep, JAT_task_number);

   int rc = sge_qmaster_send_reconnect_prepare(job_id, ja_task_id, out_exec_host,
                                               client_host, client_port,
                                               out_token, owner_uid, owner_gid);
   if (rc != 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR,
                              "failed to relay reconnect request to execd %s (rc=%d)", out_exec_host, rc);
      // Wipe the token from the output buffer so a misbehaving caller can't use a
      // token that the execd never received.
      out_token[0] = '\0';
      DRETURN(-1);
   }

   INFO("qmaster_handle_reconnect_request: job " sge_u32 ".%u brokered to execd %s for client %s:%d (token redacted)",
        job_id, ja_task_id, out_exec_host, client_host, client_port);
   DRETURN(0);
}

int sge_qmaster_send_reconnect_prepare(uint32_t job_id, uint32_t ja_task_id,
                                       const char *exec_host,
                                       const char *client_host, int client_port,
                                       const char *token,
                                       uid_t owner_uid, gid_t owner_gid) {
   DENTER(TOP_LAYER);

   if (exec_host == nullptr || client_host == nullptr || token == nullptr || client_port <= 0) {
      ERROR("sge_qmaster_send_reconnect_prepare: invalid arguments");
      DRETURN(-1);
   }

   sge_pack_buffer pb;
   if (init_packbuffer(&pb, 1024) != PACK_SUCCESS) {
      ERROR("sge_qmaster_send_reconnect_prepare: init_packbuffer failed");
      DRETURN(-1);
   }

   // Order must match do_reconnect_prepare()'s unpacker exactly.
   if (packint(&pb, job_id)                != PACK_SUCCESS ||
       packint(&pb, ja_task_id)            != PACK_SUCCESS ||
       packstr(&pb, client_host)           != PACK_SUCCESS ||
       packint(&pb, (uint32_t)client_port) != PACK_SUCCESS ||
       packstr(&pb, token)                 != PACK_SUCCESS ||
       packint(&pb, (uint32_t)owner_uid)   != PACK_SUCCESS ||
       packint(&pb, (uint32_t)owner_gid)   != PACK_SUCCESS) {
      ERROR("sge_qmaster_send_reconnect_prepare: pack failed");
      clear_packbuffer(&pb);
      DRETURN(-1);
   }

   const char *pnm = to_cstr(EXECD);
   uint32_t mid = 0;
   int rc = ocs::gdi::ClientServerBase::gdi_send_message_pb(
               0,                  // synchron — fire-and-forget
               pnm, 1, exec_host,
               ocs::gdi::ClientServerBase::TAG_RECONNECT_PREPARE,
               &pb, &mid);
   clear_packbuffer(&pb);

   // gdi_send_message_pb returns CL_RETVAL_OK (== 1000) on success, not 0.
   if (rc != CL_RETVAL_OK) {
      ERROR("sge_qmaster_send_reconnect_prepare: gdi_send_message_pb to %s failed: %d", exec_host, rc);
      DRETURN(rc);
   }

   INFO("sge_qmaster_send_reconnect_prepare: sent TAG_RECONNECT_PREPARE to %s for job " sge_u32 "." sge_u32
        " (client=%s:%d, token redacted)",
        exec_host, job_id, ja_task_id, client_host, client_port);
   DRETURN(0);
}
