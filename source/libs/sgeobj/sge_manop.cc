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

#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/sge_manop.h"
#include "sgeobj/sge_userset.h"

#include <sge_conf.h>
#include <cull/sge_userset_UE_L.h>

static bool
user_list_is_group_in_list(const char *group, const lList *usr_grp_sgrp_list, int nm) {
   DENTER(TOP_LAYER);
   bool ret = false;
   dstring group_name = DSTRING_INIT;

   sge_dstring_sprintf(&group_name, "@%s", group);
   if (lGetElemStr(usr_grp_sgrp_list, nm, sge_dstring_get_string(&group_name)) != nullptr) {
      DTRACE;
      ret = true;
   }
   sge_dstring_free(&group_name);
   DRETURN(ret);
}

/**
 * @brief Returns if the user that initiated the request is part of the list.
 *
 * True is returned if the username, the primary group or one of the supplementary groups is
 * mentioned in usr_grp_sgrp_list. The nm field is used to retrieve the string from elements
 * within the list (UE_name for entries in user lists, UO_name for operator lists or UM_name
 * for the manager list).
 *
 * @param packet              GDI packet containing the ownership information of the request creator
 * @param usr_grp_sgrp_list   user list with possible group entries (prefixed with @-character)
 * @param nm                  CULL name (e.g UE_name, UM_name or UO_name)
 * @return                    false if neither the username nor one of the groups is referenced.
 */
bool
user_list_is_user_grp_sgrp_in_list(const ocs::gdi::Packet *packet, const lList *usr_grp_sgrp_list, int nm) {
   DENTER(TOP_LAYER);

   if (packet == nullptr) {
      DRETURN(false);
   }
   // check the user
   if (lGetElemStr(usr_grp_sgrp_list, nm, packet->user) != nullptr) {
      DRETURN(true);
   }
   // check the primary group (CS-493)
   if (user_list_is_group_in_list(packet->group, usr_grp_sgrp_list, nm)) {
      DRETURN(true);
   }
   // check supplementary groups if enabled
   if (packet->amount > 0 && mconf_get_enable_sup_grp_eval()) {
      for(int i = 0; i < packet->amount; i++) {
         if (user_list_is_group_in_list(packet->grp_array[i].name, usr_grp_sgrp_list, nm)) {
            DRETURN(true);
         }
      }
   }
   DRETURN(false);
}

/**
 * @brief Returns if owner of packet is referenced in a specific user list.
 *
 * Used to check if user, primary or supplementary group in specified in the user list with the
 * given name. Can be used to check if packet belongs to the e.g 'arusers', 'daedlineusers', ...
 *
 * @param packet              GDI packet containing the ownership information of the request creator
 * @param master_userset_list Master list containing all user lists
 * @param userset_name        Name of one user set (e.g "arusers", "deadlineusers", ...)
 * @return                    false if neither the username nor one of the groups is referenced.
 */
static bool
user_is_X_user(const ocs::gdi::Packet *packet, const lList *master_userset_list, const char *userset_name) {
   DENTER(TOP_LAYER);

   // find the userset
   const lListElem *userset = lGetElemStr(master_userset_list, US_name, userset_name);
   if (userset == nullptr) {
      DRETURN(false);
   }

   // text if it has enntries
   const lList *user_entries_list = lGetList(userset, US_entries);
   if (user_entries_list == nullptr) {
      DRETURN(false);
   }

   // identify if user, primary grp or a sup-grp is part of that list
   if (user_list_is_user_grp_sgrp_in_list(packet, user_entries_list, UE_name)) {
      DRETURN(true);
   }

   DRETURN(false);
}

/**
 * Was packet initiated by a user referenced in the 'arusers'
 *
 * @param packet              GDI packet containing the ownership information of the request creator
 * @param master_userset_list Master list containing all user lists
 * @return                    True if the user or one of the groups to which the packet belongs is
 *                            referenced in the "arusers"
 */
bool
user_is_ar_user(const ocs::gdi::Packet *packet, const lList *master_userset_list) {
   return user_is_X_user(packet, master_userset_list, AR_USERS);
}

/**
 * Was packet initiated by a user referenced in the 'deadlineusers'?
 *
 * @param packet              GDI packet containing the ownership information of the request creator
 * @param master_userset_list Master list containing all user lists
 * @return                    True if the user or one of the groups to which the packet belongs is
 *                            referenced in the "deadlineusers"
 */
bool
user_is_deadline_user(const ocs::gdi::Packet *packet, const lList *master_userset_list) {
   return user_is_X_user(packet, master_userset_list, DEADLINE_USERS);
}

/**
 * Was the packet initiated by a manager?
 *
 * @param packet              GDI packet containing the ownership information of the request creator
 * @param master_manager_list Master manager list
 * @return                    True if packet initiator was a manager.
 */
bool
manop_is_manager(const ocs::gdi::Packet *packet, const lList *master_manager_list) {
   DENTER(TOP_LAYER);

   if (user_list_is_user_grp_sgrp_in_list(packet, master_manager_list, UM_name)) {
      DRETURN(true);
   }
   DRETURN(false);
}

/**
 * Was the packet initiated by an operator?
 *
 * @param packet              GDI packet containing the ownership information of the request creator
 * @param master_manager_list Master manager list
 * @return                    True if packet initiator was an operator.
 */
bool
manop_is_operator(const ocs::gdi::Packet *packet, const lList *master_manager_list, const lList *master_operator_list) {
   DENTER(TOP_LAYER);

   if (user_list_is_user_grp_sgrp_in_list(packet, master_operator_list, UO_name)) {
      DRETURN(true);
   }

   // managers are automatically operators
   if (manop_is_manager(packet, master_manager_list)) {
      DRETURN(true);
   }
   DRETURN(false);
}
