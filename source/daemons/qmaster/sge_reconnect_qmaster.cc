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

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/ocs_ProgName.h"

#include "cull/pack.h"

#include "gdi/ocs_gdi_ClientServerBase.h"

#include "sge_reconnect_qmaster.h"

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

   if (rc != 0) {
      ERROR("sge_qmaster_send_reconnect_prepare: gdi_send_message_pb to %s failed: %d", exec_host, rc);
      DRETURN(rc);
   }

   INFO("sge_qmaster_send_reconnect_prepare: sent TAG_RECONNECT_PREPARE to %s for job " sge_u32 "." sge_u32
        " (client=%s:%d, token redacted)",
        exec_host, job_id, ja_task_id, client_host, client_port);
   DRETURN(0);
}
