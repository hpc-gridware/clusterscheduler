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

#include "uti/sge.h"

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_object.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/ocs_DataStore.h"

#include "mir/msg_mirlib.h"
#include "mir/sge_mirror.h"
#include "mir/sge_queue_mirror.h"

sge_callback_result
cqueue_update_master_list(sge_evc_class_t *evc, sge_object_type type, 
                          sge_event_action action, lListElem *event, void *clientdata)
{
   DENTER(TOP_LAYER);
   sge_callback_result ret = SGE_EMA_OK;
   const char *name = nullptr;
   lList *qinstance_list = nullptr;
   lListElem *cqueue = nullptr;
   lList **list = nullptr;
   const lDescr *list_descr = nullptr;

   name = lGetString(event, ET_strkey);
   list = ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);
   list_descr = lGetListDescr(lGetList(event, ET_new_version));
   cqueue = cqueue_list_locate(*list, name);

   if ((action == SGE_EMA_MOD || action == SGE_EMA_ADD) 
       && cqueue != nullptr) {
      /*
       * modify events for CQ_Type objects; we may not update
       * - CQ_qinstances - it is maintained by QINSTANCE events
       */         
      lXchgList(cqueue, CQ_qinstances, &qinstance_list);
   }
   
   if (sge_mirror_update_master_list(list, list_descr, cqueue, name, action, event) == SGE_EM_OK) {
      ret = (sge_callback_result)(ret & SGE_EMA_OK);
   } else {
      ret = (sge_callback_result)(ret & SGE_EMA_FAILURE);
   }

   cqueue = cqueue_list_locate(*list, name);

   if ((action == SGE_EMA_MOD || action == SGE_EMA_ADD)
       && cqueue != nullptr) {
      /*
       * Replace CQ_qinstances list
       */         
      lXchgList(cqueue, CQ_qinstances, &qinstance_list);
      lFreeList(&qinstance_list);
   }

   DRETURN(ret);
}

sge_callback_result
qinstance_update_cqueue_list(sge_evc_class_t *evc, sge_object_type type, 
                             sge_event_action action, lListElem *event, void *clientdata)
{
   sge_callback_result ret = SGE_EMA_OK;
   const char *name = nullptr;
   const char *hostname = nullptr;
   lListElem *cqueue = nullptr;

   DENTER(TOP_LAYER);
   name = lGetString(event, ET_strkey);
   hostname = lGetString(event, ET_strkey2);

   cqueue = cqueue_list_locate(*ocs::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE), name);
                        
   if (cqueue != nullptr) {
      dstring key_buffer = DSTRING_INIT;
      lList *list = lGetListRW(cqueue, CQ_qinstances);
      const lDescr *list_descr = lGetListDescr(lGetList(event, ET_new_version));
      
      lListElem *qinstance = qinstance_list_locate(list, hostname, nullptr);
      const char *key = nullptr;
      bool is_list = list != nullptr ? true : false;
      
      sge_dstring_sprintf(&key_buffer, SFN "@" SFN, name, hostname);
      key = sge_dstring_get_string(&key_buffer);

      if (action == SGE_EMA_MOD) {
         u_long32 type = lGetUlong(event, ET_type);

         if (type == sgeE_QINSTANCE_SOS || 
             type == sgeE_QINSTANCE_USOS) {
            if (qinstance != nullptr) {
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
      if (sge_mirror_update_master_list(&list, list_descr, qinstance, key, action, event) == SGE_EM_OK) {
         ret = (sge_callback_result)(ret & SGE_EMA_OK);
      } else {
         ret = (sge_callback_result)(ret & SGE_EMA_FAILURE);
      }
      sge_dstring_free(&key_buffer);
      if (!is_list) {
         lSetList(cqueue, CQ_qinstances, list);
      }
   } else {
      ERROR(MSG_CQUEUE_CANTFINDFORUPDATEIN_SS, name, __func__);
      ret = SGE_EMA_FAILURE;
   }

   DRETURN(ret);
}
