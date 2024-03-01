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
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstdio>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_userset.h"

#include "sgeobj/sge_daemonize.h"
#include "gdi/sge_gdi2.h"

#include "cull/cull.h"

#include "oge_qconf_acl.h"
#include "msg_qconf.h"

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -

   user_args - a cull list(UE_Type) of users
   acl_args - a cull list(US_Type) of acl

   returns 
      0 on success
      -1 on error

*/
int
sge_client_add_user(lList **alpp, lList *user_args, lList *acl_args) {
   const lListElem *userarg, *aclarg;
   lList *acl=nullptr, *answers=nullptr;
   const char *acl_name, *user_name;
   lCondition *where;
   lEnumeration *what;
   u_long32 status;
   int already;

   DENTER(TOP_LAYER);

   what = lWhat("%T(ALL)", US_Type);

   for_each_ep(aclarg,acl_args) {
      acl_name = lGetString(aclarg, US_name);
      where = lWhere("%T(%I==%s)", US_Type, US_name, acl_name);

      for_each_ep(userarg, user_args) {

         already = 0;
         user_name=lGetString(userarg, UE_name);
   
         /* get old acl */
         answers = sge_gdi2(SGE_US_LIST, SGE_GDI_GET, &acl, where, what);
         lFreeList(&answers);

         if (acl && lGetNumberOfElem(acl) > 0) {
            if (!lGetSubStr(lFirst(acl), UE_name, user_name, US_entries)) {
               lAddSubStr(lFirstRW(acl), UE_name, user_name, US_entries, UE_Type);

               /* mod the acl */
               answers = sge_gdi2(SGE_US_LIST, SGE_GDI_MOD, &acl, nullptr, nullptr);
            } else {
               already = 1;
            }
         } else {
            /* build new list */
            lAddElemStr(&acl, US_name, acl_name, US_Type);
            lAddSubStr(lFirstRW(acl), UE_name, user_name, US_entries, UE_Type);
            
            /* add the acl */
            answers = sge_gdi2(SGE_US_LIST, SGE_GDI_ADD, &acl, nullptr, nullptr);
         }

         if (already) {
            status = STATUS_EEXIST;
             snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_USERINACL_SS, user_name, acl_name);
         }
         else {
            if ((status = lGetUlong(lFirst(answers), AN_status))!=STATUS_OK) {
               const char *cp;
            
               cp = lGetString(lFirst(answers), AN_text);
               if (cp) {
                  snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s", cp);
               }
               else {
                   snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_CANTADDTOACL_SS, user_name, acl_name);
               }
            } else {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_ADDTOACL_SS, user_name, acl_name);
            }
            lFreeList(&answers);
         }
         answer_list_add(alpp, SGE_EVENT, status, 
            ((status == STATUS_OK) ? ANSWER_QUALITY_INFO : ANSWER_QUALITY_ERROR));
         lFreeList(&acl);

      }
      lFreeWhere(&where);
   }
   lFreeWhat(&what);

   DRETURN(0);
}

/* - -- -- -- -- -- -- -- -- -- -- -- -- -- -

   user_args - a cull list(UE_Type) of users
   acl_args - a cull list(US_Type) of acl

   returns 
      0 on success
      -1 on error

*/
int
sge_client_del_user(lList **alpp, lList *user_args, lList *acl_args) {
   const lListElem *userarg, *aclarg;
   lList *acl=nullptr, *answers=nullptr;
   const char *acl_name, *user_name;
   lCondition *where;
   lEnumeration *what;
   u_long32 status;

   DENTER(TOP_LAYER);

   what = lWhat("%T(ALL)", US_Type);

   for_each_ep(aclarg,acl_args) {
      acl_name = lGetString(aclarg, US_name);
      where = lWhere("%T(%I==%s)", US_Type, US_name, acl_name);

      for_each_ep(userarg, user_args) {
         int breakit = 0;
         char *cp = nullptr;
         user_name=lGetString(userarg, UE_name);
         /* get old acl */
         answers = sge_gdi2(SGE_US_LIST, SGE_GDI_GET, &acl, where, what);
         cp = sge_strdup(cp, lGetString(lFirst(answers), AN_text));
         lFreeList(&answers);
         if (acl && lGetNumberOfElem(acl) > 0) {
            sge_free(&cp);
            if (lGetSubStr(lFirst(acl), UE_name, user_name, US_entries)) {
               lDelSubStr(lFirstRW(acl), UE_name, user_name, US_entries);
               answers = sge_gdi2(SGE_US_LIST, SGE_GDI_MOD, &acl, nullptr, nullptr);
               cp = sge_strdup(cp, lGetString(lFirst(answers), AN_text));
               status = lGetUlong(lFirst(answers), AN_status);
               lFreeList(&answers);
            }
            else
               status = STATUS_EEXIST + 1;
         }
         else 
            status = STATUS_EEXIST;

         if (status != STATUS_OK) {
            if (status == STATUS_EEXIST) {
                snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_ACLDOESNOTEXIST_S, acl_name);
               breakit = 1;        
            }
	    else if (status == STATUS_EEXIST + 1) {
                snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_USERNOTINACL_SS, user_name, acl_name);
            }
            else if (cp) {
               snprintf(SGE_EVENT, SGE_EVENT_SIZE, "%s", cp);
            }
	    else {
                snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_CANTDELFROMACL_SS, user_name, acl_name);
            }

         }
         else {
            snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ACL_DELFROMACL_SS, user_name, acl_name);
         }
         answer_list_add(alpp, SGE_EVENT, status, 
                        ((status == STATUS_OK) ? ANSWER_QUALITY_INFO : ANSWER_QUALITY_ERROR));
         lFreeList(&acl);
         
         if (cp) {
            sge_free(&cp);
         }
         if (breakit)
            break;
            
      }
      lFreeWhere(&where);
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
int
sge_client_get_acls(lList **alpp, lList *acl_args, lList **dst) {
   lList *answers;
   const lListElem *aclarg;
   lCondition *where, *newcp;
   lEnumeration *what;
   const char *acl_name;
   
   DENTER(TOP_LAYER);

   where = nullptr;
   for_each_ep(aclarg, acl_args) {
      acl_name = lGetString(aclarg, US_name);
      newcp = lWhere("%T(%I==%s)", US_Type, US_name, acl_name);
      if (where == nullptr) {
         where = newcp;
      } else {
         where = lOrWhere(where, newcp);
      }
   }
   what = lWhat("%T(ALL)", US_Type);
   answers = sge_gdi2(SGE_US_LIST, SGE_GDI_GET, dst, where, what);
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
