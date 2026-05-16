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

/*
 * sge_test_reconnect - minimal test/debug client for the IJS reconnect broker.
 *
 * This is a deliberately small standalone client that exercises the qmaster
 * RECONNECT_PREPARE GDI handler added in CS-2144.  It is NOT the user-facing
 * `qrsh -reconnect` mode — that is delivered in a follow-on commit.
 *
 * Usage:
 *   sge_test_reconnect <job_id> <client_host> <client_port>
 *
 * On success it prints the token and exec host from the qmaster response and
 * exits 0.  On failure it prints the qmaster's error message and exits 1.
 *
 * To exercise the full pipeline end-to-end:
 *   1. Set ijs_reconnect_timeout=30 in qmaster_params.
 *   2. Start a qrsh / qrlogin session and SIGKILL it so the shepherd enters
 *      its grace period.
 *   3. Run `sge_test_reconnect <job_id> <my_host> <my_port>` (the host:port can
 *      point at any listener — `nc -l <port>` is enough to see the inbound
 *      RECONNECT_REQUEST_MSG carrying the token in the trace).
 *   4. Observe the shepherd's trace pick up reconnect.info and attempt the
 *      connection back to the supplied host:port.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/ocs_TerminationManager.h"
#include "uti/ocs_ProgName.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_id.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/cull/sge_id_ID_L.h"

#include "gdi/ocs_gdi_Client.h"
#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_Command.h"
#include "gdi/ocs_gdi_Target.h"

#include "basis_types.h"

#include "sig_handlers.h"

static void usage(const char *prog) {
   fprintf(stderr, "usage: %s <job_id> <client_host> <client_port>\n", prog);
}

int main(int argc, char **argv) {
   DENTER_MAIN(TOP_LAYER, "sge_test_reconnect");

   if (argc != 4) {
      usage(argv[0]);
      DRETURN(1);
   }

   const char *job_id_str = argv[1];
   const char *client_host = argv[2];
   const char *client_port_str = argv[3];

   sge_setup_sig_handlers(QMOD);
   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   lList *alp = nullptr;
   if (ocs::gdi::ClientBase::setup_and_enroll(QMOD, MAIN_THREAD, &alp) != ocs::gdi::AE_OK) {
      answer_list_output(&alp);
      DRETURN(1);
   }

   // Encode <job_id>:<host>:<port> into the ID_str field; the qmaster qmod handler
   // splits this and calls qmaster_handle_reconnect_request().
   char spec[1024];
   snprintf(spec, sizeof(spec), "%s:%s:%s", job_id_str, client_host, client_port_str);

   lList *ref_list = nullptr;
   lListElem *idep = lAddElemStr(&ref_list, ID_str, spec, ID_Type);
   lSetUlong(idep, ID_action, QI_DO_RECONNECT | JOB_DO_ACTION);
   lSetUlong(idep, ID_force, 0);

   alp = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::JB_LIST, ocs::gdi::Command::TRIGGER,
                                   ocs::gdi::SubCommand::NONE, &ref_list, nullptr, nullptr);

   bool ok = false;
   for_each_ep_lv(aep, alp) {
      const char *text = lGetString(aep, AN_text);
      uint32_t status = lGetUlong(aep, AN_status);
      printf("[%u] %s\n", status, text != nullptr ? text : "(null)");
      if (status == STATUS_OK && text != nullptr && strncmp(text, "RECONNECT_OK ", 13) == 0) {
         ok = true;
      }
   }

   lFreeList(&alp);
   lFreeList(&ref_list);

   DRETURN(ok ? 0 : 1);
}
