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
 * Test client for CS-3135, used by the testsuite check bugs/jira_cs/3135.
 *
 * sge_qmaster has to reject requests which act on objects of another user:
 *   - the exit message of an event client registered by another user
 *   - an acknowledgement of the events of an event client registered by another user
 *   - with Munge authentication, an acknowledgement of a signal (ACK_SIGJOB) from a user who
 *     is neither the admin user nor root
 *
 * The client sends these messages the same way the event client library and sge_execd do.
 * It is run as a user who does not own the event client. A second mode registers an event
 * client, so that the check has an event client of a known user.
 *
 * Usage:
 *   test_qmaster_foreign_requests register <seconds> <edtime>
 *      register an event client subscribing sgeE_JOB_ADD with the event delivery interval
 *      <edtime>, print "EVENT_CLIENT_ID <id>", then "JOB_ADD <job_id>" for every job add event
 *      received, deregister after <seconds>
 *   test_qmaster_foreign_requests exit <event_client_id>
 *      send the exit message of the event client
 *   test_qmaster_foreign_requests ack_event <event_client_id> <event_number>
 *      acknowledge the events of the event client up to <event_number>
 *   test_qmaster_foreign_requests ack_sigjob <job_id> <task_id>
 *      acknowledge the delivery of a signal to the job task
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_time.h"

#include "tom/ocs_List.h"
#include "tom/ocs_PackCore.h"

#include "sgeobj/tom/ocs_tom_ET.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_ack.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_event.h"

#include "gdi/ocs_gdi_ClientBase.h"
#include "gdi/ocs_gdi_ClientServerBase.h"

#include "evc/sge_event_client.h"

/** @brief Print the usage to stderr. */
static void
usage() {
   fprintf(stderr, "usage: test_qmaster_foreign_requests register <seconds> <edtime>\n"
                   "       test_qmaster_foreign_requests exit <event_client_id>\n"
                   "       test_qmaster_foreign_requests ack_event <event_client_id> "
                   "<event_number>\n"
                   "       test_qmaster_foreign_requests ack_sigjob <job_id> <task_id>\n");
}

/** @brief Parse an unsigned number argument.
 * @param str the argument
 * @param value out: the number
 * @return true if str is a number
 */
static bool
parse_ulong(const char *str, uint32_t *value) {
   char *end = nullptr;
   unsigned long v = strtoul(str, &end, 10);
   if (end == str || *end != '\0') {
      return false;
   }
   *value = static_cast<uint32_t>(v);
   return true;
}

/** @brief Register an event client and print the job add events it receives.
 * @param seconds how long the event client stays registered
 * @param edtime event delivery interval in seconds
 * @return 0 on success, 1 on error
 */
static int
run_event_client(uint32_t seconds, uint32_t edtime) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   sge_evc_class_t *evc = nullptr;

   if (!sge_gdi2_evc_setup(&evc, EV_ID_ANY, &alp, "foreign_test")) {
      answer_list_output(&alp);
      DRETURN(1);
   }
   evc->ec_set_edtime(evc, edtime);
   evc->ec_subscribe(evc, sgeE_JOB_ADD);
   if (!evc->ec_register(evc, false, &alp)) {
      answer_list_output(&alp);
      sge_evc_class_destroy(&evc);
      DRETURN(1);
   }
   printf("EVENT_CLIENT_ID %d\n", static_cast<int>(evc->ec_get_id(evc)));
   fflush(stdout);

   uint64_t end = sge_get_gmt64() + sge_gmt32_to_gmt64(seconds);
   while (sge_get_gmt64() < end) {
      ocs::tom::ListData *event_list_raw = nullptr;
      if (evc->ec_get(evc, &event_list_raw, false)) {
         // the event client delivers ET events, the handle frees the list
         const auto event_list = ocs::tom::ListPtr<ocs::tom::ET>::from_typed(event_list_raw);
         if (event_list) {
            for (const auto event : *event_list) {
               if (event.get<ET_type>() == sgeE_JOB_ADD) {
                  printf("JOB_ADD " sge_u32 "\n", event.get<ET_intkey>());
                  fflush(stdout);
               }
            }
         }
      }
   }

   evc->ec_deregister(evc);
   sge_evc_class_destroy(&evc);
   DRETURN(0);
}

