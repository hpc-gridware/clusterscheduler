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
 * @brief Mirroring the cluster queues and their instances
 *
 * Queue instances are a sublist of their cluster queue, which is why both live
 * in one file.
 *
 * @see sge_queue_mirror.h
 * @see sge_mirror.h
 */

#include "uti/sge.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/ocs_DataStore.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"
#include "sgeobj/tom/ocs_tom_CQ.h"
#include "sgeobj/tom/ocs_tom_QU.h"

#include "mir/msg_mirlib.h"
#include "mir/sge_mirror.h"
#include "mir/sge_queue_mirror.h"

/**
 * @brief Update the master list of cluster queues
 *
 * @param evc the event client the event arrived on
 * @param type event type
 * @param action action to perform
 * @param event the event (ET); its object is taken over into the master list
 * @param clientdata client data
 *
 * @return SGE_EMA_OK if the update is successful, else SGE_EMA_FAILURE
 *
 * @note The function should only be called from the event mirror interface.
 */
sge_callback_result
cqueue_update_master_list([[maybe_unused]] sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                          const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event,
                          [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);
   sge_callback_result ret = SGE_EMA_OK;
   // the queue instances of the cluster queue, kept across the event
   ocs::tom::ListPtr<ocs::tom::QU> qinstance_list;

   const char *name = event.get<ET_strkey>();
   // a cluster queue of the master list (CQ)
   const auto cqueue = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>().find_first<CQ_name>(name);

   if ((action == SGE_EMA_MOD || action == SGE_EMA_ADD) && cqueue) {
      /*
       * modify events for CQ objects; we may not update
       * - CQ_qinstances - it is maintained by QINSTANCE events
       */
      qinstance_list = cqueue.exchange<CQ_qinstances>(std::move(qinstance_list));
   }

   if (ocs_mirror_update_data_store_list<SGE_TYPE_CQUEUE>(cqueue, name, action, event) == SGE_EM_OK) {
      ret = (sge_callback_result)(ret & SGE_EMA_OK);
   } else {
      ret = (sge_callback_result)(ret & SGE_EMA_FAILURE);
   }

   // the element the event brought in, or the old one if the merge failed
   const auto new_cqueue = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>().find_first<CQ_name>(name);

   if ((action == SGE_EMA_MOD || action == SGE_EMA_ADD) && new_cqueue) {
      /*
       * Replace CQ_qinstances list
       */
      qinstance_list = new_cqueue.exchange<CQ_qinstances>(std::move(qinstance_list));
      qinstance_list.reset();
   }

   DRETURN(ret);
}

/**
 * @brief Apply a queue instance event to the cluster queue that owns it
 *
 * Queue instances are a sublist of their cluster queue, so an event about one
 * is applied by finding its cluster queue first.
 *
 * @param evc the event client the event arrived on
 * @param type event type
 * @param action action to perform
 * @param event the event (ET); its object is taken over into the master list
 * @param clientdata client data
 *
 * @return SGE_EMA_OK if the update is successful, else SGE_EMA_FAILURE
 *
 * @note The function should only be called from the event mirror interface.
 */
sge_callback_result
qinstance_update_cqueue_list([[maybe_unused]] sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                             const sge_event_action action, const ocs::tom::Element<ocs::tom::ET> event,
                             [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   sge_callback_result ret = SGE_EMA_OK;

   const char *name = event.get<ET_strkey>();
   const char *hostname = event.get<ET_strkey2>();

   // a cluster queue of the master list (CQ)
   const auto cqueue = ocs::DataStore::master_list_rw<SGE_TYPE_CQUEUE>().find_first<CQ_name>(name);

   if (cqueue) {
      dstring key_buffer = DSTRING_INIT;
      const auto qinstance = qinstance_list_locate(cqueue.get<CQ_qinstances>(), hostname, nullptr);
      const char *key = nullptr;

      sge_dstring_sprintf(&key_buffer, SFN "@" SFN, name, hostname);
      key = sge_dstring_get_string(&key_buffer);

      if (action == SGE_EMA_MOD) {
         uint32_t type = event.get<ET_type>();

         if (type == sgeE_QINSTANCE_SOS ||
             type == sgeE_QINSTANCE_USOS) {
            if (qinstance) {
               if (type == sgeE_QINSTANCE_SOS) {
                  qinstance_state_set_susp_on_sub(qinstance, true);
               } else {
                  qinstance_state_set_susp_on_sub(qinstance, false);
               }
            } else {
               ERROR(MSG_QINSTANCE_CANTFINDFORUPDATEIN_SS, key, __func__);
               ret = SGE_EMA_FAILURE;
            }
            sge_dstring_free(&key_buffer);
            DRETURN(ret);
         }
      }
      // a list created for a cluster queue without one goes into CQ_qinstances
      if (ocs_mirror_update_sub_list<CQ_qinstances>(cqueue, qinstance, key, action, event) == SGE_EM_OK) {
         ret = (sge_callback_result)(ret & SGE_EMA_OK);
      } else {
         ret = (sge_callback_result)(ret & SGE_EMA_FAILURE);
      }
      sge_dstring_free(&key_buffer);
   } else {
      ERROR(MSG_CQUEUE_CANTFINDFORUPDATEIN_SS, name, __func__);
      ret = SGE_EMA_FAILURE;
   }

   DRETURN(ret);
}
