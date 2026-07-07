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
 *  The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *  Copyright: 2001 by Sun Microsystems, Inc.
 *
 *  All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/ocs_DataStore.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/cull/sge_userset_US_L.h"
#include "sgeobj/cull/sge_userset_UE_L.h"

#include "spool/sge_spooling.h"

#include "sge_persistence_qmaster.h"
#include "sge_manop_qmaster.h"
#include "evm/sge_event_master.h"
#include "msg_common.h"
#include "msg_qmaster.h"
#include "ocs_Bootstrap.h"

/**
 * @brief Add a manager or operator.
 *
 * Adds the given user/group name to the reserved "manager"/"operator" userset
 * (CS-2394), creating the userset (type US_ACL) on first use, then spools it and
 * emits the corresponding userset event. The UM_LIST/UO_LIST GDI interface is
 * unchanged: @a ep is still a UM_Type/UO_Type element carrying the name.
 *
 * @param packet  GDI packet of the request
 * @param task    GDI task (unused)
 * @param ep      request element (UM_Type/UO_Type) carrying the name to add
 * @param alpp    answer list for messages
 * @param ruser   user that triggered the request
 * @param rhost   host from which the request was triggered
 * @param target  UM_LIST (manager) or UO_LIST (operator)
 * @return STATUS_OK, or a STATUS_* error code
 */
