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

/** @file
 * @brief Mirroring the share tree
 *
 * @see sge_sharetree_mirror.h
 * @see sge_mirror.h
 */

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_DataStore.h"

#include "mir/sge_mirror.h"
#include "mir/sge_sharetree_mirror.h"

/**
 * @brief Update the master sharetree list
 *
 * Update the global master list for the sharetree
 * based on an event.
 * The function is called from the event mirroring interface.
 * Sharetree events always contain the whole sharetree, that
 * replaces an existing sharetree in the master list.
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
sharetree_update_master_list(sge_evc_class_t *evc, sge_object_type type, sge_event_action action,
                             lListElem *event, void *clientdata) {
   DENTER(TOP_LAYER);

   lList **list = nullptr;
   lList *src = nullptr;

   /* remove old share tree */
   list = ocs::DataStore::get_master_list_rw(type);
   lFreeList(list);
   

   if ((src = lGetListRW(event, ET_new_version))) {
      
      /* install new one */
      *list = lCreateList("share tree", lGetElemDescr(lFirst(lGetList(event, ET_new_version))));
      lAppendElem(*list, lDechainElem(src, lFirstRW(src)));
   }

   DRETURN(SGE_EMA_OK);
}
