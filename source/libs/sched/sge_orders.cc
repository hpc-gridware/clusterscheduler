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
#include "cull/ocs_cull_descr.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_order.h"
#include "sgeobj/sge_mesobj.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_ja_task.h"
#include "tom/ocs_Element.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_JG.h"
#include "sgeobj/tom/ocs_tom_MES.h"
#include "sgeobj/tom/ocs_tom_ST.h"
#include "sgeobj/tom/ocs_tom_OR.h"
#include "sgeobj/tom/ocs_tom_SME.h"
#include "sgeobj/tom/ocs_tom_GRU.h"
#include "sgeobj/tom/ocs_tom_JAT.h"
#include "sgeobj/tom/ocs_tom_JB.h"
#include "sgeobj/tom/ocs_tom_OQ.h"
#include "tom/ocs_List.h"

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
 * @param[in,out] or_list the order list to which the message order is added; created if it is empty
 * @param[out] global_mes_count global message count
 * @param[out] job_mes_count job message count
 *
 * @note MT-NOTE: sge_add_schedd_info() is not MT safe
 */
void sge_add_schedd_info(ocs::tom::ListPtr<ocs::tom::OR> &or_list, int *global_mes_count, int *job_mes_count) {
   DENTER(TOP_LAYER);

   auto sme = schedd_mes_obtain_package(global_mes_count, job_mes_count);

   if (!sme || (lGetNumberOfElem(ocs::tom::get<SME_message_list>(sme)) < 1
         && lGetNumberOfElem(ocs::tom::get<SME_global_message_list>(sme)) < 1)) {
      DRETURN_VOID;
   }

   /* create orders list if not existent */
   if (!or_list) {
      or_list = ocs::tom::ListPtr<ocs::tom::OR>::create("orderlist");
   }

   /* build order */
   auto ep=ocs::tom::ElementPtr<ocs::tom::OR>::create();

   auto jlist = ocs::tom::ListPtr<ocs::tom::SME>::create("");
   jlist->push_back(std::move(sme));
   ocs::tom::set<OR_joker>(ep, std::move(jlist));

   ocs::tom::set<OR_type>(ep, ORT_job_schedd_info);
   or_list->push_back(std::move(ep));

   DRETURN_VOID;
}

/**
 * @brief Create a new order-list or add orders to an existing one
 *
 * - If the or_list is nullptr, a new one will be generated
 * - in case of a clear_pri order, the ja_task is important. If nullptr is put
 *   in for ja_task, only the pending tasks of the specified job are set to nullptr.
 *   If a ja_task is put in, all tasks of the job are set to nullptr
 *
 * @param[in,out] or_list the order list; created if it is empty, freed and emptied if @p job is empty
 * @param type order type
 * @param job job
 * @param ja_task ja_task ref or nullptr(there is only one case, where it can be nullptr)
 * @param granted granted queue list
 * @param update_execd should the execd get new ticket values?
 *
 * @note MT-NOTE: sge_create_orders() is MT safe
 */
