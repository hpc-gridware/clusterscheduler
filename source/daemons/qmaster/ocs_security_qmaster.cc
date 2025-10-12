/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
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

#if defined(OCS_WITH_OPENSSL)

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include <uti/sge_time.h>

#include "msg_qmaster.h"
#include "sge_qmaster_timed_event.h"

#include "ocs_security_qmaster.h"


namespace ocs::qmaster {

   static void
   cert_renewal_create_event() {
      DENTER(TOP_LAYER);

      cl_com_handle_t *handle = cl_com_get_handle(prognames[QMASTER], 1);
      if (handle == nullptr) {
         CRITICAL(SFNMAX, MSG_NO_COMMLIB_HANDLE_FOUND);
      } else {
         ocs::uti::OpenSSL::OpenSSLContext *context = handle->ssl_server_context;
         if (context == nullptr) {
            CRITICAL(SFNMAX, MSG_NO_SSL_CONTEXT_FOUND);
         } else {
            u_long64 when = context->get_renewal_time();
            DSTRING_STATIC(buffer, 128);
            INFO(MSG_NEXT_CERTIFICATE_RENEWAL_AT_S, sge_ctime64(when, &buffer));
            te_event_t ev = te_new_event(when, TYPE_SSL_CERT_RENEWAL_EVENT, ONE_TIME_EVENT, 0, 0, "cert-renewal-event");
            te_add_event(ev);
            te_free_event(&ev);
         }
      }

      DRETURN_VOID;
   }

   void
   cert_renewal_initialize() {
      DENTER(TOP_LAYER);

      DPRINTF(SFNMAX "\n", "initializing certificate renewal");
      te_register_event_handler(cert_renewal_event_handler, TYPE_SSL_CERT_RENEWAL_EVENT);
      cert_renewal_create_event();

      DRETURN_VOID;
   }

   void
   cert_renewal_event_handler(te_event_t anEvent, monitoring_t *monitor) {
      DENTER(TOP_LAYER);

      DPRINTF(SFNMAX "\n", "certificate renewal event handler called");
      cl_com_handle_t *handle = cl_com_get_handle(prognames[QMASTER], 1);
      if (handle == nullptr) {
         CRITICAL(SFNMAX, MSG_NO_COMMLIB_HANDLE_FOUND);
      } else {
         cl_commlib_check_refresh_server_context(handle);
      }
      cert_renewal_create_event();

      DRETURN_VOID;
   }

}
#endif // OCS_WITH_OPENSSL