int
sge_add_manop(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **alpp, char *ruser, char *rhost, ocs::gdi::Target target) {
   const char *manop_name;
   const char *object_name;
   const char *userset_name;
   int pos;
   int key;

   DENTER(TOP_LAYER);

   if (!ep || !ruser || !rhost) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   switch (target) {
      case ocs::gdi::Target::UM_LIST:
         object_name = MSG_OBJ_MANAGER;
         userset_name = MANAGER_USERSET;
         key = UM_name;
         break;
      case ocs::gdi::Target::UO_LIST:
         object_name = MSG_OBJ_OPERATOR;
         userset_name = OPERATOR_USERSET;
         key = UO_name;
         break;
      default :
         DPRINTF("unknown target passed to %s\n", __func__);
         DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no acl element, if ep has no UM_name/UO_name */
   if ((pos = lGetPosViaElem(ep, key, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(key), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   manop_name = lGetPosString(ep, pos);
   if (!manop_name) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* locate the reserved userset; reject a duplicate entry */
   lList **master_userset_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_USERSET);
   lListElem *userset = lGetElemStrRW(*master_userset_list, US_name, userset_name);
   if (userset != nullptr && lGetSubStr(userset, UE_name, manop_name, US_entries) != nullptr) {
      ERROR(MSG_SGETEXT_ALREADYEXISTS_SS, object_name, manop_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* create the reserved userset (US_ACL) on first use */
   bool created_userset = false;
   if (userset == nullptr) {
      userset = lAddElemStr(master_userset_list, US_name, userset_name, US_Type);
      lSetUlong(userset, US_type, US_ACL);
      created_userset = true;
   }

   /* add the entry to the userset */
   lAddSubStr(userset, UE_name, manop_name, US_entries, UE_Type);

   /* spool the userset and emit the userset event */
   ev_event eve = created_userset ? sgeE_USERSET_ADD : sgeE_USERSET_MOD;
   if (!sge_event_spool(alpp, 0, eve,
                        0, 0, userset_name, nullptr, nullptr,
                        userset, nullptr, nullptr, true, true, packet->gdi_session)) {
      ERROR(MSG_CANTSPOOL_SS, object_name, manop_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);

      /* roll back */
      lDelSubStr(userset, UE_name, manop_name, US_entries);
      if (created_userset) {
         lRemoveElem(*master_userset_list, &userset);
      }
      DRETURN(STATUS_EDISK);
   }

   INFO(MSG_SGETEXT_ADDEDTOLIST_SSSS, ruser, rhost, manop_name, object_name);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}

/**
 * @brief Delete a manager or operator.
 *
 * Removes the given user/group name from the reserved "manager"/"operator"
 * userset (CS-2394), then spools the userset and emits a userset MOD event. The
 * reserved userset itself is kept even when it becomes empty. Removal of "root"
 * and of the configured admin user is refused. The UM_LIST/UO_LIST GDI interface
 * is unchanged.
 *
 * @param packet  GDI packet of the request
 * @param task    GDI task (unused)
 * @param ep      request element (UM_Type/UO_Type) carrying the name to remove
 * @param alpp    answer list for messages
 * @param ruser   user that triggered the request
 * @param rhost   host from which the request was triggered
 * @param target  UM_LIST (manager) or UO_LIST (operator)
 * @return STATUS_OK, or a STATUS_* error code
 *
 * MT-NOTE: sge_del_manop() is MT safe - if we hold the global lock.
 */
int
sge_del_manop(ocs::gdi::Packet *packet, ocs::gdi::Task *task, lListElem *ep, lList **alpp, char *ruser, char *rhost, ocs::gdi::Target target) {
   int pos;
   const char *manop_name;
   const char *object_name;
   const char *userset_name;
   int key = NoName;

   DENTER(TOP_LAYER);

   if (ep == nullptr || ruser == nullptr || rhost == nullptr) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   switch (target) {
      case ocs::gdi::Target::UM_LIST:
         object_name = MSG_OBJ_MANAGER;
         userset_name = MANAGER_USERSET;
         key = UM_name;
         break;
      case ocs::gdi::Target::UO_LIST:
         object_name = MSG_OBJ_OPERATOR;
         userset_name = OPERATOR_USERSET;
         key = UO_name;
         break;
      default :
         DPRINTF("unknown target passed to %s\n", __func__);
         DRETURN(STATUS_EUNKNOWN);
   }

   /* ep is no manop element, if ep has no UM_name/UO_name */
   if ((pos = lGetPosViaElem(ep, key, SGE_NO_ABORT)) < 0) {
      CRITICAL(MSG_SGETEXT_MISSINGCULLFIELD_SS, lNm2Str(key), __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   manop_name = lGetPosString(ep, pos);
   if (manop_name == nullptr) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EUNKNOWN);
   }

   /* prevent removing of root from man/op-list */
   if (strcmp(manop_name, "root") == 0) {
      ERROR(MSG_SGETEXT_MAY_NOT_REMOVE_USER_FROM_LIST_SS, "root", object_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* prevent removing the admin user from man/op-list */
   const char *admin_user = ocs::Bootstrap::get_admin_user();
   if (strcmp(manop_name, admin_user) == 0) {
      ERROR(MSG_SGETEXT_MAY_NOT_REMOVE_USER_FROM_LIST_SS, admin_user, object_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   lList **master_userset_list = ocs::DataStore::get_master_list_rw(SGE_TYPE_USERSET);
   lListElem *userset = lGetElemStrRW(*master_userset_list, US_name, userset_name);
   if (userset == nullptr || lGetSubStr(userset, UE_name, manop_name, US_entries) == nullptr) {
      ERROR(MSG_SGETEXT_DOESNOTEXIST_SS, object_name, manop_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
      DRETURN(STATUS_EEXIST);
   }

   /* remove the entry; the reserved userset itself is kept */
   lDelSubStr(userset, UE_name, manop_name, US_entries);

   if (!sge_event_spool(alpp, 0, sgeE_USERSET_MOD,
                        0, 0, userset_name, nullptr, nullptr,
                        userset, nullptr, nullptr, true, true, packet->gdi_session)) {
      ERROR(MSG_CANTSPOOL_SS, object_name, manop_name);
      answer_list_add(alpp, SGE_EVENT, STATUS_EDISK, ANSWER_QUALITY_ERROR);

      /* roll back */
      lAddSubStr(userset, UE_name, manop_name, US_entries, UE_Type);

      DRETURN(STATUS_EDISK);
   }

   INFO(MSG_SGETEXT_REMOVEDFROMLIST_SSSS, ruser, rhost, manop_name, object_name);
   answer_list_add(alpp, SGE_EVENT, STATUS_OK, ANSWER_QUALITY_INFO);
   DRETURN(STATUS_OK);
}