void sge_create_orders(ocs::tom::ListPtr<ocs::tom::OR> &or_list, const uint32_t type,
                       const ocs::tom::ConstElement<ocs::tom::JB> job,
                       const ocs::tom::ConstElement<ocs::tom::JAT> ja_task,
                       const ocs::tom::ConstList<ocs::tom::JG> granted, const bool update_execd) {
   DENTER(TOP_LAYER);
   ocs::tom::ListPtr<ocs::tom::OQ> ql;
   uint32_t qslots;

   if (job == nullptr) {
      or_list.reset();
      DRETURN_VOID;
   }

   /* create orders list if not existent */
   if (!or_list) {
      or_list = ocs::tom::ListPtr<ocs::tom::OR>::create("orderlist");
   }

   /* build sublist of granted */
   if (update_execd) {
      for (const auto gel : granted) {
         qslots = ocs::tom::get<JG_slots>(gel);
         if (qslots) { /* ignore Qs with slots==0 */
            auto ep2 = ocs::tom::ElementPtr<ocs::tom::OQ>::create();
            ocs::tom::set<OQ_slots>(ep2, qslots);
            ocs::tom::set<OQ_binding_to_use>(ep2, ocs::tom::get<JG_binding_to_use>(gel).clone("binding_to_use"));
            ocs::tom::set<OQ_dest_queue>(ep2, ocs::tom::get<JG_qname>(gel));
            ocs::tom::set<OQ_dest_version>(ep2, ocs::tom::get<JG_qversion>(gel));
            ocs::tom::set<OQ_ticket>(ep2, ocs::tom::get<JG_ticket>(gel));
            ocs::tom::set<OQ_oticket>(ep2, ocs::tom::get<JG_oticket>(gel));
            ocs::tom::set<OQ_fticket>(ep2, ocs::tom::get<JG_fticket>(gel));
            ocs::tom::set<OQ_sticket>(ep2, ocs::tom::get<JG_sticket>(gel));
            if (ql == nullptr) {
               ql = ocs::tom::ListPtr<ocs::tom::OQ>::create("orderlist");
            }
            ql->push_back(std::move(ep2));
         }
      }
   }

   /* build order */
   auto ep=ocs::tom::ElementPtr<ocs::tom::OR>::create();

   if(ja_task != nullptr) {
      ocs::tom::set<OR_ticket>(ep,    ocs::tom::get<JAT_tix>(ja_task));
      ocs::tom::set<OR_ntix>(ep,      ocs::tom::get<JAT_ntix>(ja_task));
      ocs::tom::set<OR_prio>(ep,      ocs::tom::get<JAT_prio>(ja_task));
   }

   if (type == ORT_tickets || type == ORT_ptickets) {

      // built once and never released, so every use only takes a reference
      static const lDescr *const tixDesc =
         ocs_cull_descr_reduce(JAT_Type, {JAT_task_number, JAT_tix, JAT_oticket, JAT_fticket, JAT_sticket, JAT_share,
                                          JAT_prio, JAT_ntix, JAT_granted_destin_identifier_list});
      static const lDescr *const tix2Desc =
         ocs_cull_descr_reduce(JAT_Type, {JAT_task_number, JAT_tix, JAT_oticket, JAT_fticket, JAT_sticket, JAT_share,
                                          JAT_prio, JAT_ntix});
      static const lDescr *const jobDesc =
         ocs_cull_descr_reduce(JB_Type, {JB_nurg, JB_urg, JB_rrcontr, JB_dlcontr, JB_wtcontr, JB_ja_tasks});
      // the job and its task are reductions to the fields an order carries
      auto jep = ocs::tom::ElementPtr<ocs::tom::JB>::from_typed(lCreateElem(jobDesc));
      auto jlist = ocs::tom::ListPtr<ocs::tom::JB>::from_typed(lCreateList("", jobDesc));

      /* Create a reduced task list with only the required fields */
      {
         const lDescr *const task_descr = update_execd ? tixDesc : tix2Desc;
         auto tlist = ocs::tom::ListPtr<ocs::tom::JAT>::from_typed(lCreateList("", task_descr));
         auto new_task = ocs::tom::ElementPtr<ocs::tom::JAT>::from_typed(lCreateElem(task_descr));
         if (update_execd) {
            ocs::tom::set<JAT_granted_destin_identifier_list>(
               new_task, ocs::tom::get<JAT_granted_destin_identifier_list>(ja_task).clone(""));
         }
         const auto tempElem = tlist->push_back(std::move(new_task));

         ocs::tom::set<JAT_tix>(tempElem, ocs::tom::get<JAT_tix>(ja_task));
         ocs::tom::set<JAT_oticket>(tempElem, ocs::tom::get<JAT_oticket>(ja_task));
         ocs::tom::set<JAT_fticket>(tempElem, ocs::tom::get<JAT_fticket>(ja_task));
         ocs::tom::set<JAT_sticket>(tempElem, ocs::tom::get<JAT_sticket>(ja_task));
         ocs::tom::set<JAT_share>(tempElem, ocs::tom::get<JAT_share>(ja_task));
         ocs::tom::set<JAT_prio>(tempElem, ocs::tom::get<JAT_prio>(ja_task));
         ocs::tom::set<JAT_ntix>(tempElem, ocs::tom::get<JAT_ntix>(ja_task));

         ocs::tom::set<JB_ja_tasks>(jep, std::move(tlist));
      }

      /* Create a reduced job list with only the required fields */
      const auto order_job = jlist->push_back(std::move(jep));
      ocs::tom::set<JB_nurg>(order_job, ocs::tom::get<JB_nurg>(job));
      ocs::tom::set<JB_urg>(order_job, ocs::tom::get<JB_urg>(job));
      ocs::tom::set<JB_rrcontr>(order_job, ocs::tom::get<JB_rrcontr>(job));
      ocs::tom::set<JB_dlcontr>(order_job, ocs::tom::get<JB_dlcontr>(job));
      ocs::tom::set<JB_wtcontr>(order_job, ocs::tom::get<JB_wtcontr>(job));
      // the joker of an order holds a list of any type (CS-3102), so it takes the raw list
      ocs::tom::set<OR_joker>(ep, jlist.release());
   }

   ocs::tom::set<OR_type>(ep, type);
   ocs::tom::set<OR_job_number>(ep, ocs::tom::get<JB_job_number>(job));
   ocs::tom::set<OR_job_version>(ep, ocs::tom::get<JB_version>(job));
   ocs::tom::set<OR_queuelist>(ep, std::move(ql));

   if (ja_task != nullptr) {
      const char *s = nullptr;

      ocs::tom::set<OR_ja_task_number>(ep, ocs::tom::get<JAT_task_number>(ja_task));
      s = ocs::tom::get<JAT_granted_pe>(ja_task);
      if (s != nullptr) {
         ocs::tom::set<OR_pe>(ep, s);
      }

      /* RSMAP: copy from JAT_granted_resources_list
       * we can not lSwapList() it - it is still needed for debiting later
       */
      ocs::tom::set<OR_granted_resources_list>(ep, ocs::tom::get<JAT_granted_resources_list>(ja_task).clone());
   }

   or_list->push_back(std::move(ep));

   DRETURN_VOID;
}


/* CS-1239: create_delete_job_orders removed - the worker thread now buries
 * the finished job inline (sge_commit_job(COMMIT_ST_FINISHED_FAILED_EE)
 * books usage and buries the job in one step), so the scheduler no longer
 * needs to emit ORT_remove_job orders. */

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
int sge_GetNumberOfOrders(const order_t *orders) {
   DENTER(TOP_LAYER);

   std::size_t count = 0;

   count += orders->configOrderList->size();
   count += orders->pendingOrderList->size();
   count += orders->jobStartOrderList->size();
   count += orders->sentOrderList->size();

   DRETURN(static_cast<int>(count));
}
