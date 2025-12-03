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

   /**
    * @brief Creates and schedules a timed event for SSL certificate renewal.
    *
    * This function retrieves the SSL server context from the communication library handle
    * and determines when the SSL certificate needs to be renewed. It then creates a
    * one-time timed event that will trigger the certificate renewal process at the
    * appropriate time.
    *
    * The renewal time is obtained from the OpenSSL context, which typically schedules
    * renewal when 75% of the certificate's lifetime has passed.
    */
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

   /**
    * @brief Initializes the SSL certificate renewal system for the qmaster daemon.
    *
    * This function performs the initial setup for automatic SSL certificate renewal:
    * - Registers the event handler that will be called when certificate renewal is needed
    * - Creates and schedules the first certificate renewal event based on the current
    *   certificate's expiration time
    *
    * This function should be called once during qmaster startup after the SSL context
    * has been established.
    *
    */
   void
   cert_renewal_initialize() {
      DENTER(TOP_LAYER);

      if (bootstrap_has_security_mode(BS_SEC_MODE_TLS)) {
         DPRINTF(SFNMAX "\n", "initializing certificate renewal");
         te_register_event_handler(cert_renewal_event_handler, TYPE_SSL_CERT_RENEWAL_EVENT);
         cert_renewal_create_event();
      }

      DRETURN_VOID;
   }

   /**
    * @brief Event handler callback for SSL certificate renewal events.
    *
    * This function is called by the timed event system when a certificate renewal
    * event is triggered. It performs the following actions:
    * - Retrieves the communication library handle for the qmaster
    * - Triggers a refresh of the SSL server context, which recreates certificates
    *   if they have expired or are approaching expiration
    * - Schedules the next certificate renewal event
    *
    * The handler ensures continuous certificate renewal throughout the lifetime
    * of the qmaster daemon.
    *
    * @param anEvent The timed event that triggered this handler (TYPE_SSL_CERT_RENEWAL_EVENT)
    * @param monitor Pointer to monitoring structure (currently unused)
    *
    */
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
