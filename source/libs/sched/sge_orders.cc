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
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief The orders the scheduler sends back to qmaster
 */
#include <stdio.h>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_order.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"

#include "evc/sge_event_client.h"

#include "sge_orders.h"
#include "schedd_message.h"

#include "msg_schedd.h"

/**
 * @brief Retrieves the messages and generates an order out
 *
 * retrieves all messages, puts them into an order package, and frees the
 * orginal messages. It also returns the number of global and job messages.
 *
 * @param or_list int: the order list to which the message order is added
 * @param global_mes_count out: global message count
 * @param job_mes_count out: job message count
 *
 * @return the order list
 *
 * @note MT-NOTE: sge_add_schedd_info() is not MT safe
 */
lList *sge_add_schedd_info(lList *or_list, int *global_mes_count, int *job_mes_count) {
   DENTER(TOP_LAYER);

   lList *jlist;
   lListElem *sme, *ep;

   sme = schedd_mes_obtain_package(global_mes_count, job_mes_count);

   if (!sme || (lGetNumberOfElem(lGetList(sme, SME_message_list)) < 1
         && lGetNumberOfElem(lGetList(sme, SME_global_message_list)) < 1)) {
      DRETURN(or_list);
   }

   /* create orders list if not existent */
   if (!or_list) {
      or_list = lCreateList("orderlist", OR_Type);
   }

   /* build order */
   ep=lCreateElem(OR_Type);

   jlist = lCreateList("", SME_Type);
   lAppendElem(jlist, sme);
   lSetList(ep, OR_joker, jlist);

   lSetUlong(ep, OR_type, ORT_job_schedd_info);
   lAppendElem(or_list, ep);

   DRETURN(or_list);
}

/*************************************************************
 Create a new order-list or add orders to an existing one.
 or_list==nullptr -> create new one.
 or_list!=nullptr -> append orders.
 returns updated order-list.

 The granted list contains granted queues.

 TODO SG: add adoc header, and comment on use of ja_task in here.

 is MT safe
 *************************************************************/


/**
 * @brief Create a new order-list or add orders to an existing one
 *
 * - If the or_list is nullptr, a new one will be generated
 * - in case of a clear_pri order, teh ja_task is improtant. If nullptr is put
 *   in for ja_task, only the pendin tasks of the spedified job are set to nullptr.
 *   If a ja_task is put in, all tasks of the job are set to nullptr
 *
 * @param or_list the order list
 * @param type order type
 * @param job job
 * @param ja_task ja_task ref or nullptr(there is only one case, where it can be nullptr)
 * @param granted granted queue list
 * @param update_execd should the execd get new ticket values?
 *
 * @return returns the orderlist
 *
 * @note MT-NOTE: sge_create_orders() is MT safe
 */
