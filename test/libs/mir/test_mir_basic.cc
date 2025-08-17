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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>
#include <cstdlib>

#include "uti/ocs_TerminationManager.h"
#include "uti/sge_bootstrap.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_event.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/ocs_DataStore.h"

#include "mir/sge_mirror.h"

#include "gdi/ocs_gdi_Client.h"

#include "sig_handlers.h"
#include "msg_clients_common.h"

int events;
u_long32 events_size;

static sge_callback_result
print_event(sge_evc_class_t *evc, sge_object_type type, sge_event_action action, lListElem *event, void *clientdata)
{
   char buffer[1024];
   sge_pack_buffer pb;
   dstring buffer_wrapper;

   DENTER(TOP_LAYER);

   events++;
   init_packbuffer(&pb, 0, true, false);
   cull_pack_elem(&pb, event);
   events_size += pb.bytes_used;

   sge_dstring_init(&buffer_wrapper, buffer, sizeof(buffer));

   printf("%s\n", event_text(event, &buffer_wrapper));

   DRETURN(SGE_EMA_OK);
}

int main()
{
   DENTER_MAIN(TOP_LAYER, "test_sge_mirror");
   lList *alp = nullptr;
   sge_evc_class_t *evc = nullptr;

   sge_setup_sig_handlers(QEVENT);

   ocs::TerminationManager::install_signal_handler();
   ocs::TerminationManager::install_terminate_handler();

   // this thread will use the GLOBAL data store
   ocs::DataStore::select_active_ds(ocs::DataStore::Id::GLOBAL);

   /* setup event client */
   int cl_err = ocs::gdi::ClientBase::setup_and_enroll(QEVENT, MAIN_THREAD, &alp);
   if (cl_err != ocs::gdi::ErrorValue::AE_OK) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   if (false == sge_gdi2_evc_setup(&evc, EV_ID_ANY, &alp, nullptr)) {
      answer_list_output(&alp);
      sge_exit(1);
   }

   sge_mirror_initialize(evc, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
   sge_mirror_subscribe(evc, SGE_TYPE_ALL, print_event, nullptr, nullptr, nullptr, nullptr);
   
   while(!shut_me_down) {
      events = 0;
      events_size = 0;
      printf("---------------------\n");
      sge_mirror_process_events(evc);
      printf("received " sge_u32 " kbytes in with %d events\n", events_size/1024, events);
   }

   sge_mirror_shutdown(evc);

   DRETURN(EXIT_SUCCESS);
}
