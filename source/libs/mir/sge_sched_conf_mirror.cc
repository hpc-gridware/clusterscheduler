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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Mirroring the scheduler configuration
 *
 * @see sge_sched_conf_mirror.h
 * @see sge_mirror.h
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/sge_schedd_conf.h"
#include "sgeobj/sge_answer.h"

#include "mir/sge_mirror.h"
#include "mir/sge_sched_conf_mirror.h"

/**
 * @brief Update the scheduler configuration
 *
 * Update the global master list of scheduler configurations
 * based on an event.
 * The function is called from the event mirroring interface.
 * The list only contains one element that is replaced when a
 * modify event arrives.
 *
 * @param evc the event client the event arrived on
 * @param type event type
 * @param action action to perform
 * @param event the raw event
 * @param clientdata client data
 *
 * @return true, if update is successful, else false
 *
 * @note The function should only be called from the event mirror interface.
 */
sge_callback_result
schedd_conf_update_master_list(sge_evc_class_t *evc, sge_object_type type,
                               sge_event_action action, lListElem *event, void *clientdata) {
   DENTER(TOP_LAYER);

   lList *list = nullptr;
   lList *answer_list = nullptr;
   lDescr *list_descr;

   lList *data_list;
   lListElem *ep = nullptr;

   list_descr = SC_Type;

   if ((data_list = lGetListRW(event, ET_new_version)) != nullptr) {
      if ((ep = lFirstRW(data_list)) != nullptr) {
         ep = lDechainElem(data_list, ep);
      }
   }

   /* if neccessary, create list and copy schedd info */
   if (ep != nullptr) {
      list = lCreateList("schedd config", list_descr);
      lAppendElem(list, ep);
   }

   if (!sconf_set_config(&list, &answer_list)) {
      lFreeList(&list);
      answer_list_output(&answer_list);
   }

   DRETURN(SGE_EMA_OK);
}
