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
 * @brief qconf - the userset and manager/operator list switches
 */

#include <cstdio>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userset.h"

#include "gdi/ocs_gdi_Client.h"

#include "cull/cull.h"

#include "ocs_qconf_acl.h"
#include "msg_qconf.h"

/** @brief Add users to access lists, the `qconf -au` switch
 *
 * Every named user is added to every named list, so one invocation can be a
 * cross product rather than a single pair.
 *
 * CS-2754: one GET and at most one write PER ACCESS LIST, not per (user, list)
 * pair. The old shape fetched the whole user set and sent the whole user set
 * back once for every single user, so the payload grew with the list while the
 * request count grew with the users -- quadratic on the wire for what is one
 * administrative command.
 *
 * The per-pair messages are unchanged; they were always produced here rather
 * than by the qmaster. Only their source moves: when the single write fails,
 * its text goes to every user of that batch instead of to the one user whose
 * own request failed.
 *
 * @param alpp used to return error messages
 * @param user_args the users to add (`UE_Type`)
 * @param acl_args the access lists to add them to (`US_Type`)
 * @return 0 on success, -1 on error, with `alpp` filled
 */
int
sge_client_add_user(lList **alpp, lList *user_args, lList *acl_args) {
   DENTER(TOP_LAYER);

   lEnumeration *what = lWhat("%T(ALL)", US_Type);

   for_each_ep_lv(aclarg, acl_args) {
      const char *acl_name = lGetString(aclarg, US_name);
      lCondition *where = lWhere("%T(%I==%s)", US_Type, US_name, acl_name);
      lList *acl = nullptr;

      /* get the old acl -- once for all users */
      lList *answers = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::US_LIST, ocs::gdi::Command::GET,
                                                 ocs::gdi::SubCommand::NONE, &acl, where, what);
      lFreeList(&answers);
      lFreeWhere(&where);

      const bool acl_exists = (acl != nullptr && lGetNumberOfElem(acl) > 0);

      if (!acl_exists) {
         /* the list does not exist yet -- one ADD creates it with every user in it */
         lFreeList(&acl);
         lAddElemStr(&acl, US_name, acl_name, US_Type);
      }
      lListElem *acl_elem = lFirstRW(acl);

      /*
       * Decide locally who has to be added. "added" is also what tells the
       * message loop below which users this request carried, so a user named
       * twice gets the success message once and "already in list" for the
       * repeat, exactly as the per-user version did.
       */
      lList *added = nullptr;
      for_each_ep_lv(userarg, user_args) {
         const char *user_name = lGetString(userarg, UE_name);

         if (lGetSubStr(acl_elem, UE_name, user_name, US_entries) == nullptr) {
            lAddSubStr(acl_elem, UE_name, user_name, US_entries, UE_Type);
            lAddElemStr(&added, UE_name, user_name, UE_Type);
         }
      }

      /* one write for all of them */
      uint32_t write_status = STATUS_OK;
      char *write_text = nullptr;
      if (!acl_exists || lGetNumberOfElem(added) > 0) {
         answers = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::US_LIST,
                                             acl_exists ? ocs::gdi::Command::MOD : ocs::gdi::Command::ADD,
                                             ocs::gdi::SubCommand::NONE, &acl, nullptr, nullptr);
         write_status = lGetUlong(lFirst(answers), AN_status);
         if (write_status != STATUS_OK) {
            write_text = sge_strdup(nullptr, lGetString(lFirst(answers), AN_text));
         }
         lFreeList(&answers);
      }

      /* report in the order the users were named */
      for_each_ep_lv(userarg, user_args) {
         const char *user_name = lGetString(userarg, UE_name);
         lListElem *carried = lGetElemStrRW(added, UE_name, user_name);
         uint32_t status;

         if (carried == nullptr) {
            status = STATUS_EEXIST;
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_USERINACL_SS, user_name, acl_name);
         } else {
            lRemoveElem(added, &carried);
            status = write_status;
            if (status == STATUS_OK) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_ADDTOACL_SS, user_name, acl_name);
            } else if (write_text != nullptr) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s", write_text);
            } else {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_CANTADDTOACL_SS, user_name, acl_name);
            }
         }
         answer_list_add(alpp, SGE_EVENT, status,
                         ((status == STATUS_OK) ? ANSWER_QUALITY_INFO : ANSWER_QUALITY_ERROR));
      }

      sge_free(&write_text);
      lFreeList(&added);
      lFreeList(&acl);
   }
   lFreeWhat(&what);

   DRETURN(0);
}

/** @brief Remove users from access lists, the `qconf -du` switch
 *
 * The counterpart of #sge_client_add_user, and a cross product in the same way.
 * CS-2754: one GET and at most one MOD per access list, see there.
 *
 * @param alpp used to return error messages
 * @param user_args the users to remove (`UE_Type`)
 * @param acl_args the access lists to remove them from (`US_Type`)
 * @return 0 on success, -1 on error, with `alpp` filled
 */