lList *sge_create_orders(lList *or_list, uint32_t type, const lListElem *job, const lListElem *ja_task,
                         const lList *granted, bool update_execd) {
   DENTER(TOP_LAYER);

   lList *ql = nullptr;
   lListElem *ep, *ep2;
   uint32_t qslots;

   if (!job) {
      lFreeList(&or_list);
      DRETURN(or_list);
   }

   /* create orders list if not existent */
   if (!or_list) {
      or_list = lCreateList("orderlist", OR_Type);
   }

   /* build sublist of granted */
   if (update_execd) {
      for_each_ep_lv(gel, granted) {
         qslots = lGetUlong(gel, JG_slots);
         if (qslots) { /* ignore Qs with slots==0 */
            ep2=lCreateElem(OQ_Type);

            lSetUlong(ep2, OQ_slots, qslots);
            lSetList(ep2, OQ_binding_to_use, lCopyList("binding_to_use", lGetList(gel, JG_binding_to_use)));
            lSetString(ep2, OQ_dest_queue, lGetString(gel, JG_qname));
            lSetUlong(ep2, OQ_dest_version, lGetUlong(gel, JG_qversion));
            lSetDouble(ep2, OQ_ticket, lGetDouble(gel, JG_ticket));
            lSetDouble(ep2, OQ_oticket, lGetDouble(gel, JG_oticket));
            lSetDouble(ep2, OQ_fticket, lGetDouble(gel, JG_fticket));
            lSetDouble(ep2, OQ_sticket, lGetDouble(gel, JG_sticket));
            if (!ql)
               ql=lCreateList("orderlist",OQ_Type);
            lAppendElem(ql, ep2);
         }
      }
   }

   /* build order */
   ep=lCreateElem(OR_Type);

   if(ja_task != nullptr) {
      lSetDouble(ep, OR_ticket,    lGetDouble(ja_task, JAT_tix));
      lSetDouble(ep, OR_ntix,      lGetDouble(ja_task, JAT_ntix));
      lSetDouble(ep, OR_prio,      lGetDouble(ja_task, JAT_prio));
   }

   if (type == ORT_tickets || type == ORT_ptickets) {

      static order_pos_t *order_pos = nullptr;

      const lDescr tixDesc[] = {
                            {JAT_task_number, lUlongT | CULL_IS_REDUCED, nullptr},
                            {JAT_tix, lDoubleT | CULL_IS_REDUCED, nullptr},
                            {JAT_oticket, lDoubleT | CULL_IS_REDUCED, nullptr},
                            {JAT_fticket, lDoubleT | CULL_IS_REDUCED, nullptr},
                            {JAT_sticket, lDoubleT | CULL_IS_REDUCED, nullptr},
                            {JAT_share, lDoubleT | CULL_IS_REDUCED, nullptr},
                            {JAT_prio, lDoubleT | CULL_IS_REDUCED, nullptr},
                            {JAT_ntix, lDoubleT | CULL_IS_REDUCED, nullptr},
                            {JAT_granted_destin_identifier_list, lListT | CULL_IS_REDUCED, nullptr},
                            {NoName, lEndT | CULL_IS_REDUCED, nullptr}
                           };
      const lDescr tix2Desc[] = {
                             {JAT_task_number, lUlongT | CULL_IS_REDUCED, nullptr},
                             {JAT_tix, lDoubleT| CULL_IS_REDUCED, nullptr},
                             {JAT_oticket, lDoubleT | CULL_IS_REDUCED, nullptr},
                             {JAT_fticket, lDoubleT | CULL_IS_REDUCED, nullptr},
                             {JAT_sticket, lDoubleT | CULL_IS_REDUCED, nullptr},
                             {JAT_share, lDoubleT | CULL_IS_REDUCED, nullptr},
                             {JAT_prio, lDoubleT | CULL_IS_REDUCED, nullptr},
                             {JAT_ntix, lDoubleT | CULL_IS_REDUCED, nullptr},
                             {NoName, lEndT | CULL_IS_REDUCED, nullptr}
                            };
      const lDescr jobDesc[] = {
                                 {JB_nurg, lDoubleT | CULL_IS_REDUCED, nullptr},
                                 {JB_urg, lDoubleT | CULL_IS_REDUCED, nullptr},
                                 {JB_rrcontr, lDoubleT | CULL_IS_REDUCED, nullptr},
                                 {JB_dlcontr, lDoubleT | CULL_IS_REDUCED, nullptr},
                                 {JB_wtcontr, lDoubleT | CULL_IS_REDUCED, nullptr},
                                 {JB_ja_tasks, lListT | CULL_IS_REDUCED, nullptr},
                                 {NoName, lEndT | CULL_IS_REDUCED, nullptr}
                               };
      ja_task_pos_t *ja_pos;
      ja_task_pos_t *order_ja_pos;
      job_pos_t   *job_pos;
      job_pos_t   *order_job_pos;
      lListElem *jep = lCreateElem(jobDesc);
      lList *jlist = lCreateList("", jobDesc);

      if (order_pos == nullptr) {
         lListElem *tempElem = lCreateElem(tix2Desc);

         sge_create_cull_order_pos(&order_pos, job, ja_task, jep, tempElem);

         lFreeElem(&tempElem);
      }

      ja_pos = &(order_pos->ja_task);
      order_ja_pos = &(order_pos->order_ja_task);
      job_pos = &(order_pos->job);
      order_job_pos = &(order_pos->order_job);


      /* Create a reduced task list with only the required fields */
      {
         lList *tlist = nullptr;
         lListElem *tempElem = nullptr;

         if (update_execd){
            tlist = lCreateList("", tixDesc);
            tempElem = lCreateElem(tixDesc);
            lSetList(tempElem, JAT_granted_destin_identifier_list,
                     lCopyList("", lGetList(ja_task, JAT_granted_destin_identifier_list)));
         }
         else {
            tlist = lCreateList("", tix2Desc);
            tempElem = lCreateElem(tix2Desc);
         }

         lAppendElem(tlist, tempElem);

         lSetPosDouble(tempElem, order_ja_pos->JAT_tix_pos,     lGetPosDouble(ja_task,ja_pos->JAT_tix_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_oticket_pos, lGetPosDouble(ja_task,ja_pos->JAT_oticket_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_fticket_pos, lGetPosDouble(ja_task,ja_pos->JAT_fticket_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_sticket_pos, lGetPosDouble(ja_task,ja_pos->JAT_sticket_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_share_pos,   lGetPosDouble(ja_task,ja_pos->JAT_share_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_prio_pos,    lGetPosDouble(ja_task,ja_pos->JAT_prio_pos));
         lSetPosDouble(tempElem, order_ja_pos->JAT_ntix_pos,    lGetPosDouble(ja_task,ja_pos->JAT_ntix_pos));

         lSetList(jep, JB_ja_tasks, tlist);
      }

      /* Create a reduced job list with only the required fields */
      lAppendElem(jlist, jep);

      lSetPosDouble(jep, order_job_pos->JB_nurg_pos,    lGetPosDouble(job, job_pos->JB_nurg_pos));
      lSetPosDouble(jep, order_job_pos->JB_urg_pos,     lGetPosDouble(job, job_pos->JB_urg_pos));
      lSetPosDouble(jep, order_job_pos->JB_rrcontr_pos, lGetPosDouble(job, job_pos->JB_rrcontr_pos));
      lSetPosDouble(jep, order_job_pos->JB_dlcontr_pos, lGetPosDouble(job, job_pos->JB_dlcontr_pos));
      lSetPosDouble(jep, order_job_pos->JB_wtcontr_pos, lGetPosDouble(job, job_pos->JB_wtcontr_pos));

      lSetList(ep, OR_joker, jlist);
   }

   lSetUlong(ep, OR_type, type);
   lSetUlong(ep, OR_job_number, lGetUlong(job, JB_job_number));
   lSetUlong(ep, OR_job_version, lGetUlong(job, JB_version));
   lSetList(ep, OR_queuelist, ql);

   if (ja_task != nullptr) {
      const char *s = nullptr;

      lSetUlong(ep, OR_ja_task_number, lGetUlong(ja_task, JAT_task_number));
      s = lGetString(ja_task, JAT_granted_pe);
      if (s != nullptr) {
         lSetString(ep, OR_pe, s);
      }

      /* RSMAP: copy from JAT_granted_resources_list
       * we can not lSwapList() it - it is still needed for debiting later
       */
      lSetList(ep, OR_granted_resources_list, lCopyList(nullptr, lGetList(ja_task, JAT_granted_resources_list)));
   }

   lAppendElem(or_list, ep);

   DRETURN(or_list);
}


/**
 * @brief Sends a list of orders to qmaster
 *
 * The list is sent as one GDI request and is freed on success, so the caller
 * must not reuse it.
 *
 * @param[in]     evc    the event client the scheduler runs on, for the GDI
 *                       connection
 * @param[in,out] orders the orders to send; freed and set to nullptr
 *
 * @return `STATUS_OK`, or the error status of the GDI request
 */
int sge_send_orders2master(sge_evc_class_t *evc, lList **orders) {
   DENTER(TOP_LAYER);

   int ret = STATUS_OK;
   lList *alp = nullptr;

   int order_id = 0;
   ocs::gdi::Request gdi_multi{};

   if (*orders != nullptr) {
      DPRINTF("SENDING %d ORDERS TO QMASTER\n", lGetNumberOfElem(*orders));
      order_id = gdi_multi.request(&alp, ocs::gdi::Mode::SEND, ocs::gdi::Target::ORDER_LIST,
                                   ocs::gdi::Command::ADD, ocs::gdi::SubCommand::NONE,
                                   orders, nullptr, nullptr, false);

      if (alp != nullptr) {
         ret = answer_list_handle_request_answer_list(&alp, stderr);
         DRETURN(ret);
      }

      gdi_multi.wait();
   }

   /* check result of orders */
   if(order_id > 0) {
      gdi_multi.get_response(&alp, ocs::gdi::Command::ADD, ocs::gdi::SubCommand::NONE,
                             ocs::gdi::Target::ORDER_LIST, order_id, nullptr);

      ret = answer_list_handle_request_answer_list(&alp, stderr);
   }

   DRETURN(ret);
}


/* CS-1239: create_delete_job_orders removed - the worker thread now buries
 * the finished job inline (sge_commit_job(COMMIT_ST_FINISHED_FAILED_EE)
 * books usage and buries the job in one step), so the scheduler no longer
 * needs to emit ORT_remove_job orders. */

/**
 * @brief Generates one order list from the order structure
 *
 *  generates one order list from the order structure, and cleans the
 *  the order structure. The orders, which have been send already, are
 *  removed.
 *
 * @param orders the order strucutre
 *
 * @return a order list
 *
 * @note MT-NOTE: sge_join_orders() is not  safe
 */
lList *sge_join_orders(order_t *orders){
      lList *orderlist=nullptr;
   
      orderlist = orders->configOrderList;
      orders->configOrderList = nullptr;
  
      
      if (orderlist == nullptr) {
         orderlist = orders->jobStartOrderList;
      }
      else {
         lAddList(orderlist, &(orders->jobStartOrderList));
      }   
      orders->jobStartOrderList = nullptr;
    
      
      if (orderlist == nullptr) {
         orderlist = orders->pendingOrderList;
      }
      else {
         lAddList(orderlist, &(orders->pendingOrderList));
      }
      orders->pendingOrderList= nullptr;

      
      /* they have been send earlier, so we can remove them */
      lFreeList(&(orders->sentOrderList));

      return orderlist;
}


/**
 * @brief Returns the number of orders generated
 *
 * returns the number of orders generated
 *
 * @param orders a structure of orders
 *
 * @return number of orders in the structure
 *
 * @note MT-NOTE: sge_GetNumberOfOrders() is  MT safe
 */
int sge_GetNumberOfOrders(order_t *orders) {
   int count = 0;

   count += lGetNumberOfElem(orders->configOrderList);
   count += lGetNumberOfElem(orders->pendingOrderList);
   count += lGetNumberOfElem(orders->jobStartOrderList);
   count += lGetNumberOfElem(orders->sentOrderList);

   return count;
}