/** @brief Send the exit message of an event client, as ec2_deregister() does.
 * @param event_client_id id of the event client
 * @return 0 if the message was sent, 1 otherwise
 */
static int
send_event_client_exit(uint32_t event_client_id) {
   DENTER(TOP_LAYER);
   sge_pack_buffer pb;
   ocs::tom::ListPtr<ocs::tom::AN> alp;

   if (ocs::tom::try_init_buffer(&pb, sizeof(uint32_t)) != PACK_SUCCESS) {
      DRETURN(1);
   }
   ocs::tom::try_put_uint32(&pb, event_client_id);
   int ret = ocs::gdi::ClientServerBase::sge_gdi_send_any_request(0, nullptr,
                ocs::gdi::ClientBase::gdi_get_act_master_host(false), to_cstr(QMASTER), 1, &pb,
                ocs::gdi::ClientServerBase::TAG_EVENT_CLIENT_EXIT, 0, &alp);
   ocs::tom::clear_buffer(&pb);
   answer_list_output(&alp);
   DRETURN(ret == CL_RETVAL_OK ? 0 : 1);
}

/** @brief Send an acknowledgement to sge_qmaster.
 * @param type ACK_EVENT_DELIVERY or ACK_SIGJOB
 * @param id the first id of the acknowledgement
 * @param id2 the second id of the acknowledgement
 * @return 0 if the message was sent, 1 otherwise
 */
static int
send_ack(uint32_t type, uint32_t id, uint32_t id2) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::AN> alp;
   int ret = sge_send_ack_to_qmaster(type, id, id2, nullptr, &alp);
   DRETURN(ret == CL_RETVAL_OK ? 0 : 1);
}

/** @brief Entry point, see the usage at the top of the file.
 * @param argc argument count
 * @param argv argument vector
 * @return 0 on success, 1 on error, 2 on a usage error
 */
int
main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_qmaster_foreign_requests");
   uint32_t arg1 = 0;
   uint32_t arg2 = 0;

   if (argc < 3 || !parse_ulong(argv[2], &arg1) || (argc > 3 && !parse_ulong(argv[3], &arg2))) {
      usage();
      DRETURN(2);
   }
   const char *mode = argv[1];
   bool needs_two = strcmp(mode, "register") == 0 || strcmp(mode, "ack_event") == 0 ||
                    strcmp(mode, "ack_sigjob") == 0;
   if ((needs_two && argc != 4) || (!needs_two && argc != 3)) {
      usage();
      DRETURN(2);
   }

   ocs::tom::ListPtr<ocs::tom::AN> alp;
   ocs::DataStore::select_active_ds(ocs::DataStore::Id::GLOBAL);
   if (ocs::gdi::ClientBase::setup_and_enroll(QEVENT, MAIN_THREAD, &alp) !=
       ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      DRETURN(1);
   }

   int ret;
   if (strcmp(mode, "register") == 0) {
      ret = run_event_client(arg1, arg2);
   } else if (strcmp(mode, "exit") == 0) {
      ret = send_event_client_exit(arg1);
   } else if (strcmp(mode, "ack_event") == 0) {
      // the event client library sends the event number as id and the client id as id2
      ret = send_ack(ACK_EVENT_DELIVERY, arg2, arg1);
   } else if (strcmp(mode, "ack_sigjob") == 0) {
      ret = send_ack(ACK_SIGJOB, arg1, arg2);
   } else {
      usage();
      ret = 2;
   }
   if (ret != 2) {
      printf("%s done, rc=%d\n", mode, ret);
   }

   ocs::gdi::ClientBase::shutdown();
   DRETURN(ret);
}