int
sge_client_del_user(lList **alpp, lList *user_args, lList *acl_args) {
   DENTER(TOP_LAYER);

   lEnumeration *what = lWhat("%T(ALL)", US_Type);

   for_each_ep_lv(aclarg, acl_args) {
      const char *acl_name = lGetString(aclarg, US_name);
      lCondition *where = lWhere("%T(%I==%s)", US_Type, US_name, acl_name);
      lList *acl = nullptr;

      /* get the old acl -- once for all users */
      lList *answers = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::US_LIST, ocs::gdi::Command::GET,
                                                 ocs::gdi::SubCommand::NONE, &acl, where, what);
      lFreeList(&answers);
      lFreeWhere(&where);

      if (acl == nullptr || lGetNumberOfElem(acl) == 0) {
         /*
          * One message for the list, not one per user: the per-user version
          * reported this for the first user and then left the user loop.
          */
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_ACLDOESNOTEXIST_S, acl_name);
         answer_list_add(alpp, SGE_EVENT, STATUS_EEXIST, ANSWER_QUALITY_ERROR);
         lFreeList(&acl);
         continue;
      }
      lListElem *acl_elem = lFirstRW(acl);

      /* decide locally who actually has to go */
      lList *removed = nullptr;
      for_each_ep_lv(userarg, user_args) {
         const char *user_name = lGetString(userarg, UE_name);

         if (lGetSubStr(acl_elem, UE_name, user_name, US_entries) != nullptr) {
            lDelSubStr(acl_elem, UE_name, user_name, US_entries);
            lAddElemStr(&removed, UE_name, user_name, UE_Type);
         }
      }

      /* one write for all of them */
      uint32_t write_status = STATUS_OK;
      char *write_text = nullptr;
      if (lGetNumberOfElem(removed) > 0) {
         answers = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::US_LIST, ocs::gdi::Command::MOD,
                                             ocs::gdi::SubCommand::NONE, &acl, nullptr, nullptr);
         write_status = lGetUlong(lFirst(answers), AN_status);
         if (write_status != STATUS_OK) {
            write_text = sge_strdup(nullptr, lGetString(lFirst(answers), AN_text));
         }
         lFreeList(&answers);
      }

      /* report in the order the users were named */
      for_each_ep_lv(userarg, user_args) {
         const char *user_name = lGetString(userarg, UE_name);
         lListElem *carried = lGetElemStrRW(removed, UE_name, user_name);
         uint32_t status;

         if (carried == nullptr) {
            status = STATUS_EEXIST + 1;
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_USERNOTINACL_SS, user_name, acl_name);
         } else {
            lRemoveElem(removed, &carried);
            status = write_status;
            if (status == STATUS_OK) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_DELFROMACL_SS, user_name, acl_name);
            } else if (write_text != nullptr) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s", write_text);
            } else {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_CANTDELFROMACL_SS, user_name, acl_name);
            }
         }
         answer_list_add(alpp, SGE_EVENT, status,
                         ((status == STATUS_OK) ? ANSWER_QUALITY_INFO : ANSWER_QUALITY_ERROR));
      }

      sge_free(&write_text);
      lFreeList(&removed);
      lFreeList(&acl);
   }
   lFreeWhat(&what);

   DRETURN(0);
}

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -   

   sge_client_get_acls()

   acl_args 
      a list containing US_name fields 
   
   try to get all acls named in acl_args from qmaster

   returns 
      -1 on error
      0 on success

*/
/** @brief Fetch the named access lists from qmaster
 *
 * @param alpp used to return error messages
 * @param acl_args the lists to fetch, as elements carrying `US_name`
 * @param dst receives the fetched access lists (`US_Type`)
 * @return 0 on success, -1 on error, with `alpp` filled
 */
int
sge_client_get_acls(lList **alpp, lList *acl_args, lList **dst) {
   DENTER(TOP_LAYER);
   lList *answers;
   lCondition *where, *newcp;
   lEnumeration *what;
   const char *acl_name;

   where = nullptr;
   for_each_ep_lv(aclarg, acl_args) {
      acl_name = lGetString(aclarg, US_name);
      newcp = lWhere("%T(%I==%s)", US_Type, US_name, acl_name);
      if (where == nullptr) {
         where = newcp;
      } else {
         where = lOrWhere(where, newcp);
      }
   }
   what = lWhat("%T(ALL)", US_Type);
   answers = ocs::gdi::Client::sge_gdi(ocs::gdi::Target::US_LIST, ocs::gdi::Command::GET, ocs::gdi::SubCommand::NONE, dst, where, what);
   lFreeWhat(&what);
   lFreeWhere(&where);

   answer_list_append_list(alpp, &answers);
  
   /*
    * if nullptr was passwd to alpp, answers will not be
    * freed in answer_list_append_list!
    */
   lFreeList(&answers);

   DRETURN(0);
}
