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

#include <utility>

#include "uti/sge_rmon_macros.h"

#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/tom/ocs_tom_STN.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

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
 * @param type event type; always SGE_TYPE_SHARETREE, the mirror table calls this function for it only
 * @param action action to perform
 * @param event the event (ET_Type); its share tree list is taken over into the master list
 * @param clientdata client data
 *
 * @return always SGE_EMA_OK
 *
 * @note The function should only be called from the event mirror interface.
 */
sge_callback_result
sharetree_update_master_list([[maybe_unused]] sge_evc_class_t *evc, [[maybe_unused]] const sge_object_type type,
                             [[maybe_unused]] const sge_event_action action,
                             const ocs::tom::Element<ocs::tom::ET> event, [[maybe_unused]] void *clientdata) {
   DENTER(TOP_LAYER);

   // the event carries the whole share tree (its root node) and the master list takes the event's list over;
   // an event without a tree leaves no tree
   auto tree = ocs_mirror_exchange_event_objects<ocs::tom::STN>(event, ocs::tom::ListPtr<ocs::tom::STN>());
   if (tree && tree->empty()) {
      tree.reset();
   }

   // the data store slot of the share tree (never nullptr) gets the new tree; the old one is freed
   ocs::DataStore::replace_master_list<SGE_TYPE_SHARETREE>(std::move(tree));

   DRETURN(SGE_EMA_OK);
}
