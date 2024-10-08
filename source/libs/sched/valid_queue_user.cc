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
#include <cstring>

#include "uti/sge_hostname.h"
#include "uti/sge_rmon_macros.h"

#include "cull/cull.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_userset.h"
#include "sgeobj/sge_advance_reservation.h"

#include "valid_queue_user.h"
#include "msg_qmaster.h"

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -

   sge_has_access - determines if a user/group has access to a queue

   returns
      1 for true
      0 for false

*/
int
sge_has_access(const char *user, const char *group, const lList *grp_list,
               const lListElem *q, const lList *acl_list) {
   return sge_has_access_(user, group, grp_list, lGetList(q, QU_acl), lGetList(q, QU_xacl), acl_list);
}

/* needed to test also sge_queue_type structures without converting the
** whole queue
*/
int
sge_has_access_(const char *user, const char *group, const lList *grp_list,
                const lList *q_acl, const lList *q_xacl, const lList *acl_list) {
   int ret;

   DENTER(TOP_LAYER);

   ret = sge_contained_in_access_list_(user, group, grp_list, q_xacl, acl_list);
   if (ret < 0 || ret == 1) { /* also deny when an xacl entry was not found in acl_list */
      DRETURN(0);
   }

   if (!q_acl) {  /* no entry in allowance list - ok everyone */
       DRETURN(1);
   }

   ret = sge_contained_in_access_list_(user, group, grp_list, q_acl, acl_list);
   if (ret < 0) {
      DRETURN(0);
   }
   if (ret) {
      /* we're explicitly allowed to access */
      DRETURN(1);
   }

   /* there is an allowance list but the owner isn't there -> denied */
   DRETURN(0);
} 


/* sge_contained_in_access_list_() returns 
   1  yes it is contained in the acl
   0  no 
   -1 got nullptr for user/group

   user, group: may be nullptr
*/
int
sge_contained_in_access_list_(const char *user, const char *group, const lList *grp_list,
                              const lList *acl, const lList *acl_list) {
   DENTER(TOP_LAYER);

   const lListElem *acl_search;
   for_each_ep(acl_search, acl) {
      const lListElem *acl_found = lGetElemStr(acl_list, US_name, lGetString(acl_search, US_name));
      if (acl_found != nullptr) {
         /* ok - there is such an access list */
         if (sge_contained_in_access_list(user, group, grp_list, acl_found)) {
            DRETURN(1);
         } 
      } else {
      	DPRINTF("cannot find userset list entry \"%s\"\n", lGetString(acl_search, US_name));
      }
   }
   DRETURN(0);
}

/****** valid_queue_user/sge_ar_queue_have_users_access() ***********************
*  NAME
*     sge_ar_queue_have_users_access() -- verify that all users of an AR have queue
*                                        access
*
*  SYNOPSIS
*     bool sge_ar_queue_have_users_access(lList **alpp, lListElem *ar, lListElem 
*     *queue, lList *master_userset_list) 
*
*  FUNCTION
*     Iterates over the AR_acl_list and proves that every entry has queue access.
*     If only one has no access the function returns false
*
*  INPUTS
*     lList **alpp               - answer list
*     lListElem *ar              - advance reservation object (AR_Type)
*     lListElem *queue           - queue instance object (QU_Type)
*     lList *master_userset_list - master userset list
*
*  RESULT
*     bool - true if all have access
*            false if only one has no access
*
*  NOTES
*     MT-NOTE: sge_ar_queue_have_users_access() is MT safe 
*******************************************************************************/
bool sge_ar_have_users_access(lList **alpp, lListElem *ar, const char *name, const lList *acl_list,
                                    const lList *xacl_list, const lList *master_userset_list)
{
   bool ret = true;
   const lListElem *acl_entry;
   const char *user= nullptr;

   DENTER(TOP_LAYER);

   for_each_ep(acl_entry, lGetList(ar, AR_acl_list)) {
      user = lGetString(acl_entry, ARA_name);

      DPRINTF("check permissions for user %s\n", user);
      if (!is_hgroup_name(user)) {
         if (sge_has_access_(user, lGetString(acl_entry, ARA_group), nullptr,
                             acl_list, xacl_list,master_userset_list) == 0) {
             answer_list_add_sprintf(alpp, STATUS_OK, ANSWER_QUALITY_INFO, MSG_AR_QUEUEDNOPERMISSIONS, name); 
            ret = false;
            break;
         }
      } else {
         /* skip preattached \@ sign */
         const char *acl_name = ++user;

         DPRINTF("acl :%s", acl_name);

         /* at first xacl */
         if (xacl_list && lGetElemStr(xacl_list, US_name, acl_name) != nullptr) {
            ret = false;
            break;
         }

         /* at second acl */
         if (acl_list && (lGetElemStr(acl_list, US_name, acl_name) == nullptr)) {
            answer_list_add_sprintf(alpp, STATUS_OK, ANSWER_QUALITY_INFO, MSG_AR_QUEUEDNOPERMISSIONS, name); 
            ret = false;
            break;
         }
      }
   }

   DRETURN(ret);
}
