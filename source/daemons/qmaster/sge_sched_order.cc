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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2007 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "uti/sge_rmon_macros.h"
#include "uti/sge_sl.h"

#include "gdi/sge_gdi.h"
#include "sgeobj/sge_daemonize.h"

#include "sgeobj/sge_answer.h"

#include "sge_sched_order.h"

#include "msg_common.h"

gdi_request_queue_t Master_Request_Queue;

bool
schedd_order_initialize() {
   bool ret = true;

   DENTER(TOP_LAYER);
   Master_Request_Queue.order_list = nullptr;
   ret &= sge_sl_create(&Master_Request_Queue.request_list);
   DRETURN(ret);
}

bool
schedd_order_destroy() {
   bool ret = true;

   DENTER(TOP_LAYER);
   ret &= sge_sl_destroy(&Master_Request_Queue.request_list, nullptr);
   DRETURN(ret);
}


bool
sge_schedd_send_orders(order_t *orders, lList **order_list, lList **answer_list, const char *name) {
   bool ret = true;

   DENTER(TOP_LAYER);

   if ((order_list != nullptr) && (*order_list != nullptr) && (lGetNumberOfElem(*order_list) != 0)) {
      /*
       * Add the new orders 
       */
      if (Master_Request_Queue.order_list == nullptr) {
         Master_Request_Queue.order_list = *order_list;
         *order_list = nullptr;
      } else {
         lAddList(Master_Request_Queue.order_list, order_list);
      }

      ret = sge_schedd_add_gdi_order_request(orders, answer_list, &Master_Request_Queue.order_list);
   }
   lFreeList(order_list);

   DRETURN(ret);
}

bool
sge_schedd_add_gdi_order_request(order_t *orders, lList **answer_list, lList **order_list) {
   DENTER(TOP_LAYER);
   bool ret = true;
   auto *gdi_multi = new ocs::GdiMulti();

   if (gdi_multi != nullptr) {
      int order_id;

      orders->numberSendOrders += lGetNumberOfElem(*order_list);
      orders->numberSendPackages++;

      /*
       * order_list will be nullptr after the call of gdi_multi. This saves a copy operation.
       */
      order_id = gdi_multi->request(answer_list, ocs::GdiMode::SEND, ocs::GdiTarget::Target::SGE_ORDER_LIST, ocs::GdiCommand::SGE_GDI_ADD, ocs::GdiSubCommand::SGE_GDI_SUB_NONE, order_list, nullptr, nullptr, false);

      if (order_id != -1) {
         sge_sl_insert(Master_Request_Queue.request_list, gdi_multi, SGE_SL_BACKWARD);
      } else {
         answer_list_log(answer_list, false, false);
         ret = false;
      }
   } else {
      answer_list_add(answer_list, MSG_SGETEXT_NOMEM, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}

bool
sge_schedd_block_until_orders_processed(lList **answer_list) {
   bool ret = true;
   sge_sl_elem_t *next_elem = nullptr;
   sge_sl_elem_t *current_elem = nullptr;

   DENTER(TOP_LAYER);

   /*
    * wait till all GDI order requests are finished
    */
   sge_sl_elem_next(Master_Request_Queue.request_list, &next_elem, SGE_SL_FORWARD);
   while ((current_elem = next_elem) != nullptr) {
      auto *gdi_multi = static_cast<ocs::GdiMulti *>(sge_sl_elem_data(current_elem));
      lList *request_answer_list = nullptr;
      lList *multi_answer_list = nullptr;
      int order_id;

      /* get next element, dechain current and destroy it */
      sge_sl_elem_next(Master_Request_Queue.request_list, &next_elem, SGE_SL_FORWARD);
      sge_sl_dechain(Master_Request_Queue.request_list, current_elem);
      sge_sl_elem_destroy(&current_elem, nullptr);

      /*
       * wait for answer. this call might block if the request
       * has not been handled by any worker until now
       */
      gdi_multi->wait();

      /*
       * now we have an answer. is it positive?
       */
      order_id = 1;
      gdi_multi->get_response(&request_answer_list, ocs::GdiCommand::SGE_GDI_ADD, ocs::GdiSubCommand::SGE_GDI_SUB_NONE, ocs::GdiTarget::Target::SGE_ORDER_LIST, order_id, nullptr);
      if (request_answer_list != nullptr) {
         answer_list_log(&request_answer_list, false, false);
         ret = false;
      }

      /*
       * memory cleanup
       */
      lFreeList(&request_answer_list);
      lFreeList(&multi_answer_list);
      delete gdi_multi;
   }
   DRETURN(ret);
}
