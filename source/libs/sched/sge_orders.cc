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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <stdio.h>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_order.h"
#include "sgeobj/sge_feature.h"

#include "evc/sge_event_client.h"

#include "gdi/sge_gdi.h"

#include "sge_orders.h"
#include "sgeobj/cull/sge_all_listsL.h"
#include "schedd_message.h"

#include "msg_schedd.h"

/****** sge_orders/sge_add_schedd_info() ***************************************
*  NAME
*     sge_add_schedd_info() -- retrieves the messages and generates an order out
*                              of it.
*
*  SYNOPSIS
*     lList* sge_add_schedd_info(lList *or_list, int *global_mes_count, int
*     *job_mes_count)
*
*  FUNCTION
*     retrieves all messages, puts them into an order package, and frees the
*     orginal messages. It also returns the number of global and job messages.
*
*  INPUTS
*     lList *or_list        - int: the order list to which the message order is added
*     int *global_mes_count - out: global message count
*     int *job_mes_count    - out: job message count
*
*  RESULT
*     lList* - the order list
*
*  NOTES
*     MT-NOTE: sge_add_schedd_info() is not MT safe
*
*******************************************************************************/
lList *sge_add_schedd_info(lList *or_list, int *global_mes_count, int *job_mes_count)
{
   lList *jlist;
   lListElem *sme, *ep;

   DENTER(TOP_LAYER);

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


/****** sge_orders/sge_create_orders() *****************************************
*  NAME
*     sge_create_orders() -- Create a new order-list or add orders to an existing one
*
*  SYNOPSIS
*     lList* sge_create_orders(lList *or_list, u_long32 type, lListElem *job,
*     lListElem *ja_task, lList *granted, bool update_execd)
*
*  FUNCTION
*     - If the or_list is nullptr, a new one will be generated
*
*     - in case of a clear_pri order, teh ja_task is improtant. If nullptr is put
*       in for ja_task, only the pendin tasks of the spedified job are set to nullptr.
*       If a ja_task is put in, all tasks of the job are set to nullptr
*
*  INPUTS
*     lList *or_list     - the order list
*     u_long32 type      - order type
*     lListElem *job     - job
*     lListElem *ja_task - ja_task ref or nullptr(there is only one case, where it can be nullptr)
*     lList *granted     - granted queue list
*     bool update_execd  - should the execd get new ticket values?
*
*  RESULT
*     lList* - returns the orderlist
*
*  NOTES
*     MT-NOTE: sge_create_orders() is MT safe
*
*  SEE ALSO
*     ???/???
*******************************************************************************/
lList
*sge_create_orders(lList *or_list, u_long32 type, const lListElem *job, const lListElem *ja_task,
                   const lList *granted , bool update_execd)
{
   lList *ql = nullptr;
   const lListElem *gel;
   lListElem *ep, *ep2;
   u_long32 qslots;

   DENTER(TOP_LAYER);

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
      for_each_ep(gel, granted) {
         qslots = lGetUlong(gel, JG_slots);
         if (qslots) { /* ignore Qs with slots==0 */
            ep2=lCreateElem(OQ_Type);

            lSetUlong(ep2, OQ_slots, qslots);
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
                                 {JB_nppri, lDoubleT | CULL_IS_REDUCED, nullptr},
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

      lSetPosDouble(jep, order_job_pos->JB_nppri_pos,   lGetPosDouble(job, job_pos->JB_nppri_pos));
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

      /* RSMAP: copy from JAT_granted_resources_list */
      lSwapList(ep, OR_granted_resources_list, (lListElem *)ja_task, JAT_granted_resources_list);
   }

   lAppendElem(or_list, ep);

   DRETURN(or_list);
}


/*************************************************************

 *************************************************************/
int
sge_send_orders2master(sge_evc_class_t *evc, lList **orders)
{
   int ret = STATUS_OK;
   lList *alp = nullptr;
   lList *malp = nullptr;

   int order_id = 0;
   state_gdi_multi state = STATE_GDI_MULTI_INIT;

   DENTER(TOP_LAYER);

   if (*orders != nullptr) {
      DPRINTF(("SENDING %d ORDERS TO QMASTER\n", lGetNumberOfElem(*orders)));
      order_id = sge_gdi_multi(&alp, SGE_GDI_SEND, SGE_ORDER_LIST, SGE_GDI_ADD,
                               orders, nullptr, nullptr, &state, false);
      sge_gdi_wait(&malp, &state);

      if (alp != nullptr) {
         ret = answer_list_handle_request_answer_list(&alp, stderr);
         DRETURN(ret);
      }
   }   

   /* check result of orders */
   if(order_id > 0) {
      gdi_extract_answer(&alp, SGE_GDI_ADD, SGE_ORDER_LIST, order_id, malp, nullptr);

      ret = answer_list_handle_request_answer_list(&alp, stderr);
   }

   lFreeList(&malp);
   DRETURN(ret);
}



/*--------------------------------------------------------------------
 * build a ORT_remove_job order for each finished job 
 *--------------------------------------------------------------------*/
lList *create_delete_job_orders(
lList *finished_jobs, 
lList *order_list  
) {
   const lListElem *job, *ja_task;

   DENTER(TOP_LAYER);

   for_each_ep(job, finished_jobs) {
      for_each_ep(ja_task, lGetList(job, JB_ja_tasks)) {
         DPRINTF(("DELETE JOB " sge_u32"." sge_u32"\n", lGetUlong(job, JB_job_number),
            lGetUlong(ja_task, JAT_task_number)));
         order_list = sge_create_orders(order_list, ORT_remove_job, job, 
            ja_task, nullptr, true);
      }
   }

   DRETURN(order_list);
}



/****** sge_orders/sge_join_orders() ******************************************
*  NAME
*     sge_join_orders() -- generates one order list from the order structure 
*
*  SYNOPSIS
*     lLlist* sge_join_orders(order_t orders) 
*
*  FUNCTION
*      generates one order list from the order structure, and cleans the
*      the order structure. The orders, which have been send already, are
*      removed.
*
*  INPUTS
*     order_t orders - the order strucutre
*
*  RESULT
*     lLlist* - a order list
*
*  NOTES
*     MT-NOTE: sge_join_orders() is not  safe 
*
*******************************************************************************/
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


/****** sge_orders/sge_GetNumberOfOrders() *************************************
*  NAME
*     sge_GetNumberOfOrders() -- returns the number of orders generated
*
*  SYNOPSIS
*     int sge_GetNumberOfOrders(order_t *orders) 
*
*  FUNCTION
*     returns the number of orders generated
*
*  INPUTS
*     order_t *orders - a structure of orders
*
*  RESULT
*     int - number of orders in the structure
*
*  NOTES
*     MT-NOTE: sge_GetNumberOfOrders() is  MT safe 
*
*******************************************************************************/
int sge_GetNumberOfOrders(order_t *orders) {
   int count = 0;

   count += lGetNumberOfElem(orders->configOrderList);
   count += lGetNumberOfElem(orders->pendingOrderList);
   count += lGetNumberOfElem(orders->jobStartOrderList);
   count += lGetNumberOfElem(orders->sentOrderList);

   return count;
}
