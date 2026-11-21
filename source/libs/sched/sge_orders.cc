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

   if (!sme || (sme->get<SME_message_list>().empty() && sme->get<SME_global_message_list>().empty())) {
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
   ep->set<OR_joker>(std::move(jlist));

   ep->set<OR_type>(ORT_job_schedd_info);
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
         qslots = gel.get<JG_slots>();
         if (qslots) { /* ignore Qs with slots==0 */
            auto ep2 = ocs::tom::ElementPtr<ocs::tom::OQ>::create();
            ep2->set<OQ_slots>(qslots);
            ep2->set<OQ_binding_to_use>(gel.get<JG_binding_to_use>().clone("binding_to_use"));
            ep2->set<OQ_dest_queue>(gel.get<JG_qname>());
            ep2->set<OQ_dest_version>(gel.get<JG_qversion>());
            ep2->set<OQ_ticket>(gel.get<JG_ticket>());
            ep2->set<OQ_oticket>(gel.get<JG_oticket>());
            ep2->set<OQ_fticket>(gel.get<JG_fticket>());
            ep2->set<OQ_sticket>(gel.get<JG_sticket>());
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
      ep->set<OR_ticket>(ja_task.get<JAT_tix>());
      ep->set<OR_ntix>(ja_task.get<JAT_ntix>());
      ep->set<OR_prio>(ja_task.get<JAT_prio>());
   }

   if (type == ORT_tickets || type == ORT_ptickets) {

      // built once for the life of the process, so no use allocates
      const auto tix_layout = ocs::tom::layout<JAT_task_number, JAT_tix, JAT_oticket, JAT_fticket, JAT_sticket,
                                               JAT_share, JAT_prio, JAT_ntix, JAT_granted_destin_identifier_list>();
      const auto tix2_layout = ocs::tom::layout<JAT_task_number, JAT_tix, JAT_oticket, JAT_fticket, JAT_sticket,
                                                JAT_share, JAT_prio, JAT_ntix>();
      const auto job_layout = ocs::tom::layout<JB_nurg, JB_urg, JB_rrcontr, JB_dlcontr, JB_wtcontr, JB_ja_tasks>();
      // the job and its task are reductions to the fields an order carries
      auto jep = ocs::tom::ElementPtr<ocs::tom::JB>::create(job_layout);
      auto jlist = ocs::tom::ListPtr<ocs::tom::JB>::create("", job_layout);

      /* Create a reduced task list with only the required fields */
      {
         const auto task_layout = update_execd ? tix_layout : tix2_layout;
         auto tlist = ocs::tom::ListPtr<ocs::tom::JAT>::create("", task_layout);
         auto new_task = ocs::tom::ElementPtr<ocs::tom::JAT>::create(task_layout);
         if (update_execd) {
            new_task->set<JAT_granted_destin_identifier_list>(
               ja_task.get<JAT_granted_destin_identifier_list>().clone(""));
         }
         const auto tempElem = tlist->push_back(std::move(new_task));

         tempElem.set<JAT_tix>(ja_task.get<JAT_tix>());
         tempElem.set<JAT_oticket>(ja_task.get<JAT_oticket>());
         tempElem.set<JAT_fticket>(ja_task.get<JAT_fticket>());
         tempElem.set<JAT_sticket>(ja_task.get<JAT_sticket>());
         tempElem.set<JAT_share>(ja_task.get<JAT_share>());
         tempElem.set<JAT_prio>(ja_task.get<JAT_prio>());
         tempElem.set<JAT_ntix>(ja_task.get<JAT_ntix>());

         jep->set<JB_ja_tasks>(std::move(tlist));
      }

      /* Create a reduced job list with only the required fields */
      const auto order_job = jlist->push_back(std::move(jep));
      order_job.set<JB_nurg>(job.get<JB_nurg>());
      order_job.set<JB_urg>(job.get<JB_urg>());
      order_job.set<JB_rrcontr>(job.get<JB_rrcontr>());
      order_job.set<JB_dlcontr>(job.get<JB_dlcontr>());
      order_job.set<JB_wtcontr>(job.get<JB_wtcontr>());
      // the joker of an order holds a list of any type (CS-3102); it takes the owner of any type
      ep->set<OR_joker>(std::move(jlist));
   }

   ep->set<OR_type>(type);
   ep->set<OR_job_number>(job.get<JB_job_number>());
   ep->set<OR_job_version>(job.get<JB_version>());
   ep->set<OR_queuelist>(std::move(ql));

   if (ja_task != nullptr) {
      const char *s = nullptr;

      ep->set<OR_ja_task_number>(ja_task.get<JAT_task_number>());
      s = ja_task.get<JAT_granted_pe>();
      if (s != nullptr) {
         ep->set<OR_pe>(s);
      }

      /* RSMAP: copy from JAT_granted_resources_list
       * we can not lSwapList() it - it is still needed for debiting later
       */
      ep->set<OR_granted_resources_list>(ja_task.get<JAT_granted_resources_list>().clone());
   }

   or_list->push_back(std::move(ep));

   DRETURN_VOID;
}


/* CS-1239: create_delete_job_orders removed - the worker thread now buries
 * the finished job inline (sge_commit_job(COMMIT_ST_FINISHED_FAILED_EE)
 * books usage and buries the job in one step), so the scheduler no longer
 * needs to emit ORT_remove_job orders. */

